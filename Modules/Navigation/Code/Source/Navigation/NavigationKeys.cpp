#include <Navigation/NavigationKeys.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>

namespace GOAT_Navigation
{
    namespace
    {
        //! Declares one variable, treating an already declared name as success.
        //! Re-declaration happens whenever a level reloads, and is not an error.
        GOAT::BlackboardKey DeclareOne(
            GOAT::IBlackboardSystem& blackboard, const AZ::Name& name, GOAT::BlackboardType type, AZStd::any defaultValue)
        {
            auto declared = blackboard.Declare(name, GOAT::BlackboardScope::Agent, type, AZStd::move(defaultValue));
            if (declared.IsSuccess())
            {
                return declared.GetValue();
            }

            const GOAT::BlackboardKey existing = blackboard.FindKey(name);
            AZ_Error("GOAT", existing.IsValid(), "Navigation variable '%s' could not be declared: %s",
                name.GetCStr(), declared.GetError().c_str());
            return existing;
        }
    } // namespace

    bool NavigationKeys::Declare(GOAT::IBlackboardSystem& blackboard)
    {
        m_waypoint = DeclareOne(
            blackboard, AZ_NAME_LITERAL("nav_waypoint"), GOAT::BlackboardType::Vector3, AZStd::any(AZ::Vector3::CreateZero()));
        m_remaining = DeclareOne(
            blackboard, AZ_NAME_LITERAL("nav_remaining"), GOAT::BlackboardType::Float, AZStd::any(0.0f));
        m_steer = DeclareOne(
            blackboard, AZ_NAME_LITERAL("nav_steer"), GOAT::BlackboardType::Bool, AZStd::any(true));

        AZ_Assert(IsValid(), "Every navigation blackboard variable must resolve to a key");
        return IsValid();
    }

    bool NavigationKeys::IsValid() const
    {
        return m_waypoint.IsValid() && m_remaining.IsValid() && m_steer.IsValid();
    }
} // namespace GOAT_Navigation
