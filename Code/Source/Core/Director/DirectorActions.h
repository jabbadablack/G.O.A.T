#pragma once

#include <Core/Director/DirectorKeys.h>
#include <Core/Director/DirectorRegistry.h>

#include <GOAT/Interfaces/IActionState.h>

#include <AzCore/Memory/SystemAllocator.h>

namespace GOAT
{
    class IAgentSystem;

    //! What every director verb shares: who it may command, what it reports, and how it counts.
    //!
    //! A director verb never runs for its own agent -- it runs for the agents that agent governs
    //! -- so the shared part is resolving the reach, narrowing it, applying, and writing the
    //! tally back. Only the "applying" differs between the five words.
    class DirectorActionBase
        : public IActionState
    {
    public:
        DirectorActionBase(DirectorRegistry& directors, const DirectorKeys& keys);

        void Begin(const ActionContext& context) override;
        ActionResult Step(const ActionContext& context, float deltaTime) override;
        void End(const ActionContext& context) override;

    protected:
        //! What one verb did to one agent.
        enum class Outcome
        {
            Changed,  //!< The agent is different than it was.
            Refused   //!< Already there, outranked, or on cooldown.
        };

        //! Applies this verb to one agent. Called only for agents in reach.
        virtual Outcome Apply(const ActionContext& context, AgentId director, AgentId agent) = 0;

        //! True when this verb takes a cooldown. Only a destructive one needs it.
        virtual bool UsesCooldown() const { return false; }

        //! The verb's own id, so a cooldown is per verb rather than per director.
        ActionStateId GetVerbId(const ActionContext& context) const;

        DirectorRegistry& m_directors;
        const DirectorKeys& m_keys;
    };

    //! Puts the agents in reach onto another tree, forgetting what they had interrupted.
    class OrderTreeAction final
        : public DirectorActionBase
    {
    public:
        AZ_CLASS_ALLOCATOR(OrderTreeAction, AZ::SystemAllocator);
        using DirectorActionBase::DirectorActionBase;

        AZ::Name GetName() const override;

    protected:
        Outcome Apply(const ActionContext& context, AgentId director, AgentId agent) override;
        bool UsesCooldown() const override { return true; }
    };

    //! Interrupts the agents in reach with another tree, which they can return from themselves.
    class OrderInterruptAction final
        : public DirectorActionBase
    {
    public:
        AZ_CLASS_ALLOCATOR(OrderInterruptAction, AZ::SystemAllocator);
        using DirectorActionBase::DirectorActionBase;

        AZ::Name GetName() const override;

    protected:
        Outcome Apply(const ActionContext& context, AgentId director, AgentId agent) override;
        bool UsesCooldown() const override { return true; }
    };

    //! Moves the agents in reach between pacing bands. Cheap, so it takes no cooldown.
    class OrderBandAction final
        : public DirectorActionBase
    {
    public:
        AZ_CLASS_ALLOCATOR(OrderBandAction, AZ::SystemAllocator);
        using DirectorActionBase::DirectorActionBase;

        AZ::Name GetName() const override;

    protected:
        Outcome Apply(const ActionContext& context, AgentId director, AgentId agent) override;
    };

    //! Writes a blackboard variable, reaching whoever that variable's declared scope says.
    //!
    //! Not one of the shared shape: a Global variable is written once no matter how large the
    //! reach, and a Squad one once per squad represented in it, so this counts differently.
    class OrderValueAction final
        : public IActionState
    {
    public:
        AZ_CLASS_ALLOCATOR(OrderValueAction, AZ::SystemAllocator);

        OrderValueAction(DirectorRegistry& directors, const DirectorKeys& keys);

        AZ::Name GetName() const override;
        void Begin(const ActionContext& context) override;
        ActionResult Step(const ActionContext& context, float deltaTime) override;
        void End(const ActionContext& context) override;

    private:
        DirectorRegistry& m_directors;
        const DirectorKeys& m_keys;
    };

    //! Points a subtree slot at another tree, reshaping every tree that used it.
    //! Level wide rather than per agent, so it ignores the reach entirely.
    class RebindSubtreeAction final
        : public IActionState
    {
    public:
        AZ_CLASS_ALLOCATOR(RebindSubtreeAction, AZ::SystemAllocator);

        explicit RebindSubtreeAction(const DirectorKeys& keys);

        AZ::Name GetName() const override;
        void Begin(const ActionContext& context) override;
        ActionResult Step(const ActionContext& context, float deltaTime) override;
        void End(const ActionContext& context) override;

    private:
        const DirectorKeys& m_keys;
    };
} // namespace GOAT
