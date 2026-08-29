#include <GOAT/Domain/AgentDebug.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    void ProgramNodeRef::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ProgramNodeRef>()
                ->Version(1)
                ->Field("Program", &ProgramNodeRef::m_program)
                ->Field("Path", &ProgramNodeRef::m_path);
        }
    }

    void AgentSnapshot::Reflect(AZ::ReflectContext* context)
    {
        ProgramNodeRef::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AgentSnapshot>()
                ->Version(1)
                ->Field("AgentIndex", &AgentSnapshot::m_agentIndex)
                ->Field("AgentGeneration", &AgentSnapshot::m_agentGeneration)
                ->Field("Entity", &AgentSnapshot::m_entity)
                ->Field("Program", &AgentSnapshot::m_program)
                ->Field("Backend", &AgentSnapshot::m_backend)
                ->Field("Squad", &AgentSnapshot::m_squad)
                ->Field("Action", &AgentSnapshot::m_action)
                ->Field("Band", &AgentSnapshot::m_band)
                ->Field("Step", &AgentSnapshot::m_step)
                ->Field("PlanSize", &AgentSnapshot::m_planSize)
                ->Field("Elapsed", &AgentSnapshot::m_elapsed)
                ->Field("Interrupted", &AgentSnapshot::m_interrupted)
                ->Field("ActivePath", &AgentSnapshot::m_activePath);
        }
    }
} // namespace GOAT
