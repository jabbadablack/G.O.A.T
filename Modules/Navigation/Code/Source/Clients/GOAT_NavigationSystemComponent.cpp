
#include "GOAT_NavigationSystemComponent.h"

#include <GOAT_Navigation/GOAT_NavigationTypeIds.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT_Navigation
{
    AZ_COMPONENT_IMPL(GOAT_NavigationSystemComponent, "GOAT_NavigationSystemComponent",
        GOAT_NavigationSystemComponentTypeId);

    void GOAT_NavigationSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOAT_NavigationSystemComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void GOAT_NavigationSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOAT_NavigationService"));
    }

    void GOAT_NavigationSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOAT_NavigationService"));
    }

    void GOAT_NavigationSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
    }

    void GOAT_NavigationSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    GOAT_NavigationSystemComponent::GOAT_NavigationSystemComponent()
    {
        if (GOAT_NavigationInterface::Get() == nullptr)
        {
            GOAT_NavigationInterface::Register(this);
        }
    }

    GOAT_NavigationSystemComponent::~GOAT_NavigationSystemComponent()
    {
        if (GOAT_NavigationInterface::Get() == this)
        {
            GOAT_NavigationInterface::Unregister(this);
        }
    }

    void GOAT_NavigationSystemComponent::Init()
    {
    }

    void GOAT_NavigationSystemComponent::Activate()
    {
        GOAT_NavigationRequestBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    void GOAT_NavigationSystemComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        GOAT_NavigationRequestBus::Handler::BusDisconnect();
    }

    void GOAT_NavigationSystemComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
    }

} // namespace GOAT_Navigation
