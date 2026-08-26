#pragma once

#include <GOAT/Domain/BlackboardKey.h>

#include <AzCore/std/any.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/utils.h>

namespace GOAT
{
    //! Slot counts and declared defaults for one blackboard scope.
    //! A storage instance is sized and seeded from this.
    struct BlackboardLayout final
    {
        //! How many slots of each blackboard type this scope holds.
        AZStd::array<AZ::u32, static_cast<size_t>(BlackboardType::Count)> m_slotCounts{};

        //! Initial value for each slot that declared one.
        AZStd::vector<AZStd::pair<BlackboardKey, AZStd::any>> m_defaults;
    };
} // namespace GOAT
