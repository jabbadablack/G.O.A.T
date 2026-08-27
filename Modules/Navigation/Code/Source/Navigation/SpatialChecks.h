#pragma once

#include <Navigation/NavigationService.h>

#include <GOAT/Interfaces/IActionState.h>

#include <AzCore/Memory/SystemAllocator.h>

namespace GOAT_Navigation
{
    //! Succeeds when the agent is already within tolerance of a position.
    //!
    //! A leaf rather than a decorator, exactly like the core's `condition`: it evaluates and
    //! reports to the composite it sits in, so a sequence stops at it when it fails.
    class IsAtLocationAction final
        : public GOAT::IActionState
    {
    public:
        AZ_CLASS_ALLOCATOR(IsAtLocationAction, AZ::SystemAllocator);

        AZ::Name GetName() const override;
        GOAT::ActionResult Step(const GOAT::ActionContext& context, float deltaTime) override;
    };

    //! Succeeds when a walkable path to a position exists.
    //!
    //! The query is asynchronous, so this leaf reports Running until it answers. That is the
    //! same contract every other verb has, and it keeps the check off the main thread.
    class DoesPathExistAction final
        : public GOAT::IActionState
    {
    public:
        AZ_CLASS_ALLOCATOR(DoesPathExistAction, AZ::SystemAllocator);

        explicit DoesPathExistAction(NavigationService& service);

        AZ::Name GetName() const override;
        void Begin(const GOAT::ActionContext& context) override;
        GOAT::ActionResult Step(const GOAT::ActionContext& context, float deltaTime) override;
        void End(const GOAT::ActionContext& context) override;

    private:
        NavigationService& m_service;
    };
} // namespace GOAT_Navigation
