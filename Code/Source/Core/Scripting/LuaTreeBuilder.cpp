#include <Core/Scripting/LuaTreeBuilder.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContextAttributes.h>

namespace GOAT
{
    void LuaTreeBuilder::BeginTree(AZStd::string name)
    {
        m_records.clear();
        m_root = BehaviorTreeNode{};
        m_name = AZStd::move(name);
        m_error.clear();
        m_complete = false;
    }

    void LuaTreeBuilder::AddNode(AZStd::string type, int childCount, int serviceCount)
    {
        Record record;
        record.m_type = AZStd::move(type);
        record.m_childCount = childCount;
        record.m_serviceCount = serviceCount;
        m_records.push_back(AZStd::move(record));
    }

    void LuaTreeBuilder::SetBoolProperty(AZStd::string key, bool value)
    {
        if (m_records.empty())
        {
            return;
        }
        BehaviorTreeProperty property;
        property.m_name = AZStd::move(key);
        property.m_value = AZStd::any(value);
        m_records.back().m_properties.push_back(AZStd::move(property));
    }

    void LuaTreeBuilder::SetNumberProperty(AZStd::string key, double value)
    {
        if (m_records.empty())
        {
            return;
        }
        BehaviorTreeProperty property;
        property.m_name = AZStd::move(key);
        property.m_value = AZStd::any(value);
        m_records.back().m_properties.push_back(AZStd::move(property));
    }

    void LuaTreeBuilder::SetStringProperty(AZStd::string key, AZStd::string value)
    {
        if (m_records.empty())
        {
            return;
        }
        BehaviorTreeProperty property;
        property.m_name = AZStd::move(key);
        property.m_value = AZStd::any(AZStd::move(value));
        m_records.back().m_properties.push_back(AZStd::move(property));
    }

    size_t LuaTreeBuilder::Build(size_t index, BehaviorTreeNode& out)
    {
        if (index >= m_records.size())
        {
            m_error = "Tree emission ended before every declared child was provided";
            return m_records.size();
        }

        const Record& record = m_records[index++];
        out.m_type = record.m_type;
        out.m_properties = record.m_properties;

        for (int i = 0; i < record.m_serviceCount && m_error.empty(); ++i)
        {
            BehaviorTreeNode service;
            index = Build(index, service);
            out.m_services.push_back(AZStd::move(service));
        }

        for (int i = 0; i < record.m_childCount && m_error.empty(); ++i)
        {
            BehaviorTreeNode child;
            index = Build(index, child);
            out.m_children.push_back(AZStd::move(child));
        }

        return index;
    }

    void LuaTreeBuilder::EndTree()
    {
        m_complete = false;

        if (m_records.empty())
        {
            m_error = "Tree emission produced no nodes";
            return;
        }

        const size_t consumed = Build(0, m_root);
        if (!m_error.empty())
        {
            return;
        }

        if (consumed != m_records.size())
        {
            m_error = "Tree emission produced nodes that no parent claimed";
            return;
        }

        m_complete = true;
    }

    void LuaTreeBuilder::Reflect(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->Class<LuaTreeBuilder>("GoatTreeBuilder")
            ->Attribute(AZ::Script::Attributes::Category, "GOAT")
            ->Method("BeginTree", &LuaTreeBuilder::BeginTree)
            ->Method("AddNode", &LuaTreeBuilder::AddNode)
            ->Method("SetBoolProperty", &LuaTreeBuilder::SetBoolProperty)
            ->Method("SetNumberProperty", &LuaTreeBuilder::SetNumberProperty)
            ->Method("SetStringProperty", &LuaTreeBuilder::SetStringProperty)
            ->Method("EndTree", &LuaTreeBuilder::EndTree);
    }
} // namespace GOAT
