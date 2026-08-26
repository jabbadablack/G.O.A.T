-- GOAT behaviour tree authoring vocabulary.
-- Executed once into the script context at startup, so tree files need no require.

GOAT = GOAT or {}

-- Status values a behaviour tick returns. These mirror the C++ ActionResult order.
RUNNING, SUCCESS, FAILURE = 0, 1, 2

-- Behaviours, services and backends the user has defined, keyed by name.
GOAT._behaviors = GOAT._behaviors or {}
GOAT._backends = GOAT._backends or {}
-- Per agent, per behaviour scratch tables, so one behaviour can serve many agents.
GOAT._state = GOAT._state or {}

--! Node types whose single string argument names their main property.
local defaultProperty = {
    script = "behavior",
    behavior = "behavior",
    service = "behavior",
    condition = "key",
    conditional_loop = "key",
    compare = "key",
    delegate = "backend",
    subtree = "tree",
    wait = "seconds",
    cooldown = "seconds",
    time_limit = "seconds",
    loop = "count",
}

--! Lets a node be written as `name "x"`, as `name { ... }`, or as `name "x" { ... }`.
--! Numbers need ordinary parentheses, as in `wait(2)`, because Lua only allows the
--! parenthesis free call form for a string literal or a table constructor.
local function callable(node)
    return setmetatable(node, {
        __call = function(self, arg)
            if type(arg) == "table" then
                for index, child in ipairs(arg) do
                    if child.__goat_service then
                        table.insert(self.services, child)
                    else
                        table.insert(self.children, child)
                    end
                end
                for key, value in pairs(arg) do
                    if type(key) == "string" then
                        self.properties[key] = value
                    end
                end
            elseif arg ~= nil then
                self.properties[defaultProperty[self.type] or "name"] = arg
            end
            return self
        end,
    })
end

--! Builds the constructor for one node type.
local function nodeType(typeName, isService)
    return function(arg)
        local node = callable({
            type = typeName,
            properties = {},
            children = {},
            services = {},
            __goat_service = isService or nil,
        })
        return node(arg)
    end
end

-- Composites.
selector = nodeType("selector")
sequence = nodeType("sequence")

-- Decorators.
invert = nodeType("invert")
force_success = nodeType("force_success")
cooldown = nodeType("cooldown")
loop = nodeType("loop")
conditional_loop = nodeType("conditional_loop")
time_limit = nodeType("time_limit")
condition = nodeType("condition")
compare = nodeType("compare")

-- Leaves.
wait = nodeType("wait")
script = nodeType("script")
delegate = nodeType("delegate")
subtree = nodeType("subtree")

-- Services attach to a composite rather than sitting in its child list.
service = nodeType("service", true)

--! Defines a leaf behaviour: `behavior "Patrol" { start = ..., tick = ..., stop = ... }`.
function behavior(name)
    return function(body)
        GOAT._behaviors[name] = body
        return body
    end
end

--! Defines a backend in Lua: `backend "MyGoap" { plan = function(me, intent) ... end }`.
function backend(name)
    return function(body)
        GOAT._backends[name] = body
        return body
    end
end

--! Declares a tree: `tree "Guard" { selector { ... } }`.
function tree(name)
    return function(body)
        local root = body[1]
        assert(root ~= nil, "tree '" .. tostring(name) .. "' has no root node")
        return GOAT.Compile(name, root)
    end
end

--! Flattens a node graph into the pre-order record list the C++ loader reads.
--! Doing this in Lua keeps the fragile C++ side to reading a plain array.
function GOAT.Compile(name, root)
    local records = {}

    local function emit(node)
        assert(type(node) == "table" and node.type, "a tree may only contain nodes")
        local record = {
            type = node.type,
            childCount = #node.children,
            serviceCount = #node.services,
            properties = node.properties,
        }
        table.insert(records, record)
        for _, attached in ipairs(node.services) do
            table.insert(records, {
                type = attached.type,
                childCount = 0,
                serviceCount = 0,
                properties = attached.properties,
            })
        end
        for _, child in ipairs(node.children) do
            emit(child)
        end
    end

    emit(root)
    return { __goat_tree = true, name = name, nodes = records }
end

--! Returns the scratch table one behaviour keeps for one agent.
function GOAT._stateFor(agentKey, behaviorName)
    local perAgent = GOAT._state[agentKey]
    if perAgent == nil then
        perAgent = {}
        GOAT._state[agentKey] = perAgent
    end
    local state = perAgent[behaviorName]
    if state == nil then
        state = {}
        perAgent[behaviorName] = state
    end
    return state
end

--! Drops every scratch table an agent owned, called when the agent goes away.
function GOAT._forgetAgent(agentKey)
    GOAT._state[agentKey] = nil
end

--! The single entry point C++ calls to run a behaviour phase.
--! Returning a status here avoids C++ holding any Lua reference of its own.
function GOAT_Dispatch(behaviorName, phase, agentKey, ctx, dt)
    local body = GOAT._behaviors[behaviorName]
    if body == nil then
        return FAILURE
    end

    local fn = body[phase]
    if fn == nil then
        return phase == "tick" and SUCCESS or RUNNING
    end

    local state = GOAT._stateFor(agentKey, behaviorName)
    local result = fn(state, ctx, dt)
    if result == nil then
        return SUCCESS
    end
    return result
end
