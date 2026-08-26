#pragma once

#include <GOAT/Interfaces/IBackend.h>

#include <AzCore/Memory/SystemAllocator.h>

namespace GOAT
{
    //! Runs the action a tree leaf authored inline, as a plan of exactly one step.
    //! This is what keeps the pipeline uniform when no real backend is installed.
    class DirectBackend final
        : public IBackend
    {
    public:
        AZ_CLASS_ALLOCATOR(DirectBackend, AZ::SystemAllocator);

        //! Name a tree leaf gets when it does not delegate to a backend.
        static AZ::Name GetBackendName();

        AZ::Name GetName() const override;
        bool Plan(const PlanContext& context, const Intent& intent, ActionPlan& outPlan) override;
    };
} // namespace GOAT
