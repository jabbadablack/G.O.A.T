#include <Core/Scripting/LuaPlanBuilder.h>

#include <AzCore/Console/IConsole.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContextAttributes.h>

namespace GOAT
{
    namespace
    {
        //! Catches a backend that appends without end. Plans are otherwise unbounded on purpose,
        //! so this is a guard against a bug, not a design limit -- raise it if a project needs to.
        AZ_CVAR(AZ::u32, goat_maxPlanSteps, 4096, nullptr, AZ::ConsoleFunctorFlags::Null,
            "Most steps one plan may hold before GOAT treats it as a runaway backend");
    } // namespace

    void LuaPlanBuilder::Configure(
        const ActionStateRegistry* actions, const IBlackboardSystem* blackboard, PlanStore* store)
    {
        m_actions = actions;
        m_blackboard = blackboard;
        m_store = store;
    }

    void LuaPlanBuilder::BeginPlan()
    {
        // The scratch keeps its capacity, which is what makes a plan boundary allocation free.
        m_scratch.clear();
        m_plan = ActionPlan{};
        m_sourcePlan.clear();
        m_sourceOption = 0;
        m_failed = false;
    }

    void LuaPlanBuilder::AddStep(AZStd::string verb)
    {
        if (m_failed)
        {
            return;
        }

        if (m_scratch.size() >= goat_maxPlanSteps)
        {
            AZ_Error("GOAT", false,
                "A Lua backend produced more than %u steps, which is goat_maxPlanSteps; "
                "this is a runaway backend rather than a long plan",
                static_cast<AZ::u32>(goat_maxPlanSteps));
            m_failed = true;
            return;
        }

        const AZ::Name verbName(verb);

        // A plan runs verbs. `delegate` is a tree word, so naming it here would be the one way a
        // plan could re-enter the tree that produced it; it cannot, because verbs and node types
        // live in different registries and nothing may register a verb under that name.
        const ActionStateId id = m_actions != nullptr ? m_actions->FindId(verbName) : CoreActions::Invalid;
        if (id == CoreActions::Invalid)
        {
            AZ_Warning("GOAT", false, "A Lua backend asked for unregistered verb '%s'", verbName.GetCStr());
            m_failed = true;
            return;
        }

        ActionRequest request;
        request.m_action = id;
        m_scratch.push_back(AZStd::move(request));
    }

    void LuaPlanBuilder::SetTag(AZStd::string tag)
    {
        if (!m_failed && !m_scratch.empty())
        {
            m_scratch.back().m_tag = AZ::Name(tag);
        }
    }

    void LuaPlanBuilder::SetDuration(double seconds)
    {
        if (!m_failed && !m_scratch.empty())
        {
            m_scratch.back().m_amount = static_cast<float>(seconds);
        }
    }

    void LuaPlanBuilder::SetTolerance(double tolerance)
    {
        if (!m_failed && !m_scratch.empty())
        {
            m_scratch.back().m_tolerance = static_cast<float>(tolerance);
        }
    }

    void LuaPlanBuilder::SetTargetKey(AZStd::string blackboardName)
    {
        if (m_failed || m_scratch.empty() || m_blackboard == nullptr)
        {
            return;
        }

        const BlackboardKey key = m_blackboard->FindKey(AZ::Name(blackboardName));
        if (!key.IsValid())
        {
            AZ_Warning("GOAT", false, "A Lua backend referred to undeclared variable '%s'", blackboardName.c_str());
            m_failed = true;
            return;
        }
        m_scratch.back().m_targetKey = key;
    }

    void LuaPlanBuilder::SetTargetPosition(const AZ::Vector3& position)
    {
        if (!m_failed && !m_scratch.empty())
        {
            m_scratch.back().m_position = position;
        }
    }

    void LuaPlanBuilder::SetTargetEntity(AZ::EntityId entity)
    {
        // Snapshotted here, not read when the step runs. A key resolves at action time and so
        // survives the world changing mid plan; a literal does not. Prefer a key where a verb
        // supports one.
        if (!m_failed && !m_scratch.empty())
        {
            m_scratch.back().m_targetEntity = entity;
        }
    }

    bool LuaPlanBuilder::BakeOption(AZStd::string plan, double option)
    {
        AZ_Assert(m_store != nullptr, "Baking a plan needs a store to bake into");

        // Baking the same option twice is a no op rather than a second copy. Every script load
        // re-walks every declared plan, and an agent's plan is a span into what was baked before:
        // clearing and re-baking would leave a running agent pointing at freed steps.
        const AZ::Name planName(plan);
        const int wanted = static_cast<int>(option);
        for (const BakedOption& baked : m_bakedOptions)
        {
            if (baked.m_plan == planName && baked.m_option == wanted)
            {
                return true;
            }
        }

        if (m_failed || m_scratch.empty() || m_store == nullptr)
        {
            AZ_Error("GOAT", false, "Option %d of plan '%s' could not be baked", static_cast<int>(option),
                plan.c_str());
            return false;
        }

        const PlanStore::Span span = m_store->Bake(m_scratch.data(), static_cast<AZ::u32>(m_scratch.size()));
        if (span.IsEmpty())
        {
            return false;
        }

        m_bakedOptions.push_back(BakedOption{ planName, wanted, span });

        AZ_Assert(span.m_block == InvalidPlanBlock, "A baked option is shared, so it is never owed back");
        return true;
    }

    void LuaPlanBuilder::ClearBaked()
    {
        m_bakedOptions.clear();
        if (m_store != nullptr)
        {
            m_store->ClearBaked();
        }

        AZ_Assert(m_bakedOptions.empty(), "Clearing must leave no baked option behind");
    }

    bool LuaPlanBuilder::ChooseBaked(AZStd::string plan, double option)
    {
        const AZ::Name planName(plan);
        const int wanted = static_cast<int>(option);

        for (const BakedOption& baked : m_bakedOptions)
        {
            if (baked.m_plan != planName || baked.m_option != wanted)
            {
                continue;
            }

            // Nothing is copied and nothing is borrowed: every agent running this option shares
            // the one run of steps baked when the vocabulary loaded.
            m_plan.m_span = baked.m_span;
            m_sourcePlan = AZStd::move(plan);
            m_sourceOption = wanted;
            m_failed = false;

            AZ_Assert(!m_plan.IsBorrowed(), "A baked plan is shared, so it is never owed back");
            return true;
        }

        AZ_Error("GOAT", false, "Plan '%s' has no baked option %d; the vocabulary and the plan disagree",
            planName.GetCStr(), wanted);
        return false;
    }

    bool LuaPlanBuilder::EndPlan()
    {
        AZ_Assert(m_store != nullptr, "Finishing a plan needs a store to borrow from");

        if (m_failed || m_scratch.empty() || m_store == nullptr)
        {
            return false;
        }

        m_plan.m_span = m_store->Acquire(m_scratch.data(), static_cast<AZ::u32>(m_scratch.size()));

        AZ_Assert(m_plan.IsBorrowed(), "A computed plan borrows its steps and owes them back");
        return !m_plan.IsEmpty();
    }

    void LuaPlanBuilder::Reflect(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->Class<LuaPlanBuilder>("GoatPlanBuilder")
            ->Attribute(AZ::Script::Attributes::Category, "GOAT")
            ->Method("BeginPlan", &LuaPlanBuilder::BeginPlan)
            ->Method("AddStep", &LuaPlanBuilder::AddStep)
            ->Method("SetTag", &LuaPlanBuilder::SetTag)
            ->Method("SetDuration", &LuaPlanBuilder::SetDuration)
            ->Method("SetTolerance", &LuaPlanBuilder::SetTolerance)
            ->Method("SetTargetKey", &LuaPlanBuilder::SetTargetKey)
            ->Method("SetTargetPosition", &LuaPlanBuilder::SetTargetPosition)
            ->Method("SetTargetEntity", &LuaPlanBuilder::SetTargetEntity)
            ->Method("ChooseBaked", &LuaPlanBuilder::ChooseBaked)
            ->Method("BakeOption", &LuaPlanBuilder::BakeOption)
            ->Method("EndPlan", &LuaPlanBuilder::EndPlan);
    }
} // namespace GOAT
