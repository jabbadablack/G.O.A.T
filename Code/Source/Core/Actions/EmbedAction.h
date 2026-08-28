#pragma once

#include <Core/Application/NestedRun.h>

#include <GOAT/Interfaces/IActionState.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

namespace GOAT
{
    class ActionStateRegistry;
    class AgentRegistry;
    class AgentRuntime;

    //! Runs another paradigm's program to completion, as one step of the plan it sits in.
    //!
    //! A step of a plan is where one paradigm can hand over for a while and get an answer back,
    //! which is why this is a verb rather than something backends do to each other: a plan step
    //! is a point the runtime can already see, and every paradigm already produces them. The
    //! nested program keeps its own guards and replans behind this step without the host
    //! noticing; ending the step is what tears it down, however the step ends.
    class EmbedAction final
        : public IActionState
    {
    public:
        AZ_CLASS_ALLOCATOR(EmbedAction, AZ::SystemAllocator);

        using ProgramTable = AZStd::unordered_map<AZ::Name, AZStd::shared_ptr<const AgentProgram>>;

        EmbedAction(
            AgentRegistry& agents, AgentRuntime& runtime, const ActionStateRegistry& actions,
            const ProgramTable& programs);

        AZ::Name GetName() const override;

        void Begin(const ActionContext& context) override;
        ActionResult Step(const ActionContext& context, float deltaTime) override;
        void End(const ActionContext& context) override;

    private:
        //! The frame this step is running, or nullptr when it never started one.
        //! Kept in the step's own scratch, which is the only per agent memory a verb has.
        static NestedFrame* Frame(const ActionContext& context);
        static void SetFrame(const ActionContext& context, NestedFrame* frame);

        AgentRegistry& m_agents;
        AgentRuntime& m_runtime;
        const ActionStateRegistry& m_actions;
        const ProgramTable& m_programs;
    };
} // namespace GOAT
