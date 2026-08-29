#pragma once

#include <GOAT/Domain/AgentId.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/vector.h>

namespace AZ
{
    class ReflectContext;
}

namespace GOAT
{
    //! One node of an authored program, as the tool that drew it can find it again.
    //!
    //! Authored rather than compiled, because a compiled index means nothing outside the backend
    //! that made it, and the core is not allowed to know what a node is.
    struct ProgramNodeRef final
    {
        AZ_TYPE_INFO(ProgramNodeRef, ProgramNodeRefTypeId);
        AZ_CLASS_ALLOCATOR(ProgramNodeRef, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        //! The program this node was authored in. Not the running program when a subtree was
        //! inlined into it, which is the only way a tool can tell those two apart.
        AZ::Name m_program;
        //! Steps from that program's root, each one an index into StepInto: services first,
        //! then children.
        AZStd::vector<AZ::u16> m_path;
    };

    //! Everything a tool shows about one agent, gathered in one pass rather than one call per
    //! column. What DescribeAgent prints as a line, as fields.
    struct AgentSnapshot final
    {
        AZ_TYPE_INFO(AgentSnapshot, AgentSnapshotTypeId);
        AZ_CLASS_ALLOCATOR(AgentSnapshot, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        //! The agent, taken apart into what a handle is made of. Kept this way because a
        //! snapshot is also a wire type, and this is the whole of a handle's public surface.
        AZ::u32 m_agentIndex = AgentId::NullIndex;
        AZ::u32 m_agentGeneration = 0;

        AgentId GetAgent() const { return AgentId(m_agentIndex, m_agentGeneration); }
        void SetAgent(AgentId agent)
        {
            m_agentIndex = agent.GetIndex();
            m_agentGeneration = agent.GetGeneration();
        }

        //! The entity this agent drives. Belongs to the process the snapshot came from, so a
        //! remote one has to be mapped before it means anything to the editor.
        AZ::EntityId m_entity;

        AZ::Name m_program;
        AZ::Name m_backend;
        AZ::Name m_squad;
        //! The verb it is running, or "idle".
        AZ::Name m_action;

        AZ::u8 m_band = 0;
        //! Which step of its plan is in flight, and how long that plan is.
        AZ::u32 m_step = 0;
        AZ::u32 m_planSize = 0;
        //! Seconds the running action has been going.
        float m_elapsed = 0.0f;
        //! How many programs it interrupted to get here.
        AZ::u32 m_interrupted = 0;

        //! Root first, ending at whatever the agent is actually running. Empty when its backend
        //! offers nothing, which is a backend that has not implemented this rather than a fault.
        AZStd::vector<ProgramNodeRef> m_activePath;
    };
} // namespace GOAT
