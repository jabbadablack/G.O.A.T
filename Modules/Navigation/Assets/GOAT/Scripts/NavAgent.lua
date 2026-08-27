-- A worked example for the GOAT_Navigation gem.
--
-- Every spatial word here -- move_to, does_path_exist -- is registered by that gem, not by
-- the core. Disable the gem and this tree stops compiling, which is the point: ExampleAgent.lua
-- uses none of them and keeps working, so the core stays genre neutral.
--
-- Pair it with Navigation.bbx, which declares move_target. nav_waypoint, nav_remaining and
-- nav_steer are declared by the module itself, so no .bbx has to mention them.

-- The patrol route. A real project would read these from its own level data.
local corners = {
    Vector3(0.0, 0.0, 0.0),
    Vector3(8.0, 0.0, 0.0),
    Vector3(8.0, 8.0, 0.0),
    Vector3(0.0, 8.0, 0.0),
}

-- Variable handles, resolved on first use and kept: a name is a constant in a script, while
-- looking one up is a dictionary hash and a map probe on every call.
local navRemaining, patrolStop, moveTarget

--! Resolves this script's handles, retrying until it succeeds. ctx:Key answers zero for a
--! variable it could not resolve, and zero is truthy in Lua, so caching on "not nil" would
--! freeze a failed lookup and leave every read here pointing at nothing.
local function keys(ctx)
    if (navRemaining or 0) ~= 0 then return end
    navRemaining = ctx:Key("nav_remaining")
    patrolStop = ctx:Key("patrol_stop")
    moveTarget = ctx:Key("move_target")
end

-- A service turns polling into a blackboard write on a fixed interval. This one only picks a
-- new corner once the last has been reached, which move_to reports through nav_remaining.
behavior "PickTarget" {
    tick = function(me, ctx)
        keys(ctx)
        if ctx:GetFloat(navRemaining) > 0.5 then
            return
        end

        local stop = (ctx:GetInt(patrolStop) % #corners) + 1
        ctx:SetInt(patrolStop, stop)
        ctx:SetVector3(moveTarget, corners[stop])
    end,
}

return tree "NavAgent" {
    selector { service "PickTarget" { interval = 0.25 },

        -- Reaching the target is the normal case, so it is tried first.
        sequence {
            does_path_exist "move_target",
            move_to { key = "move_target", tolerance = 0.5, speed = 4.0 },
        },

        -- Nowhere walkable: pause rather than re-running the same failing query every tick.
        wait(1.0),
    },
}
