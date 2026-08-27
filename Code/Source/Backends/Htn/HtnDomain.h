#pragma once

#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/AgentProgram.h>
#include <GOAT/Domain/BlackboardKey.h>

#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! Not one of the tasks in a domain.
    inline constexpr AZ::u16 InvalidTask = static_cast<AZ::u16>(-1);

    //! How deep a decomposition may go before recursion is treated as a bug.
    inline constexpr AZ::u16 MaxDecomposeDepth = 16;

    //! Primitive steps one plan may hold. Bounded so the plan an agent is running fits in
    //! the state its backend carries, which is what lets it be re-checked while it runs.
    inline constexpr AZ::u16 MaxPlanTasks = 32;

    //! Blackboard slots one domain may reason about.
    inline constexpr AZ::u16 MaxDomainKeys = 32;

    //! A slot a method or a primitive requires, and what it must read as.
    struct HtnCondition final
    {
        BlackboardKey m_key;
        bool m_expected = true;
    };

    //! What a primitive is assumed to change, so later conditions see it while planning.
    struct HtnEffect final
    {
        BlackboardKey m_key;
        bool m_value = true;
    };

    //! One way of carrying out a compound task.
    struct HtnMethod final
    {
        AZ::u16 m_firstCondition = 0;
        AZ::u16 m_conditionCount = 0;
        AZ::u16 m_firstSubtask = 0;
        AZ::u16 m_subtaskCount = 0;
    };

    //! Either a compound task with methods, or a primitive that runs one verb.
    struct HtnTask final
    {
        AZ::Name m_name;
        bool m_isPrimitive = false;
        AZ::u16 m_firstMethod = 0;
        AZ::u16 m_methodCount = 0;
        AZ::u16 m_firstCondition = 0;
        AZ::u16 m_conditionCount = 0;
        AZ::u16 m_firstEffect = 0;
        AZ::u16 m_effectCount = 0;
        ActionRequest m_action;
    };

    //! A task network compiled for execution: flat, immutable, shared by every agent using it.
    class HtnDomain final
        : public AgentProgram
    {
    public:
        AZ_RTTI(HtnDomain, "{8F30C6A5-41D7-4B92-BE08-5A7C1D93E264}", AgentProgram);
        AZ_CLASS_ALLOCATOR(HtnDomain, AZ::SystemAllocator);

        //! Index of a task by name, or InvalidTask.
        AZ::u16 FindTask(const AZ::Name& name) const;

        AZStd::vector<HtnTask> m_tasks;
        AZStd::vector<HtnMethod> m_methods;
        AZStd::vector<HtnCondition> m_conditions;
        AZStd::vector<HtnEffect> m_effects;
        //! Task indices, referenced by method ranges.
        AZStd::vector<AZ::u16> m_subtasks;
        //! Every slot this domain reads or writes, sorted and deduplicated.
        AZStd::vector<BlackboardKey> m_touchedKeys;
        //! Where planning starts.
        AZ::u16 m_root = InvalidTask;
    };
} // namespace GOAT
