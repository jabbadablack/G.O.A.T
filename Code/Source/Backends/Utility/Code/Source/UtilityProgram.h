#pragma once

#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/AgentProgram.h>
#include <GOAT/Domain/BlackboardKey.h>

#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! Not one of the choices in a program.
    inline constexpr AZ::u16 InvalidChoice = static_cast<AZ::u16>(-1);

    //! Choices one program may hold, so a scoring pass is a fixed size array.
    inline constexpr AZ::u16 MaxChoices = 32;

    //! Variables one choice may read. Bounded for the same reason, and because a choice
    //! arguing from more than a handful of numbers is one nobody can reason about.
    inline constexpr AZ::u16 MaxConsiderations = 8;

    //! Steps one choice may run.
    inline constexpr AZ::u16 MaxChoiceSteps = 16;

    //! The shortest gap between re-scores when a program named none of its own.
    inline constexpr float DefaultRecheck = 0.25f;

    //! How the values a choice considered become the one number it is compared by.
    enum class CombineRule : AZ::u8
    {
        Multiply, //!< Everything must argue for it, so one zero rules the choice out.
        Mean,
        Min,
        Max,
        Behavior //!< A behaviour is handed the values and answers with the score.
    };

    //! How the winner is taken once every choice has a score.
    enum class PickRule : AZ::u8
    {
        Best,    //!< The highest, with written order breaking a tie.
        Weighted //!< Drawn from the best few, in proportion to what they scored.
    };

    //! One variable a choice reads, already scaled to 0 to 1.
    struct UtilityConsideration final
    {
        BlackboardKey m_key;
        //! Set once this had to bring a value into range. On the program rather than the agent,
        //! because the program is what was written wrongly and every agent runs the same one.
        mutable bool m_warned = false;
    };

    //! One thing an agent may do, and what makes it worth doing.
    struct UtilityChoice final
    {
        AZ::Name m_name;
        AZ::u16 m_firstConsideration = 0;
        AZ::u16 m_considerationCount = 0;
        AZ::u16 m_firstStep = 0;
        AZ::u16 m_stepCount = 0;
        CombineRule m_combine = CombineRule::Multiply;
        //! The behaviour that folds the values, when m_combine is Behavior.
        AZ::Name m_combineBehavior;
        //! A behaviour whose answer counts as one more consideration, or empty.
        AZ::Name m_scoreBehavior;
        //! True when this runs its steps to the end whatever else starts scoring higher.
        bool m_commit = false;
        mutable bool m_warnedCombine = false;
        mutable bool m_warnedScore = false;
    };

    //! Scored choices compiled for execution: flat, immutable, shared by every agent using it.
    class UtilityProgram final
        : public AgentProgram
    {
    public:
        AZ_RTTI(UtilityProgram, "{4D8B6EE6-E986-43CC-8974-B584E3722AB5}", AgentProgram);
        AZ_CLASS_ALLOCATOR(UtilityProgram, AZ::SystemAllocator);

        //! Index of a choice by name, or InvalidChoice.
        AZ::u16 FindChoice(const AZ::Name& name) const;

        //! True when anything this program scores from can move under it. A program that reads
        //! no variable and asks no behaviour can only ever reach the answer it just reached.
        bool CanChange() const;

        AZStd::vector<UtilityChoice> m_choices;
        AZStd::vector<UtilityConsideration> m_considerations;
        //! What every choice's steps run, referred to by the ranges on each choice.
        AZStd::vector<ActionRequest> m_steps;
        //! The shortest gap between scoring a running choice against the others.
        float m_recheck = DefaultRecheck;
        //! How much the running choice's score is raised by, so two near equal choices do not
        //! trade the agent back and forth every time one of them twitches.
        float m_momentum = 0.0f;
        PickRule m_pick = PickRule::Best;
        //! How many of the best choices a weighted draw is taken from.
        AZ::u16 m_top = 1;
    };
} // namespace GOAT
