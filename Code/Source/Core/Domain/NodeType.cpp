#include <GOAT/Domain/NodeType.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    void ReflectNodeTypes(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        serializeContext->Enum<NodeKind>()
            ->Value("Composite", NodeKind::Composite)
            ->Value("Decorator", NodeKind::Decorator)
            ->Value("Leaf", NodeKind::Leaf)
            ->Value("Service", NodeKind::Service);

        serializeContext->Enum<NodeOp>()
            ->Value("Selector", NodeOp::Selector)
            ->Value("Sequence", NodeOp::Sequence)
            ->Value("Parallel", NodeOp::Parallel)
            ->Value("Invert", NodeOp::Invert)
            ->Value("ForceSuccess", NodeOp::ForceSuccess)
            ->Value("Cooldown", NodeOp::Cooldown)
            ->Value("Loop", NodeOp::Loop)
            ->Value("ConditionalLoop", NodeOp::ConditionalLoop)
            ->Value("TimeLimit", NodeOp::TimeLimit)
            ->Value("Condition", NodeOp::Condition)
            ->Value("Compare", NodeOp::Compare)
            ->Value("Action", NodeOp::Action)
            ->Value("Script", NodeOp::Script)
            ->Value("Delegate", NodeOp::Delegate)
            ->Value("Subtree", NodeOp::Subtree)
            ->Value("LuaComposite", NodeOp::LuaComposite)
            ->Value("LuaDecorator", NodeOp::LuaDecorator);
    }
} // namespace GOAT
