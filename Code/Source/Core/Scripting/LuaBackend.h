#pragma once

#include <Core/Scripting/AgentScriptContext.h>
#include <Core/Scripting/LuaDispatch.h>

#include <GOAT/Interfaces/IBackend.h>

#include <AzCore/Memory/SystemAllocator.h>

namespace GOAT
{
    //! Fronts a backend that was written in Lua.
    //! One of these is registered per Lua backend, so a scripted backend and a C++ one are
    //! indistinguishable to the rest of the pipeline.
    class LuaBackend final
        : public IBackend
    {
    public:
        AZ_CLASS_ALLOCATOR(LuaBackend, AZ::SystemAllocator);

        LuaBackend(AZ::Name name, LuaDispatch& dispatch, AgentScriptContext& scriptContext);

        AZ::Name GetName() const override;
        bool Plan(const PlanContext& context, const Intent& intent, ActionPlan& outPlan) override;

    private:
        AZ::Name m_name;
        LuaDispatch& m_dispatch;
        AgentScriptContext& m_scriptContext;
    };
} // namespace GOAT
