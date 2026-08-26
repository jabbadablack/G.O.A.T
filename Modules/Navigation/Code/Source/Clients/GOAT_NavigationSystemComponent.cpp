#include "GOAT_NavigationSystemComponent.h"

#include <Navigation/MoveToAction.h>
#include <Navigation/SpatialChecks.h>

#include <GOAT_Navigation/GOAT_NavigationTypeIds.h>

#include <GOAT/Interfaces/IAgentSystem.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>
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

    void GOAT_NavigationSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        // The core must be running first: this module registers into its registries on activation.
        required.push_back(AZ_CRC_CE("GOATService"));
    }

    void GOAT_NavigationSystemComponent::GetDependentServices(
        [[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
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

    bool GOAT_NavigationSystemComponent::InstallVerb(
        AZStd::unique_ptr<GOAT::IActionState> action,
        const char* parameterName,
        GOAT::BlackboardType parameterType,
        const char* description)
    {
        AZ_Assert(action != nullptr, "A verb must exist before it can be installed");
        AZ_Assert(parameterName != nullptr, "A verb's main property must be named");

        GOAT::IAgentSystem* agents = GOAT::AgentSystemInterface::Get();
        AZ_Assert(agents != nullptr, "Installing a verb needs the GOAT agent system");
        if (agents == nullptr || action == nullptr)
        {
            return false;
        }

        const AZ::Name name = action->GetName();

        const GOAT::ActionStateId id = agents->RegisterAction(AZStd::move(action));
        if (id == GOAT::CoreActions::Invalid)
        {
            AZ_Error("GOAT", false, "Navigation verb '%s' could not be registered", name.GetCStr());
            return false;
        }
        m_installedActions.push_back(id);

        // A leaf node type whose name matches a verb runs that verb, so both halves share a name.
        GOAT::NodeTypeDescriptor descriptor;
        descriptor.m_name = name;
        descriptor.m_kind = GOAT::NodeKind::Leaf;
        descriptor.m_op = GOAT::NodeOp::Action;
        descriptor.m_category = "Navigation";
        descriptor.m_description = description;

        GOAT::NodeParameter target;
        target.m_name = AZ::Name(parameterName);
        target.m_type = parameterType;
        target.m_isBlackboardKey = parameterType == GOAT::BlackboardType::Vector3;
        target.m_required = true;
        descriptor.m_parameters.push_back(target);

        GOAT::NodeParameter tolerance;
        tolerance.m_name = AZ_NAME_LITERAL("tolerance");
        tolerance.m_type = GOAT::BlackboardType::Float;
        descriptor.m_parameters.push_back(tolerance);

        GOAT::NodeParameter speed;
        speed.m_name = AZ_NAME_LITERAL("speed");
        speed.m_type = GOAT::BlackboardType::Float;
        descriptor.m_parameters.push_back(speed);

        if (!agents->RegisterNodeType(AZStd::move(descriptor)))
        {
            return false;
        }
        m_installedNodeTypes.push_back(name);

        AZLOG_INFO("GOAT: navigation registered verb '%s'", name.GetCStr());
        return true;
    }

    bool GOAT_NavigationSystemComponent::InstallVocabulary()
    {
        GOAT::IBlackboardSystem* blackboard = GOAT::BlackboardSystemInterface::Get();
        AZ_Assert(blackboard != nullptr, "The GOAT blackboard system must exist before this module activates");
        if (blackboard == nullptr)
        {
            AZ_Error("GOAT", false, "The GOAT blackboard system is not running, so navigation cannot install its verbs");
            return false;
        }

        if (!m_keys.Declare(*blackboard))
        {
            return false;
        }

        const bool installed =
            InstallVerb(
                AZStd::unique_ptr<GOAT::IActionState>(aznew MoveToAction(*m_service, *m_paths, m_keys)),
                "key", GOAT::BlackboardType::Vector3, "Walks to a position, publishing progress to the blackboard") &&
            InstallVerb(
                AZStd::unique_ptr<GOAT::IActionState>(aznew IsAtLocationAction()),
                "key", GOAT::BlackboardType::Vector3, "Succeeds when the agent is already at a position") &&
            InstallVerb(
                AZStd::unique_ptr<GOAT::IActionState>(aznew DoesPathExistAction(*m_service)),
                "key", GOAT::BlackboardType::Vector3, "Succeeds when a walkable path to a position exists");

        AZ_Error("GOAT", installed, "The navigation module could not install its full vocabulary");
        return installed;
    }

    void GOAT_NavigationSystemComponent::RemoveVocabulary()
    {
        GOAT::IAgentSystem* agents = GOAT::AgentSystemInterface::Get();
        if (agents == nullptr)
        {
            // The core shut down first, which takes its registries with it.
            m_installedActions.clear();
            m_installedNodeTypes.clear();
            return;
        }

        for (const AZ::Name& name : m_installedNodeTypes)
        {
            agents->UnregisterNodeType(name);
        }
        m_installedNodeTypes.clear();

        for (const GOAT::ActionStateId id : m_installedActions)
        {
            agents->UnregisterAction(id);
        }
        m_installedActions.clear();

        AZ_Assert(m_installedActions.empty(), "Removing the vocabulary must leave no verb installed");
    }

    void GOAT_NavigationSystemComponent::Activate()
    {
        m_service = AZStd::make_unique<NavigationService>();
        m_paths = AZStd::make_unique<PathPool>();

        GOAT_NavigationRequestBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();

        InstallVocabulary();
    }

    void GOAT_NavigationSystemComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        GOAT_NavigationRequestBus::Handler::BusDisconnect();

        // Verbs hold references to the service and the pool, so they go first.
        RemoveVocabulary();

        m_service.reset();
        m_paths.reset();

        AZ_Assert(m_service == nullptr, "Deactivating must release the navigation service");
    }

    void GOAT_NavigationSystemComponent::SetNavigationMesh(AZ::EntityId navMeshEntity)
    {
        AZ_Assert(m_service != nullptr, "Binding a navigation mesh needs an active navigation service");
        if (m_service != nullptr)
        {
            m_service->SetNavigationMesh(navMeshEntity);
        }
    }

    void GOAT_NavigationSystemComponent::ClearNavigationMesh()
    {
        if (m_service != nullptr)
        {
            m_service->ClearNavigationMesh();
        }
    }

    bool GOAT_NavigationSystemComponent::IsNavigationReady() const
    {
        return m_service != nullptr && m_service->IsReady();
    }

    void GOAT_NavigationSystemComponent::OnTick(
        [[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        AZ_Assert(m_service != nullptr, "The navigation service must exist while this component ticks");
        if (m_service != nullptr)
        {
            m_service->Update();
        }
    }
} // namespace GOAT_Navigation
