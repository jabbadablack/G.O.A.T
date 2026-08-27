-- A task network, which is the other way of writing an agent.
--
-- A tree says what to try, in what order. A domain says what has to be true, and lets the
-- planner work out the order -- including steps taken only to make a later one possible.
--
-- `Rouse` is the whole point: nothing asks for it. It is planned because `Shout` requires
-- `awake`, and `Rouse` is what makes `awake` true. A tree cannot work that out.

behavior "SayIt" {
    tick = function(me, ctx)
        return SUCCESS
    end,
}

return domain "ExampleDomain" {
    root = "Greet",

    task "Greet" {
        -- Tried in order, so the first method that holds is the one taken.
        method {
            condition "announced",
            subtask "Settle",
        },
        method {
            subtask "Shout",
            subtask "Settle",
        },
    },

    task "Shout" {
        method {
            condition "awake",
            subtask "SayHello",
        },
        -- Not awake yet, so wake up first and come back to it.
        method {
            subtask "Rouse",
            subtask "SayHello",
        },
    },

    primitive "Rouse" {
        wait(0.25),
        effect "awake",
    },

    primitive "SayHello" {
        condition "awake",
        script "SayIt",
        effect "announced",
    },

    primitive "Settle" { wait(1.0) },
}
