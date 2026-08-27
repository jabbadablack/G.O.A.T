#include <Core/Scripting/LuaTreeBuilder.h>

#include <AzCore/Console/ILogger.h>
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

        AZ_Assert(m_records.empty(), "Beginning a tree must discard whatever the last emission left");
        AZ_Warning("GOAT", !m_name.empty(), "A tree is being emitted with no name, so nothing can reference it");
    }

    void LuaTreeBuilder::AddNode(AZStd::string type, int childCount, int serviceCount)
    {
        AZ_Assert(!type.empty(), "Every emitted node names its type");
        AZ_Assert(childCount >= 0 && serviceCount >= 0, "A node cannot declare a negative number of children");

        if (childCount < 0 || serviceCount < 0)
        {
            m_error = AZStd::string::format("Node '%s' declared a negative child or service count", type.c_str());
            return;
        }

        Record record;
        record.m_type = AZStd::move(type);
        record.m_childCount = childCount;
        record.m_serviceCount = serviceCount;
        m_records.push_back(AZStd::move(record));
    }

    void LuaTreeBuilder::SetBoolProperty(AZStd::string key, bool value)
    {
        AZ_Assert(!m_records.empty(), "A property must be set on a node that was already added");
        if (m_records.empty())
        {
            m_error = AZStd::string::format("Property '%s' was set before any node was added", key.c_str());
            return;
        }

        BehaviorTreeProperty property;
        property.m_name = AZStd::move(key);
        property.m_value = AZStd::any(value);
        m_records.back().m_properties.push_back(AZStd::move(property));
    }

    void LuaTreeBuilder::SetNumberProperty(AZStd::string key, double value)
    {
        AZ_Assert(!m_records.empty(), "A property must be set on a node that was already added");
        if (m_records.empty())
        {
            m_error = AZStd::string::format("Property '%s' was set before any node was added", key.c_str());
            return;
        }

        BehaviorTreeProperty property;
        property.m_name = AZStd::move(key);
        property.m_value = AZStd::any(value);
        m_records.back().m_properties.push_back(AZStd::move(property));
    }

    void LuaTreeBuilder::SetStringProperty(AZStd::string key, AZStd::string value)
    {
        AZ_Assert(!m_records.empty(), "A property must be set on a node that was already added");
        if (m_records.empty())
        {
            m_error = AZStd::string::format("Property '%s' was set before any node was added", key.c_str());
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

        AZ_Assert(index < m_records.size(), "A build step must address a record that was emitted");

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
            AZ_Error("GOAT", false, "Tree '%s' emitted no nodes", m_name.c_str());
            return;
        }

        const size_t consumed = Build(0, m_root);
        if (!m_error.empty())
        {
            AZ_Error("GOAT", false, "Tree '%s' could not be assembled: %s", m_name.c_str(), m_error.c_str());
            return;
        }

        if (consumed != m_records.size())
        {
            m_error = "Tree emission produced nodes that no parent claimed";
            AZ_Error("GOAT", false, "Tree '%s' emitted %zu nodes but only %zu were claimed by a parent",
                m_name.c_str(), m_records.size(), consumed);
            return;
        }

        m_complete = true;

        AZ_Assert(m_error.empty(), "A complete tree cannot also carry an error");
        AZ_Assert(!m_root.m_type.empty(), "A complete tree must have a typed root node");
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
