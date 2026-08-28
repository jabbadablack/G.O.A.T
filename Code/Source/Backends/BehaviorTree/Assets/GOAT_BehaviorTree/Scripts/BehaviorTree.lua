-- Behaviour tree authoring vocabulary, run straight after GOAT.lua.
-- Every other tree word declares itself from the node type registry; only the two forms
-- GOAT_DeclareNode cannot reach are written here.

-- A service attaches to a composite rather than sitting in its child list.
service = GOAT.nodeType("service", true)

-- A bare string names the tree to run, not the slot to rebind.
GOAT_DeclareNode("subtree", "tree")

--! Declares a tree: `tree "Guard" { selector { ... } }`.
function tree(name)
    return function(body)
        local root = body[1]
        assert(root ~= nil, "tree '" .. tostring(name) .. "' has no root node")
        local compiled = GOAT.Compile(name, root)
        GOAT._trees[name] = compiled
        return compiled
    end
end
