#include <Core/Application/AgentArchetype.h>

#include <AzCore/Debug/Trace.h>

namespace GOAT
{
    void AgentArchetype::Add(const AZ::Name& name, AZStd::shared_ptr<const AgentProgram> program)
    {
        AZ_Assert(m_names.size() < MaxArchetypeTrees, "An entity cannot declare more trees than a slot can address");
        if (m_names.size() >= MaxArchetypeTrees)
        {
            return;
        }

        m_names.push_back(name);
        m_programs.push_back(AZStd::move(program));
    }

    TreeSlot AgentArchetype::FindTree(const AZ::Name& name) const
    {
        // Linear because an entity declares a handful of trees, and comparing an AZ::Name is a
        // four byte compare: a map here would cost more to build than it ever saved.
        for (size_t slot = 0; slot < m_names.size(); ++slot)
        {
            if (m_names[slot] == name)
            {
                return static_cast<TreeSlot>(slot);
            }
        }

        return InvalidTreeSlot;
    }

    bool AgentArchetype::Resolve(const AZ::Name& name, AZStd::shared_ptr<const AgentProgram> program)
    {
        const TreeSlot slot = FindTree(name);
        if (slot == InvalidTreeSlot || m_programs[slot] != nullptr)
        {
            return false;
        }

        m_programs[slot] = AZStd::move(program);
        return true;
    }

    const AgentProgram* AgentArchetype::GetProgram(TreeSlot slot) const
    {
        return slot < m_programs.size() ? m_programs[slot].get() : nullptr;
    }

    AZ::Name AgentArchetype::GetName(TreeSlot slot) const
    {
        return slot < m_names.size() ? m_names[slot] : AZ::Name{};
    }

    bool AgentArchetype::Matches(AZStd::span<const AZ::Name> names) const
    {
        if (names.size() != m_names.size())
        {
            return false;
        }

        for (size_t i = 0; i < names.size(); ++i)
        {
            if (names[i] != m_names[i])
            {
                return false;
            }
        }

        return true;
    }
} // namespace GOAT
