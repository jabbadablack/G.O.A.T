-- Scoring, which is the third way of writing an agent.
--
-- A tree says what to try, in order. A domain says what has to be true. This says what is
-- worth doing: every choice reads a few numbers already scaled to 0 to 1, and the best one runs.

-- A scorer is handed the values its choice already considered and answers with one number,
-- which is where any shaping a consideration cannot express is written.
behavior "Panic" {
    score = function(me, ctx, considered)
        local fear = considered[1] or 0.0
        return fear * fear
    end,
}

return utility "ExampleChoices" {
    recheck = 0.25,
    momentum = 0.15,
    pick = "weighted",
    top = 2,

    choice "Flee" {
        consider "fear",
        consider "health_low",
        score = "Panic",
        commit = true,
        wait(2.0),
    },

    choice "Fight" {
        consider "morale",
        combine = "mean",
        script "Swing",
        wait(0.5),
    },

    -- Doing nothing much, which is worth a little and never worth a lot. `idle_worth` is a
    -- constant: a variable with a default that nobody writes. A choice with no considerations
    -- at all scores *one* -- the top of the range, not the bottom -- and wins almost everything.
    choice "Idle" {
        consider "idle_worth",
        wait(1.0),
    },
}
