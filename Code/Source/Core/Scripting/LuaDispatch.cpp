#include <Core/Scripting/LuaDispatch.h>

#include <AzCore/Console/ILogger.h>
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
        AZ_Assert(m_scriptContext != nullptr, "Running a script needs a connected script context");
        if (m_scriptContext == nullptr || !asset.GetId().IsValid())
        {
            AZ_Error("GOAT", false, "Cannot run a GOAT script: %s",
                m_scriptContext == nullptr ? "scripting is not connected" : "the asset id is invalid");
            return false;
        }

        bool loaded = false;
        AZ::ScriptSystemRequestBus::BroadcastResult(
            loaded, &AZ::ScriptSystemRequests::Load, asset, AZ::k_scriptLoadBinaryOrText,
            AZ::ScriptContextIds::DefaultScriptContextId);

        if (!loaded)
        {
            AZ_Error("GOAT", false, "Lua refused to load script asset %s; check the Asset Processor compiled it",
                asset.GetId().ToString<AZStd::string>().c_str());
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
        AZ_Assert(!behavior.IsEmpty(), "A behaviour is always called by name");
        AZ_Assert(phase != nullptr, "A behaviour call always names a lifecycle phase");

        if (m_scriptContext == nullptr)
        {
            AZ_Error("GOAT", false, "Behaviour '%s' cannot run: scripting is not connected", behavior.GetCStr());
            return ActionResult::Failure;
        }

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_Dispatch", call))
        {
            AZ_Error("GOAT", false, "GOAT_Dispatch is missing, so behaviour '%s' cannot run; the vocabulary did not load",
                behavior.GetCStr());
            return ActionResult::Failure;
        }

        call.PushArg(AZStd::string(behavior.GetStringView()));
        call.PushArg(AZStd::string(phase));
        call.PushArg(AgentKey(agent));
        call.PushArg(context);
        call.PushArg(static_cast<double>(deltaTime));

        if (!call.CallExecute())
        {
            AZ_Error("GOAT", false, "Behaviour '%s' raised a Lua error in its %s phase for agent %u",
                behavior.GetCStr(), phase, agent.GetIndex());
            return ActionResult::Failure;
        }

        AZ_Warning("GOAT", call.GetNumResults() >= 1,
            "Behaviour '%s' returned nothing from its %s phase, which counts as failure", behavior.GetCStr(), phase);

        // Marshalled as an int on purpose: pushing a reflected enum is broken in AzCore.
        int status = 0;
        if (call.GetNumResults() >= 1)
        {
            call.ReadResult(0, status);
        }
        return ToActionResult(status);
    }

    void LuaDispatch::ConfigurePlanBuilder(
        const ActionStateRegistry* actions, const IBlackboardSystem* blackboard, PlanStore* store)
    {
        m_planBuilder.Configure(actions, blackboard, store);
        m_planValidator.Configure(actions, blackboard);
    }

    bool LuaDispatch::BakePlans()
    {
        if (m_scriptContext == nullptr)
        {
            return false;
        }

        // Everything baked so far goes first: baking is idempotent only if it starts from empty,
        // and re-running it after a script load must not leave the old steps behind.
        m_planBuilder.ClearBaked();

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_BakePlans", call))
        {
            AZ_Error("GOAT", false, "GOAT_BakePlans is missing, so no authored plan can run");
            return false;
        }

        call.PushArg(m_planBuilder);

        // Hoisted out of the warning on purpose: a trace macro's expression is not compiled in release.
        const bool executed = call.CallExecute();
        AZ_Warning("GOAT", executed, "Baking the declared plans raised a Lua error");
        return executed;
    }

    bool LuaDispatch::ValidatePlans()
    {
        m_planValidator.Reset();
        if (m_scriptContext == nullptr)
        {
            return false;
        }

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_ValidatePlans", call))
        {
            AZ_Error("GOAT", false, "GOAT_ValidatePlans is missing, so declared plans cannot be checked");
            return false;
        }

        call.PushArg(m_planValidator);

        // Hoisted out of the warning on purpose: a trace macro's expression is not compiled in release.
        const bool executed = call.CallExecute();
        AZ_Warning("GOAT", executed, "Checking the declared plans raised a Lua error");
        return executed && m_planValidator.IsClean();
    }

    bool LuaDispatch::HasLuaBackend(const AZ::Name& backend)
    {
        if (m_scriptContext == nullptr)
        {
            return false;
        }

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_HasBackend", call))
        {
            AZ_Error("GOAT", false, "GOAT_HasBackend is missing, so backend '%s' cannot be looked up", backend.GetCStr());
            return false;
        }

        call.PushArg(AZStd::string(backend.GetStringView()));
        if (!call.CallExecute())
        {
            AZ_Error("GOAT", false, "Looking up Lua backend '%s' raised a Lua error", backend.GetCStr());
            return false;
        }

        bool defined = false;
        if (call.GetNumResults() >= 1)
        {
            call.ReadResult(0, defined);
        }
        return defined;
    }

    AZStd::vector<AZ::Name> LuaDispatch::GetLuaBackendNames()
    {
        m_nameCollector.Clear();
        if (m_scriptContext == nullptr)
        {
            return {};
        }

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_EmitBackendNames", call))
        {
            AZ_Error("GOAT", false, "GOAT_EmitBackendNames is missing, so Lua backends cannot be discovered");
            return {};
        }

        call.PushArg(m_nameCollector);

        // Hoisted out of the warning on purpose: a trace macro's expression is not compiled in release.
        const bool executed = call.CallExecute();
        AZ_Warning("GOAT", executed, "Listing Lua backends raised a Lua error");
        return m_nameCollector.GetNames();
    }

    AZStd::vector<AZ::Name> LuaDispatch::GetDeclaredTreeNames()
    {
        m_nameCollector.Clear();
        if (m_scriptContext == nullptr)
        {
            return {};
        }

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_EmitTreeNames", call))
        {
            AZ_Error("GOAT", false, "GOAT_EmitTreeNames is missing, so declared trees cannot be listed");
            return {};
        }

        call.PushArg(m_nameCollector);

        // Hoisted out of the warning on purpose: a trace macro's expression is not compiled in release.
        const bool executed = call.CallExecute();
        AZ_Warning("GOAT", executed, "Listing declared trees raised a Lua error");
        return m_nameCollector.GetNames();
    }

    const ActionPlan* LuaDispatch::CallBackendPlan(
        const AZ::Name& backend, const AZ::Name& goal, AgentId agent, AgentScriptContext& context)
    {
        AZ_Assert(!backend.IsEmpty(), "A backend is always asked to plan by name");

        if (m_scriptContext == nullptr)
        {
            AZ_Error("GOAT", false, "Backend '%s' cannot plan: scripting is not connected", backend.GetCStr());
            return nullptr;
        }

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_Plan", call))
        {
            AZ_Error("GOAT", false, "GOAT_Plan is missing, so backend '%s' cannot plan", backend.GetCStr());
            return nullptr;
        }

        call.PushArg(AZStd::string(backend.GetStringView()));
        call.PushArg(AgentKey(agent));
        call.PushArg(context);
        call.PushArg(AZStd::string(goal.GetStringView()));
        call.PushArg(m_planBuilder);

        if (!call.CallExecute())
        {
            AZ_Error("GOAT", false, "Backend '%s' raised a Lua error planning goal '%s' for agent %u",
                backend.GetCStr(), goal.GetCStr(), agent.GetIndex());
            return nullptr;
        }

        bool planned = false;
        if (call.GetNumResults() >= 1)
        {
            call.ReadResult(0, planned);
        }
        return planned ? &m_planBuilder.GetPlan() : nullptr;
    }

    int LuaDispatch::CallFlowBegin(
        const AZ::Name& flow,
        AgentId agent,
        AgentScriptContext& context,
        NodeIndex node,
        int childCount,
        ActionResult& outResult)
    {
        outResult = ActionResult::Failure;
        AZ_Assert(!flow.IsEmpty(), "Lua control flow is always entered by name");
        AZ_Assert(childCount >= 0, "A composite cannot have a negative number of children");

        if (m_scriptContext == nullptr)
        {
            AZ_Error("GOAT", false, "Flow '%s' cannot start: scripting is not connected", flow.GetCStr());
            return NoChild;
        }

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_FlowBegin", call))
        {
            AZ_Error("GOAT", false, "GOAT_FlowBegin is missing, so flow '%s' cannot start", flow.GetCStr());
            return NoChild;
        }

        call.PushArg(AZStd::string(flow.GetStringView()));
        call.PushArg(AgentKey(agent));
        call.PushArg(context);
        call.PushArg(static_cast<double>(node));
        call.PushArg(static_cast<double>(childCount));

        if (!call.CallExecute() || call.GetNumResults() < 2)
        {
            AZ_Error("GOAT", false,
                "Flow '%s' start must return a child index and a status; it raised an error or returned too few values",
                flow.GetCStr());
            return NoChild;
        }

        int child = NoChild;
        int status = 0;
        call.ReadResult(0, child);
        call.ReadResult(1, status);

        AZ_Warning("GOAT", child == NoChild || (child >= 0 && child < childCount),
            "Flow '%s' start chose child %d, which is outside its %d children", flow.GetCStr(), child, childCount);

        outResult = ToActionResult(status);
        return child;
    }

    int LuaDispatch::CallFlowAdvance(
        const AZ::Name& flow,
        AgentId agent,
        AgentScriptContext& context,
        NodeIndex node,
        int childIndex,
        ActionResult childResult,
        ActionResult& outResult)
    {
        outResult = childResult;
        AZ_Assert(!flow.IsEmpty(), "Lua control flow is always advanced by name");
        AZ_Assert(childIndex >= 0, "A composite always reports which child just finished");

        if (m_scriptContext == nullptr)
        {
            AZ_Error("GOAT", false, "Flow '%s' cannot advance: scripting is not connected", flow.GetCStr());
            return NoChild;
        }

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_FlowAdvance", call))
        {
            AZ_Error("GOAT", false, "GOAT_FlowAdvance is missing, so flow '%s' cannot advance", flow.GetCStr());
            return NoChild;
        }

        call.PushArg(AZStd::string(flow.GetStringView()));
        call.PushArg(AgentKey(agent));
        call.PushArg(context);
        call.PushArg(static_cast<double>(node));
        call.PushArg(static_cast<double>(childIndex));
        call.PushArg(static_cast<double>(static_cast<int>(childResult)));

        if (!call.CallExecute() || call.GetNumResults() < 2)
        {
            AZ_Error("GOAT", false,
                "Flow '%s' result must return a child index and a status; it raised an error or returned too few values",
                flow.GetCStr());
            return NoChild;
        }

        int child = NoChild;
        int status = 0;
        call.ReadResult(0, child);
        call.ReadResult(1, status);
        outResult = ToActionResult(status);
        return child;
    }

    ActionResult LuaDispatch::CallFlowFilter(
        const AZ::Name& flow, AgentId agent, AgentScriptContext& context, NodeIndex node, ActionResult childResult)
    {
        AZ_Assert(!flow.IsEmpty(), "Lua control flow is always filtered by name");

        if (m_scriptContext == nullptr)
        {
            AZ_Error("GOAT", false, "Flow '%s' cannot filter: scripting is not connected", flow.GetCStr());
            return childResult;
        }

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_FlowFilter", call))
        {
            AZ_Error("GOAT", false, "GOAT_FlowFilter is missing, so flow '%s' cannot filter", flow.GetCStr());
            return childResult;
        }

        call.PushArg(AZStd::string(flow.GetStringView()));
        call.PushArg(AgentKey(agent));
        call.PushArg(context);
        call.PushArg(static_cast<double>(node));
        call.PushArg(static_cast<double>(static_cast<int>(childResult)));

        if (!call.CallExecute() || call.GetNumResults() < 1)
        {
            AZ_Error("GOAT", false, "Flow '%s' result must return a status; it raised an error or returned nothing",
                flow.GetCStr());
            return childResult;
        }

        int status = static_cast<int>(childResult);
        call.ReadResult(0, status);
        return ToActionResult(status);
    }

    bool LuaDispatch::DeclareNode(const AZ::Name& typeName, const AZ::Name& mainProperty)
    {
        AZ_Assert(!typeName.IsEmpty(), "A declared node word must have a name");
        if (m_scriptContext == nullptr || typeName.IsEmpty())
        {
            AZ_Error("GOAT", false, "Cannot declare node word '%s' before scripting is running", typeName.GetCStr());
            return false;
        }

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_DeclareNode", call))
        {
            AZ_Error("GOAT", false,
                "GOAT_DeclareNode is missing, so node word '%s' cannot be authored; the vocabulary did not load",
                typeName.GetCStr());
            return false;
        }

        call.PushArg(AZStd::string(typeName.GetStringView()));
        call.PushArg(AZStd::string(mainProperty.GetStringView()));
        if (!call.CallExecute())
        {
            AZ_Error("GOAT", false, "Declaring node word '%s' raised a Lua error", typeName.GetCStr());
            return false;
        }

        AZLOG_INFO("GOAT: node word '%s' is now available to authored trees", typeName.GetCStr());
        return true;
    }

    void LuaDispatch::ForgetAgent(AgentId agent)
    {
        if (m_scriptContext == nullptr)
        {
            return;
        }

        AZ::ScriptDataContext call;
        if (!m_scriptContext->Call("GOAT_ForgetAgent", call))
        {
            AZ_Error("GOAT", false, "GOAT_ForgetAgent is missing, so agent %u's Lua scratch will outlive it",
                agent.GetIndex());
            return;
        }

        call.PushArg(AgentKey(agent));

        // Hoisted out of the warning on purpose: a trace macro's expression is not compiled in release.
        const bool executed = call.CallExecute();
        AZ_Warning("GOAT", executed, "Dropping agent %u's Lua scratch raised a Lua error", agent.GetIndex());
    }
} // namespace GOAT
