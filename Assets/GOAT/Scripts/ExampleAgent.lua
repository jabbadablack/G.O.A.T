-- A worked example that runs with no modules and no backends installed.
-- It uses only the genre neutral core vocabulary, so it is valid in any project.

-- A leaf behaviour. `me` is this agent's own scratch table for this behaviour.
behavior "Patrol" {
    start = function(me)
        me.stop = 0
    end,
    tick = function(me, ctx)
        me.stop = me.stop + 1
        ctx:SetInt("patrol_stop", me.stop)
        return SUCCESS
    end,
}

behavior "Alert" {
    tick = function(me, ctx)
        ctx:SetBool("alerted", true)
        return SUCCESS
    end,
}

-- A service turns polling into blackboard writes on a fixed interval.
-- Pairing one with an observing condition is how a tree reacts without checking every frame.
behavior "Sense" {
    tick = function(me, ctx)
        ctx:SetBool("target_seen", ctx:GetInt("patrol_stop") % 4 == 0)
    end,
}

return tree "ExampleAgent" {
    selector { service "Sense" { interval = 0.25 },
        sequence {
            -- Observes target_seen; flipping it preempts the patrol branch below.
            condition "target_seen" { abort = "lower_priority" },
            script "Alert",
            wait(1.0),
        },
        sequence {
            script "Patrol",

            -- A parallel runs its main branch while its background branch of conditions is
            -- re-checked. The wait below is cut short the moment Sense sees the target, without
            -- the wait itself knowing anything about it.
            parallel {
                wait(0.5),
                invert { condition "target_seen" },
            },
        },
    },
}
