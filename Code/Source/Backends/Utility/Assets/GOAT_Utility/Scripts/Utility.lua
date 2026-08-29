-- Utility authoring vocabulary, run straight after GOAT.lua.
-- A utility program is a node tree like any other, so it reaches C++ the same way a tree does.

GOAT_DeclareNode("choice", "name")
GOAT_DeclareNode("consider", "key")

--! Declares a set of scored choices: `utility "Soldier" { choice "Flee" { consider "fear" } }`.
--! Every choice scores itself from what it considers, and the best one runs.
--! Written `choice` rather than `option` because `option` already belongs to declared plans,
--! and GOAT_DeclareNode leaves a word that exists alone rather than taking it over.
function utility(name)
    return function(body)
        assert(type(body) == "table", "a utility program takes a table of choices")
        assert(GOAT._trees[name] == nil, "'" .. name .. "' is already declared")

        local compiled = GOAT.Compile(name, GOAT.nodeType("utility")(body))
        GOAT._trees[name] = compiled
        return compiled
    end
end
