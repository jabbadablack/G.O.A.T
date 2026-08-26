#include <Core/Scripting/LuaDispatch.h>

#include <AzCore/Script/ScriptContext.h>
#include <AzCore/Script/ScriptSystemBus.h>

extern "C" {
#   include <Lua/lua.h>
}

namespace GOAT
{
    namespace
    {
        //! Lua's number for one agent. The slot index alone is enough because an agent's
        //! scratch is dropped when it is released, so a reused slot never inherits state.
        double AgentKey(AgentId agent)
        {
            return static_cast<double>(agent.GetIndex());
        }

        //! Turns the integer a behaviour returned into a result, defaulting to failure.
        ActionResult ToActionResult(int status)
        {
            switch (status)
            {
            case 0:
                return ActionResult::Running;
            case 1:
                return ActionResult::Success;
            case 2:
                return ActionResult::Failure;
            default:
                return ActionResult::Failure;
            }
        }
    } // namespace

    bool LuaDispatch::Connect()
    {
        AZ::ScriptSystemRequestBus::BroadcastResult(
            m_scriptContext, &AZ::ScriptSystemRequests::GetContext, AZ::ScriptContextIds::DefaultScriptContextId);
        return m_scriptContext != nullptr;
    }

    void LuaDispatch::Disconnect()
    {
        m_scriptContext = nullptr;
    }

    bool LuaDispatch::RunScript(const AZ::Data::Asset<AZ::ScriptAsset>& asset)
    {
        if (m_scriptContext == nullptr || !asset.GetId().IsValid())
        {
            return false;
        }

        bool loaded = false;
        AZ::ScriptSystemRequestBus::BroadcastResult(
            loaded, &AZ::ScriptSystemRequests::Load, asset, AZ::k_scriptLoadBinaryOrText,
            AZ::ScriptContextIds::DefaultScriptContextId);

        if (!loaded)
        {
            return false;
        }

        // Load leaves whatever the file returned on the stack; the vocabulary registers
        // itself as a side effect, so drop the value rather than reading it here.
        lua_pop(m_scriptContext->NativeContext(), 1);
        return true;
    }

    AZ::Outcome<AZStd::shared_ptr<const BehaviorTreeNode>, AZStd::string> LuaDispatch::EmitTree(const AZ::Name& treeName)
    {
        if (m_scriptContext == nullptr)
        {
            return AZ::Failure(AZStd::string("The Lua script context is not available"));
        }

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_EmitTree", call))
        {
            return AZ::Failure(AZStd::string("The GOAT Lua vocabulary is not loaded"));
        }

        call.PushArg(AZStd::string(treeName.GetStringView()));
        call.PushArg(m_builder);

        if (!call.CallExecute())
        {
            return AZ::Failure(AZStd::string::format("Emitting tree '%s' raised a Lua error", treeName.GetCStr()));
        }

        bool found = false;
        if (call.GetNumResults() >= 1)
        {
            call.ReadResult(0, found);
        }

        if (!found)
        {
            return AZ::Failure(AZStd::string::format("No tree named '%s' was declared", treeName.GetCStr()));
        }

        if (!m_builder.IsComplete())
        {
            return AZ::Failure(AZStd::string::format(
                "Tree '%s' could not be assembled: %s", treeName.GetCStr(), m_builder.GetError().c_str()));
        }

        return AZ::Success(AZStd::shared_ptr<const BehaviorTreeNode>(aznew BehaviorTreeNode(m_builder.GetRoot())));
    }

    ActionResult LuaDispatch::CallBehavior(
        const AZ::Name& behavior, const char* phase, AgentId agent, AgentScriptContext& context, float deltaTime)
    {
        if (m_scriptContext == nullptr)
        {
            return ActionResult::Failure;
        }

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_Dispatch", call))
        {
            return ActionResult::Failure;
        }

        call.PushArg(AZStd::string(behavior.GetStringView()));
        call.PushArg(AZStd::string(phase));
        call.PushArg(AgentKey(agent));
        call.PushArg(context);
        call.PushArg(static_cast<double>(deltaTime));

        if (!call.CallExecute())
        {
            return ActionResult::Failure;
        }

        // Marshalled as an int on purpose: pushing a reflected enum is broken in AzCore.
        int status = 0;
        if (call.GetNumResults() >= 1)
        {
            call.ReadResult(0, status);
        }
        return ToActionResult(status);
    }

    void LuaDispatch::ForgetAgent(AgentId agent)
    {
        if (m_scriptContext == nullptr)
        {
            return;
        }

        AZ::ScriptDataContext call;
        if (m_scriptContext->Call("GOAT._forgetAgent", call))
        {
            call.PushArg(AgentKey(agent));
            call.CallExecute();
        }
    }
} // namespace GOAT
