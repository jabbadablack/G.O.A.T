#include <SmartObject/SmartObjectActions.h>

#include <SmartObject/SmartObjectRegistry.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Console/IConsole.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>

namespace GOAT_SmartObject
{
    namespace
    {
        //! How far a claim looks when a node names no radius, in metres.
        AZ_CVAR(float, goat_smartObjectRadius, 20.0f, nullptr, AZ::ConsoleFunctorFlags::Null,
            "How far GOAT looks for a smart object when a claim_smart_object node names no radius");

        //! Seconds this agent has been using the object, kept in the agent's scratch.
        float& Elapsed(const GOAT::ActionContext& context)
        {
            return *reinterpret_cast<float*>(context.m_scratch->data());
        }

        static_assert(sizeof(float) <= AZStd::tuple_size<GOAT::ActionScratch>::value,
            "Smart object state does not fit in the action scratch");

        //! Declares one variable, treating an already declared name as success.
        //! Re-declaration happens whenever a level reloads, and is not an error.
        GOAT::BlackboardKey DeclareOne(
            GOAT::IBlackboardSystem& blackboard, const AZ::Name& name, GOAT::BlackboardType type, AZStd::any defaultValue)
        {
            auto declared = blackboard.Declare(name, GOAT::BlackboardScope::Agent, type, AZStd::move(defaultValue));
            if (declared.IsSuccess())
            {
                return declared.GetValue();
            }

            const GOAT::BlackboardKey existing = blackboard.FindKey(name);
            AZ_Error("GOAT", existing.IsValid(), "Smart object variable '%s' could not be declared: %s",
                name.GetCStr(), declared.GetError().c_str());
            return existing;
        }
    } // namespace

    bool SmartObjectKeys::Declare(GOAT::IBlackboardSystem& blackboard)
    {
        m_entity = DeclareOne(
            blackboard, AZ_NAME_LITERAL("so_entity"), GOAT::BlackboardType::EntityId, AZStd::any(AZ::EntityId{}));
        m_anchor = DeclareOne(
            blackboard, AZ_NAME_LITERAL("so_anchor"), GOAT::BlackboardType::Vector3, AZStd::any(AZ::Vector3::CreateZero()));
        m_use = DeclareOne(
            blackboard, AZ_NAME_LITERAL("so_use"), GOAT::BlackboardType::Name, AZStd::any(AZ::Name{}));

        AZ_Assert(IsValid(), "Every smart object blackboard variable must resolve to a key");
        return IsValid();
    }

    bool SmartObjectKeys::IsValid() const
    {
        return m_entity.IsValid() && m_anchor.IsValid() && m_use.IsValid();
    }

    ClaimSmartObjectAction::ClaimSmartObjectAction(SmartObjectRegistry& registry, const SmartObjectKeys& keys)
        : m_registry(registry)
        , m_keys(keys)
    {
        AZ_Assert(m_keys.IsValid(), "claim_smart_object needs its blackboard variables declared before it runs");
    }

    AZ::Name ClaimSmartObjectAction::GetName() const
    {
        return AZ_NAME_LITERAL("claim_smart_object");
    }

    void ClaimSmartObjectAction::Begin([[maybe_unused]] const GOAT::ActionContext& context)
    {
    }

    GOAT::ActionResult ClaimSmartObjectAction::Step(
        const GOAT::ActionContext& context, [[maybe_unused]] float deltaTime)
    {
        AZ_Assert(context.m_request != nullptr, "A claim always runs with a request");
        AZ_Assert(context.m_blackboard != nullptr, "A claim always runs with a blackboard");

        const AZ::Name use = context.m_request->m_tag;
        AZ_Assert(!use.IsEmpty(), "A claim_smart_object leaf always names the use it wants");
        if (use.IsEmpty())
        {
            AZ_Error("GOAT", false, "claim_smart_object ran with no use named, so nothing could match");
            return GOAT::ActionResult::Failure;
        }

        AZ::Vector3 from = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(from, context.m_entity, &AZ::TransformInterface::GetWorldTranslation);

        const float radius = context.m_request->m_amount > 0.0f ? context.m_request->m_amount : goat_smartObjectRadius;
        AZ_Assert(radius > 0.0f, "A smart object search radius must be positive");

        const SmartObjectClaim claim = m_registry.Claim(context.m_agent, use, from, radius);
        if (!claim.IsValid())
        {
            return GOAT::ActionResult::Failure;
        }

        // Published rather than returned, so whatever moves the agent can read it without this
        // module knowing that gem exists.
        context.m_blackboard->Set<AZ::EntityId>(m_keys.m_entity, claim.m_entity, context.m_agent);
        context.m_blackboard->Set<AZ::Vector3>(m_keys.m_anchor, claim.m_anchor, context.m_agent);
        context.m_blackboard->Set<AZ::Name>(m_keys.m_use, use, context.m_agent);

        return GOAT::ActionResult::Success;
    }

    void ClaimSmartObjectAction::End([[maybe_unused]] const GOAT::ActionContext& context)
    {
        // The claim deliberately outlives this leaf: the agent still has to travel there.
        // It is given back by use_smart_object, by the next claim, or when the object goes away.
    }

    UseSmartObjectAction::UseSmartObjectAction(SmartObjectRegistry& registry, const SmartObjectKeys& keys)
        : m_registry(registry)
        , m_keys(keys)
    {
        AZ_Assert(m_keys.IsValid(), "use_smart_object needs its blackboard variables declared before it runs");
    }

    AZ::Name UseSmartObjectAction::GetName() const
    {
        return AZ_NAME_LITERAL("use_smart_object");
    }

    void UseSmartObjectAction::Begin(const GOAT::ActionContext& context)
    {
        AZ_Assert(context.m_scratch != nullptr, "A use always runs with agent scratch to time it");
        Elapsed(context) = 0.0f;
    }

    GOAT::ActionResult UseSmartObjectAction::Step(const GOAT::ActionContext& context, float deltaTime)
    {
        AZ_Assert(deltaTime >= 0.0f, "A use cannot be stepped backwards in time");

        const AZ::EntityId* claimed = context.m_blackboard->Find<AZ::EntityId>(m_keys.m_entity, context.m_agent);
        if (claimed == nullptr || !claimed->IsValid())
        {
            AZ_Warning("GOAT", false, "use_smart_object ran for agent %u, which holds no claim",
                context.m_agent.GetIndex());
            return GOAT::ActionResult::Failure;
        }

        float& elapsed = Elapsed(context);
        elapsed += deltaTime;

        return elapsed >= context.m_request->m_amount ? GOAT::ActionResult::Success : GOAT::ActionResult::Running;
    }

    void UseSmartObjectAction::End(const GOAT::ActionContext& context)
    {
        // In End rather than on success, so an aborted branch gives the slot back too.
        m_registry.Release(context.m_agent);

        context.m_blackboard->Set<AZ::EntityId>(m_keys.m_entity, AZ::EntityId{}, context.m_agent);
    }
} // namespace GOAT_SmartObject
