#pragma once

#include <Core/Domain/BlackboardLayout.h>

#include <GOAT/Domain/BlackboardKey.h>

#include <AzCore/Name/Name.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! The merged set of blackboard variables declared by every loaded .bbx asset.
    //! Names share one namespace across all assets, so any stage can reach any variable.
    class BlackboardSchema final
    {
    public:
        //! Declares one variable and assigns it a slot.
        //! Fails when the name is already declared, so a collision is caught at load.
        AZ::Outcome<BlackboardKey, AZStd::string> Declare(
            const AZ::Name& name, BlackboardScope scope, BlackboardType type, AZStd::any defaultValue = {});

        //! Resolves a name to its key, or an invalid key when the name is undeclared.
        BlackboardKey Find(const AZ::Name& name) const;

        //! Slot counts and defaults for one scope, used to size a storage instance.
        const BlackboardLayout& GetLayout(BlackboardScope scope) const;

        //! Every declared variable, for validation messages and console output.
        const AZStd::unordered_map<AZ::Name, BlackboardKey>& GetVariables() const { return m_keysByName; }

        //! Forgets every declaration.
        void Clear();

    private:
        AZStd::unordered_map<AZ::Name, BlackboardKey> m_keysByName;
        AZStd::array<BlackboardLayout, static_cast<size_t>(BlackboardScope::Count)> m_layouts;
    };
} // namespace GOAT
