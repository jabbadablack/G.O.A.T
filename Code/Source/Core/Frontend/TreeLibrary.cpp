#include <Core/Frontend/TreeLibrary.h>

namespace GOAT
{
    void TreeLibrary::Add(const AZ::Name& name, AZStd::shared_ptr<const BehaviorTreeAsset> asset)
    {
        if (name.IsEmpty() || asset == nullptr)
        {
            return;
        }
        m_trees[name] = AZStd::move(asset);
    }

    const BehaviorTreeAsset* TreeLibrary::Find(const AZ::Name& name) const
    {
        const auto found = m_trees.find(name);
        return found != m_trees.end() ? found->second.get() : nullptr;
    }

    void TreeLibrary::Remove(const AZ::Name& name)
    {
        m_trees.erase(name);
    }

    void TreeLibrary::Bind(const AZ::Name& slot, const AZ::Name& treeName)
    {
        if (slot.IsEmpty())
        {
            return;
        }
        m_bindings[slot] = treeName;
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
        for (const auto& [name, asset] : m_trees)
        {
            names.push_back(name);
        }
        return names;
    }

    void TreeLibrary::Clear()
    {
        m_trees.clear();
        m_bindings.clear();
    }
} // namespace GOAT
