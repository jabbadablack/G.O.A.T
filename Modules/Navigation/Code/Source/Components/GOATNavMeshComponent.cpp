#include <Components/GOATNavMeshComponent.h>

#include <GOAT_Navigation/GOAT_NavigationBus.h>
#include <GOAT_Navigation/GOAT_NavigationTypeIds.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT_Navigation
{
    AZ_COMPONENT_IMPL(GOATNavMeshComponent, "GOATNavMeshComponent", GOATNavMeshComponentTypeId);

    void GOATNavMeshComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOATNavMeshComponent, AZ::Component>()
                ->Version(1)
                ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<GOATNavMeshComponent>("GOAT Nav Mesh", "Lets GOAT agents path on this navigation mesh")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "GOAT")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ;
            }
        }
    }

    void GOATNavMeshComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        // Placing this beside the mesh is the whole point, so require one rather than warn later.
        required.push_back(AZ_CRC_CE("RecastNavigationMeshComponent"));
    }

    void GOATNavMeshComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOATNavMeshBindingService"));
    }

    void GOATNavMeshComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOATNavMeshBindingService"));
    }

    void GOATNavMeshComponent::Activate()
    {
        AZ_Assert(GetEntityId().IsValid(), "A component only activates on a valid entity");

        if (GOAT_NavigationInterface::Get() == nullptr)
        {
            AZ_Error("GOAT", false, "The GOAT navigation system is not running, so entity %s cannot be bound",
                GetEntityId().ToString().c_str());
            return;
        }

        AZLOG_INFO("GOAT: binding path queries to navigation mesh entity %s", GetEntityId().ToString().c_str());
        GOAT_NavigationRequestBus::Broadcast(&GOAT_NavigationRequests::SetNavigationMesh, GetEntityId());
    }

    void GOATNavMeshComponent::Deactivate()
    {
        if (GOAT_NavigationInterface::Get() == nullptr)
        {
            return;
        }

        GOAT_NavigationRequestBus::Broadcast(&GOAT_NavigationRequests::ClearNavigationMesh);
    }
} // namespace GOAT_Navigation
