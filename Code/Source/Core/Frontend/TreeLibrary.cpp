#include <Core/Frontend/TreeLibrary.h>

#include <AzCore/Console/ILogger.h>

namespace GOAT
{
    void TreeLibrary::Add(const AZ::Name& name, AZStd::shared_ptr<const BehaviorTreeNode> root)
    {
        AZ_Assert(!name.IsEmpty(), "A tree must be stored under a name");
        AZ_Assert(root != nullptr, "A tree must be stored with a root node");

        if (name.IsEmpty() || root == nullptr)
        {
            AZ_Error("GOAT", false, "Refusing to store tree '%s': %s", name.GetCStr(),
                name.IsEmpty() ? "it has no name" : "it has no root node");
            return;
        }

        // Re-adding is routine: every agent sharing a script re-emits the same tree from it,
        // so this is a replace, not a redefinition, and there is nothing to report.
        m_trees[name] = AZStd::move(root);

        AZ_Assert(Find(name) != nullptr, "A stored tree must be findable by its name");
    }

    const BehaviorTreeNode* TreeLibrary::Find(const AZ::Name& name) const
    {
        const auto found = m_trees.find(name);
        return found != m_trees.end() ? found->second.get() : nullptr;
    }
    void TreeLibrary::Bind(const AZ::Name& slot, const AZ::Name& treeName)
    {
        AZ_Assert(!slot.IsEmpty(), "A dynamic subtree slot must be named");
        if (slot.IsEmpty())
        {
            AZ_Error("GOAT", false, "Refusing to bind an unnamed subtree slot to tree '%s'", treeName.GetCStr());
            return;
        }

        AZLOG_INFO("GOAT: subtree slot '%s' now runs tree '%s'", slot.GetCStr(), treeName.GetCStr());
        m_bindings[slot] = treeName;

        AZ_Assert(GetBinding(slot) == treeName, "Binding a slot must leave it bound to that tree");
    }

    AZ::Name TreeLibrary::GetBinding(const AZ::Name& slot) const
    {
        const auto found = m_bindings.find(slot);
        return found != m_bindings.end() ? found->second : AZ::Name{};
    }

    AZStd::vector<AZ::Name> TreeLibrary::GetNames() const
    {
        AZStd::vector<AZ::Name> names;
        names.reserve(m_trees.size());
        for (const auto& [name, root] : m_trees)
        {
            names.push_back(name);
        }

        AZ_Assert(names.size() == m_trees.size(), "Listing trees must report exactly as many as are stored");
        return names;
    }
} // namespace GOAT
