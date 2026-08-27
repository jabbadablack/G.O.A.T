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
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! One agent's own state, in whatever shape the backend that owns it chose.
    using BrainState = AZStd::span<AZ::u8>;

    //! A compiled program, or why it could not be compiled.
    using CompileOutcome = AZ::Outcome<AZStd::shared_ptr<const AgentProgram>, AZStd::string>;

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
        virtual CompileOutcome Compile(const AZ::Name& name, const AuthoredNode& root) const = 0;

        //! How much per agent state this backend needs.
        virtual size_t GetStateSize() const = 0;

        //! Starts an agent on a program, and is where its state is set up.
        virtual void Attach([[maybe_unused]] const PlanContext& context,
            [[maybe_unused]] const AgentProgram& program, [[maybe_unused]] BrainState state)
        {
        }

        //! Produces the next plan for an agent. False when it has none.
        virtual bool Decide(
            const PlanContext& context, const AgentProgram& program, BrainState state, ActionPlan& outPlan) = 0;

        //! Releases any per agent state held for this agent.
        virtual void Release([[maybe_unused]] const PlanContext& context)
        {
        }
    };
} // namespace GOAT
