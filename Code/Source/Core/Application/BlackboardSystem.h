#pragma once

#include <Core/Application/SquadRegistry.h>
#include <Core/Domain/BlackboardSchema.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/unordered_map.h>

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
        BlackboardStorage* FindStorage(BlackboardScope scope, AgentId agent) override;
        const BlackboardStorage* FindStorage(BlackboardScope scope, AgentId agent) const override;
        ////////////////////////////////////////////////////////////////////////

        //! Drops every declaration and every storage instance.
        void Clear();

        //! The declared variables, for validation messages and console output.
        const BlackboardSchema& GetSchema() const { return m_schema; }

    private:
        BlackboardSchema m_schema;
        BlackboardStorage m_global;
        AZStd::unordered_map<AgentId, BlackboardStorage> m_agents;
        SquadRegistry m_squads;
    };
} // namespace GOAT
