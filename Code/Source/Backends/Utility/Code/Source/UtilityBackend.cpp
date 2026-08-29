#include <UtilityBackend.h>

#include <GOAT/Domain/PlanStore.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>

namespace GOAT
{
    namespace
    {
        //! Folds what a choice considered into the one number it is compared by.
        float Fold(CombineRule rule, const AZStd::fixed_vector<float, MaxConsiderations + 1>& values)
        {
            // Nothing was considered, so nothing argues against running it either.
            if (values.empty())
            {
                return 1.0f;
            }

            switch (rule)
            {
            case CombineRule::Mean:
                {
                    float total = 0.0f;
                    for (const float value : values)
                    {
                        total += value;
                    }
                    return total / static_cast<float>(values.size());
                }
            case CombineRule::Min:
                {
                    float lowest = values.front();
                    for (const float value : values)
                    {
                        lowest = AZStd::min(lowest, value);
                    }
                    return lowest;
                }
            case CombineRule::Max:
                {
                    float highest = values.front();
                    for (const float value : values)
                    {
                        highest = AZStd::max(highest, value);
                    }
                    return highest;
                }
            default:
                {
                    float product = 1.0f;
                    for (const float value : values)
                    {
                        product *= value;
                    }
                    return product;
                }
            }
        }

        //! Brings a value into the range a score is measured in, saying so the first time it had
        //! to. @param warned lives on the program, so this is one line per mistake rather than
        //! one per agent per frame.
        float InRange(const float* read, const UtilityProgram& program, const UtilityChoice& choice,
            const char* source, bool& warned)
        {
            if (read == nullptr)
            {
                AZ_Warning("GOAT", warned, "'%s' choice '%s' cannot read '%s', so nothing argues for it",
                    program.m_name.GetCStr(), choice.m_name.GetCStr(), source);
                warned = true;
                return 0.0f;
            }

            if (*read >= 0.0f && *read <= 1.0f)
            {
                return *read;
            }

            AZ_Warning("GOAT", warned,
                "'%s' choice '%s' read %.3f from '%s', which is outside 0 to 1; a score is a float already "
                "scaled to that range, and this one was clamped", program.m_name.GetCStr(),
                choice.m_name.GetCStr(), static_cast<double>(*read), source);
            warned = true;
            return AZStd::clamp(*read, 0.0f, 1.0f);
        }

        //! The next number in one agent's own draw sequence, in 0 to 1.
        //! xorshift32, which is four instructions: what this is avoiding is a shared stream,
        //! not a weak generator.
        float Draw(AZ::u32& sequence)
        {
            sequence ^= sequence << 13;
            sequence ^= sequence >> 17;
            sequence ^= sequence << 5;
            return static_cast<float>(sequence >> 8) * (1.0f / 16777216.0f);
        }
    } // namespace

    UtilityBackend::UtilityBackend(IAgentSystem& host, IBlackboardSystem& blackboard)
        : m_host(host)
        , m_blackboard(blackboard)
    {
    }

    AZ::Name UtilityBackend::GetBackendName()
    {
        return AZ_NAME_LITERAL("utility");
    }

    AZ::Name UtilityBackend::GetName() const
    {
        return GetBackendName();
    }

    AZStd::vector<AZ::Name> UtilityBackend::GetNodeTypes() const
    {
        return { AZ_NAME_LITERAL("utility"), AZ_NAME_LITERAL("choice"), AZ_NAME_LITERAL("consider") };
    }

    size_t UtilityBackend::GetStateSize() const
    {
        return sizeof(UtilityCursor);
    }

    UtilityCursor& UtilityBackend::Cursor(BrainState state)
    {
        AZ_Assert(state.size() >= sizeof(UtilityCursor), "An agent's brain state must hold its utility cursor");
        return *reinterpret_cast<UtilityCursor*>(state.data());
    }

    CompileOutcome UtilityBackend::Compile(const AZ::Name& name, const AuthoredNode& root)
    {
        const UtilityCompiler compiler(m_host, m_blackboard);
        auto compiled = compiler.Compile(name, root);
        if (!compiled.IsSuccess())
        {
            return AZ::Failure(compiled.TakeError());
        }

        auto program = AZStd::shared_ptr<UtilityProgram>(aznew UtilityProgram(AZStd::move(compiled.GetValue())));
        program->m_backend = this;

        AZLOG_INFO("GOAT: utility program '%s' compiled to %zu choices and %zu considerations",
            name.GetCStr(), program->m_choices.size(), program->m_considerations.size());
        return AZ::Success(AZStd::shared_ptr<AgentProgram>(AZStd::move(program)));
    }

    void UtilityBackend::Attach(const PlanContext& context, const AgentProgram&, BrainState state)
    {
        AZ_Assert(state.size() >= sizeof(UtilityCursor), "An agent's brain state must hold its utility cursor");

        UtilityCursor* cursor = new (state.data()) UtilityCursor();

        // Seeded from this agent's own handle, and never zero, which xorshift cannot leave.
        // One stream shared by every agent puts the ones drawing in the same tick in lockstep.
        cursor->m_draw = ((context.m_agent.GetIndex() * 2654435761u) ^ (context.m_agent.GetGeneration() * 40503u)) | 1u;
    }

    float UtilityBackend::Measure(const PlanContext& context, const UtilityProgram& program,
        const UtilityChoice& choice, const AZ::Name& behavior, const Considered& values, bool& warned) const
    {
        float measured = 0.0f;
        if (!m_host.MeasureBehavior(behavior, "score", context.m_agent,
                AZStd::span<const float>(values.data(), values.size()), measured))
        {
            AZ_Warning("GOAT", warned, "'%s' choice '%s' asked '%s' for a score and got no answer",
                program.m_name.GetCStr(), choice.m_name.GetCStr(), behavior.GetCStr());
            warned = true;
            return 0.0f;
        }

        return InRange(&measured, program, choice, behavior.GetCStr(), warned);
    }

    float UtilityBackend::Score(const PlanContext& context, const UtilityProgram& program, AZ::u16 index) const
    {
        const UtilityChoice& choice = program.m_choices[index];

        Considered values;
        for (AZ::u16 i = 0; i < choice.m_considerationCount; ++i)
        {
            const UtilityConsideration& read = program.m_considerations[choice.m_firstConsideration + i];
            const float* found = context.m_blackboard->Find<float>(read.m_key, context.m_agent);
            values.push_back(
                InRange(found, program, choice, m_blackboard.GetKeyName(read.m_key).GetCStr(), read.m_warned));
        }

        // Folded first, so a choice nothing argues for never reaches a script. A choice whose
        // combining is written in a behaviour has no fold to run first, so it is always asked:
        // an author wanting the cheap way out of one names a scorer instead.
        if (choice.m_combine != CombineRule::Behavior && Fold(choice.m_combine, values) <= 0.0f)
        {
            return 0.0f;
        }

        if (!choice.m_scoreBehavior.IsEmpty())
        {
            values.push_back(
                Measure(context, program, choice, choice.m_scoreBehavior, values, choice.m_warnedScore));
        }

        if (choice.m_combine == CombineRule::Behavior)
        {
            return Measure(context, program, choice, choice.m_combineBehavior, values, choice.m_warnedCombine);
        }

        return Fold(choice.m_combine, values);
    }

    void UtilityBackend::ScoreAll(
        const PlanContext& context, const UtilityProgram& program, ScoreBoard& outScores) const
    {
        outScores.clear();
        for (AZ::u16 index = 0; index < program.m_choices.size(); ++index)
        {
            outScores.push_back(Score(context, program, index));
        }
    }

    AZ::u16 UtilityBackend::Pick(
        const UtilityProgram& program, const ScoreBoard& scores, UtilityCursor& cursor) const
    {
        // Raised before anything is compared, so a choice merely level with what is already
        // running does not take the agent off it.
        ScoreBoard weighted = scores;
        if (cursor.m_choice < weighted.size())
        {
            weighted[cursor.m_choice] = AZStd::min(weighted[cursor.m_choice] * (1.0f + program.m_momentum), 1.0f);
        }

        AZStd::fixed_vector<AZ::u16, MaxChoices> ranked;
        for (AZ::u16 index = 0; index < weighted.size(); ++index)
        {
            if (weighted[index] > 0.0f)
            {
                ranked.push_back(index);
            }
        }
        if (ranked.empty())
        {
            return InvalidChoice;
        }

        // Written order breaks a tie, so the same scores always answer the same way.
        AZStd::sort(ranked.begin(), ranked.end(),
            [&weighted](AZ::u16 lhs, AZ::u16 rhs)
            {
                return weighted[lhs] != weighted[rhs] ? weighted[lhs] > weighted[rhs] : lhs < rhs;
            });

        if (program.m_pick == PickRule::Best)
        {
            return ranked.front();
        }

        if (ranked.size() > program.m_top)
        {
            ranked.resize(program.m_top);
        }

        float total = 0.0f;
        for (const AZ::u16 index : ranked)
        {
            total += weighted[index];
        }

        float drawn = Draw(cursor.m_draw) * total;
        for (const AZ::u16 index : ranked)
        {
            drawn -= weighted[index];
            if (drawn <= 0.0f)
            {
                return index;
            }
        }
        return ranked.back();
    }

    TickResult UtilityBackend::Advance(const PlanContext& context, const AgentProgram& program, BrainState state,
        float elapsed, size_t runningStep)
    {
        const UtilityProgram& scored = static_cast<const UtilityProgram&>(program);
        UtilityCursor& cursor = Cursor(state);
        cursor.m_since += elapsed;

        if (runningStep == NoRunningStep || cursor.m_choice >= scored.m_choices.size() ||
            context.m_blackboard == nullptr)
        {
            return TickResult::Continue;
        }

        // A choice that said it would see itself through is not scored against anything. That is
        // the whole of what committing means: a better idea arriving halfway is not a reason to
        // drop what is already underway.
        if (scored.m_choices[cursor.m_choice].m_commit)
        {
            return TickResult::Continue;
        }

        if (cursor.m_since < scored.m_recheck)
        {
            return TickResult::Continue;
        }
        cursor.m_since = 0.0f;

        ScoreBoard scores;
        ScoreAll(context, scored, scores);
        const AZ::u16 chosen = Pick(scored, scores, cursor);

        if (chosen == InvalidChoice)
        {
            AZLOG(GoatUtility, "GOAT: agent %u program '%s' dropped '%s': nothing argues for it any more",
                context.m_agent.GetIndex(), scored.m_name.GetCStr(),
                scored.m_choices[cursor.m_choice].m_name.GetCStr());
            return TickResult::Abandon;
        }

        if (chosen == cursor.m_choice)
        {
            cursor.m_score = scores[chosen];
            return TickResult::Continue;
        }

        AZLOG(GoatUtility, "GOAT: agent %u program '%s' left '%s' at %.3f for '%s' at %.3f",
            context.m_agent.GetIndex(), scored.m_name.GetCStr(),
            scored.m_choices[cursor.m_choice].m_name.GetCStr(), static_cast<double>(cursor.m_score),
            scored.m_choices[chosen].m_name.GetCStr(), static_cast<double>(scores[chosen]));
        return TickResult::Abandon;
    }

    Decision UtilityBackend::Decide(const PlanContext& context, const AgentProgram& program, BrainState state,
        ActionResult lastResult, float, ActionPlan& outPlan)
    {
        Decision decision;

        const UtilityProgram& scored = static_cast<const UtilityProgram&>(program);
        AZ_Assert(context.m_blackboard != nullptr, "Scoring always runs with a blackboard");
        AZ_Assert(context.m_planStore != nullptr, "Producing a plan needs somewhere to put its steps");
        if (context.m_blackboard == nullptr || context.m_planStore == nullptr)
        {
            return decision;
        }

        UtilityCursor& cursor = Cursor(state);
        cursor.m_since = 0.0f;

        ScoreBoard scores;
        ScoreAll(context, scored, scores);
        const AZ::u16 chosen = Pick(scored, scores, cursor);

        if (chosen == InvalidChoice)
        {
            cursor.m_choice = InvalidChoice;
            cursor.m_score = 0.0f;

            // Nothing is worth doing. Whether it is worth asking again is whether anything this
            // scores from can move: one that reads nothing and asks nothing would only ever
            // reach this same answer, so it is finished rather than idle.
            if (scored.CanChange())
            {
                decision.m_wakeIn = scored.m_recheck;
                return decision;
            }

            AZLOG(GoatUtility, "GOAT: agent %u program '%s' has nothing worth doing and nothing to wait for",
                context.m_agent.GetIndex(), scored.m_name.GetCStr());
            decision.m_result = lastResult == ActionResult::Failure ? ActionResult::Failure : ActionResult::Success;
            return decision;
        }

        const UtilityChoice& choice = scored.m_choices[chosen];
        cursor.m_choice = chosen;
        cursor.m_score = scores[chosen];

        outPlan.m_span = context.m_planStore->Acquire(scored.m_steps.data() + choice.m_firstStep, choice.m_stepCount);
        decision.m_planned = true;

        AZLOG(GoatUtility, "GOAT: agent %u program '%s' chose '%s' at %.3f from %zu choice(s)",
            context.m_agent.GetIndex(), scored.m_name.GetCStr(), choice.m_name.GetCStr(),
            static_cast<double>(cursor.m_score), scores.size());
        return decision;
    }
} // namespace GOAT
