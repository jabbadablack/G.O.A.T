#pragma once

#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/Intent.h>

#include <AzCore/Name/Name.h>

namespace GOAT
{
    struct PlanContext;

    //! Child index meaning "this node is finished", rather than "run this child next".
    inline constexpr int NoChild = -1;

    //! How the walker reaches control flow a user wrote themselves.
    //! Declared here and implemented by the scripting layer, so the walker never depends
    //! on Lua and a different scripting front end could supply the same behaviour.
    class INodeScripting
    {
    public:
        virtual ~INodeScripting() = default;

        //! Which child a user defined composite runs first.
        //! Returns NoChild to finish immediately, reporting through outResult.
        virtual int BeginComposite(
            const AZ::Name& behavior,
            const PlanContext& context,
            NodeIndex node,
            int childCount,
            ActionResult& outResult) = 0;

        //! Which child a user defined composite runs after one finished.
        //! Returns NoChild to finish, reporting through outResult.
        virtual int AdvanceComposite(
            const AZ::Name& behavior,
            const PlanContext& context,
            NodeIndex node,
            int childIndex,
            ActionResult childResult,
            ActionResult& outResult) = 0;

        //! What a user defined decorator reports for its child's result.
        virtual ActionResult FilterDecorator(
            const AZ::Name& behavior, const PlanContext& context, NodeIndex node, ActionResult childResult) = 0;
    };
} // namespace GOAT
