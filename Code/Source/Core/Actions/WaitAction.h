#pragma once

#include <GOAT/Interfaces/IActionState.h>

#include <AzCore/Memory/SystemAllocator.h>

namespace GOAT
{
    //! Runs for the duration named by the action request, then succeeds.
    //! Genre neutral, so it lives in the core rather than a module.
    class WaitAction final
        : public IActionState
    {
    public:
        AZ_CLASS_ALLOCATOR(WaitAction, AZ::SystemAllocator);

        AZ::Name GetName() const override;
        void Begin(const ActionContext& context) override;
        ActionResult Step(const ActionContext& context, float deltaTime) override;
        void End(const ActionContext& context) override;
    };
} // namespace GOAT
