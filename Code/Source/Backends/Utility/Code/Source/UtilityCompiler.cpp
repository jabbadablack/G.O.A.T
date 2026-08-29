#include <UtilityCompiler.h>

#include <AzCore/std/limits.h>
#include <AzCore/std/string/string.h>

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

        //! Whether a property was written at all, whatever it holds.
        bool Given(const AuthoredNode& node, const char* name)
        {
            for (const AuthoredProperty& property : node.m_properties)
            {
                if (property.m_name == name)
                {
                    return true;
                }
            }
            return false;
        }

        //! What a blackboard type is called when a message has to say one was the wrong kind.
        const char* TypeName(BlackboardType type)
        {
            switch (type)
            {
            case BlackboardType::Bool: return "boolean";
            case BlackboardType::Int: return "whole number";
            case BlackboardType::Float: return "float";
            case BlackboardType::Vector3: return "position";
            case BlackboardType::EntityId: return "entity";
            case BlackboardType::Name: return "name";
            case BlackboardType::Quaternion: return "rotation";
            case BlackboardType::Transform: return "transform";
            case BlackboardType::EntityIdList: return "list of entities";
            default: return "value";
            }
        }
    } // namespace

    UtilityCompiler::UtilityCompiler(IAgentSystem& host, const IBlackboardSystem& blackboard)
        : m_host(host)
        , m_blackboard(blackboard)
    {
    }

    AZ::Outcome<BlackboardKey, AZStd::string> UtilityCompiler::ResolveKey(const AuthoredNode& authored) const
    {
        const AZStd::string name = Text(authored, "key");
        if (name.empty())
        {
            return AZ::Failure(AZStd::string("a 'consider' names no variable"));
        }

        const BlackboardKey key = m_blackboard.FindKey(AZ::Name(name));
        if (!key.IsValid())
        {
            return AZ::Failure(AZStd::string::format("'%s' is not a declared variable", name.c_str()));
        }
        if (key.GetType() != BlackboardType::Float)
        {
            return AZ::Failure(AZStd::string::format(
                "'%s' is a %s; a consideration reads a float already scaled to 0 to 1", name.c_str(),
                TypeName(key.GetType())));
        }
        return AZ::Success(key);
    }

    AZ::Outcome<void, AZStd::string> UtilityCompiler::ResolveCombine(
        const AuthoredNode& authored, UtilityChoice& choice) const
    {
        const AZStd::string named = Text(authored, "combine");
        if (named.empty())
        {
            return AZ::Success();
        }

        if (named == "multiply") { choice.m_combine = CombineRule::Multiply; return AZ::Success(); }
        if (named == "mean")     { choice.m_combine = CombineRule::Mean;     return AZ::Success(); }
        if (named == "min")      { choice.m_combine = CombineRule::Min;      return AZ::Success(); }
        if (named == "max")      { choice.m_combine = CombineRule::Max;      return AZ::Success(); }

        // Anything else names a behaviour, which is where an author writes the maths and the
        // curves this has none of. Checked now rather than found missing on the tick that needed it.
        if (!m_host.HasBehavior(AZ::Name(named)))
        {
            return AZ::Failure(AZStd::string::format(
                "choice '%s' combines with '%s', which is neither one of multiply, mean, min and max "
                "nor a declared behaviour", choice.m_name.GetCStr(), named.c_str()));
        }

        choice.m_combine = CombineRule::Behavior;
        choice.m_combineBehavior = AZ::Name(named);
        return AZ::Success();
    }

    AZ::Outcome<ActionRequest, AZStd::string> UtilityCompiler::ResolveStep(
        const AuthoredNode& authored, const UtilityChoice& choice, UtilityProgram& program) const
    {
        const NodeTypeDescriptor* descriptor = m_host.FindNodeType(AZ::Name(authored.m_type));
        if (descriptor == nullptr)
        {
            return AZ::Failure(AZStd::string::format("'%s' is not a word any backend registered",
                authored.m_type.c_str()));
        }

        // A choice's steps already run in order, so the shapes that exist to order things are
        // a tree's job. One is reached from here by embedding it, not by restating it.
        if (descriptor->m_kind != NodeKind::Leaf)
        {
            return AZ::Failure(AZStd::string::format(
                "choice '%s' holds a '%s', which is a shape a tree walks; a choice's steps already run "
                "in order, and a tree it should embed", choice.m_name.GetCStr(), authored.m_type.c_str()));
        }

        // Reserved by the core: a plan step naming it would let a plan re-enter the tree that
        // asked for it, so there is no verb behind it and the message has to say what to use.
        if (descriptor->m_op == NodeOp::Delegate)
        {
            return AZ::Failure(AZStd::string::format(
                "choice '%s' delegates to '%s'; a delegate is a behaviour tree leaf, and a choice reaches "
                "another paradigm with embed", choice.m_name.GetCStr(),
                Text(authored, "backend").c_str()));
        }

        // Read through the word's own declared properties, the way a tree reads them, so a
        // choice can run any verb a module contributed rather than the few named here.
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

        // What this step names is compiled by whoever owns that word, which is the core's job:
        // the compiler running here knows one paradigm and the named program may be another.
        if (descriptor->m_nestsProgram && !request.m_tag.IsEmpty())
        {
            program.m_nested.push_back({ AZ::Name{}, request.m_tag, true });
        }

        return AZ::Success(request);
    }

    AZ::Outcome<void, AZStd::string> UtilityCompiler::EmitChoice(
        const AuthoredNode& authored, UtilityProgram& program, AZ::u16 index) const
    {
        UtilityChoice& choice = program.m_choices[index];
        choice.m_commit = Flag(authored, "commit", false);

        if (auto combine = ResolveCombine(authored, choice); !combine.IsSuccess())
        {
            return combine;
        }

        if (const AZStd::string scorer = Text(authored, "score"); !scorer.empty())
        {
            if (!m_host.HasBehavior(AZ::Name(scorer)))
            {
                return AZ::Failure(AZStd::string::format("choice '%s' scores through '%s', which is not a "
                    "declared behaviour", choice.m_name.GetCStr(), scorer.c_str()));
            }
            choice.m_scoreBehavior = AZ::Name(scorer);
        }

        choice.m_firstConsideration = static_cast<AZ::u16>(program.m_considerations.size());
        choice.m_firstStep = static_cast<AZ::u16>(program.m_steps.size());

        for (const AuthoredNode& child : authored.m_children)
        {
            if (child.m_type == "consider")
            {
                auto key = ResolveKey(child);
                if (!key.IsSuccess())
                {
                    return AZ::Failure(AZStd::string::format("choice '%s': %s", choice.m_name.GetCStr(),
                        key.GetError().c_str()));
                }

                for (AZ::u16 i = 0; i < choice.m_considerationCount; ++i)
                {
                    if (program.m_considerations[choice.m_firstConsideration + i].m_key == key.GetValue())
                    {
                        return AZ::Failure(AZStd::string::format("choice '%s' considers '%s' twice",
                            choice.m_name.GetCStr(), Text(child, "key").c_str()));
                    }
                }

                if (choice.m_considerationCount >= MaxConsiderations)
                {
                    return AZ::Failure(AZStd::string::format(
                        "choice '%s' considers more than %u variables", choice.m_name.GetCStr(),
                        MaxConsiderations));
                }

                program.m_considerations.push_back({ key.GetValue(), false });
                ++choice.m_considerationCount;
                continue;
            }

            auto step = ResolveStep(child, choice, program);
            if (!step.IsSuccess())
            {
                return AZ::Failure(step.TakeError());
            }

            if (choice.m_stepCount >= MaxChoiceSteps)
            {
                return AZ::Failure(AZStd::string::format("choice '%s' runs more than %u steps",
                    choice.m_name.GetCStr(), MaxChoiceSteps));
            }

            program.m_steps.push_back(step.GetValue());
            ++choice.m_stepCount;
        }

        if (choice.m_stepCount == 0)
        {
            return AZ::Failure(AZStd::string::format("choice '%s' does nothing", choice.m_name.GetCStr()));
        }

        return AZ::Success();
    }

    AZ::Outcome<void, AZStd::string> UtilityCompiler::ReadPicking(
        const AuthoredNode& authored, UtilityProgram& program) const
    {
        program.m_recheck = Number(authored, "recheck", DefaultRecheck);
        if (program.m_recheck < 0.0f)
        {
            return AZ::Failure(AZStd::string::format(
                "rechecks every %.2f seconds, and a floor cannot be negative",
                static_cast<double>(program.m_recheck)));
        }

        program.m_momentum = Number(authored, "momentum", 0.0f);
        if (program.m_momentum < 0.0f)
        {
            return AZ::Failure(AZStd::string::format(
                "has a momentum of %.2f; momentum raises the running choice and cannot be negative",
                static_cast<double>(program.m_momentum)));
        }

        const AZStd::string pick = Text(authored, "pick");
        if (!pick.empty() && pick != "best" && pick != "weighted")
        {
            return AZ::Failure(AZStd::string::format(
                "picks by '%s', and a program picks the 'best' or 'weighted'", pick.c_str()));
        }
        program.m_pick = pick == "weighted" ? PickRule::Weighted : PickRule::Best;

        const float top = Number(authored, "top", 0.0f);
        if (Given(authored, "top"))
        {
            if (program.m_pick != PickRule::Weighted)
            {
                return AZ::Failure(AZStd::string(
                    "names a top but picks the best, where only one is ever in the running"));
            }
            if (top < 1.0f || top > static_cast<float>(MaxChoices))
            {
                return AZ::Failure(AZStd::string::format(
                    "draws from the top %.0f, which is not a number of choices", static_cast<double>(top)));
            }
        }
        program.m_top = Given(authored, "top") ? static_cast<AZ::u16>(top) : MaxChoices;

        return AZ::Success();
    }

    AZ::Outcome<UtilityProgram, AZStd::string> UtilityCompiler::Compile(
        const AZ::Name& name, const AuthoredNode& root) const
    {
        if (root.m_type != "utility")
        {
            return AZ::Failure(AZStd::string::format("'%s' is a '%s', not a utility program", name.GetCStr(),
                root.m_type.c_str()));
        }

        UtilityProgram program;
        program.m_name = name;

        if (auto picking = ReadPicking(root, program); !picking.IsSuccess())
        {
            return AZ::Failure(AZStd::string::format("utility program '%s' %s", name.GetCStr(),
                picking.GetError().c_str()));
        }

        if (root.m_children.empty())
        {
            return AZ::Failure(AZStd::string::format("utility program '%s' holds nothing to choose between",
                name.GetCStr()));
        }
        if (root.m_children.size() > MaxChoices)
        {
            return AZ::Failure(AZStd::string::format("utility program '%s' holds %zu choices, and a program "
                "may hold %u", name.GetCStr(), root.m_children.size(), MaxChoices));
        }

        // Named first so a message about one can say which it meant, whatever fails later.
        for (size_t i = 0; i < root.m_children.size(); ++i)
        {
            const AuthoredNode& child = root.m_children[i];
            if (child.m_type != "choice")
            {
                return AZ::Failure(AZStd::string::format(
                    "utility program '%s' holds a '%s', and a utility program holds choices", name.GetCStr(),
                    child.m_type.c_str()));
            }

            const AZStd::string choiceName = Text(child, "name");
            if (choiceName.empty())
            {
                return AZ::Failure(AZStd::string::format("utility program '%s' holds an unnamed choice",
                    name.GetCStr()));
            }
            if (program.FindChoice(AZ::Name(choiceName)) != InvalidChoice)
            {
                return AZ::Failure(AZStd::string::format("utility program '%s' declares '%s' twice",
                    name.GetCStr(), choiceName.c_str()));
            }

            UtilityChoice choice;
            choice.m_name = AZ::Name(choiceName);
            program.m_choices.push_back(AZStd::move(choice));

            // Recorded the same way a tree records its nodes, so one path means one node
            // whatever paradigm produced it.
            ProgramNodeRef location;
            location.m_program = name;
            location.m_path.push_back(aznumeric_cast<AZ::u16>(root.m_services.size() + i));
            program.m_authored.push_back(location);
        }

        for (size_t index = 0; index < root.m_children.size(); ++index)
        {
            if (auto emitted = EmitChoice(root.m_children[index], program, static_cast<AZ::u16>(index));
                !emitted.IsSuccess())
            {
                return AZ::Failure(AZStd::string::format("utility program '%s': %s", name.GetCStr(),
                    emitted.GetError().c_str()));
            }
        }

        // Considerations only. What a choice's steps write is what the agent does, not what it
        // decides on, so a program must not wake itself on its own output.
        for (const UtilityConsideration& considered : program.m_considerations)
        {
            program.m_watchedScopes[static_cast<size_t>(considered.m_key.GetScope())] = true;
        }

        return AZ::Success(AZStd::move(program));
    }
} // namespace GOAT
