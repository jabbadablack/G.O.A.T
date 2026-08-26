#pragma once

#include <Core/Scripting/AgentScriptContext.h>
#include <Core/Scripting/LuaNameCollector.h>
#include <Core/Scripting/LuaPlanBuilder.h>
#include <Core/Scripting/LuaTreeBuilder.h>

#include <GOAT/Assets/BehaviorTreeAsset.h>
#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/Intent.h>
#include <GOAT/Interfaces/INodeScripting.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/Script/ScriptAsset.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    class ScriptContext;
}

namespace GOAT
{
    //! Calls into the Lua authoring vocabulary.
    //! Every call happens on the main thread, because AZ::ScriptContext carries no lock
    //! and only asserts about its owning thread in debug builds.
    class LuaDispatch final
    {
    public:
        //! Binds to the shared script context. Returns false when scripting is unavailable.
        bool Connect();
        void Disconnect();

        //! True when the vocabulary is loaded and calls can be made.
        bool IsReady() const { return m_scriptContext != nullptr; }

        //! Runs a script, which registers whatever behaviours, backends and trees it declares.
        bool RunScript(const AZ::Data::Asset<AZ::ScriptAsset>& asset);

        //! Asks Lua to hand a declared tree over through the reflected builder.
        AZ::Outcome<AZStd::shared_ptr<const BehaviorTreeNode>, AZStd::string> EmitTree(const AZ::Name& treeName);

        //! Runs one phase of a Lua behaviour and reports what it returned.
        ActionResult CallBehavior(
            const AZ::Name& behavior, const char* phase, AgentId agent, AgentScriptContext& context, float deltaTime);

        //! Points the plan builder at the registries a Lua backend's steps resolve against.
        //! Points the plan builder and the plan validator at what they need to resolve names.
        void ConfigurePlanBuilder(
            const ActionStateRegistry* actions, const IBlackboardSystem* blackboard, PlanStore* store);

        //! The builder, so a caller can read which authored plan and option produced a plan.
        const LuaPlanBuilder& GetPlanBuilder() const { return m_planBuilder; }
        LuaPlanBuilder& GetPlanBuilder() { return m_planBuilder; }

        //! Runs a Lua backend's plan function. Returns nullptr when it produced nothing.
        const ActionPlan* CallBackendPlan(
            const AZ::Name& backend, const AZ::Name& goal, AgentId agent, AgentScriptContext& context);

        //! True when a backend of that name is defined in Lua.
        bool HasLuaBackend(const AZ::Name& backend);

        //! Every backend name declared in Lua so far.
        AZStd::vector<AZ::Name> GetLuaBackendNames();

        //! Asks Lua which child a user defined composite runs first.
        //! Returns NoChild when the node is already finished, reporting through outResult.
        int CallFlowBegin(
            const AZ::Name& flow,
            AgentId agent,
            AgentScriptContext& context,
            NodeIndex node,
            int childCount,
            ActionResult& outResult);

        //! Asks Lua which child a user defined composite runs after one finished.
        int CallFlowAdvance(
            const AZ::Name& flow,
            AgentId agent,
            AgentScriptContext& context,
            NodeIndex node,
            int childIndex,
            ActionResult childResult,
            ActionResult& outResult);

        //! Asks Lua what a user defined decorator reports for its child's result.
        ActionResult CallFlowFilter(
            const AZ::Name& flow, AgentId agent, AgentScriptContext& context, NodeIndex node, ActionResult childResult);

        //! Drops the scratch tables an agent owned, so a reused slot starts clean.
        void ForgetAgent(AgentId agent);

        //! Makes a node type name usable as a word in authored trees.
        //! @param mainProperty the property a single string argument fills; may be empty.
        bool DeclareNode(const AZ::Name& typeName, const AZ::Name& mainProperty);

    private:
        AZ::ScriptContext* m_scriptContext = nullptr;
        //! Stable, because Lua receives raw pointers to these during a call.
        LuaTreeBuilder m_builder;
        LuaPlanBuilder m_planBuilder;
        LuaNameCollector m_nameCollector;
    };
} // namespace GOAT
