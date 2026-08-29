#include <HtnWords.h>

#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    namespace
    {
        NodeTypeDescriptor Word(
            const char* name, NodeKind kind, const char* category, const char* description)
        {
            NodeTypeDescriptor descriptor;
            descriptor.m_name = AZ::Name(name);
            descriptor.m_kind = kind;
            descriptor.m_category = category;
            descriptor.m_description = description;
            descriptor.m_backend = AZ_NAME_LITERAL("htn");
            return descriptor;
        }

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

    AZStd::vector<NodeTypeDescriptor> HtnWords()
    {
        AZStd::vector<NodeTypeDescriptor> words;

        auto domain = Word("domain", NodeKind::Composite, "Task Network",
            "A task network. Planning starts at the first task unless one is named");
        domain.m_parameters.push_back(Param("root", BlackboardType::Name));
        words.push_back(AZStd::move(domain));

        auto task = Word("task", NodeKind::Composite, "Task Network",
            "Compound task, decomposed by whichever of its methods holds");
        task.m_parameters.push_back(Param("name", BlackboardType::Name, false, true));
        words.push_back(AZStd::move(task));

        words.push_back(Word("method", NodeKind::Composite, "Task Network",
            "One way to decompose a task, holding the conditions it needs and the subtasks it runs"));

        auto primitive = Word("primitive", NodeKind::Composite, "Task Network",
            "Task that runs one verb and writes what it changed");
        primitive.m_parameters.push_back(Param("name", BlackboardType::Name, false, true));
        words.push_back(AZStd::move(primitive));

        auto subtask = Word("subtask", NodeKind::Leaf, "Task Network", "Runs another task in this domain");
        subtask.m_parameters.push_back(Param("task", BlackboardType::Name, false, true));
        words.push_back(AZStd::move(subtask));

        auto effect = Word("effect", NodeKind::Leaf, "Task Network",
            "What a primitive is taken to have changed, so planning can carry on from it");
        effect.m_parameters.push_back(Param("key", BlackboardType::Bool, true, true));
        effect.m_parameters.push_back(Param("is", BlackboardType::Bool));
        words.push_back(AZStd::move(effect));

        return words;
    }

    bool InstallHtnWords(VocabularyScope& vocabulary)
    {
        bool installed = true;
        for (NodeTypeDescriptor& word : HtnWords())
        {
            installed = vocabulary.InstallWord(AZStd::move(word)) && installed;
        }

        AZ_Error("GOAT", installed, "The task network could not install its full vocabulary");
        return installed;
    }
} // namespace GOAT
