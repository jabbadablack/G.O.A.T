#include <HtnPlanner.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/std/algorithm.h>

namespace GOAT
{
    size_t WorkingState::IndexOf(const HtnDomain& domain, BlackboardKey key)
    {
        const auto found = AZStd::lower_bound(domain.m_touchedKeys.begin(), domain.m_touchedKeys.end(), key);
        return found != domain.m_touchedKeys.end() && *found == key
            ? static_cast<size_t>(found - domain.m_touchedKeys.begin())
            : domain.m_touchedKeys.size();
    }

    void WorkingState::Snapshot(const HtnDomain& domain, const IBlackboardSystem& blackboard, AgentId agent)
    {
        m_values.clear();
        for (const BlackboardKey key : domain.m_touchedKeys)
        {
            const bool* value = blackboard.Find<bool>(key, agent);
            m_values.push_back(value != nullptr && *value);
        }
    }

    bool WorkingState::Get(const HtnDomain& domain, BlackboardKey key) const
    {
        const size_t index = IndexOf(domain, key);
        return index < m_values.size() && m_values[index];
    }

    void WorkingState::Set(const HtnDomain& domain, BlackboardKey key, bool value)
    {
        const size_t index = IndexOf(domain, key);
        if (index < m_values.size())
        {
            m_values[index] = value;
        }
    }

    bool HtnPlanner::Allows(const HtnDomain& domain, const HtnTask& task, const WorkingState& state)
    {
        return Holds(domain, state, task.m_firstCondition, task.m_conditionCount);
    }

    void HtnPlanner::ApplyEffects(const HtnDomain& domain, const HtnTask& task, WorkingState& state)
    {
        for (AZ::u16 e = 0; e < task.m_effectCount; ++e)
        {
            const HtnEffect& effect = domain.m_effects[task.m_firstEffect + e];
            state.Set(domain, effect.m_key, effect.m_value);
        }
    }

    bool HtnPlanner::Holds(const HtnDomain& domain, const WorkingState& state, AZ::u16 first, AZ::u16 count)
    {
        for (AZ::u16 i = 0; i < count; ++i)
        {
            const HtnCondition& condition = domain.m_conditions[first + i];
            if (state.Get(domain, condition.m_key) != condition.m_expected)
            {
                return false;
            }
        }
        return true;
    }

    bool HtnPlanner::Plan(
        const HtnDomain& domain, AZ::u16 rootTask, WorkingState& state, HtnPlanBuffer& outPlan) const
    {
        outPlan.clear();
        if (rootTask >= domain.m_tasks.size())
        {
            return false;
        }

        TaskStack toProcess;
        History history;
        UndoLog undo;
        toProcess.push_back(rootTask);

        // Tries one compound task's methods from a starting index, recording what it chose so a
        // later failure can come back and take the next one.
        const auto decompose = [&](AZ::u16 taskIndex, AZ::u16 fromMethod)
        {
            const HtnTask& task = domain.m_tasks[taskIndex];
            for (AZ::u16 i = fromMethod; i < task.m_methodCount; ++i)
            {
                const HtnMethod& method = domain.m_methods[task.m_firstMethod + i];
                if (!Holds(domain, state, method.m_firstCondition, method.m_conditionCount))
                {
                    continue;
                }

                if (history.size() >= history.capacity())
                {
                    AZ_Error("GOAT", false, "Domain '%s' decomposed deeper than %u; check for a task that "
                        "lists itself with nothing to stop it", domain.m_name.GetCStr(), MaxDecomposeDepth);
                    return false;
                }

                Decomposed record;
                record.m_task = taskIndex;
                record.m_method = i;
                record.m_toProcess = static_cast<AZ::u16>(toProcess.size());
                record.m_planned = static_cast<AZ::u16>(outPlan.size());
                record.m_undone = static_cast<AZ::u16>(undo.size());
                history.push_back(record);

                // Pushed in reverse so they come back off in the order they were written.
                for (AZ::u16 s = method.m_subtaskCount; s > 0; --s)
                {
                    if (toProcess.size() >= toProcess.capacity())
                    {
                        AZ_Error("GOAT", false, "Domain '%s' has more tasks in flight than %u",
                            domain.m_name.GetCStr(), MaxPlanTasks);
                        return false;
                    }
                    toProcess.push_back(domain.m_subtasks[method.m_firstSubtask + s - 1]);
                }
                return true;
            }
            return false;
        };

        // Takes back the last decomposition and tries the next method of the task that made it.
        const auto restore = [&]()
        {
            while (!history.empty())
            {
                const Decomposed record = history.back();
                history.pop_back();

                toProcess.resize(record.m_toProcess);
                outPlan.resize(record.m_planned);
                while (undo.size() > record.m_undone)
                {
                    state.Set(domain, undo.back().m_key, undo.back().m_was);
                    undo.pop_back();
                }

                if (decompose(record.m_task, static_cast<AZ::u16>(record.m_method + 1)))
                {
                    return true;
                }
            }
            return false;
        };

        while (!toProcess.empty())
        {
            const AZ::u16 taskIndex = toProcess.back();
            toProcess.pop_back();
            const HtnTask& task = domain.m_tasks[taskIndex];

            bool carried = false;
            if (task.m_isPrimitive)
            {
                if (Holds(domain, state, task.m_firstCondition, task.m_conditionCount) &&
                    outPlan.size() < outPlan.capacity())
                {
                    // Applied because the planner assumes this step will succeed, which is what
                    // lets a later condition reason about the state it will have left behind.
                    for (AZ::u16 e = 0; e < task.m_effectCount; ++e)
                    {
                        const HtnEffect& effect = domain.m_effects[task.m_firstEffect + e];
                        undo.push_back(Undo{ effect.m_key, state.Get(domain, effect.m_key) });
                        state.Set(domain, effect.m_key, effect.m_value);
                    }

                    outPlan.push_back(taskIndex);
                    carried = true;
                }
            }
            else
            {
                carried = decompose(taskIndex, 0);
            }

            if (!carried && !restore())
            {
                outPlan.clear();
                return false;
            }
        }

        return !outPlan.empty();
    }
} // namespace GOAT
