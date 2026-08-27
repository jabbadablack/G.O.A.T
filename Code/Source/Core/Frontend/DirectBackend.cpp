#include <Core/Frontend/DirectBackend.h>

#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    AZ::Name DirectBackend::GetBackendName()
    {
        return AZ_NAME_LITERAL("direct");
    }

    AZ::Name DirectBackend::GetName() const
    {
        return GetBackendName();
    }

    bool DirectBackend::Plan(const PlanContext& context, const Intent& intent, ActionPlan& outPlan)
    {
        AZ_Assert(intent.m_backend.IsEmpty() || intent.m_backend == GetBackendName(),
            "The direct backend was handed an intent addressed to '%s'", intent.m_backend.GetCStr());

        if (intent.m_direct.m_action == CoreActions::Invalid)
        {
            AZ_Error("GOAT", false,
                "The direct backend cannot plan node %u: its leaf named no registered verb", intent.m_node);
            return false;
        }

        AZ_Assert(context.m_planStore != nullptr, "Producing a plan needs somewhere to put its steps");
        if (context.m_planStore == nullptr)
        {
            return false;
        }

        // One step, borrowed like any other computed plan. It could not point straight at the
        // compiled node instead: a backend never sees the tree, which is what keeps it swappable.
        outPlan.m_span = context.m_planStore->Acquire(&intent.m_direct, 1);

        AZ_Assert(outPlan.Size() == 1, "A direct plan is exactly the one action the leaf asked for");
        return true;
    }
} // namespace GOAT
