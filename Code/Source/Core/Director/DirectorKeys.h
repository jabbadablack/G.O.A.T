#pragma once

#include <GOAT/Domain/BlackboardKey.h>

namespace GOAT
{
    class IBlackboardSystem;

    //! What a director verb reports about what it just did.
    //!
    //! Declared from C++ rather than from a .bbx, for the same reason the navigation module
    //! declares its own: a .bbx a project had to remember to add would make every director tree
    //! fail to compile until they did.
    //!
    //! Agent scope, which for a director means "on the director" -- these are that director's
    //! account of its own last order, not anything the agents it commands can see.
    struct DirectorKeys final
    {
        //! Declares the variables and caches their keys. Safe to call more than once.
        bool Declare(IBlackboardSystem& blackboard);

        //! True once every key resolved.
        bool IsValid() const;

        //! How many agents were in reach when the verb ran.
        BlackboardKey m_reach;
        //! How many of them it actually changed.
        BlackboardKey m_changed;
        //! How many it skipped, whether for being already there, outranked, or on cooldown.
        BlackboardKey m_refused;
    };
} // namespace GOAT
