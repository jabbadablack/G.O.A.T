#pragma once

#include <Core/Application/NestedRun.h>

#include <GOAT/Domain/AgentProgram.h>
#include <GOAT/Interfaces/IBackend.h>
#include <GOAT/Interfaces/IDecisionBackend.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

namespace GOAT
{
    //! One paradigm answering a single `delegate` leaf, as the planner that leaf expects.
    //!
    //! A backend that decides for a whole agent and one that answers one intent are asked the
    //! same question -- what should this agent do next -- so a paradigm can serve either. Asked
    //! once, it produces one plan and keeps nothing: what it was in the middle of doing is the
    //! host's business, not its own.
    class DecisionBackendAdapter final
        : public IBackend
    {
    public:
        AZ_RTTI(DecisionBackendAdapter, "{4B4B2D1E-7C2E-4B7A-9E77-3A6D5E0C1F42}", IBackend);
        AZ_CLASS_ALLOCATOR(DecisionBackendAdapter, AZ::SystemAllocator);

        using ProgramTable = AZStd::unordered_map<AZ::Name, AZStd::shared_ptr<const AgentProgram>>;

        DecisionBackendAdapter(IDecisionBackend& inner, AgentLookup agents, const ProgramTable& programs);

        AZ::Name GetName() const override;
        bool Plan(const PlanContext& context, const Intent& intent, ActionPlan& outPlan) override;

    private:
        IDecisionBackend& m_inner;
        AgentLookup m_agents;
        const ProgramTable& m_programs;
    };
} // namespace GOAT
