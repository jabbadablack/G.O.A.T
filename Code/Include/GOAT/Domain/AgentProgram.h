#pragma once

#include <GOAT/Domain/BlackboardTypes.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/array.h>

namespace GOAT
{
    class IDecisionBackend;

    //! A compiled program a backend produced, shared by every agent running it.
    class AgentProgram
    {
    public:
        AZ_RTTI(AgentProgram, AgentProgramTypeId);
        AZ_CLASS_ALLOCATOR(AgentProgram, AZ::SystemAllocator);

        virtual ~AgentProgram() = default;

        //! Name agents refer to this program by.
        AZ::Name m_name;
        //! The backend that compiled this and runs it.
        IDecisionBackend* m_backend = nullptr;
        //! Blackboard scopes this program guards on, so a write elsewhere never wakes it.
        AZStd::array<bool, static_cast<size_t>(BlackboardScope::Count)> m_watchedScopes{};
        //! True when this needs a call every tick, not only when a watched slot changed.
        bool m_wantsTick = false;
    };
} // namespace GOAT
