-- A worked example that runs with no modules and no backends installed.
-- It uses only the genre neutral core vocabulary, so it is valid in any project.

-- Variable handles, resolved the first time a behaviour runs and kept as upvalues afterwards.
-- A name is a constant in a script; looking it up is a dictionary hash and a map probe, so
-- ctx:Key turns that into a number once and every read and write below is then an array index.
local patrolStop, alerted, targetSeen

--! Resolves this script's handles, retrying until it succeeds. ctx:Key answers zero for a
--! variable it could not resolve, and zero is truthy in Lua, so caching on "not nil" would
--! freeze a failed lookup and leave every read here pointing at nothing.
local function keys(ctx)
    if (patrolStop or 0) ~= 0 then return end
    patrolStop = ctx:Key("patrol_stop")
    alerted = ctx:Key("alerted")
    targetSeen = ctx:Key("target_seen")
end

-- A leaf behaviour. `me` is this agent's own scratch table for this behaviour.
behavior "Patrol" {
    start = function(me)
        me.stop = 0
    end,
    tick = function(me, ctx)
        keys(ctx)
        me.stop = me.stop + 1
        ctx:SetInt(patrolStop, me.stop)
        return SUCCESS
    end,
}

behavior "Alert" {
    tick = function(me, ctx)
        keys(ctx)
        ctx:SetBool(alerted, true)
        return SUCCESS
    end,
}

-- A service turns polling into blackboard writes on a fixed interval.
-- Pairing one with an observing condition is how a tree reacts without checking every frame.
behavior "Sense" {
    tick = function(me, ctx)
        keys(ctx)
        ctx:SetBool(targetSeen, ctx:GetInt(patrolStop) % 4 == 0)
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
