
#include "GOATSystemComponent.h"

#include <GOAT/GOATTypeIds.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    AZ_COMPONENT_IMPL(GOATSystemComponent, "GOATSystemComponent",
        GOATSystemComponentTypeId);

    void GOATSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOATSystemComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void GOATSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOATService"));
    }

    void GOATSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOATService"));
    }

    void GOATSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
    }

    void GOATSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    GOATSystemComponent::GOATSystemComponent()
    {
        if (GOATInterface::Get() == nullptr)
        {
            GOATInterface::Register(this);
        }
    }

    GOATSystemComponent::~GOATSystemComponent()
    {
        if (GOATInterface::Get() == this)
        {
            GOATInterface::Unregister(this);
        }
    }

    void GOATSystemComponent::Init()
    {
    }

    void GOATSystemComponent::Activate()
    {
        GOATRequestBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    void GOATSystemComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        GOATRequestBus::Handler::BusDisconnect();
    }

    void GOATSystemComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
    }

} // namespace GOAT
