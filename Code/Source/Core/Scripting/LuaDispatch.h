#pragma once

#include <Core/Scripting/AgentScriptContext.h>
#include <Core/Scripting/LuaTreeBuilder.h>

#include <GOAT/Assets/BehaviorTreeAsset.h>
#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/AgentId.h>

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

        //! Drops the scratch tables an agent owned, so a reused slot starts clean.
        void ForgetAgent(AgentId agent);

    private:
        AZ::ScriptContext* m_scriptContext = nullptr;
        //! Stable, because Lua receives a raw pointer to it during an emission.
        LuaTreeBuilder m_builder;
    };
} // namespace GOAT
