-- The escape hatches, for when the built in vocabulary is not enough.
-- Everything here is still core only: no modules and no C++ backends are involved.

-- Control flow of your own. A composite's start and result each return either the next
-- child to run, or nil plus the status to finish with.
flow "AllOf" {
    start = function(me, ctx, childCount)
        me.count = childCount
        return 1
    end,
    result = function(me, ctx, childIndex, childStatus)
        if childStatus == FAILURE then
            return nil, FAILURE
        end
        if childIndex < me.count then
            return childIndex + 1
        end
        return nil, SUCCESS
    end,
}

-- A decorator of your own, filtering whatever its child reported.
flow "NeverFails" {
    result = function(me, ctx, childStatus)
        return SUCCESS
    end,
}

-- A whole backend in Lua. It receives the goal a delegate node named and returns the
-- steps to run, which reach the state machine exactly as a C++ backend's plan would.
backend "Errand" {
    plan = function(me, ctx, goal)
        if goal == "Rest" then
            return { { action = "wait", seconds = 2.0 } }
        end
        return {
            { action = "script", behavior = "Announce" },
            { action = "wait", seconds = 0.5 },
        }
    end,
}

-- The same idea declared rather than written out. Options are tried in order and the first
-- whose guard holds contributes all of its steps; the last one, having no guard, is the
-- fallback. Unlike the backend above, every step here is checked when this file loads and
-- baked once, so running it later pushes nothing across the Lua boundary at all.
plan "Chores" {
    option {
        when = "announced",
        { action = "script", behavior = "Chore" },
        { action = "wait",   seconds = 0.25 },
    },
    option {
        { action = "script", behavior = "Announce" },
        { action = "wait",   seconds = 0.5 },
    },
}

-- Resolved once, then kept: see ExampleAgent.lua for why a handle beats a name here.
local announced

behavior "Announce" {
    tick = function(me, ctx)
        announced = announced or ctx:Key("announced")
        ctx:SetBool(announced, true)
        return SUCCESS
    end,
}

behavior "Chore" {
    tick = function(me, ctx)
        return SUCCESS
    end,
}

return tree "ExampleAdvanced" {
    composite "AllOf" {
        decorator "NeverFails" { script "Chore" },
        delegate "Errand" { goal = "Deliver" },
        -- The declarative form, reached through the backend the gem ships.
        delegate "bt" { goal = "Chores" },
        -- raw reaches any registered verb by name, including one a module contributed.
        raw "wait" { seconds = 0.25 },
    },
}
