#pragma once

#include <GOAT/Domain/BlackboardKey.h>
#include <GOAT/Interfaces/IActionState.h>

#include <AzCore/Memory/SystemAllocator.h>

namespace GOAT
{
    class IBlackboardSystem;
}

namespace GOAT_SmartObject
{
    class SmartObjectRegistry;

    //! The blackboard variables this module owns, resolved once and shared by both verbs.
    struct SmartObjectKeys final
    {
        //! Declares the variables and caches their keys. Safe to call more than once.
        bool Declare(GOAT::IBlackboardSystem& blackboard);

        //! True once every key resolved.
        bool IsValid() const;

        //! The entity whose slot this agent holds.
        GOAT::BlackboardKey m_entity;
        //! Where to stand to use it, in world space.
        GOAT::BlackboardKey m_anchor;
        //! Which use was claimed.
        GOAT::BlackboardKey m_use;
    };

    //! Takes a slot on the nearest entity offering a named use, and publishes where to stand.
    //!
    //! Split from actually using it on purpose: the agent has to travel there in between, and
    //! travelling belongs to whatever gem the project moves with. Publishing `so_anchor` is what
    //! lets `move_to` -- or a project's own controller -- carry the agent without this module
    //! ever depending on navigation.
    class ClaimSmartObjectAction final
        : public GOAT::IActionState
    {
    public:
        AZ_CLASS_ALLOCATOR(ClaimSmartObjectAction, AZ::SystemAllocator);

        ClaimSmartObjectAction(SmartObjectRegistry& registry, const SmartObjectKeys& keys);

        AZ::Name GetName() const override;
        GOAT::ActionResult Step(const GOAT::ActionContext& context, float deltaTime) override;
        void End(const GOAT::ActionContext& context) override;

    private:
        SmartObjectRegistry& m_registry;
        const SmartObjectKeys& m_keys;
    };

    //! Runs the claimed use for a while, then gives the slot back.
    //!
    //! Releasing happens in End rather than on success, so an aborted branch frees the slot too.
    class UseSmartObjectAction final
        : public GOAT::IActionState
    {
    public:
        AZ_CLASS_ALLOCATOR(UseSmartObjectAction, AZ::SystemAllocator);

        UseSmartObjectAction(SmartObjectRegistry& registry, const SmartObjectKeys& keys);

        AZ::Name GetName() const override;
        void Begin(const GOAT::ActionContext& context) override;
        GOAT::ActionResult Step(const GOAT::ActionContext& context, float deltaTime) override;
        void End(const GOAT::ActionContext& context) override;

    private:
        SmartObjectRegistry& m_registry;
        const SmartObjectKeys& m_keys;
    };
} // namespace GOAT_SmartObject
