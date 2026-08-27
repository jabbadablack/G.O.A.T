#include "GOAT_SmartObjectSystemComponent.h"

#include <Components/GOATSmartObjectComponent.h>

#include <GOAT_SmartObject/GOAT_SmartObjectTypeIds.h>

#include <GOAT/Interfaces/IAgentSystem.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT_SmartObject
{
    AZ_COMPONENT_IMPL(GOAT_SmartObjectSystemComponent, "GOAT_SmartObjectSystemComponent",
        GOAT_SmartObjectSystemComponentTypeId);

    namespace
    {
        //! Builds a leaf node type. A leaf whose name matches a verb runs that verb.
        GOAT::NodeTypeDescriptor Leaf(const char* name, const char* description)
        {
            AZ_Assert(name != nullptr && description != nullptr, "A node type is named and described");

            GOAT::NodeTypeDescriptor descriptor;
            descriptor.m_name = AZ::Name(name);
            descriptor.m_kind = GOAT::NodeKind::Leaf;
            descriptor.m_op = GOAT::NodeOp::Action;
            descriptor.m_category = "Smart Object";
            descriptor.m_description = description;
            return descriptor;
        }

        //! Declares one authored property.
        GOAT::NodeParameter Param(const char* name, GOAT::BlackboardType type, bool required = false)
        {
            GOAT::NodeParameter parameter;
            parameter.m_name = AZ::Name(name);
            parameter.m_type = type;
            parameter.m_required = required;
            return parameter;
        }
    } // namespace

    void GOAT_SmartObjectSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOAT_SmartObjectSystemComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void GOAT_SmartObjectSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOAT_SmartObjectService"));
    }

    void GOAT_SmartObjectSystemComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOAT_SmartObjectService"));
    }

    void GOAT_SmartObjectSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        // The core must be running first: this module registers into its registries on activation.
        required.push_back(AZ_CRC_CE("GOATService"));
    }

    void GOAT_SmartObjectSystemComponent::GetDependentServices(
        [[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    GOAT_SmartObjectSystemComponent::GOAT_SmartObjectSystemComponent()
    {
        if (GOAT_SmartObjectInterface::Get() == nullptr)
        {
            GOAT_SmartObjectInterface::Register(this);
        }
    }

    GOAT_SmartObjectSystemComponent::~GOAT_SmartObjectSystemComponent()
    {
        if (GOAT_SmartObjectInterface::Get() == this)
        {
            GOAT_SmartObjectInterface::Unregister(this);
        }
    }

    void GOAT_SmartObjectSystemComponent::Init()
    {
    }

    bool GOAT_SmartObjectSystemComponent::InstallVerb(
        AZStd::unique_ptr<GOAT::IActionState> action, GOAT::NodeTypeDescriptor descriptor)
    {
        AZ_Assert(action != nullptr, "A verb must exist before it can be installed");

        GOAT::IAgentSystem* agents = GOAT::AgentSystemInterface::Get();
        AZ_Assert(agents != nullptr, "Installing a verb needs the GOAT agent system");
        if (agents == nullptr || action == nullptr)
        {
            return false;
        }

        const AZ::Name name = action->GetName();
        AZ_Assert(descriptor.m_name == name, "A leaf word and the verb it runs must share a name");

        const GOAT::ActionStateId id = agents->RegisterAction(AZStd::move(action));
        if (id == GOAT::CoreActions::Invalid)
        {
            AZ_Error("GOAT", false, "Smart object verb '%s' could not be registered", name.GetCStr());
            return false;
        }
        m_installedActions.push_back(id);

        if (!agents->RegisterNodeType(AZStd::move(descriptor)))
        {
            return false;
        }
        m_installedNodeTypes.push_back(name);

        AZLOG_INFO("GOAT: smart object registered verb '%s'", name.GetCStr());
        return true;
    }

    bool GOAT_SmartObjectSystemComponent::InstallVocabulary()
    {
        GOAT::IBlackboardSystem* blackboard = GOAT::BlackboardSystemInterface::Get();
        AZ_Assert(blackboard != nullptr, "The GOAT blackboard system must exist before this module activates");
        if (blackboard == nullptr)
        {
            AZ_Error("GOAT", false, "The GOAT blackboard system is not running, so smart objects cannot install");
            return false;
        }

        if (!m_keys.Declare(*blackboard))
        {
            return false;
        }

        // `claim_smart_object "sit" { radius = 15 }`
        auto claim = Leaf("claim_smart_object", "Takes a slot on the nearest entity offering a named use");
        claim.m_parameters.push_back(Param("use", GOAT::BlackboardType::Name, true));
        claim.m_parameters.push_back(Param("radius", GOAT::BlackboardType::Float));

        // `use_smart_object { seconds = 5 }`
        auto use = Leaf("use_smart_object", "Runs the claimed use for a while, then gives the slot back");
        use.m_parameters.push_back(Param("seconds", GOAT::BlackboardType::Float, true));

        const bool installed =
            InstallVerb(
                AZStd::unique_ptr<GOAT::IActionState>(aznew ClaimSmartObjectAction(*m_registry, m_keys)),
                AZStd::move(claim)) &&
            InstallVerb(
                AZStd::unique_ptr<GOAT::IActionState>(aznew UseSmartObjectAction(*m_registry, m_keys)),
                AZStd::move(use));

        AZ_Error("GOAT", installed, "The smart object module could not install its full vocabulary");
        return installed;
    }

    void GOAT_SmartObjectSystemComponent::RemoveVocabulary()
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

    void GOAT_SmartObjectSystemComponent::Activate()
    {
        m_registry = AZStd::make_unique<SmartObjectRegistry>();

        GOAT_SmartObjectRequestBus::Handler::BusConnect();
        InstallVocabulary();
    }

    void GOAT_SmartObjectSystemComponent::Deactivate()
    {
        GOAT_SmartObjectRequestBus::Handler::BusDisconnect();

        // The verbs hold a reference to the registry, so they go first.
        RemoveVocabulary();
        m_registry.reset();

        AZ_Assert(m_registry == nullptr, "Deactivating must release the smart object registry");
    }

    void GOAT_SmartObjectSystemComponent::RegisterObject(AZ::EntityId entity, SmartObjectDescription description)
    {
        AZ_Assert(m_registry != nullptr, "Offering a smart object needs an active registry");
        if (m_registry != nullptr)
        {
            m_registry->Add(entity, AZStd::move(description));
        }
    }

    void GOAT_SmartObjectSystemComponent::UnregisterObject(AZ::EntityId entity)
    {
        if (m_registry != nullptr)
        {
            m_registry->Remove(entity);
        }
    }

    SmartObjectClaim GOAT_SmartObjectSystemComponent::Claim(
        GOAT::AgentId agent, const AZ::Name& use, const AZ::Vector3& from, float radius)
    {
        AZ_Assert(m_registry != nullptr, "Claiming a smart object needs an active registry");
        if (m_registry == nullptr)
        {
            // Written out rather than braced: AZ::EntityId's default constructor is explicit.
            SmartObjectClaim nothing;
            return nothing;
        }

        return m_registry->Claim(agent, use, from, radius);
    }

    void GOAT_SmartObjectSystemComponent::Release(GOAT::AgentId agent)
    {
        if (m_registry != nullptr)
        {
            m_registry->Release(agent);
        }
    }

    AZ::u32 GOAT_SmartObjectSystemComponent::GetFreeSlots(AZ::EntityId entity) const
    {
        return m_registry != nullptr ? m_registry->GetFreeSlots(entity) : 0;
    }
} // namespace GOAT_SmartObject
