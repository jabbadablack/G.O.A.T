#include <UtilityWords.h>

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
            descriptor.m_backend = AZ_NAME_LITERAL("utility");
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

    AZStd::vector<NodeTypeDescriptor> UtilityWords()
    {
        AZStd::vector<NodeTypeDescriptor> words;

        auto utility = Word("utility", NodeKind::Composite, "Utility",
            "A set of scored choices, of which the best one runs");
        utility.m_parameters.push_back(Param("pick", BlackboardType::Name));
        utility.m_parameters.push_back(Param("top", BlackboardType::Float));
        utility.m_parameters.push_back(Param("recheck", BlackboardType::Float));
        utility.m_parameters.push_back(Param("momentum", BlackboardType::Float));
        words.push_back(AZStd::move(utility));

        auto choice = Word("choice", NodeKind::Composite, "Utility",
            "One option, scored from what it considers and holding the steps it runs");
        choice.m_parameters.push_back(Param("name", BlackboardType::Name, false, true));
        choice.m_parameters.push_back(Param("combine", BlackboardType::Name));
        choice.m_parameters.push_back(Param("score", BlackboardType::Name));
        choice.m_parameters.push_back(Param("commit", BlackboardType::Bool));
        words.push_back(AZStd::move(choice));

        auto consider = Word("consider", NodeKind::Leaf, "Utility",
            "A variable, already scaled to zero and one, that this choice scores from");
        consider.m_parameters.push_back(Param("key", BlackboardType::Float, true, true));
        words.push_back(AZStd::move(consider));

        return words;
    }

    bool InstallUtilityWords(VocabularyScope& vocabulary)
    {
        bool installed = true;
        for (NodeTypeDescriptor& word : UtilityWords())
        {
            installed = vocabulary.InstallWord(AZStd::move(word)) && installed;
        }

        AZ_Error("GOAT", installed, "The utility backend could not install its full vocabulary");
        return installed;
    }
} // namespace GOAT
