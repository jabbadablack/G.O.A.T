-- The behaviour tree plan backend.
--
-- A tree reaches it as `delegate "bt" { goal = "SecurePerimeter" }`, and it answers with the
-- steps of the first option in that plan whose guard holds.
--
-- An ordered list of guarded step lists is a behaviour tree: it is exactly
--   selector { sequence { ... }, sequence { ... } }
-- restricted to two levels. What it adds over writing that in the tree itself is commitment --
-- the whole sequence reaches the state machine as one plan, and the tree is not consulted again
-- until the plan finishes or a guard above the delegate node aborts it.
--
-- Nothing here pushes a step. Every option was baked into C++ when the vocabulary loaded, so
-- planning is: read a few blackboard bools, name the option that won, and hand back the steps
-- C++ already holds. That is why a five hundred step plan costs the same as a one step plan.

backend "bt" {
    choose = function(me, ctx, goal, builder)
        local declared = GOAT._plans[goal]
        if declared == nil then
            return false
        end

        for index, entry in ipairs(declared.options) do
            if GOAT._optionHolds(entry, ctx) then
                -- All or nothing: an option contributes every one of its steps or none of them,
                -- and no later option is considered once one has won.
                return builder:ChooseBaked(goal, index)
            end
        end

        -- No option matched. An empty plan is a refusal, which fails the delegate leaf and lets
        -- the tree carry on -- the same thing any other backend does when it cannot help.
        return false
    end,
}
