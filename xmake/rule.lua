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

local function release_lto_enabled(target)
    if not enable_lto then
        return false
    end
    -- The qualified N64 native-debug profiles uses GNU gold without an
    -- LLVM LTO plugin. Apply the same choice to every release-family rule;
    -- otherwise the default LTO option passes LLVM bitcode to that linker.
    local triple = string.lower(tostring(get_config("target") or ""))
    local llvm_triple = string.lower(tostring(get_config("llvm-target") or ""))
    if target:is_plat("linux") and get_config("use-llvm-compiler") and
        (get_config("linux-native-debug") or get_config("linux-x64-native-debug")) and
        (target:is_arch("mips64", "mips64el") or triple:find("^mips64el%-") or llvm_triple:find("^mips64el%-") or
            triple:find("^mips64%-") or llvm_triple:find("^mips64%-")) then
        return false
    end
    return true
end

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
        local macos_4gib_test = get_config("macos-4gib-test")
        if macos_4gib_test then
            assert(target:is_plat("macosx") and target:is_arch("arm64", "aarch64"),
                "--macos-4gib-test requires macOS ARM64 release mode")
            target:set("optimize", "none")
        else
            target:set("optimize", "fastest")
        end
        --target:set("fpmodels", "precise")
        local level = strip_level("none")
        if should_strip_symbols(level) then
            target:set("strip", "all")
        end
        target:set("policy", "build.optimization.lto", release_lto_enabled(target) and not macos_4gib_test)
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
        target:set("policy", "build.optimization.lto", release_lto_enabled(target))
    end)
end)

rule("releasedbg", function()
    on_load(function(target)
        target:add("defines", "NDEBUG")
        target:add("defines", "UWVM_MODE_RELEASEDBG")
        target:set("optimize", "fastest")
        --target:set("fpmodels", "precise")
        target:set("symbols", "debug")
        target:set("policy", "build.optimization.lto", release_lto_enabled(target))
        local level = strip_level("none")
        if should_strip_symbols(level) then
            target:set("strip", "all")
        end
    end)
end)


-- Match the JIT's at-most-4 KiB probe interval. In particular, AArch64 toolchains
-- may otherwise assume a 64 KiB guard while pthreads expose smaller guards.
-- Unsupported compiler flags remain subject to xmake's normal flag checks.
rule("native_stack_probes", function()
    on_config(function(target)
        for _, toolkind in ipairs({"cc", "cxx"}) do
            local flagkind = toolkind == "cc" and "cflags" or "cxxflags"
            target:add(flagkind, "-fstack-clash-protection")
            if target:has_tool(toolkind, "clang", "clangxx") then
                -- LLVM RISC-V's split CSR save can leave an unprobed
                -- sub-2-KiB adjustment before the first regular probe.
                local probe_size = target:arch():find("^riscv") and "2048" or "4096"
                target:add(flagkind, "-mstack-probe-size=" .. probe_size)
            elseif target:has_tool(toolkind, "gcc", "gxx") then
                target:add(flagkind, "--param=stack-clash-protection-guard-size=12")
                target:add(flagkind, "--param=stack-clash-protection-probe-interval=12")
            end
        end
    end)
end)

rule("module_initializer_check", function()
    on_config(function(target)
        local check = import("utility.module_initializers", {anonymous = true})
        check.check_target(target)
    end)
end)

---rule tables
---@type string[]
support_rules_table = { "debug", "release", "minsizerel", "releasedbg" }

---set rules
