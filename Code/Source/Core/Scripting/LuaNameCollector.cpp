#include <Core/Scripting/LuaNameCollector.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContextAttributes.h>

namespace GOAT
{
    void LuaNameCollector::Clear()
    {
        m_names.clear();
    }

    void LuaNameCollector::Add(AZStd::string name)
    {
        AZ_Warning("GOAT", !name.empty(), "A script added an empty name, which nothing can reference");
        if (!name.empty())
        {
            m_names.emplace_back(name);
        }
    }

    void LuaNameCollector::Reflect(AZ::ReflectContext* context)
    {
        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<LuaNameCollector>("GoatNameCollector")
                ->Attribute(AZ::Script::Attributes::Category, "GOAT")
                ->Method("Add", &LuaNameCollector::Add);
        }
    }
} // namespace GOAT
