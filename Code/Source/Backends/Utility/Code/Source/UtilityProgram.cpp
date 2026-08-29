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
} // namespace GOAT
