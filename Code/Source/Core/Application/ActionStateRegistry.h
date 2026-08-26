#pragma once

#include <GOAT/Domain/ActionState.h>
#include <GOAT/Interfaces/IActionState.h>

#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/limits.h>

namespace GOAT
{
    //! The action verbs currently available. Modules and backends add their own.
    //! Removing a module removes its verbs; nothing else refers to them by type.
    class ActionStateRegistry final
    {
    public:
        //! Registers a core verb at its reserved id. Fails when the id is taken.
        bool RegisterAt(ActionStateId id, AZStd::unique_ptr<IActionState> state);

        //! Registers a verb and returns the id assigned to it, or Invalid when the name is taken.
        ActionStateId Register(AZStd::unique_ptr<IActionState> state);

        //! Removes a verb. Agents running it will fail their current action.
        void Unregister(ActionStateId id);

        //! Resolves a verb name to its id, or Invalid when it is not registered.
        ActionStateId FindId(const AZ::Name& name) const;

        //! The verb for an id, or nullptr when nothing is registered there.
        IActionState* Find(ActionStateId id) const;

        //! Every registered verb name, for console output and authoring validation.
        AZStd::vector<AZ::Name> GetNames() const;

    private:
        //! Grows the table so an id is addressable.
        void EnsureSlot(ActionStateId id);

        AZStd::vector<AZStd::unique_ptr<IActionState>> m_states;
    };
} // namespace GOAT
