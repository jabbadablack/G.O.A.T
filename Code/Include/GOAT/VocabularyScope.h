#pragma once

#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/NodeType.h>
#include <GOAT/Interfaces/IActionState.h>
#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT
{
    //! The verbs and authored words a module added to the core, taken back out when it goes.
    //!
    //! Every module was writing this out: the same install, the same two lists, and the same
    //! undo that each had to remember to call from Deactivate. Holding it here makes the undo
    //! the destructor's job, so a module that forgets cannot leave a word behind in a core that
    //! outlives it -- which would be a leaf that authors can still type and nothing can run.
    //!
    //! Header only because a module reaches the core through GOAT.API, which carries headers
    //! and no compiled code.
    class VocabularyScope final
    {
    public:
        //! @param noun which module this is, for the line each installed verb reports.
        explicit VocabularyScope(const char* noun)
            : m_noun(noun)
        {
        }

        ~VocabularyScope() { Clear(); }

        VocabularyScope(const VocabularyScope&) = delete;
        VocabularyScope& operator=(const VocabularyScope&) = delete;

        //! Installs a verb and the leaf word that runs it.
        //! The word and the verb share a name, which is what lets a tree name one and get both.
        bool Install(AZStd::unique_ptr<IActionState> action, NodeTypeDescriptor descriptor)
        {
            AZ_Assert(action != nullptr, "A verb must exist before it can be installed");

            IAgentSystem* agents = AgentSystemInterface::Get();
            AZ_Assert(agents != nullptr, "Installing a verb needs the GOAT agent system");
            if (agents == nullptr || action == nullptr)
            {
                return false;
            }

            const AZ::Name name = action->GetName();
            AZ_Assert(descriptor.m_name == name, "A leaf word and the verb it runs must share a name");

            const ActionStateId id = agents->RegisterAction(AZStd::move(action));
            if (id == CoreActions::Invalid)
            {
                AZ_Error("GOAT", false, "%s verb '%s' could not be registered", m_noun, name.GetCStr());
                return false;
            }
            m_actions.push_back(id);

            if (!agents->RegisterNodeType(AZStd::move(descriptor)))
            {
                return false;
            }
            m_nodeTypes.push_back(name);

            AZLOG_INFO("GOAT: %s registered verb '%s'", m_noun, name.GetCStr());
            return true;
        }

        //! Installs a word that runs no verb of its own, as a composite or a decorator does.
        bool InstallWord(NodeTypeDescriptor descriptor)
        {
            IAgentSystem* agents = AgentSystemInterface::Get();
            AZ_Assert(agents != nullptr, "Installing a word needs the GOAT agent system");
            if (agents == nullptr)
            {
                return false;
            }

            const AZ::Name name = descriptor.m_name;
            AZ_Assert(!name.IsEmpty(), "A word is always installed under a name");
            if (!agents->RegisterNodeType(AZStd::move(descriptor)))
            {
                return false;
            }

            m_nodeTypes.push_back(name);
            return true;
        }

        //! Remembers a reach filter so it leaves with the rest.
        void Own(const AZ::Name& reachFilter) { m_reachFilters.push_back(reachFilter); }

        //! Takes everything back out, in the reverse of the order it went in.
        void Clear()
        {
            IAgentSystem* agents = AgentSystemInterface::Get();
            if (agents == nullptr)
            {
                // The core shut down first, which takes its registries with it.
                m_reachFilters.clear();
                m_nodeTypes.clear();
                m_actions.clear();
                return;
            }

            for (const AZ::Name& name : m_reachFilters)
            {
                agents->UnregisterReachFilter(name);
            }
            m_reachFilters.clear();

            for (const AZ::Name& name : m_nodeTypes)
            {
                agents->UnregisterNodeType(name);
            }
            m_nodeTypes.clear();

            for (const ActionStateId id : m_actions)
            {
                agents->UnregisterAction(id);
            }
            m_actions.clear();
        }

    private:
        AZStd::vector<ActionStateId> m_actions;
        AZStd::vector<AZ::Name> m_nodeTypes;
        AZStd::vector<AZ::Name> m_reachFilters;
        const char* m_noun = "module";
    };
} // namespace GOAT
