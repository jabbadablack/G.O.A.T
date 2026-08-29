#include <BehaviorTreeWords.h>

#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    namespace
    {
        //! A word that takes no properties.
        NodeTypeDescriptor Word(
            const char* name, NodeKind kind, NodeOp op, const char* category, const char* description)
        {
            NodeTypeDescriptor descriptor;
            descriptor.m_name = AZ::Name(name);
            descriptor.m_kind = kind;
            descriptor.m_op = op;
            descriptor.m_category = category;
            descriptor.m_description = description;
            descriptor.m_backend = AZ_NAME_LITERAL("tree");
            return descriptor;
        }

        //! One property on a word.
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

    AZStd::vector<NodeTypeDescriptor> BehaviorTreeWords()
    {
        AZStd::vector<NodeTypeDescriptor> words;
        words.push_back(Word("selector", NodeKind::Composite, NodeOp::Selector, "Composite", "Runs children until one succeeds"));
        words.push_back(
            Word("sequence", NodeKind::Composite, NodeOp::Sequence, "Composite", "Runs children until one fails"));

        // Exactly two children: the main branch, then a background branch of conditions that is
        // re-checked while it runs. One agent has one action slot, so the background may not act.
        words.push_back(Word("parallel", NodeKind::Composite, NodeOp::Parallel, "Composite",
                        "Runs a main branch while a background branch of conditions is re-checked"));

        words.push_back(
            Word("invert", NodeKind::Decorator, NodeOp::Invert, "Decorator", "Flips success and failure"));
        words.push_back(Word("force_success", NodeKind::Decorator, NodeOp::ForceSuccess,
                        "Decorator", "Always reports success"));

        auto cooldown = Word("cooldown", NodeKind::Decorator, NodeOp::Cooldown, "Decorator",
            "Blocks re-entry until a duration has passed");
        cooldown.m_parameters.push_back(Param("seconds", BlackboardType::Float, false, true));
        words.push_back(AZStd::move(cooldown));

        auto loop = Word("loop", NodeKind::Decorator, NodeOp::Loop, "Decorator", "Repeats a fixed number of times");
        loop.m_parameters.push_back(Param("count", BlackboardType::Int, false, true));
        words.push_back(AZStd::move(loop));

        auto conditionalLoop = Word("conditional_loop", NodeKind::Decorator, NodeOp::ConditionalLoop, "Decorator",
            "Repeats while a blackboard condition holds");
        conditionalLoop.m_parameters.push_back(Param("key", BlackboardType::Bool, true, true));
        words.push_back(AZStd::move(conditionalLoop));

        auto timeLimit = Word("time_limit", NodeKind::Decorator, NodeOp::TimeLimit, "Decorator",
            "Fails the child once a duration elapses");
        timeLimit.m_parameters.push_back(Param("seconds", BlackboardType::Float, false, true));
        words.push_back(AZStd::move(timeLimit));

        auto luaComposite = Word("composite", NodeKind::Composite, NodeOp::LuaComposite, "Composite",
            "Control flow written in Lua, choosing which child runs and when to stop");
        luaComposite.m_parameters.push_back(Param("behavior", BlackboardType::Name, false, true));
        words.push_back(AZStd::move(luaComposite));

        auto luaDecorator = Word("decorator", NodeKind::Decorator, NodeOp::LuaDecorator, "Decorator",
            "Result filtering written in Lua");
        luaDecorator.m_parameters.push_back(Param("behavior", BlackboardType::Name, false, true));
        words.push_back(AZStd::move(luaDecorator));

        auto subtree = Word("subtree", NodeKind::Leaf, NodeOp::Subtree, "Leaf",
            "Runs another behavior tree in place, by name or through a rebindable slot");
        subtree.m_parameters.push_back(Param("tree", BlackboardType::Name));
        subtree.m_parameters.push_back(Param("tag", BlackboardType::Name));
        words.push_back(AZStd::move(subtree));

        auto service = Word("service", NodeKind::Service, NodeOp::Script, "Service",
            "Runs a Lua behavior on an interval while its subtree is active");
        service.m_parameters.push_back(Param("behavior", BlackboardType::Name, false, true));
        service.m_parameters.push_back(Param("interval", BlackboardType::Float));
        words.push_back(AZStd::move(service));

        return words;
    }

    bool InstallBehaviorTreeWords(VocabularyScope& vocabulary)
    {
        bool installed = true;
        for (NodeTypeDescriptor& word : BehaviorTreeWords())
        {
            installed = vocabulary.InstallWord(AZStd::move(word)) && installed;
        }

        AZ_Error("GOAT", installed, "The behaviour tree could not install its full vocabulary");
        return installed;
    }
} // namespace GOAT
