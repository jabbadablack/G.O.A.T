#include <Clients/GOAT_UtilitySystemComponent.h>

#include <UtilityBackend.h>

#include <GOAT_Utility/GOAT_UtilityTypeIds.h>

#include <GOAT/GOATBackendBus.h>
#include <GOAT/Interfaces/IAgentSystem.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT_Utility
{
    namespace
    {
        //! Where this gem's scan folder puts its vocabulary, without the extension.
        constexpr const char* VocabularyScriptPath = "goat_utility/scripts/utility";
    } // namespace

    AZ_COMPONENT_IMPL(GOAT_UtilitySystemComponent, "GOAT_UtilitySystemComponent",
        GOAT_UtilitySystemComponentTypeId);

    void GOAT_UtilitySystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOAT_UtilitySystemComponent, AZ::Component>()->Version(0);
        }
    }

    void GOAT_UtilitySystemComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOAT_UtilityService"));
    }

    void GOAT_UtilitySystemComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOAT_UtilityService"));
    }

    void GOAT_UtilitySystemComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        // The core must be running first: this registers into its registries on activation.
        required.push_back(AZ_CRC_CE("GOATService"));
    }

    void GOAT_UtilitySystemComponent::Activate()
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
            AZStd::make_unique<GOAT::UtilityBackend>(*agents, *blackboard);

        bool registered = false;
        GOAT::GOATBackendRequestBus::BroadcastResult(
            registered, &GOAT::GOATBackendRequests::RegisterDecisionBackend, backend);
        AZ_Error("GOAT", registered, "The utility backend could not be installed");
    }

    void GOAT_UtilitySystemComponent::Deactivate()
    {
        GOAT::GOATBackendRequestBus::Broadcast(&GOAT::GOATBackendRequests::UnregisterDecisionBackend,
            GOAT::UtilityBackend::GetBackendName());

        if (GOAT::IAgentSystem* agents = GOAT::AgentSystemInterface::Get())
        {
            agents->UnregisterVocabularyScript(VocabularyScriptPath);
        }
    }
} // namespace GOAT_Utility
