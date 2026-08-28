#include <Clients/GOAT_HtnSystemComponent.h>

#include <HtnBackend.h>

#include <GOAT_Htn/GOAT_HtnTypeIds.h>

#include <GOAT/GOATBackendBus.h>
#include <GOAT/Interfaces/IAgentSystem.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT_Htn
{
    namespace
    {
        //! Where this gem's scan folder puts its vocabulary, without the extension.
        constexpr const char* VocabularyScriptPath = "goat_htn/scripts/htn";
    } // namespace

    AZ_COMPONENT_IMPL(GOAT_HtnSystemComponent, "GOAT_HtnSystemComponent",
        GOAT_HtnSystemComponentTypeId);

    void GOAT_HtnSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOAT_HtnSystemComponent, AZ::Component>()->Version(0);
        }
    }

    void GOAT_HtnSystemComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOAT_HtnService"));
    }

    void GOAT_HtnSystemComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOAT_HtnService"));
    }

    void GOAT_HtnSystemComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        // The core must be running first: this registers into its registries on activation.
        required.push_back(AZ_CRC_CE("GOATService"));
    }

    void GOAT_HtnSystemComponent::Activate()
    {
        GOAT::IAgentSystem* agents = GOAT::AgentSystemInterface::Get();
        GOAT::IBlackboardSystem* blackboard = GOAT::BlackboardSystemInterface::Get();
        AZ_Assert(agents != nullptr && blackboard != nullptr, "The GOAT core must be running first");
        if (agents == nullptr || blackboard == nullptr)
        {
            return;
        }

        agents->RegisterVocabularyScript(VocabularyScriptPath);

        // Handed over rather than held: the core's registry owns it, and unregistering by name
        // is what takes it back out.
        AZStd::unique_ptr<GOAT::IDecisionBackend> backend =
            AZStd::make_unique<GOAT::HtnBackend>(*agents, *blackboard);

        bool registered = false;
        GOAT::GOATBackendRequestBus::BroadcastResult(
            registered, &GOAT::GOATBackendRequests::RegisterDecisionBackend, backend);
        AZ_Error("GOAT", registered, "The task network backend could not be installed");
    }

    void GOAT_HtnSystemComponent::Deactivate()
    {
        GOAT::GOATBackendRequestBus::Broadcast(&GOAT::GOATBackendRequests::UnregisterDecisionBackend,
            GOAT::HtnBackend::GetBackendName());

        if (GOAT::IAgentSystem* agents = GOAT::AgentSystemInterface::Get())
        {
            agents->UnregisterVocabularyScript(VocabularyScriptPath);
        }
    }
} // namespace GOAT_Htn
