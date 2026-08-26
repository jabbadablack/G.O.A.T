#include <Core/Application/NodeTypeRegistry.h>

#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    namespace
    {
        //! Builds a descriptor for a node type that takes no properties.
        NodeTypeDescriptor Simple(const char* name, NodeKind kind, NodeOp op, const char* category, const char* description)
        {
            NodeTypeDescriptor descriptor;
            descriptor.m_name = AZ::Name(name);
            descriptor.m_kind = kind;
            descriptor.m_op = op;
            descriptor.m_category = category;
            descriptor.m_description = description;
            return descriptor;
        }

        //! Declares one property on a node type.
        NodeParameter Param(const char* name, BlackboardType type, bool isKey = false, bool required = false)
        {
            NodeParameter parameter;
            parameter.m_name = AZ::Name(name);
            parameter.m_type = type;
            parameter.m_isBlackboardKey = isKey;
            parameter.m_required = required;
            return parameter;
        }
    } // namespace

    NodeTypeRegistry::NodeTypeRegistry()
    {
        RegisterBuiltIns();
    }

    void NodeTypeRegistry::RegisterBuiltIns()
    {
        Register(Simple("selector", NodeKind::Composite, NodeOp::Selector, "Composite", "Runs children until one succeeds"));
        Register(Simple("sequence", NodeKind::Composite, NodeOp::Sequence, "Composite", "Runs children until one fails"));

        Register(Simple("invert", NodeKind::Decorator, NodeOp::Invert, "Decorator", "Flips success and failure"));
        Register(Simple("force_success", NodeKind::Decorator, NodeOp::ForceSuccess, "Decorator", "Always reports success"));

        auto cooldown = Simple("cooldown", NodeKind::Decorator, NodeOp::Cooldown, "Decorator",
            "Blocks re-entry until a duration has passed");
        cooldown.m_parameters.push_back(Param("seconds", BlackboardType::Float, false, true));
        Register(AZStd::move(cooldown));

        auto loop = Simple("loop", NodeKind::Decorator, NodeOp::Loop, "Decorator", "Repeats a fixed number of times");
        loop.m_parameters.push_back(Param("count", BlackboardType::Int, false, true));
        Register(AZStd::move(loop));

        auto conditionalLoop = Simple("conditional_loop", NodeKind::Decorator, NodeOp::ConditionalLoop, "Decorator",
            "Repeats while a blackboard condition holds");
        conditionalLoop.m_parameters.push_back(Param("key", BlackboardType::Bool, true, true));
        Register(AZStd::move(conditionalLoop));

        auto timeLimit = Simple("time_limit", NodeKind::Decorator, NodeOp::TimeLimit, "Decorator",
            "Fails the child once a duration elapses");
        timeLimit.m_parameters.push_back(Param("seconds", BlackboardType::Float, false, true));
        Register(AZStd::move(timeLimit));

        auto condition = Simple("condition", NodeKind::Decorator, NodeOp::Condition, "Decorator",
            "Guards a subtree on a blackboard value and may abort when it changes");
        condition.m_parameters.push_back(Param("key", BlackboardType::Bool, true, true));
        condition.m_parameters.push_back(Param("abort", BlackboardType::Name));
        Register(AZStd::move(condition));

        auto compare = Simple("compare", NodeKind::Decorator, NodeOp::Compare, "Decorator",
            "Guards a subtree by comparing two blackboard values");
        compare.m_parameters.push_back(Param("key", BlackboardType::Float, true, true));
        compare.m_parameters.push_back(Param("other", BlackboardType::Float, true, true));
        compare.m_parameters.push_back(Param("abort", BlackboardType::Name));
        Register(AZStd::move(compare));

        auto wait = Simple("wait", NodeKind::Leaf, NodeOp::Action, "Leaf", "Waits for a number of seconds");
        wait.m_parameters.push_back(Param("seconds", BlackboardType::Float, false, true));
        Register(AZStd::move(wait));

        auto raw = Simple("raw", NodeKind::Leaf, NodeOp::Action, "Leaf",
            "Runs any registered verb directly, including one a module contributed");
        raw.m_parameters.push_back(Param("action", BlackboardType::Name, false, true));
        raw.m_parameters.push_back(Param("payload", BlackboardType::Name));
        raw.m_parameters.push_back(Param("seconds", BlackboardType::Float));
        raw.m_parameters.push_back(Param("tolerance", BlackboardType::Float));
        raw.m_parameters.push_back(Param("key", BlackboardType::Vector3, true));
        Register(AZStd::move(raw));

        auto script = Simple("script", NodeKind::Leaf, NodeOp::Script, "Leaf", "Runs a Lua behavior");
        script.m_parameters.push_back(Param("behavior", BlackboardType::Name, false, true));
        Register(AZStd::move(script));

        auto delegate = Simple("delegate", NodeKind::Leaf, NodeOp::Delegate, "Leaf", "Hands an intent to a named backend");
        delegate.m_parameters.push_back(Param("backend", BlackboardType::Name, false, true));
        delegate.m_parameters.push_back(Param("goal", BlackboardType::Name));
        Register(AZStd::move(delegate));

        auto luaComposite = Simple("composite", NodeKind::Composite, NodeOp::LuaComposite, "Composite",
            "Control flow written in Lua, choosing which child runs and when to stop");
        luaComposite.m_parameters.push_back(Param("behavior", BlackboardType::Name, false, true));
        Register(AZStd::move(luaComposite));

        auto luaDecorator = Simple("decorator", NodeKind::Decorator, NodeOp::LuaDecorator, "Decorator",
            "Result filtering written in Lua");
        luaDecorator.m_parameters.push_back(Param("behavior", BlackboardType::Name, false, true));
        Register(AZStd::move(luaDecorator));

        auto subtree = Simple("subtree", NodeKind::Leaf, NodeOp::Subtree, "Leaf",
            "Runs another behavior tree in place, by name or through a rebindable slot");
        subtree.m_parameters.push_back(Param("tree", BlackboardType::Name));
        subtree.m_parameters.push_back(Param("tag", BlackboardType::Name));
        Register(AZStd::move(subtree));

        auto service = Simple("service", NodeKind::Service, NodeOp::Script, "Service",
            "Runs a Lua behavior on an interval while its subtree is active");
        service.m_parameters.push_back(Param("behavior", BlackboardType::Name, false, true));
        service.m_parameters.push_back(Param("interval", BlackboardType::Float));
        Register(AZStd::move(service));
    }

    bool NodeTypeRegistry::Register(NodeTypeDescriptor descriptor)
    {
        if (descriptor.m_name.IsEmpty())
        {
            return false;
        }

        if (m_types.contains(descriptor.m_name))
        {
            AZ_Warning("GOAT", false, "Node type '%s' is already registered", descriptor.m_name.GetCStr());
            return false;
        }

        m_types.emplace(descriptor.m_name, AZStd::move(descriptor));
        return true;
    }

    void NodeTypeRegistry::Unregister(const AZ::Name& name)
    {
        m_types.erase(name);
    }

    const NodeTypeDescriptor* NodeTypeRegistry::Find(const AZ::Name& name) const
    {
        const auto found = m_types.find(name);
        return found != m_types.end() ? &found->second : nullptr;
    }

    AZStd::vector<const NodeTypeDescriptor*> NodeTypeRegistry::GetAll() const
    {
        AZStd::vector<const NodeTypeDescriptor*> all;
        all.reserve(m_types.size());
        for (const auto& [name, descriptor] : m_types)
        {
            all.push_back(&descriptor);
        }
        return all;
    }
} // namespace GOAT
