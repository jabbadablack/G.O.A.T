#include <Core/Frontend/TreeCompiler.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/algorithm.h>
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
        //! True when a node type may appear inside a parallel's background branch.
        //! The branch is re-evaluated whole, so nothing in it may take time or emit an action.
        bool IsInstantaneous(NodeOp op)
        {
            switch (op)
            {
            case NodeOp::Condition:
            case NodeOp::Compare:
            case NodeOp::Invert:
            case NodeOp::ForceSuccess:
            case NodeOp::Selector:
            case NodeOp::Sequence:
                return true;
            default:
                return false;
            }
        }

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

    TreeCompiler::TreeCompiler(
        const NodeTypeRegistry& types,
        const IBlackboardSystem& blackboard,
        const TreeLibrary& library,
        const ActionStateRegistry& actions)
        : m_types(types)
        , m_blackboard(blackboard)
        , m_library(library)
        , m_actions(actions)
    {
    }

    AZ::Outcome<NodeIndex, AZStd::string> TreeCompiler::Inline(
        const BehaviorTreeNode& authored,
        NodeIndex parent,
        AZ::u32 depth,
        DecisionProgram& program,
        AZStd::vector<AZ::Name>& inlining) const
    {
        AZ_Assert(depth < MaxTreeDepth, "Inlining is only reached from a depth the caller already checked");
        AZ_Assert(!inlining.empty(), "The inlining stack always holds at least the tree being compiled");

        // A subtree may be named directly, or reached through a slot a director can rebind.
        AZ::Name treeName;
        if (const AZStd::any* named = FindProperty(authored, AZ_NAME_LITERAL("tree")))
        {
            ReadName(*named, treeName);
        }
        else if (const AZStd::any* slot = FindProperty(authored, AZ_NAME_LITERAL("tag")))
        {
            AZ::Name slotName;
            if (ReadName(*slot, slotName))
            {
                treeName = m_library.GetBinding(slotName);

                // Remembered so a rebind of this slot can recompile exactly the trees that used
                // it, rather than every tree in the project.
                if (AZStd::find(program.m_boundSlots.begin(), program.m_boundSlots.end(), slotName) ==
                    program.m_boundSlots.end())
                {
                    program.m_boundSlots.push_back(slotName);
                }

                if (treeName.IsEmpty())
                {
                    return AZ::Failure(
                        AZStd::string::format("No tree is bound to subtree slot '%s'", slotName.GetCStr()));
                }
            }
        }

        if (treeName.IsEmpty())
        {
            return AZ::Failure(AZStd::string("A subtree needs either a tree name or a bound tag"));
        }

        if (AZStd::find(inlining.begin(), inlining.end(), treeName) != inlining.end())
        {
            return AZ::Failure(
                AZStd::string::format("Subtree '%s' refers to itself, directly or through another tree", treeName.GetCStr()));
        }

        const BehaviorTreeNode* referenced = m_library.Find(treeName);
        if (referenced == nullptr)
        {
            return AZ::Failure(AZStd::string::format("Unknown subtree '%s'", treeName.GetCStr()));
        }

        // The subtree node itself leaves no trace: its referenced root takes its place.
        const size_t depthBefore = inlining.size();

        inlining.push_back(treeName);
        auto emitted = Emit(*referenced, parent, depth, program, inlining);
        inlining.pop_back();

        AZ_Assert(inlining.size() == depthBefore, "Inlining must leave the cycle detection stack as it found it");
        return emitted;
    }

    AZ::Outcome<void, AZStd::string> TreeCompiler::RegisterParallel(NodeIndex index, DecisionProgram& program) const
    {
        AZ_Assert(index < program.m_nodes.size(), "A parallel index must address a node in the program");
        AZ_Assert(program.m_nodes[index].m_childCount == 2, "Arity was already checked before emitting");

        const NodeIndex main = program.m_nodes[index].m_firstChild;
        AZ_Assert(main != InvalidNodeIndex, "A parallel always has its two children by now");

        const NodeIndex background = program.m_nodes[main].m_subtreeEnd;
        const NodeIndex backgroundEnd = program.m_nodes[index].m_subtreeEnd;
        AZ_Assert(background < backgroundEnd, "A parallel's background branch must be a real range");

        // Nothing in the background may take time, because it is re-evaluated whole rather than
        // resumed. Rejecting it here is what lets EvaluateSubtree be a plain recursive walk.
        for (NodeIndex i = background; i < backgroundEnd; ++i)
        {
            if (!IsInstantaneous(program.m_nodes[i].m_op))
            {
                return AZ::Failure(AZStd::string(
                    "a parallel's background branch may only contain conditions, comparisons and the "
                    "composites over them, because it is re-checked rather than resumed; "
                    "anything that acts belongs in the main branch"));
            }

            // An abort mode inside the background would register that node as a guard as well,
            // scoped to a branch that never runs. The background is already a continuous check,
            // so asking for one on top of it is a mistake worth naming.
            if (program.m_nodes[i].m_abort != AbortMode::None)
            {
                return AZ::Failure(AZStd::string(
                    "a node inside a parallel's background branch cannot declare an abort mode; "
                    "the branch is already re-checked whenever a variable it reads changes"));
            }

            // The background is checked when a variable it reads changes, exactly like a guard,
            // so an agent whose blackboard is quiet still evaluates nothing at all.
            if (program.m_nodes[i].m_key.IsValid())
            {
                program.m_observedKeys.push_back(program.m_nodes[i].m_key);
            }
            if (program.m_nodes[i].m_otherKey.IsValid())
            {
                program.m_observedKeys.push_back(program.m_nodes[i].m_otherKey);
            }
        }

        program.m_parallelNodes.push_back(index);
        return AZ::Success();
    }

    AZ::Outcome<void, AZStd::string> TreeCompiler::Validate(
        const BehaviorTreeNode& authored, const NodeTypeDescriptor& descriptor) const
    {
        AZ_Assert(!descriptor.m_name.IsEmpty(), "A node type descriptor is always registered under a name");

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

        // A parallel is a main branch and a background branch, in that order, so its arity is
        // exact rather than "one or more" like every other composite.
        if (descriptor.m_op == NodeOp::Parallel && authored.m_children.size() != 2)
        {
            return AZ::Failure(AZStd::string::format(
                "'parallel' takes exactly two children, a main branch then a background branch, but has %zu",
                authored.m_children.size()));
        }

        return AZ::Success();
    }

    AZ::Outcome<NodeIndex, AZStd::string> TreeCompiler::Emit(
        const BehaviorTreeNode& authored,
        NodeIndex parent,
        AZ::u32 depth,
        DecisionProgram& program,
        AZStd::vector<AZ::Name>& inlining) const
    {
        AZ_Assert(!authored.m_type.empty(), "Every authored node names a type");

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

        if (descriptor->m_op == NodeOp::Subtree)
        {
            return Inline(authored, parent, depth, program, inlining);
        }

        AZ_Assert(program.m_nodes.size() < InvalidNodeIndex, "A program cannot hold more nodes than an index can address");

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

                if (parameter.m_name == AZ_NAME_LITERAL("goal") || parameter.m_name == AZ_NAME_LITERAL("payload"))
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

            if (parameter.m_name == AZ_NAME_LITERAL("tolerance"))
            {
                node.m_tolerance = static_cast<float>(number);
            }
            else
            {
                node.m_amount = static_cast<float>(number);
            }
        }

        // An action leaf runs a registered verb. Most name the verb by their own type;
        // a raw leaf names it in a property, which is what lets a tree reach any verb a
        // module registered without the core knowing about it.
        if (descriptor->m_op == NodeOp::Action)
        {
            DecisionNode& node = program.m_nodes[index];
            const bool isRaw = typeName == AZ_NAME_LITERAL("raw");
            const AZ::Name verbName = isRaw ? node.m_tag : typeName;

            const ActionStateId verb = m_actions.FindId(verbName);
            if (verb == CoreActions::Invalid)
            {
                return AZ::Failure(AZStd::string::format(
                    "'%s' runs verb '%s', which no module has registered", authored.m_type.c_str(),
                    verbName.GetCStr()));
            }

            node.m_action.m_action = verb;
            node.m_action.m_amount = node.m_amount;
            node.m_action.m_tolerance = node.m_tolerance;
            node.m_action.m_targetKey = node.m_key;

            // An action request carries one name, so authoring has to pick which property fills
            // it. `raw` spends its own tag naming the verb, so its payload arrives in m_goal;
            // every other leaf puts its authored name straight in the tag. Reading only m_goal
            // here was invisible while `wait` and `raw` were the only action leaves in the
            // world, and silently emptied the name for every verb a module contributed.
            node.m_action.m_tag = isRaw || node.m_tag.IsEmpty() ? node.m_goal : node.m_tag;

            AZ_Assert(node.m_action.m_action != CoreActions::Invalid,
                "A compiled action leaf must name a registered verb");

            // Validate proved the property was authored; this proves it survived the mapping
            // above and actually reached the verb. Without it a name can go missing between
            // the two and only show up as a verb failing at tick time.
            const bool needsName = AZStd::any_of(
                descriptor->m_parameters.begin(), descriptor->m_parameters.end(),
                [](const NodeParameter& parameter)
                {
                    return parameter.m_required && parameter.m_type == BlackboardType::Name &&
                        !parameter.m_isBlackboardKey;
                });

            // `raw` is exempt: its required name is the verb to run, which the lookup above
            // already proved, and its payload is genuinely optional.
            if (!isRaw && needsName && node.m_action.m_tag.IsEmpty())
            {
                return AZ::Failure(AZStd::string::format(
                    "'%s' requires a name but none reached the verb; the property it was authored "
                    "with does not map to an action request",
                    authored.m_type.c_str()));
            }
        }

        // Guards are the only thing that needs observing, so collect just those keys.
        {
            const DecisionNode& node = program.m_nodes[index];
            if (node.m_abort != AbortMode::None && node.m_key.IsValid())
            {
                program.m_guardNodes.push_back(index);
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
            if (node.m_serviceCount > 0)
            {
                program.m_serviceNodes.push_back(index);
            }
        }

        // The first child follows its parent immediately; each later sibling starts at the
        // previous sibling's subtree end, which is what makes a subtree one contiguous range.
        const NodeIndex firstChild = aznumeric_cast<NodeIndex>(program.m_nodes.size());
        for (const BehaviorTreeNode& child : authored.m_children)
        {
            auto emitted = Emit(child, index, depth + 1, program, inlining);
            if (!emitted.IsSuccess())
            {
                return emitted;
            }
        }

        DecisionNode& node = program.m_nodes[index];
        node.m_firstChild = authored.m_children.empty() ? InvalidNodeIndex : firstChild;
        node.m_subtreeEnd = aznumeric_cast<NodeIndex>(program.m_nodes.size());

        // The walker steps between siblings with m_subtreeEnd and scopes guards and services
        // with the range [index, m_subtreeEnd), so both must hold for every node it emits.
        AZ_Assert(node.m_subtreeEnd > index, "A node's subtree must end after the node itself");
        AZ_Assert(node.m_firstChild == InvalidNodeIndex || node.m_firstChild == index + 1,
            "A node's first child must immediately follow it in pre-order");

        if (node.m_op == NodeOp::Parallel)
        {
            if (auto checked = RegisterParallel(index, program); !checked.IsSuccess())
            {
                return AZ::Failure(checked.TakeError());
            }
        }

        return AZ::Success(index);
    }

    AZ::Outcome<DecisionProgram, AZStd::string> TreeCompiler::Compile(
        const AZ::Name& name, const BehaviorTreeNode& root) const
    {
        AZ_Assert(!name.IsEmpty(), "A tree is always compiled under a name");

        DecisionProgram program;
        program.m_name = name;

        if (root.m_type.empty())
        {
            return AZ::Failure(AZStd::string::format("Tree '%s' has no root node", name.GetCStr()));
        }

        AZStd::vector<AZ::Name> inlining;
        inlining.push_back(name);
        auto emitted = Emit(root, InvalidNodeIndex, 0, program, inlining);
        if (!emitted.IsSuccess())
        {
            return AZ::Failure(AZStd::string::format("Tree '%s': %s", name.GetCStr(), emitted.GetError().c_str()));
        }

        AZ_Assert(emitted.GetValue() == 0, "The root of a compiled program is always node zero");
        AZ_Assert(!program.m_nodes.empty(), "A successful compile always produces at least one node");

        // Sorted only so the duplicates can be removed: the same slot guarded twice is one
        // thing to watch, and the count reported below is what an author reads back.
        AZStd::sort(program.m_observedKeys.begin(), program.m_observedKeys.end());
        program.m_observedKeys.erase(
            AZStd::unique(program.m_observedKeys.begin(), program.m_observedKeys.end()), program.m_observedKeys.end());

        // One slot per node that keeps something between ticks, then one per service. Assigning
        // them here is what lets an agent's cursor be a fixed block: the compiler is the only
        // thing that knows how much state a tree actually needs.
        AZ::u16 nextSlot = 0;
        for (DecisionNode& node : program.m_nodes)
        {
            const bool keepsState = node.m_op == NodeOp::Cooldown || node.m_op == NodeOp::TimeLimit ||
                node.m_op == NodeOp::Loop || node.m_op == NodeOp::LuaComposite;
            if (keepsState)
            {
                node.m_cursorSlot = nextSlot++;
            }
        }

        program.m_serviceSlotBase = nextSlot;
        nextSlot = static_cast<AZ::u16>(nextSlot + program.m_services.size());
        program.m_cursorSlotCount = nextSlot;

        if (program.m_cursorSlotCount > MaxCursorSlots)
        {
            return AZ::Failure(AZStd::string::format(
                "Tree '%s' needs %u cursor slots but an agent carries %u. Cooldowns, time limits, "
                "loops, Lua composites and services each take one.",
                name.GetCStr(), static_cast<AZ::u32>(program.m_cursorSlotCount),
                static_cast<AZ::u32>(MaxCursorSlots)));
        }

        // A Lua composite or decorator decides in script, so what it does cannot be predicted
        // from the blackboard and the clock. Noting it here is what lets every other tree be left
        // dormant when nothing it reads has changed, without guessing which trees are safe.
        for (const DecisionNode& node : program.m_nodes)
        {
            if (node.m_op == NodeOp::LuaComposite || node.m_op == NodeOp::LuaDecorator)
            {
                program.m_pollEveryTick = true;
                break;
            }
        }

        AZ_Assert(program.m_nodes[0].m_subtreeEnd == program.m_nodes.size(),
            "The root's subtree must span the whole program");

        AZLOG_INFO("GOAT: tree '%s' compiled to %zu nodes, %zu guards, %zu services and %zu observed variables",
            name.GetCStr(), program.m_nodes.size(), program.m_guardNodes.size(), program.m_services.size(),
            program.m_observedKeys.size());

        return AZ::Success(AZStd::move(program));
    }
} // namespace GOAT
