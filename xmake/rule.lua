includes("option.lua")

---@type string | nil
local debug_strip = get_config("debug-strip")

if not debug_strip then -- not strip
    debug_strip = nil
end

---@type boolean
local enable_lto = get_config("enable-lto")

rule("debug", function()
    on_load(function(target)
        target:add("defines", "DEBUG", "_DEBUG")
        target:add("defines", "UWVM_MODE_DEBUG")
        target:set("symbols", "debug")
        target:set("optimize", "none")
        target:set("strip", debug_strip)
    end)
end)

rule("release", function()
    on_load(function(target)
        target:add("defines", "NDEBUG")
        target:add("defines", "UWVM_MODE_RELEASE")
        target:set("optimize", "fastest")
        target:set("fpmodels", "fast")
        target:set("strip", "all")
        target:set("policy", "build.optimization.lto", enable_lto)
    end)
end)

rule("minsizerel", function()
    on_load(function(target)
        target:add("defines", "NDEBUG")
        target:add("defines", "UWVM_MODE_MINSIZEREL")
        target:set("optimize", "smallest")
        target:set("fpmodels", "fast")
        target:set("strip", "all")
        target:set("policy", "build.optimization.lto", enable_lto)
    end)
end)

rule("releasedbg", function()
    on_load(function(target)
        target:add("defines", "NDEBUG")
        target:add("defines", "UWVM_MODE_RELEASEDBG")
        target:set("optimize", "fastest")
        target:set("fpmodels", "fast")
        target:set("symbols", "debug")
        target:set("policy", "build.optimization.lto", enable_lto)
        target:set("strip", debug_strip)
    end)
end)
rule("profile.generate")
    on_config(function(target)
    if target:toolchain("gcc") or target:toochain("gcc-cross") then
        targer:add("cxflags","-fprofile-generate",{force = true})
        rger:add("ldflags","-fprofile-generate",{force = true})
    end
end)

---rule tables
---@type string[]
support_rules_table = { "debug", "release", "minsizerel", "releasedbg" }

---set rules
