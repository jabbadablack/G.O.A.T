#pragma once

#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/BlackboardKey.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    class IBlackboardSystem;

    //! The object a Lua behaviour receives, giving it the blackboard and its own entity.
    //! Held as a stable member rather than a per tick temporary: Lua receives a raw pointer
    //! to it, and a script that stores the value would otherwise be left holding freed memory.
    class AgentScriptContext final
    {
    public:
        AZ_TYPE_INFO(AgentScriptContext, AgentScriptContextTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! Points the context at one agent for the duration of a call.
        void Bind(AgentId agent, AZ::EntityId entity, IBlackboardSystem* blackboard);

        //! Points the context at nothing, so a stashed reference cannot reach an agent.
        void Unbind();

        //! The entity this agent drives.
        AZ::EntityId GetSelf() const { return m_entity; }

        //! True when a variable of that name is declared.
        bool Has(const AZStd::string& name) const;

        bool GetBool(const AZStd::string& name) const;
        void SetBool(const AZStd::string& name, bool value);

        //! Reads an int or float slot as Lua's single number type.
        double GetNumber(const AZStd::string& name) const;
        //! Writes an int or float slot, converting to whichever the slot holds.
        void SetNumber(const AZStd::string& name, double value);

        AZ::Vector3 GetVector3(const AZStd::string& name) const;
        void SetVector3(const AZStd::string& name, const AZ::Vector3& value);

        AZ::EntityId GetEntity(const AZStd::string& name) const;
        void SetEntity(const AZStd::string& name, AZ::EntityId value);

        AZStd::string GetName(const AZStd::string& name) const;
        void SetName(const AZStd::string& name, const AZStd::string& value);

        //! Puts this agent onto another of its trees, ending what it is running first.
        //! The change lands on the agent's next tick, because this is reachable from a behaviour
        //! running inside the current one.
        bool SetTree(const AZStd::string& treeName);

        //! Interrupts this agent with another tree, remembering what to come back to.
        bool PushTree(const AZStd::string& treeName);

        //! Returns this agent to the tree it last interrupted.
        bool PopTree();

        //! Which tree this agent is running.
        AZStd::string GetTree() const;

    private:
        //! Resolves a name a script supplied, reporting an undeclared one rather than letting
        //! the read return a silent default. @param access what the script was trying to do.
        BlackboardKey Resolve(const AZStd::string& name, const char* access) const;

        AgentId m_agent;
        AZ::EntityId m_entity;
        IBlackboardSystem* m_blackboard = nullptr;
    };
} // namespace GOAT
