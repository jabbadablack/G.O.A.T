#pragma once

#include <GOAT/Assets/BehaviorTreeAsset.h>

#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

namespace GOAT
{
    //! The authored trees available to compile against, so one tree can reference another.
    //! Holds the authored form, not the compiled one, because composition happens at compile time.
    class TreeLibrary final
    {
    public:
        //! Adds or replaces a tree under a name.
        void Add(const AZ::Name& name, AZStd::shared_ptr<const BehaviorTreeAsset> asset);

        //! The tree registered under a name, or nullptr when there is none.
        const BehaviorTreeAsset* Find(const AZ::Name& name) const;

        //! Removes a tree.
        void Remove(const AZ::Name& name);

        //! Binds a named slot to a tree, which is how a subtree is swapped at runtime.
        //! Rebinding a slot means recompiling the trees that reference it.
        void Bind(const AZ::Name& slot, const AZ::Name& treeName);

        //! The tree bound to a slot, or an empty name when the slot is unbound.
        AZ::Name GetBinding(const AZ::Name& slot) const;

        //! Every registered tree name, for console output.
        AZStd::vector<AZ::Name> GetNames() const;

        void Clear();

    private:
        AZStd::unordered_map<AZ::Name, AZStd::shared_ptr<const BehaviorTreeAsset>> m_trees;
        AZStd::unordered_map<AZ::Name, AZ::Name> m_bindings;
    };
} // namespace GOAT
