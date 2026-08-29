#include <UtilityProgram.h>

namespace GOAT
{
    AZ::u16 UtilityProgram::FindChoice(const AZ::Name& name) const
    {
        for (size_t index = 0; index < m_choices.size(); ++index)
        {
            if (m_choices[index].m_name == name)
            {
                return static_cast<AZ::u16>(index);
            }
        }
        return InvalidChoice;
    }

    bool UtilityProgram::CanChange() const
    {
        if (!m_considerations.empty())
        {
            return true;
        }

        // A behaviour answers from whatever it likes, so a program that asks one can never be
        // said to have run out of answers.
        for (const UtilityChoice& choice : m_choices)
        {
            if (!choice.m_scoreBehavior.IsEmpty() || choice.m_combine == CombineRule::Behavior)
            {
                return true;
            }
        }
        return false;
    }
} // namespace GOAT
