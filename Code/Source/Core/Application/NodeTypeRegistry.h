#pragma once

#include <GOAT/Domain/NodeType.h>

#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! Every node type the authoring layers may use.
    //! Seeded with the genre neutral built-ins; modules and backends add their own.
    class NodeTypeRegistry final
    {
    public:
        NodeTypeRegistry();

        //! Adds a node type. Fails when the name is already registered.
        bool Register(NodeTypeDescriptor descriptor);

        //! Removes a node type, so a module can take its vocabulary with it.
        void Unregister(const AZ::Name& name);

        //! The descriptor for a node type name, or nullptr when it is not registered.
        const NodeTypeDescriptor* Find(const AZ::Name& name) const;

        //! Every registered node type, for console output and a future graph palette.
        AZStd::vector<const NodeTypeDescriptor*> GetAll() const;

    private:
        //! Registers the node types the core always provides.
        void RegisterBuiltIns();

        AZStd::unordered_map<AZ::Name, NodeTypeDescriptor> m_types;
    };
} // namespace GOAT
