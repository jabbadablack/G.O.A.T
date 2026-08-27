#include "GOAT_AnimationSystemComponent.h"

#include <Animation/AnimateAction.h>

#include <GOAT_Animation/GOAT_AnimationTypeIds.h>

#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT_Animation
{
    AZ_COMPONENT_IMPL(GOAT_AnimationSystemComponent, "GOAT_AnimationSystemComponent",
        GOAT_AnimationSystemComponentTypeId);

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
            descriptor.m_category = "Animation";
            descriptor.m_description = description;
            return descriptor;
        }

        //! Declares one authored property.
        GOAT::NodeParameter Param(
            const char* name, GOAT::BlackboardType type, bool isBlackboardKey = false, bool required = false)
        {
            GOAT::NodeParameter parameter;
            parameter.m_name = AZ::Name(name);
            parameter.m_type = type;
            parameter.m_isBlackboardKey = isBlackboardKey;
            parameter.m_required = required;
            return parameter;
        }
    } // namespace

    void GOAT_AnimationSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOAT_AnimationSystemComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void GOAT_AnimationSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOAT_AnimationService"));
    }

    void GOAT_AnimationSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOAT_AnimationService"));
    }

    void GOAT_AnimationSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        // The core must be running first: this module registers into its registries on activation.
        required.push_back(AZ_CRC_CE("GOATService"));
    }

    void GOAT_AnimationSystemComponent::GetDependentServices(
        [[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    bool GOAT_AnimationSystemComponent::InstallVerb(
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
            AZ_Error("GOAT", false, "Animation verb '%s' could not be registered", name.GetCStr());
            return false;
        }
        m_installedActions.push_back(id);

        if (!agents->RegisterNodeType(AZStd::move(descriptor)))
        {
            return false;
        }
        m_installedNodeTypes.push_back(name);

        AZLOG_INFO("GOAT: animation registered verb '%s'", name.GetCStr());
        return true;
    }

    bool GOAT_AnimationSystemComponent::InstallVocabulary()
    {
        // `animate "Speed" { key = "nav_remaining" }` -- the tree states what the agent is and
        // the anim graph decides what to play, so no clip name ever appears in a behaviour tree.
        auto animate = Leaf("animate", "Writes one named anim graph parameter");
        animate.m_parameters.push_back(Param("parameter", GOAT::BlackboardType::Name, false, true));
        animate.m_parameters.push_back(Param("key", GOAT::BlackboardType::Float, true));
        animate.m_parameters.push_back(Param("amount", GOAT::BlackboardType::Float));

        // `play_motion "animations/wave.motion" { seconds = 2 }` -- for a project with no graph.
        auto playMotion = Leaf("play_motion", "Plays a motion and runs for its duration");
        playMotion.m_parameters.push_back(Param("motion", GOAT::BlackboardType::Name));
        playMotion.m_parameters.push_back(Param("seconds", GOAT::BlackboardType::Float));

        const bool installed =
            InstallVerb(AZStd::unique_ptr<GOAT::IActionState>(aznew AnimateAction()), AZStd::move(animate)) &&
            InstallVerb(AZStd::unique_ptr<GOAT::IActionState>(aznew PlayMotionAction()), AZStd::move(playMotion));

        AZ_Error("GOAT", installed, "The animation module could not install its full vocabulary");
        return installed;
    }

    void GOAT_AnimationSystemComponent::RemoveVocabulary()
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

    void GOAT_AnimationSystemComponent::Activate()
    {
        InstallVocabulary();
    }

    void GOAT_AnimationSystemComponent::Deactivate()
    {
        RemoveVocabulary();
    }
} // namespace GOAT_Animation
