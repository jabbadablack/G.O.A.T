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

behavior "Announce" {
    tick = function(me, ctx)
        ctx:SetBool("announced", true)
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
        -- raw reaches any registered verb by name, including one a module contributed.
        raw "wait" { seconds = 0.25 },
    },
}
