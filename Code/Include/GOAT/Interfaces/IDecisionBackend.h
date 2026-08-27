#pragma once

#include <GOAT/Assets/BehaviorTreeAsset.h>
#include <GOAT/Domain/ActionPlan.h>
#include <GOAT/Domain/AgentProgram.h>
#include <GOAT/Domain/PlanContext.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Name/Name.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! One agent's own state, in whatever shape the backend that owns it chose.
    using BrainState = AZStd::span<AZ::u8>;

    //! A compiled program, or why it could not be compiled.
    using CompileOutcome = AZ::Outcome<AZStd::shared_ptr<const AgentProgram>, AZStd::string>;

    //! The agent is running no plan, so there is nothing to re-check.
    inline constexpr size_t NoRunningStep = static_cast<size_t>(-1);

    //! What a backend wants done with the plan an agent is running.
    enum class TickResult : AZ::u8
    {
        Continue, //!< Leave it running.
        Abandon   //!< Drop it and decide again.
    };

    //! What a backend decided for one agent.
    struct Decision final
    {
        //! True when a plan was produced.
        bool m_planned = false;
        //! Seconds until this agent is worth asking again, when nothing was produced.
        float m_wakeIn = AZStd::numeric_limits<float>::max();
    };

    //! Decides how an agent acts. One of these per paradigm.
    class IDecisionBackend
    {
    public:
        AZ_RTTI(IDecisionBackend, IDecisionBackendTypeId);

        virtual ~IDecisionBackend() = default;

        //! Name this backend is registered under.
        virtual AZ::Name GetName() const = 0;

        //! Node type names this backend gives meaning to.
        virtual AZStd::vector<AZ::Name> GetNodeTypes() const = 0;

        //! Turns an authored node tree into a program agents can run.
        virtual CompileOutcome Compile(const AZ::Name& name, const AuthoredNode& root) = 0;

        //! How much per agent state this backend needs.
        virtual size_t GetStateSize() const = 0;

        //! Starts an agent on a program, and is where its state is set up.
        virtual void Attach([[maybe_unused]] const PlanContext& context,
            [[maybe_unused]] const AgentProgram& program, [[maybe_unused]] BrainState state)
        {
        }

        //! Re-checks whatever could interrupt the agent, and runs any periodic work it has.
        //! Only called when a watched slot changed or the program asked to be ticked.
        //! @param runningStep which step of the plan is in flight, or NoRunningStep.
        virtual TickResult Advance([[maybe_unused]] const PlanContext& context,
            [[maybe_unused]] const AgentProgram& program, [[maybe_unused]] BrainState state,
            [[maybe_unused]] float elapsed, [[maybe_unused]] size_t runningStep)
        {
            return TickResult::Continue;
        }

        //! Produces the next plan for an agent, given how the last one ended.
        virtual Decision Decide(const PlanContext& context, const AgentProgram& program, BrainState state,
            ActionResult lastResult, float elapsed, ActionPlan& outPlan) = 0;

        //! Releases any per agent state held for this agent.
        virtual void Release([[maybe_unused]] const PlanContext& context)
        {
        }
    };
} // namespace GOAT
