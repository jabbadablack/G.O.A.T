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
            m_vocabulary.Install(AZStd::unique_ptr<GOAT::IActionState>(aznew AnimateAction()), AZStd::move(animate)) &&
            m_vocabulary.Install(AZStd::unique_ptr<GOAT::IActionState>(aznew PlayMotionAction()), AZStd::move(playMotion));

        AZ_Error("GOAT", installed, "The animation module could not install its full vocabulary");
        return installed;
    }
    void GOAT_AnimationSystemComponent::Activate()
    {
        InstallVocabulary();
    }

    void GOAT_AnimationSystemComponent::Deactivate()
    {
        m_vocabulary.Clear();
    }
} // namespace GOAT_Animation
