#pragma once

#include <Core/Application/SquadRegistry.h>
#include <Core/Domain/BlackboardSchema.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! Owns the global blackboard, every agent blackboard, and every squad blackboard.
    //! One schema is shared by all of them, so a name resolves to the same slot everywhere.
    class BlackboardSystem final
        : public IBlackboardSystem
    {
    public:
        AZ_RTTI(BlackboardSystem, "{86063A37-6BE5-4BEA-A2FE-005BCC81BDBD}", IBlackboardSystem);
        AZ_CLASS_ALLOCATOR(BlackboardSystem, AZ::SystemAllocator);

        BlackboardSystem();
        ~BlackboardSystem() override;

        ////////////////////////////////////////////////////////////////////////
        // IBlackboardSystem
        AZ::Outcome<BlackboardKey, AZStd::string> Declare(
            const AZ::Name& name, BlackboardScope scope, BlackboardType type, AZStd::any defaultValue = {}) override;
        BlackboardKey FindKey(const AZ::Name& name) const override;
        void CreateAgentBlackboard(AgentId agent) override;
        void DestroyAgentBlackboard(AgentId agent) override;
        void JoinSquad(AgentId agent, const AZ::Name& squad) override;
        void LeaveSquad(AgentId agent) override;
        AZ::Name GetSquad(AgentId agent) const override;
        BlackboardStorage* FindSquadStorage(const AZ::Name& squad) override;
        AZStd::vector<AZ::Name> GetSquadNames() const override;
        BlackboardStorage* FindStorage(BlackboardScope scope, AgentId agent) override;
        const BlackboardStorage* FindStorage(BlackboardScope scope, AgentId agent) const override;
        ////////////////////////////////////////////////////////////////////////



        //! The declared variables, for validation messages and console output.
        const BlackboardSchema& GetSchema() const { return m_schema; }

    private:
        //! One agent's storage, in the slot its handle carries. Indexed rather than hashed
        //! because a slot never moves while its agent lives, so reaching an agent's variables
        //! is an array index. The generation is kept so a stale handle finds nothing rather
        //! than finding whoever took the slot over.
        struct AgentSlot final
        {
            BlackboardStorage m_storage;
            //! Zero when the slot holds nobody.
            AZ::u32 m_generation = 0;
        };

        BlackboardSchema m_schema;
        BlackboardStorage m_global;
        //! Grows to the highest slot ever used and keeps the buffers of departed agents, so a
        //! reused slot gets its storage back rather than allocating again.
        AZStd::vector<AgentSlot> m_agents;
        SquadRegistry m_squads;
    };
} // namespace GOAT
