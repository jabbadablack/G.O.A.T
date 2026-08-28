#include <Clients/GOAT_BehaviorTreeSystemComponent.h>

#include <BehaviorTreeBackend.h>
#include <BehaviorTreeWords.h>

#include <GOAT_BehaviorTree/GOAT_BehaviorTreeTypeIds.h>
#include <GOAT_BehaviorTree/Guard.h>

#include <GOAT/GOATBackendBus.h>
#include <GOAT/Interfaces/IAgentSystem.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT_BehaviorTree
{
    AZ_COMPONENT_IMPL(GOAT_BehaviorTreeSystemComponent, "GOAT_BehaviorTreeSystemComponent",
        GOAT_BehaviorTreeSystemComponentTypeId);

    void GOAT_BehaviorTreeSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        // The abort modes are this gem's, so it is what reflects them.
        GOAT::ReflectGuardTypes(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOAT_BehaviorTreeSystemComponent, AZ::Component>()->Version(0);
        }
    }

    void GOAT_BehaviorTreeSystemComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOAT_BehaviorTreeService"));
    }

    void GOAT_BehaviorTreeSystemComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOAT_BehaviorTreeService"));
    }

    void GOAT_BehaviorTreeSystemComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        // The core must be running first: this registers into its registries on activation.
        required.push_back(AZ_CRC_CE("GOATService"));
    }

    void GOAT_BehaviorTreeSystemComponent::Activate()
    {
        GOAT::IAgentSystem* agents = GOAT::AgentSystemInterface::Get();
        GOAT::IBlackboardSystem* blackboard = GOAT::BlackboardSystemInterface::Get();
        AZ_Assert(agents != nullptr && blackboard != nullptr, "The GOAT core must be running first");
        if (agents == nullptr || blackboard == nullptr)
        {
            return;
        }

        GOAT::InstallBehaviorTreeWords(m_vocabulary);

        // Handed over rather than held: the core's registry owns it, and unregistering by name
        // is what takes it back out.
        AZStd::unique_ptr<GOAT::IDecisionBackend> backend =
            AZStd::make_unique<GOAT::BehaviorTreeBackend>(*agents, *blackboard);

        bool registered = false;
        GOAT::GOATBackendRequestBus::BroadcastResult(
            registered, &GOAT::GOATBackendRequests::RegisterDecisionBackend, backend);
        AZ_Error("GOAT", registered, "The behaviour tree backend could not be installed");
    }

    void GOAT_BehaviorTreeSystemComponent::Deactivate()
    {
        GOAT::GOATBackendRequestBus::Broadcast(&GOAT::GOATBackendRequests::UnregisterDecisionBackend,
            GOAT::BehaviorTreeBackend::GetBackendName());

        m_vocabulary.Clear();
    }
} // namespace GOAT_BehaviorTree
