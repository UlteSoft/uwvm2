includes("option.lua")

---@type string
local strip_cfg = get_config("strip") or "default"

---@param default_level "none" | "symbol" | "ident"
---@return "none" | "symbol" | "ident"
local function strip_level(default_level)
    if strip_cfg == "default" then
        return default_level
    end
    return strip_cfg
end

---@param level "none" | "symbol" | "ident"
---@return boolean
local function should_strip_symbols(level)
    return level == "symbol" or level == "ident"
end

---@type boolean
local enable_lto = get_config("enable-lto")

rule("debug", function()
    on_load(function(target)
        target:add("defines", "DEBUG", "_DEBUG")
        target:add("defines", "UWVM_MODE_DEBUG")
        target:set("symbols", "debug")
        target:set("optimize", "none")
        --target:set("fpmodels", "precise")
        local level = strip_level("none")
        if should_strip_symbols(level) then
            target:set("strip", "all")
        end
    end)
end)

rule("release", function()
    on_load(function(target)
        target:add("defines", "NDEBUG")
        target:add("defines", "UWVM_MODE_RELEASE")
        target:set("optimize", "fastest")
        --target:set("fpmodels", "precise")
        local level = strip_level("none")
        if should_strip_symbols(level) then
            target:set("strip", "all")
        end
        target:set("policy", "build.optimization.lto", enable_lto)
    end)
end)

rule("minsizerel", function()
    on_load(function(target)
        target:add("defines", "NDEBUG")
        target:add("defines", "UWVM_MODE_MINSIZEREL")
        target:set("optimize", "smallest")
        --target:set("fpmodels", "precise")
        local level = strip_level("ident")
        if should_strip_symbols(level) then
            target:set("strip", "all")
        end
        target:set("policy", "build.optimization.lto", enable_lto)
    end)
end)

rule("releasedbg", function()
    on_load(function(target)
        target:add("defines", "NDEBUG")
        target:add("defines", "UWVM_MODE_RELEASEDBG")
        target:set("optimize", "fastest")
        --target:set("fpmodels", "precise")
        target:set("symbols", "debug")
        target:set("policy", "build.optimization.lto", enable_lto)
        local level = strip_level("none")
        if should_strip_symbols(level) then
            target:set("strip", "all")
        end
    end)
end)


---rule tables
---@type string[]
support_rules_table = { "debug", "release", "minsizerel", "releasedbg" }

---set rules
