#include <Components/GOATNavMeshComponent.h>

#include <GOAT_Navigation/GOAT_NavigationBus.h>

#include <RecastNavigation/RecastNavigationMeshBus.h>
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
                ->Field("BuildOnActivate", &GOATNavMeshComponent::m_buildOnActivate)
                ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<GOATNavMeshComponent>("GOAT Nav Mesh", "Lets GOAT agents path on this navigation mesh")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "GOAT")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &GOATNavMeshComponent::m_buildOnActivate,
                        "Build on activate",
                        "Builds the navigation mesh when the level starts. Turn this off only if the project "
                        "decides for itself when to build, because an unbuilt mesh fails every path query.")
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

        if (m_buildOnActivate)
        {
            // Deferred to the first tick, not done here: the geometry the provider voxelizes
            // belongs to other entities, which have not all activated yet.
            AZ::TickBus::Handler::BusConnect();
        }
    }

    void GOATNavMeshComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        AZ_Assert(m_buildOnActivate, "This component only ticks in order to build the navigation mesh once");
        AZ::TickBus::Handler::BusDisconnect();

        bool started = false;
        RecastNavigation::RecastNavigationMeshRequestBus::EventResult(
            started, GetEntityId(), &RecastNavigation::RecastNavigationMeshRequests::UpdateNavigationMeshAsync);

        AZ_Warning("GOAT", started,
            "Navigation mesh entity %s did not start building; agents will find no paths until it does",
            GetEntityId().ToString().c_str());
    }

    void GOATNavMeshComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();

        if (GOAT_NavigationInterface::Get() == nullptr)
        {
            return;
        }

        GOAT_NavigationRequestBus::Broadcast(&GOAT_NavigationRequests::ClearNavigationMesh);
    }
} // namespace GOAT_Navigation
