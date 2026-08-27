#pragma once

#include <GOAT/Domain/DecisionProgram.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

namespace GOAT
{
    //! A tree's place in an archetype. One byte, because an entity lists a handful of trees.
    using TreeSlot = AZ::u8;

    //! Not one of the trees this archetype holds.
    inline constexpr TreeSlot InvalidTreeSlot = static_cast<TreeSlot>(-1);

    //! How many trees one entity may declare.
    inline constexpr size_t MaxArchetypeTrees = 32;

    //! Everything a group of identically authored agents shares.
    //!
    //! Two entities that list the same trees get the same archetype and hold one copy of it
    //! between them, so ten thousand agents of one kind cost one list of programs rather than
    //! ten thousand. An agent then remembers a tree by its slot here, which is why switching
    //! is a byte and why "may this agent run that tree" is a lookup in shared memory instead
    //! of a scan over a per agent copy of the same names.
    class AgentArchetype final
    {
    public:
        AZ_CLASS_ALLOCATOR(AgentArchetype, AZ::SystemAllocator);

        //! Adds a tree. The first one added is the tree agents of this kind start in.
        void Add(const AZ::Name& name, AZStd::shared_ptr<const DecisionProgram> program);

        //! Slot of a tree by name, or InvalidTreeSlot when this kind of agent never declared it.
        TreeSlot FindTree(const AZ::Name& name) const;

        //! The compiled tree in a slot, or nullptr when the slot is not one of ours.
        const DecisionProgram* GetProgram(TreeSlot slot) const;

        //! What the tree in a slot is called, or an empty name.
        AZ::Name GetName(TreeSlot slot) const;

        size_t Size() const { return m_names.size(); }

        //! True when this archetype declares exactly these trees, in this order. Two entities
        //! that agree on that can share one.
        bool Matches(AZStd::span<const AZ::Name> names) const;

    private:
        AZStd::vector<AZ::Name> m_names;
        AZStd::vector<AZStd::shared_ptr<const DecisionProgram>> m_programs;
    };
} // namespace GOAT
