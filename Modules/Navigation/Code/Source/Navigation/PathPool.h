#pragma once

#include <AzCore/Math/Vector3.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT_Navigation
{
    //! Index of a borrowed path slot.
    using PathSlot = AZ::u32;

    //! Value meaning "no slot".
    inline constexpr PathSlot InvalidPathSlot = 0xFFFFFFFFu;

    //! Reuses path buffers across agents so following a path allocates nothing after warm-up.
    //! An agent's state machine scratch is 32 bytes, far too small to hold a path, so the path
    //! lives here and the scratch holds only the slot.
    class PathPool final
    {
    public:
        //! Borrows a cleared slot.
        PathSlot Acquire();

        //! Returns a slot, keeping its capacity for the next borrower.
        void Release(PathSlot slot);

        //! The buffer behind a slot, or nullptr when the slot is not borrowed.
        AZStd::vector<AZ::Vector3>* Find(PathSlot slot);

        //! How many slots are currently borrowed, for diagnostics.
        size_t GetBorrowedCount() const;

    private:
        struct Entry
        {
            AZStd::vector<AZ::Vector3> m_path;
            bool m_borrowed = false;
        };

        AZStd::vector<Entry> m_entries;
        AZStd::vector<PathSlot> m_free;
    };
} // namespace GOAT_Navigation
