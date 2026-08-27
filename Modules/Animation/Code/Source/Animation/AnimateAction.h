#pragma once

#include <GOAT/Interfaces/IActionState.h>

#include <AzCore/Memory/SystemAllocator.h>

namespace GOAT_Animation
{
    //! Writes one named anim graph parameter, then succeeds.
    //!
    //! This is the decoupled half of the module: the tree says what the agent *is* -- alerted,
    //! moving this fast -- and the anim graph decides what to play. Nothing in the tree names a
    //! clip, so re-authoring the graph does not re-author the behaviour.
    //!
    //! The value is a blackboard variable when the node names one, and the node's own number
    //! otherwise. Which parameter setter runs follows the variable's declared type.
    class AnimateAction final
        : public GOAT::IActionState
    {
    public:
        AZ_CLASS_ALLOCATOR(AnimateAction, AZ::SystemAllocator);

        AZ::Name GetName() const override;
        void Begin(const GOAT::ActionContext& context) override;
        GOAT::ActionResult Step(const GOAT::ActionContext& context, float deltaTime) override;
        void End(const GOAT::ActionContext& context) override;

    private:
        //! Pushes the agent's value for one blackboard variable into the anim graph.
        //! @return false when the variable holds a type no anim graph parameter can take.
        bool WriteFromBlackboard(const GOAT::ActionContext& context, const char* parameter) const;
    };

    //! Plays a motion on the entity's Simple Motion component and runs for its duration.
    //!
    //! The blunt half of the module, for a project with no anim graph. The node may name a
    //! motion asset to switch to; with no name it plays whatever the component already holds.
    class PlayMotionAction final
        : public GOAT::IActionState
    {
    public:
        AZ_CLASS_ALLOCATOR(PlayMotionAction, AZ::SystemAllocator);

        AZ::Name GetName() const override;
        void Begin(const GOAT::ActionContext& context) override;
        GOAT::ActionResult Step(const GOAT::ActionContext& context, float deltaTime) override;
        void End(const GOAT::ActionContext& context) override;
    };
} // namespace GOAT_Animation
