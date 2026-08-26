#include <Core/Director/DirectorKeys.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    namespace
    {
        //! Declares one variable, treating an already declared name as success.
        //! Re-declaration happens whenever a level reloads, and is not an error.
        BlackboardKey DeclareOne(IBlackboardSystem& blackboard, const AZ::Name& name)
        {
            auto declared = blackboard.Declare(name, BlackboardScope::Agent, BlackboardType::Int, AZStd::any(AZ::s64{ 0 }));
            if (declared.IsSuccess())
            {
                return declared.GetValue();
            }

            const BlackboardKey existing = blackboard.FindKey(name);
            AZ_Error("GOAT", existing.IsValid(), "Director variable '%s' could not be declared: %s",
                name.GetCStr(), declared.GetError().c_str());
            return existing;
        }
    } // namespace

    bool DirectorKeys::Declare(IBlackboardSystem& blackboard)
    {
        m_reach = DeclareOne(blackboard, AZ_NAME_LITERAL("director_reach"));
        m_changed = DeclareOne(blackboard, AZ_NAME_LITERAL("director_changed"));
        m_refused = DeclareOne(blackboard, AZ_NAME_LITERAL("director_refused"));

        AZ_Assert(IsValid(), "Every director blackboard variable must resolve to a key");
        return IsValid();
    }

    bool DirectorKeys::IsValid() const
    {
        return m_reach.IsValid() && m_changed.IsValid() && m_refused.IsValid();
    }
} // namespace GOAT
