#pragma once

#include <GOAT/Domain/BlackboardTypes.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>

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
        //! Subtree slots this was compiled against, deduplicated. Recorded because the compiler
        //! that resolved them is the only thing that knows, and rebinding one has to find
        //! whoever used it again.
        AZStd::vector<AZ::Name> m_boundSlots;
        //! True when this needs a call every tick, not only when a watched slot changed.
        bool m_wantsTick = false;
        //! Brain state one agent needs to run this, which the core fills in once it knows.
        size_t m_stateBytes = 0;
    };

    //! Rounds a brain state size up to what the block after it needs to stay aligned.
    inline constexpr size_t AlignState(size_t bytes)
    {
        return (bytes + 7u) & ~static_cast<size_t>(7u);
    }
} // namespace GOAT
