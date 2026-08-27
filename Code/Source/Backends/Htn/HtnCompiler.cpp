#include <Backends/Htn/HtnCompiler.h>

#include <AzCore/std/algorithm.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/sort.h>

namespace GOAT
{
    namespace
    {
        //! An authored property as a string, or empty.
        AZStd::string Text(const AuthoredNode& node, const char* name)
        {
            for (const AuthoredProperty& property : node.m_properties)
            {
                if (property.m_name == name && property.m_value.is<AZStd::string>())
                {
                    return AZStd::any_cast<AZStd::string>(property.m_value);
                }
            }
            return {};
        }

        //! An authored number, or a fallback.
        float Number(const AuthoredNode& node, const char* name, float fallback)
        {
            for (const AuthoredProperty& property : node.m_properties)
            {
                if (property.m_name == name && property.m_value.is<double>())
                {
                    return static_cast<float>(AZStd::any_cast<double>(property.m_value));
                }
            }
            return fallback;
        }

        //! An authored bool, or a fallback.
        bool Flag(const AuthoredNode& node, const char* name, bool fallback)
        {
            for (const AuthoredProperty& property : node.m_properties)
            {
                if (property.m_name == name && property.m_value.is<bool>())
                {
                    return AZStd::any_cast<bool>(property.m_value);
                }
            }
            return fallback;
        }
    } // namespace

    AZ::u16 HtnDomain::FindTask(const AZ::Name& name) const
    {
        for (size_t i = 0; i < m_tasks.size(); ++i)
        {
            if (m_tasks[i].m_name == name)
            {
                return static_cast<AZ::u16>(i);
            }
        }
        return InvalidTask;
    }

    HtnCompiler::HtnCompiler(IAgentSystem& host, const IBlackboardSystem& blackboard)
        : m_host(host)
        , m_blackboard(blackboard)
    {
    }

    AZ::Outcome<BlackboardKey, AZStd::string> HtnCompiler::ResolveKey(const AuthoredNode& authored) const
    {
        const AZStd::string name = Text(authored, "key");
        if (name.empty())
        {
            return AZ::Failure(AZStd::string::format("a '%s' names no variable", authored.m_type.c_str()));
        }

        const BlackboardKey key = m_blackboard.FindKey(AZ::Name(name));
        if (!key.IsValid())
        {
            return AZ::Failure(AZStd::string::format("'%s' is not a declared variable", name.c_str()));
        }
        if (key.GetType() != BlackboardType::Bool)
        {
            return AZ::Failure(AZStd::string::format("'%s' is not a boolean, which is all a task "
                "network reasons about", name.c_str()));
        }
        return AZ::Success(key);
    }

    AZ::Outcome<ActionRequest, AZStd::string> HtnCompiler::ResolveOperator(const AuthoredNode& authored) const
    {
        // Read through the word's own declared properties, the way a tree reads them, so a
        // primitive can run any verb a module contributed rather than the few named here.
        const NodeTypeDescriptor* descriptor = m_host.FindNodeType(AZ::Name(authored.m_type));
        if (descriptor == nullptr)
        {
            return AZ::Failure(AZStd::string::format("'%s' is not a word any backend registered",
                authored.m_type.c_str()));
        }

        const bool isRaw = authored.m_type == "raw";
        BlackboardKey key;
        AZ::Name tag;
        AZ::Name goal;
        float amount = 0.0f;
        float tolerance = 0.0f;

        for (const NodeParameter& parameter : descriptor->m_parameters)
        {
            if (parameter.m_isBlackboardKey)
            {
                const AZStd::string named = Text(authored, parameter.m_name.GetCStr());
                if (named.empty())
                {
                    continue;
                }
                key = m_blackboard.FindKey(AZ::Name(named));
                if (!key.IsValid())
                {
                    return AZ::Failure(AZStd::string::format("'%s' refers to undeclared variable '%s'",
                        authored.m_type.c_str(), named.c_str()));
                }
                continue;
            }

            if (parameter.m_type == BlackboardType::Name)
            {
                const AZStd::string text = Text(authored, parameter.m_name.GetCStr());
                if (text.empty())
                {
                    continue;
                }
                if (parameter.m_name == AZ_NAME_LITERAL("goal") || parameter.m_name == AZ_NAME_LITERAL("payload"))
                {
                    goal = AZ::Name(text);
                }
                else
                {
                    tag = AZ::Name(text);
                }
                continue;
            }

            const float number = Number(authored, parameter.m_name.GetCStr(), AZStd::numeric_limits<float>::max());
            if (number == AZStd::numeric_limits<float>::max())
            {
                continue;
            }
            if (parameter.m_name == AZ_NAME_LITERAL("tolerance"))
            {
                tolerance = number;
            }
            else
            {
                amount = number;
            }
        }

        // `raw` spends its own name saying which verb to run, so its payload is what it carries.
        const AZ::Name verbName = isRaw ? tag : AZ::Name(authored.m_type);
        const ActionStateId verb = m_host.FindVerb(verbName);
        if (verb == CoreActions::Invalid)
        {
            return AZ::Failure(AZStd::string::format("'%s' runs verb '%s', which no module has registered",
                authored.m_type.c_str(), verbName.GetCStr()));
        }

        ActionRequest request;
        request.m_action = verb;
        request.m_targetKey = key;
        request.m_amount = amount;
        request.m_tolerance = tolerance;
        request.m_tag = isRaw || tag.IsEmpty() ? goal : tag;
        return AZ::Success(request);
    }

    AZ::Outcome<void, AZStd::string> HtnCompiler::EmitPrimitive(
        const AuthoredNode& authored, HtnDomain& domain, AZ::u16 index) const
    {
        HtnTask& task = domain.m_tasks[index];
        task.m_firstCondition = static_cast<AZ::u16>(domain.m_conditions.size());
        bool hasOperator = false;

        for (const AuthoredNode& child : authored.m_children)
        {
            if (child.m_type == "condition")
            {
                auto key = ResolveKey(child);
                if (!key.IsSuccess())
                {
                    return AZ::Failure(key.TakeError());
                }
                domain.m_conditions.push_back(HtnCondition{ key.GetValue(), Flag(child, "is", true) });
                ++task.m_conditionCount;
            }
            else if (child.m_type != "effect")
            {
                if (hasOperator)
                {
                    return AZ::Failure(AZStd::string::format("primitive '%s' runs more than one verb",
                        task.m_name.GetCStr()));
                }
                auto request = ResolveOperator(child);
                if (!request.IsSuccess())
                {
                    return AZ::Failure(request.TakeError());
                }
                task.m_action = request.GetValue();
                hasOperator = true;
            }
        }

        // Effects after conditions, so the two ranges stay contiguous per task.
        task.m_firstEffect = static_cast<AZ::u16>(domain.m_effects.size());
        for (const AuthoredNode& child : authored.m_children)
        {
            if (child.m_type != "effect")
            {
                continue;
            }
            auto key = ResolveKey(child);
            if (!key.IsSuccess())
            {
                return AZ::Failure(key.TakeError());
            }
            domain.m_effects.push_back(HtnEffect{ key.GetValue(), Flag(child, "is", true) });
            ++task.m_effectCount;
        }

        if (!hasOperator)
        {
            return AZ::Failure(AZStd::string::format("primitive '%s' runs no verb", task.m_name.GetCStr()));
        }
        return AZ::Success();
    }

    AZ::Outcome<void, AZStd::string> HtnCompiler::EmitTask(
        const AuthoredNode& authored, HtnDomain& domain, AZ::u16 index) const
    {
        domain.m_tasks[index].m_firstMethod = static_cast<AZ::u16>(domain.m_methods.size());

        for (const AuthoredNode& child : authored.m_children)
        {
            if (child.m_type != "method")
            {
                return AZ::Failure(AZStd::string::format("task '%s' holds a '%s'; a task holds methods",
                    domain.m_tasks[index].m_name.GetCStr(), child.m_type.c_str()));
            }

            HtnMethod method;
            method.m_firstCondition = static_cast<AZ::u16>(domain.m_conditions.size());
            method.m_firstSubtask = static_cast<AZ::u16>(domain.m_subtasks.size());

            for (const AuthoredNode& part : child.m_children)
            {
                if (part.m_type == "condition")
                {
                    auto key = ResolveKey(part);
                    if (!key.IsSuccess())
                    {
                        return AZ::Failure(key.TakeError());
                    }
                    domain.m_conditions.push_back(HtnCondition{ key.GetValue(), Flag(part, "is", true) });
                    ++method.m_conditionCount;
                }
                else if (part.m_type == "subtask")
                {
                    const AZ::Name named(Text(part, "task"));
                    const AZ::u16 target = domain.FindTask(named);
                    if (target == InvalidTask)
                    {
                        return AZ::Failure(AZStd::string::format("'%s' is not a task in this domain",
                            named.GetCStr()));
                    }
                    domain.m_subtasks.push_back(target);
                    ++method.m_subtaskCount;
                }
                else
                {
                    return AZ::Failure(AZStd::string::format("a method holds conditions and subtasks, not '%s'",
                        part.m_type.c_str()));
                }
            }

            domain.m_methods.push_back(method);
            ++domain.m_tasks[index].m_methodCount;
        }

        if (domain.m_tasks[index].m_methodCount == 0)
        {
            return AZ::Failure(AZStd::string::format("task '%s' has no methods",
                domain.m_tasks[index].m_name.GetCStr()));
        }
        return AZ::Success();
    }

    AZ::Outcome<HtnDomain, AZStd::string> HtnCompiler::Compile(const AZ::Name& name, const AuthoredNode& root) const
    {
        HtnDomain domain;
        domain.m_name = name;

        if (root.m_type != "domain")
        {
            return AZ::Failure(AZStd::string::format("'%s' is a '%s', not a domain", name.GetCStr(),
                root.m_type.c_str()));
        }

        // Named first, all of them, so a method may list a task written further down the file.
        for (const AuthoredNode& child : root.m_children)
        {
            const bool primitive = child.m_type == "primitive";
            if (!primitive && child.m_type != "task")
            {
                return AZ::Failure(AZStd::string::format("a domain holds tasks and primitives, not '%s'",
                    child.m_type.c_str()));
            }

            HtnTask task;
            task.m_name = AZ::Name(Text(child, "name"));
            task.m_isPrimitive = primitive;
            if (task.m_name.IsEmpty())
            {
                return AZ::Failure(AZStd::string::format("domain '%s' holds an unnamed task", name.GetCStr()));
            }
            if (domain.FindTask(task.m_name) != InvalidTask)
            {
                return AZ::Failure(AZStd::string::format("'%s' is declared twice", task.m_name.GetCStr()));
            }
            domain.m_tasks.push_back(task);
        }

        if (domain.m_tasks.empty())
        {
            return AZ::Failure(AZStd::string::format("domain '%s' holds nothing to run", name.GetCStr()));
        }

        for (size_t i = 0; i < root.m_children.size(); ++i)
        {
            const AZ::u16 index = static_cast<AZ::u16>(i);
            auto emitted = domain.m_tasks[index].m_isPrimitive
                ? EmitPrimitive(root.m_children[i], domain, index)
                : EmitTask(root.m_children[i], domain, index);
            if (!emitted.IsSuccess())
            {
                return AZ::Failure(AZStd::string::format("domain '%s': %s", name.GetCStr(),
                    emitted.GetError().c_str()));
            }
        }

        // Sorted and deduplicated so the working state is a dense array and a lookup is a search.
        for (const HtnCondition& condition : domain.m_conditions)
        {
            domain.m_touchedKeys.push_back(condition.m_key);
        }
        for (const HtnEffect& effect : domain.m_effects)
        {
            domain.m_touchedKeys.push_back(effect.m_key);
        }
        AZStd::sort(domain.m_touchedKeys.begin(), domain.m_touchedKeys.end());
        domain.m_touchedKeys.erase(
            AZStd::unique(domain.m_touchedKeys.begin(), domain.m_touchedKeys.end()), domain.m_touchedKeys.end());

        if (domain.m_touchedKeys.size() > MaxDomainKeys)
        {
            return AZ::Failure(AZStd::string::format("domain '%s' reasons about %zu variables but a domain may "
                "hold %u", name.GetCStr(), domain.m_touchedKeys.size(), MaxDomainKeys));
        }

        // A domain guards on whatever its conditions read, which is what wakes an agent running it.
        for (const BlackboardKey key : domain.m_touchedKeys)
        {
            domain.m_watchedScopes[static_cast<size_t>(key.GetScope())] = true;
        }

        const AZStd::string named = Text(root, "root");
        domain.m_root = named.empty() ? 0 : domain.FindTask(AZ::Name(named));
        if (domain.m_root == InvalidTask)
        {
            return AZ::Failure(AZStd::string::format("domain '%s' starts at '%s', which is not one of its tasks",
                name.GetCStr(), named.c_str()));
        }

        return AZ::Success(AZStd::move(domain));
    }
} // namespace GOAT
