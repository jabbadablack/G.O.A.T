-- Task network authoring vocabulary, run straight after GOAT.lua.
-- A domain is a node tree like any other, so it reaches C++ the same way a tree does.

GOAT_DeclareNode("task", "name")
GOAT_DeclareNode("method", "")
GOAT_DeclareNode("primitive", "name")
GOAT_DeclareNode("subtask", "task")
GOAT_DeclareNode("effect", "key")

--! Declares a task network: `domain "Soldier" { task "Engage" { ... }, primitive "Slam" { ... } }`.
--! Planning starts at the first task unless `root` names another.
function domain(name)
    return function(body)
        assert(type(body) == "table", "a domain takes a table of tasks")
        assert(GOAT._trees[name] == nil, "'" .. name .. "' is already declared")

        local compiled = GOAT.Compile(name, GOAT.nodeType("domain")(body))
        GOAT._trees[name] = compiled
        return compiled
    end
end
