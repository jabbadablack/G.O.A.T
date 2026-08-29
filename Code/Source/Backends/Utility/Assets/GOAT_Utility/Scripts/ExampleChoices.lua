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

    -- Nothing argues for it, so it is what happens when nothing else scores.
    choice "Idle" { wait(1.0) },
}
