#pragma once

#include <UtilityCompiler.h>
#include <UtilityProgram.h>

#include <GOAT/Interfaces/IDecisionBackend.h>

#include <AzCore/std/containers/fixed_vector.h>

namespace GOAT
{
    //! Where one agent is inside a utility program.
    struct UtilityCursor final
    {
        //! The choice it is running, or InvalidChoice.
        AZ::u16 m_choice = InvalidChoice;
        //! What that choice scored when it was taken, which is what momentum raises.
        float m_score = 0.0f;
        //! Seconds since the last scoring pass, measured against the program's floor.
        float m_since = 0.0f;
        //! This agent's own draw sequence. Its own, because agents drawing from one shared
        //! stream in the same tick come out in step, which is the identical crowd that
        //! drawing at all was meant to break up.
        AZ::u32 m_draw = 1;
    };

    //! Runs agents by scoring everything they could do and running the best of it.
    class UtilityBackend final
        : public IDecisionBackend
    {
    public:
        AZ_RTTI(UtilityBackend, "{A4350639-6FDB-452C-B9F2-B0293EA9D5FE}", IDecisionBackend);
        AZ_CLASS_ALLOCATOR(UtilityBackend, AZ::SystemAllocator);

        UtilityBackend(IAgentSystem& host, IBlackboardSystem& blackboard);

        //! Name an entity asks for to be run by scoring.
        static AZ::Name GetBackendName();

        AZ::Name GetName() const override;
        AZStd::vector<AZ::Name> GetNodeTypes() const override;
        size_t GetStateSize() const override;
        CompileOutcome Compile(const AZ::Name& name, const AuthoredNode& root) override;
        void Attach(const PlanContext& context, const AgentProgram& program, BrainState state) override;
        TickResult Advance(const PlanContext& context, const AgentProgram& program, BrainState state,
            float elapsed, size_t runningStep) override;
        Decision Decide(const PlanContext& context, const AgentProgram& program, BrainState state,
            ActionResult lastResult, float elapsed, ActionPlan& outPlan) override;
        void DescribePosition(const AgentProgram& program, BrainState state, size_t runningStep,
            AZStd::vector<ProgramNodeRef>& outPath) const override;

    private:
        //! Every choice's score, in the order the program declared them.
        using ScoreBoard = AZStd::fixed_vector<float, MaxChoices>;
        //! The values one choice argued from, and room for what a scorer added.
        using Considered = AZStd::fixed_vector<float, MaxConsiderations + 1>;

        //! What each scope is stored in for one agent, found once for a whole scoring pass
        //! rather than once for every number that pass reads.
        struct ScopedStorage final
        {
            const BlackboardStorage* m_byScope[static_cast<size_t>(BlackboardScope::Count)]{};

            const float* Find(BlackboardKey key) const
            {
                const BlackboardStorage* storage = m_byScope[static_cast<size_t>(key.GetScope())];
                return storage != nullptr ? storage->Find<float>(key) : nullptr;
            }
        };

        //! The cursor an agent keeps inside its brain state.
        static UtilityCursor& Cursor(BrainState state);

        //! Scores every choice. Allocates nothing and keeps nothing.
        void ScoreAll(const PlanContext& context, const UtilityProgram& program, ScoreBoard& outScores) const;

        //! Scores one choice. What it considers is read and folded first, so a choice nothing
        //! argues for costs no call into a script at all.
        float Score(const PlanContext& context, const UtilityProgram& program, const ScopedStorage& storage,
            AZ::u16 index) const;

        //! Asks a behaviour for a number, treating one that did not answer as nothing.
        float Measure(const PlanContext& context, const UtilityProgram& program, const UtilityChoice& choice,
            const AZ::Name& behavior, const Considered& values, bool& warned) const;

        //! Takes the winner, given what is running and the agent's own draw sequence.
        AZ::u16 Pick(const UtilityProgram& program, const ScoreBoard& scores, UtilityCursor& cursor) const;

        IAgentSystem& m_host;
        IBlackboardSystem& m_blackboard;
    };
} // namespace GOAT
