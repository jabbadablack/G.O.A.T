#pragma once

#include <GOAT/GOATTypeIds.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/RTTI.h>

namespace GOAT
{
    //! A compiled program a backend produced, shared by every agent running it.
    class AgentProgram
    {
    public:
        AZ_RTTI(AgentProgram, AgentProgramTypeId);
        AZ_CLASS_ALLOCATOR(AgentProgram, AZ::SystemAllocator);

        virtual ~AgentProgram() = default;

        //! Name agents refer to this program by.
        AZ::Name m_name;
    };
} // namespace GOAT
