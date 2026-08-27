-- GOAT behaviour tree authoring vocabulary.
-- Executed once into the script context at startup, so tree files need no require.

-- rawget avoids O3DE's "access to undeclared global variable" warning, which fires on the
-- read half of `GOAT = GOAT or {}` the first time this file runs.
GOAT = rawget(_G, "GOAT") or {}

-- Status values a behaviour tick returns. These mirror the C++ ActionResult order.
RUNNING, SUCCESS, FAILURE = 0, 1, 2

-- Behaviours, services and backends the user has defined, keyed by name.
GOAT._behaviors = GOAT._behaviors or {}
GOAT._backends = GOAT._backends or {}
-- Control flow the user wrote, keyed by name.
GOAT._flow = GOAT._flow or {}
-- Trees the user has declared, keyed by name, so C++ can ask for one after loading a file.
GOAT._trees = GOAT._trees or {}
-- Declarative plans, keyed by goal name. A plan is an ordered list of guarded step lists.
GOAT._plans = GOAT._plans or {}
-- Per agent, per behaviour scratch tables, so one behaviour can serve many agents.
GOAT._state = GOAT._state or {}

--! Node types whose single string argument names their main property.
local defaultProperty = {
    script = "behavior",
    behavior = "behavior",
    service = "behavior",
    condition = "key",
    composite = "behavior",
    decorator = "behavior",
    conditional_loop = "key",
    compare = "key",
    delegate = "backend",
    raw = "action",
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
composite = nodeType("composite")
parallel = nodeType("parallel")

-- Decorators.
invert = nodeType("invert")
force_success = nodeType("force_success")
cooldown = nodeType("cooldown")
loop = nodeType("loop")
conditional_loop = nodeType("conditional_loop")
time_limit = nodeType("time_limit")
condition = nodeType("condition")
compare = nodeType("compare")
decorator = nodeType("decorator")

-- Leaves.
wait = nodeType("wait")
raw = nodeType("raw")
script = nodeType("script")
delegate = nodeType("delegate")
subtree = nodeType("subtree")

-- Services attach to a composite rather than sitting in its child list.
service = nodeType("service", true)

--! Declares a node word contributed by a module gem, so `move_to "player_pos"` reads the same
--! as a built-in. @mainProperty names the property the single string argument fills.
--! Called from C++ for every node type a module registers; the core never names one itself.
--! A word this file already defines is left alone, because the built-ins carry forms this
--! cannot reproduce -- `service` attaches to a composite rather than becoming a child.
function GOAT_DeclareNode(typeName, mainProperty)
    if rawget(_G, typeName) ~= nil then
        return
    end
    if mainProperty and mainProperty ~= "" then
        defaultProperty[typeName] = mainProperty
    end
    _G[typeName] = nodeType(typeName)
end

--! Reports something wrong with what a script declared.
--! Debug.Warning when the engine has reflected it, print otherwise, so this works the same in a
--! bare Lua harness as it does in the editor.
function GOAT._warn(message)
    if Debug ~= nil and Debug.Warning ~= nil then
        Debug.Warning(false, "GOAT: " .. message)
    else
        print("GOAT: " .. message)
    end
end

--! Defines a leaf behaviour: `behavior "Patrol" { start = ..., tick = ..., stop = ... }`.
function behavior(name)
    return function(body)
        -- A behaviour name is global to the vocabulary, so a second script declaring one that
        -- is taken silently replaces it -- and every tree already pointing at the first then
        -- runs the second, closed over a different script's variables. Loud, because the
        -- symptom otherwise appears in an agent that has nothing to do with the change.
        if GOAT._behaviors[name] ~= nil then
            GOAT._warn("behaviour '" .. name .. "' is declared more than once; the last one wins, "
                .. "and every tree naming it runs that one")
        end
        GOAT._behaviors[name] = body
        return body
    end
end

--! Defines control flow in Lua, used by a `composite` or `decorator` node.
--!
--! A composite's start(me, ctx, childCount) and result(me, ctx, childIndex, childStatus)
--! each return either a child index to run next, or nil plus the status to finish with.
--! A decorator's result(me, ctx, childStatus) returns the status it reports upward.
function flow(name)
    return function(body)
        GOAT._flow[name] = body
        return body
    end
end

--! One alternative inside a plan: a guard and the steps to run when it holds.
--!
--! `when` names a blackboard bool that must be true, `unless` one that must be false, and an
--! option with neither is the fallback. Guards are variable names rather than functions on
--! purpose: a name can be checked when the file loads, a closure can only be checked by calling
--! it, which needs an agent that does not exist yet. Anyone needing a real expression writes an
--! imperative `backend` instead, which has always been able to do anything.
function option(body)
    assert(type(body) == "table", "option takes a table")
    assert(body.when == nil or body.unless == nil, "an option has one guard, not both")

    local steps = {}
    for _, step in ipairs(body) do
        assert(type(step) == "table", "a plan step is a table, as in { action = \"wait\", seconds = 1 }")
        table.insert(steps, step)
    end

    return { __goat_option = true, when = body.when, unless = body.unless, steps = steps }
end

--! Declares a plan the `bt` backend can satisfy: `plan "Goal" { option { ... }, option { ... } }`.
--! Options are tried in order and the first whose guard holds contributes all of its steps.
function plan(name)
    return function(body)
        assert(type(body) == "table", "a plan takes a table of options")
        assert(GOAT._plans[name] == nil, "plan '" .. name .. "' is already declared")
        assert(GOAT._backends[name] == nil, "'" .. name .. "' is already a backend")

        local options = {}
        for _, entry in ipairs(body) do
            assert(type(entry) == "table" and entry.__goat_option,
                "a plan holds options, as in plan \"X\" { option { ... } }")
            table.insert(options, entry)
        end
        assert(#options > 0, "plan '" .. name .. "' has no options")

        -- Recorded where it was written, not where it is validated: validation may run much
        -- later, or from the console, and the useful location is always the declaration.
        local where = debug.getinfo(2, "Sl")
        GOAT._plans[name] = {
            options = options,
            source = (where and where.short_src or "?") .. ":" .. (where and where.currentline or 0),
        }
        return GOAT._plans[name]
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
        local compiled = GOAT.Compile(name, root)
        GOAT._trees[name] = compiled
        return compiled
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
--! Exposed as a plain global because C++ looks functions up with lua_getglobal, which
--! does not resolve a dotted name.
function GOAT_ForgetAgent(agentKey)
    GOAT._state[agentKey] = nil
end

--! Kept for scripts that reach for it by its namespaced name.
GOAT._forgetAgent = GOAT_ForgetAgent

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

--! Hands a declared tree to a C++ builder, one call per node and property.
--! Pushing the data out this way means C++ never has to read the Lua stack itself.
function GOAT_EmitTree(treeName, builder)
    local compiled = GOAT._trees[treeName]
    if compiled == nil then
        return false
    end

    builder:BeginTree(treeName)
    for _, record in ipairs(compiled.nodes) do
        builder:AddNode(record.type, record.childCount, record.serviceCount)
        for key, value in pairs(record.properties) do
            local kind = type(value)
            if kind == "boolean" then
                builder:SetBoolProperty(key, value)
            elseif kind == "number" then
                builder:SetNumberProperty(key, value)
            elseif kind == "string" then
                builder:SetStringProperty(key, value)
            end
        end
    end
    builder:EndTree()
    return true
end

--! Names every tree declared so far, so C++ can discover what a file produced.
function GOAT_TreeNames()
    local names = {}
    for name in pairs(GOAT._trees) do
        table.insert(names, name)
    end
    table.sort(names)
    return names
end

--! Hands every declared tree name to a C++ collector, whether or not it has compiled.
--! Told apart from what the agent system lists, which is only the trees that compiled: a tree
--! whose subtree slot was unbound failed, and rebinding that slot has to be able to find it again.
function GOAT_EmitTreeNames(collector)
    local names = {}
    for name in pairs(GOAT._trees) do
        table.insert(names, name)
    end
    table.sort(names)
    for _, name in ipairs(names) do
        collector:Add(name)
    end
end

--! Pushes one step into a C++ builder. Both the declarative and the imperative shape go
--! through here, so a step means exactly the same thing whichever wrote it.
local function pushStep(builder, step)
    builder:AddStep(step.action or "script")
    if step.behavior ~= nil then builder:SetTag(step.behavior) end
    if step.tag ~= nil then builder:SetTag(step.tag) end
    if step.seconds ~= nil then builder:SetDuration(step.seconds) end
    if step.tolerance ~= nil then builder:SetTolerance(step.tolerance) end
    if step.key ~= nil then builder:SetTargetKey(step.key) end
    if step.at ~= nil then builder:SetTargetPosition(step.at) end
    if step.entity ~= nil then builder:SetTargetEntity(step.entity) end
end
GOAT._pushStep = pushStep

--! True when an option's guard holds for this agent. An option with no guard is the fallback.
--
--! The handle is kept on the option the first time it is needed. A plan's guard is named at
--! declaration and never changes, so looking it up once per option costs nothing after that --
--! and unlike a behaviour, an option cannot hoist it into an upvalue because the name arrives
--! with the plan rather than with the script.
function GOAT._optionHolds(entry, ctx)
    if entry.when ~= nil then
        if (entry.whenKey or 0) == 0 then entry.whenKey = ctx:Key(entry.when) end
        return ctx:GetBool(entry.whenKey)
    end
    if entry.unless ~= nil then
        if (entry.unlessKey or 0) == 0 then entry.unlessKey = ctx:Key(entry.unless) end
        return not ctx:GetBool(entry.unlessKey)
    end
    return true
end

--! Runs a Lua backend and pushes the plan it returns into a C++ builder.
--! Each step is a table naming a verb, as in { action = "wait", seconds = 0.5 }.
function GOAT_Plan(backendName, agentKey, ctx, goal, builder)
    local body = GOAT._backends[backendName]
    if body == nil then
        return false
    end

    local state = GOAT._stateFor(agentKey, "backend:" .. backendName)

    -- A backend may hand back a list of steps, or drive the builder itself. The second form is
    -- what lets a backend whose steps are already baked name one instead of pushing it again.
    if body.choose ~= nil then
        return body.choose(state, ctx, goal, builder) and true or false
    end

    if body.plan == nil then
        return false
    end

    local steps = body.plan(state, ctx, goal)
    if type(steps) ~= "table" or #steps == 0 then
        return false
    end

    builder:BeginPlan()
    for _, step in ipairs(steps) do
        pushStep(builder, step)
    end
    return builder:EndPlan()
end

--! Bakes every declared plan's steps into C++ once, when the vocabulary loads.
--! After this a plan costs nothing to run: the backend names an option and C++ hands back the
--! steps it already holds, so no step ever crosses this boundary again.
function GOAT_BakePlans(builder)
    local names = {}
    for name in pairs(GOAT._plans) do
        table.insert(names, name)
    end
    table.sort(names)

    for _, name in ipairs(names) do
        for index, entry in ipairs(GOAT._plans[name].options) do
            builder:BeginPlan()
            for _, step in ipairs(entry.steps) do
                pushStep(builder, step)
            end
            builder:BakeOption(name, index)
        end
    end
end

--! Hands every declared plan to a C++ validator, which checks it against the registries.
--! Lua walks its own tables and formats the display lines; C++ only ever receives strings.
function GOAT_ValidatePlans(validator)
    local names = {}
    for name in pairs(GOAT._plans) do
        table.insert(names, name)
    end
    table.sort(names)

    for _, name in ipairs(names) do
        local declared = GOAT._plans[name]
        validator:BeginPlan(name, declared.source)

        for _, entry in ipairs(declared.options) do
            validator:BeginOption(entry.when or entry.unless or "", entry.unless ~= nil)
            for _, step in ipairs(entry.steps) do
                validator:CheckStep(step.action or "script")
                if step.key ~= nil then validator:CheckKey(step.key) end
                validator:Describe(GOAT._describeStep(step))
            end
            validator:EndOption()
        end

        validator:EndPlan()
    end
end

--! A one line rendering of a step, for the console. Formatted here because Lua holds the table.
function GOAT._describeStep(step)
    local parts = { step.action or "script" }
    if step.behavior ~= nil then table.insert(parts, step.behavior) end
    if step.tag ~= nil then table.insert(parts, step.tag) end
    if step.key ~= nil then table.insert(parts, "key=" .. step.key) end
    if step.seconds ~= nil then table.insert(parts, string.format("%.2fs", step.seconds)) end
    if step.tolerance ~= nil then table.insert(parts, string.format("tolerance=%.2f", step.tolerance)) end
    if step.at ~= nil then table.insert(parts, "at=literal") end
    if step.entity ~= nil then table.insert(parts, "entity=literal") end
    return table.concat(parts, " ")
end

--! True when a backend of that name was defined in Lua.
function GOAT_HasBackend(backendName)
    return GOAT._backends[backendName] ~= nil
end

--! Hands every declared backend name to a C++ collector.
function GOAT_EmitBackendNames(collector)
    local names = {}
    for name in pairs(GOAT._backends) do
        table.insert(names, name)
    end
    table.sort(names)
    for _, name in ipairs(names) do
        collector:Add(name)
    end
end

--! Chooses the first child a user defined composite runs.
--! Returns the child index, or -1 and a status when the node is already finished.
function GOAT_FlowBegin(flowName, agentKey, ctx, nodeKey, childCount)
    local body = GOAT._flow[flowName]
    if body == nil or body.start == nil then
        return -1, FAILURE
    end

    local state = GOAT._stateFor(agentKey, "flow:" .. flowName .. ":" .. nodeKey)
    local child, status = body.start(state, ctx, childCount)
    if type(child) ~= "number" then
        return -1, status or SUCCESS
    end
    return child - 1, SUCCESS
end

--! Chooses what a user defined composite does after a child finished.
function GOAT_FlowAdvance(flowName, agentKey, ctx, nodeKey, childIndex, childStatus)
    local body = GOAT._flow[flowName]
    if body == nil or body.result == nil then
        return -1, childStatus
    end

    local state = GOAT._stateFor(agentKey, "flow:" .. flowName .. ":" .. nodeKey)
    local child, status = body.result(state, ctx, childIndex + 1, childStatus)
    if type(child) ~= "number" then
        return -1, status or childStatus
    end
    return child - 1, SUCCESS
end

--! Filters the status a user defined decorator reports for its child.
function GOAT_FlowFilter(flowName, agentKey, ctx, nodeKey, childStatus)
    local body = GOAT._flow[flowName]
    if body == nil or body.result == nil then
        return childStatus
    end

    local state = GOAT._stateFor(agentKey, "flow:" .. flowName .. ":" .. nodeKey)
    local status = body.result(state, ctx, childStatus)
    if type(status) ~= "number" then
        return childStatus
    end
    return status
end
