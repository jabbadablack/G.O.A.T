#include <Core/Frontend/TreeCompiler.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/sort.h>

namespace GOAT
{
    namespace
    {
        //! Finds an authored property by name, or nullptr when it was not set.
        const AZStd::any* FindProperty(const BehaviorTreeNode& node, const AZ::Name& name)
        {
            for (const BehaviorTreeProperty& property : node.m_properties)
            {
                if (name == AZ::Name(property.m_name))
                {
                    return &property.m_value;
                }
            }
            return nullptr;
        }

        //! Reads a number, accepting every numeric form Lua and the editor may produce.
        bool ReadNumber(const AZStd::any& value, double& out)
        {
            if (const auto* asDouble = AZStd::any_cast<double>(&value))
            {
                out = *asDouble;
                return true;
            }
            if (const auto* asFloat = AZStd::any_cast<float>(&value))
            {
                out = *asFloat;
                return true;
            }
            if (const auto* asInt = AZStd::any_cast<AZ::s64>(&value))
            {
                out = static_cast<double>(*asInt);
                return true;
            }
            if (const auto* asBool = AZStd::any_cast<bool>(&value))
            {
                out = *asBool ? 1.0 : 0.0;
                return true;
            }
            return false;
        }

        //! Reads a name, accepting both strings and already interned names.
        bool ReadName(const AZStd::any& value, AZ::Name& out)
        {
            if (const auto* asName = AZStd::any_cast<AZ::Name>(&value))
            {
                out = *asName;
                return true;
            }
            if (const auto* asString = AZStd::any_cast<AZStd::string>(&value))
            {
                out = AZ::Name(*asString);
                return true;
            }
            return false;
        }

        //! Turns an authored abort mode name into its enum, defaulting to none.
        AbortMode ReadAbortMode(const AZ::Name& name)
        {
            if (name == AZ_NAME_LITERAL("self"))
            {
                return AbortMode::Self;
            }
            if (name == AZ_NAME_LITERAL("lower_priority"))
            {
                return AbortMode::LowerPriority;
            }
            if (name == AZ_NAME_LITERAL("both"))
            {
                return AbortMode::Both;
            }
            return AbortMode::None;
        }

        //! How many children a node kind may have.
        bool ChildCountIsLegal(NodeKind kind, size_t childCount)
        {
            switch (kind)
            {
            case NodeKind::Composite:
                return childCount >= 1;
            case NodeKind::Decorator:
                return childCount == 1;
            case NodeKind::Leaf:
            case NodeKind::Service:
                return childCount == 0;
            default:
                return false;
            }
        }
    } // namespace

    TreeCompiler::TreeCompiler(const NodeTypeRegistry& types, const IBlackboardSystem& blackboard)
        : m_types(types)
        , m_blackboard(blackboard)
    {
    }

    AZ::Outcome<void, AZStd::string> TreeCompiler::Validate(
        const BehaviorTreeNode& authored, const NodeTypeDescriptor& descriptor) const
    {
        // Reject properties the node type does not accept, so typos fail at author time.
        for (const BehaviorTreeProperty& property : authored.m_properties)
        {
            const AZ::Name propertyName(property.m_name);
            const bool accepted = AZStd::any_of(
                descriptor.m_parameters.begin(), descriptor.m_parameters.end(),
                [&propertyName](const NodeParameter& parameter)
                {
                    return parameter.m_name == propertyName;
                });

            if (!accepted)
            {
                return AZ::Failure(AZStd::string::format(
                    "'%s' has no property '%s'", descriptor.m_name.GetCStr(), property.m_name.c_str()));
            }
        }

        for (const NodeParameter& parameter : descriptor.m_parameters)
        {
            if (parameter.m_required && FindProperty(authored, parameter.m_name) == nullptr)
            {
                return AZ::Failure(AZStd::string::format(
                    "'%s' requires property '%s'", descriptor.m_name.GetCStr(), parameter.m_name.GetCStr()));
            }
        }

        if (!ChildCountIsLegal(descriptor.m_kind, authored.m_children.size()))
        {
            return AZ::Failure(AZStd::string::format(
                "'%s' cannot have %zu children", descriptor.m_name.GetCStr(), authored.m_children.size()));
        }

        return AZ::Success();
    }

    AZ::Outcome<NodeIndex, AZStd::string> TreeCompiler::Emit(
        const BehaviorTreeNode& authored, NodeIndex parent, AZ::u32 depth, DecisionProgram& program) const
    {
        if (depth >= MaxTreeDepth)
        {
            return AZ::Failure(AZStd::string::format("Tree is deeper than the %zu node limit", MaxTreeDepth));
        }
        program.m_depth = AZStd::max(program.m_depth, depth + 1);

        const AZ::Name typeName(authored.m_type);
        const NodeTypeDescriptor* descriptor = m_types.Find(typeName);
        if (descriptor == nullptr)
        {
            return AZ::Failure(AZStd::string::format("Unknown node type '%s'", authored.m_type.c_str()));
        }

        if (auto valid = Validate(authored, *descriptor); !valid.IsSuccess())
        {
            return AZ::Failure(valid.TakeError());
        }

        const NodeIndex index = aznumeric_cast<NodeIndex>(program.m_nodes.size());
        program.m_nodes.emplace_back();
        {
            DecisionNode& node = program.m_nodes[index];
            node.m_op = descriptor->m_op;
            node.m_parent = parent;
            node.m_childCount = aznumeric_cast<AZ::u16>(authored.m_children.size());
        }

        // Resolve the properties this node type declared.
        for (const NodeParameter& parameter : descriptor->m_parameters)
        {
            const AZStd::any* value = FindProperty(authored, parameter.m_name);
            if (value == nullptr)
            {
                continue;
            }

            DecisionNode& node = program.m_nodes[index];
            if (parameter.m_isBlackboardKey)
            {
                AZ::Name variableName;
                if (!ReadName(*value, variableName))
                {
                    return AZ::Failure(AZStd::string::format(
                        "'%s' property '%s' must name a blackboard variable", authored.m_type.c_str(),
                        parameter.m_name.GetCStr()));
                }

                const BlackboardKey key = m_blackboard.FindKey(variableName);
                if (!key.IsValid())
                {
                    return AZ::Failure(AZStd::string::format(
                        "'%s' refers to undeclared blackboard variable '%s'", authored.m_type.c_str(),
                        variableName.GetCStr()));
                }

                if (parameter.m_name == AZ_NAME_LITERAL("other"))
                {
                    node.m_otherKey = key;
                }
                else
                {
                    node.m_key = key;
                }
                continue;
            }

            if (parameter.m_name == AZ_NAME_LITERAL("abort"))
            {
                AZ::Name abortName;
                if (ReadName(*value, abortName))
                {
                    node.m_abort = ReadAbortMode(abortName);
                }
                continue;
            }

            if (parameter.m_type == BlackboardType::Name)
            {
                AZ::Name text;
                if (!ReadName(*value, text))
                {
                    return AZ::Failure(AZStd::string::format(
                        "'%s' property '%s' must be a name", authored.m_type.c_str(), parameter.m_name.GetCStr()));
                }

                if (parameter.m_name == AZ_NAME_LITERAL("goal"))
                {
                    node.m_goal = text;
                }
                else
                {
                    node.m_tag = text;
                }
                continue;
            }

            double number = 0.0;
            if (!ReadNumber(*value, number))
            {
                return AZ::Failure(AZStd::string::format(
                    "'%s' property '%s' must be a number", authored.m_type.c_str(), parameter.m_name.GetCStr()));
            }
            node.m_amount = static_cast<float>(number);
        }

        // A wait leaf is the one built-in that maps straight onto a core verb.
        if (typeName == AZ_NAME_LITERAL("wait"))
        {
            DecisionNode& node = program.m_nodes[index];
            node.m_action.m_action = CoreActions::Wait;
            node.m_action.m_duration = node.m_amount;
        }

        // Guards are the only thing that needs observing, so collect just those keys.
        {
            const DecisionNode& node = program.m_nodes[index];
            if (node.m_abort != AbortMode::None && node.m_key.IsValid())
            {
                program.m_observedKeys.push_back(node.m_key);
                if (node.m_otherKey.IsValid())
                {
                    program.m_observedKeys.push_back(node.m_otherKey);
                }
            }
        }

        // Services attached to this composite.
        {
            const AZ::u32 firstService = aznumeric_cast<AZ::u32>(program.m_services.size());
            for (const BehaviorTreeNode& authoredService : authored.m_services)
            {
                const NodeTypeDescriptor* serviceType = m_types.Find(AZ::Name(authoredService.m_type));
                if (serviceType == nullptr || serviceType->m_kind != NodeKind::Service)
                {
                    return AZ::Failure(AZStd::string::format(
                        "'%s' is not a service and cannot be attached to '%s'", authoredService.m_type.c_str(),
                        authored.m_type.c_str()));
                }

                if (auto valid = Validate(authoredService, *serviceType); !valid.IsSuccess())
                {
                    return AZ::Failure(valid.TakeError());
                }

                DecisionService service;
                if (const AZStd::any* behavior = FindProperty(authoredService, AZ_NAME_LITERAL("behavior")))
                {
                    ReadName(*behavior, service.m_behavior);
                }
                if (const AZStd::any* interval = FindProperty(authoredService, AZ_NAME_LITERAL("interval")))
                {
                    double seconds = 0.0;
                    if (ReadNumber(*interval, seconds))
                    {
                        service.m_interval = static_cast<float>(seconds);
                    }
                }
                program.m_services.push_back(AZStd::move(service));
            }

            DecisionNode& node = program.m_nodes[index];
            node.m_firstService = firstService;
            node.m_serviceCount = aznumeric_cast<AZ::u16>(program.m_services.size() - firstService);
        }

        // The first child follows its parent immediately; each later sibling starts at the
        // previous sibling's subtree end, which is what makes a subtree one contiguous range.
        const NodeIndex firstChild = aznumeric_cast<NodeIndex>(program.m_nodes.size());
        for (const BehaviorTreeNode& child : authored.m_children)
        {
            auto emitted = Emit(child, index, depth + 1, program);
            if (!emitted.IsSuccess())
            {
                return emitted;
            }
        }

        DecisionNode& node = program.m_nodes[index];
        node.m_firstChild = authored.m_children.empty() ? InvalidNodeIndex : firstChild;
        node.m_subtreeEnd = aznumeric_cast<NodeIndex>(program.m_nodes.size());
        return AZ::Success(index);
    }

    AZ::Outcome<DecisionProgram, AZStd::string> TreeCompiler::Compile(const BehaviorTreeAsset& asset) const
    {
        DecisionProgram program;
        program.m_name = AZ::Name(asset.m_name);

        if (asset.m_root.m_type.empty())
        {
            return AZ::Failure(AZStd::string::format("Tree '%s' has no root node", asset.m_name.c_str()));
        }

        auto root = Emit(asset.m_root, InvalidNodeIndex, 0, program);
        if (!root.IsSuccess())
        {
            return AZ::Failure(AZStd::string::format("Tree '%s': %s", asset.m_name.c_str(), root.GetError().c_str()));
        }

        AZStd::sort(program.m_observedKeys.begin(), program.m_observedKeys.end());
        program.m_observedKeys.erase(
            AZStd::unique(program.m_observedKeys.begin(), program.m_observedKeys.end()), program.m_observedKeys.end());

        return AZ::Success(AZStd::move(program));
    }
} // namespace GOAT
