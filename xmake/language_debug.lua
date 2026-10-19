-- Language selection controls build contents only, never Runtime/ROS grants.
uwvm_language_debug_options = {
    {"c", "UWVM_DEBUG_LANGUAGE_C"},
    {"cpp", "UWVM_DEBUG_LANGUAGE_CPP"},
    {"rust", "UWVM_DEBUG_LANGUAGE_RUST"},
    {"go", "UWVM_DEBUG_LANGUAGE_GO"},
    {"tinygo", "UWVM_DEBUG_LANGUAGE_TINYGO"},
    {"zig", "UWVM_DEBUG_LANGUAGE_ZIG"},
    {"java", "UWVM_DEBUG_LANGUAGE_JAVA"},
    {"csharp", "UWVM_DEBUG_LANGUAGE_CSHARP"},
    {"fsharp", "UWVM_DEBUG_LANGUAGE_FSHARP"},
    {"vb", "UWVM_DEBUG_LANGUAGE_VB"},
    {"python", "UWVM_DEBUG_LANGUAGE_PYTHON"},
    {"javascript", "UWVM_DEBUG_LANGUAGE_JAVASCRIPT"},
    {"assemblyscript", "UWVM_DEBUG_LANGUAGE_ASSEMBLYSCRIPT"},
    {"moonbit", "UWVM_DEBUG_LANGUAGE_MOONBIT"},
    {"kotlin", "UWVM_DEBUG_LANGUAGE_KOTLIN"},
}

-- Project-description functions do not expose Lua error()/assert() or the
-- script sandbox's raise(). Keep parsing pure and reject invalid input from
-- the option's after_check callback, where raise() is an Xmake script API.
local function parse_language_debug_options(profile, individual)
    local selected, known = {}, {}
    for _, entry in ipairs(uwvm_language_debug_options) do
        known[entry[1]] = true
        selected[entry[1]] = profile == "all" or
            (profile == "individual" and individual[entry[1]] == true)
    end
    if profile ~= "individual" and profile ~= "all" and profile ~= "none" then
        if type(profile) ~= "string" or profile == "" or profile:sub(1, 1) == "," or
            profile:sub(-1) == "," or profile:find(",,", 1, true) then
            return {}, "--debug-languages requires an exact nonempty comma-separated language list"
        end
        for name in profile:gmatch("[^,]+") do
            if not known[name] or selected[name] then
                return {}, "unknown or duplicate --debug-languages entry: " .. name
            end
            selected[name] = true
        end
    end
    return selected
end

option("debug-languages", function()
    set_default("individual")
    set_showmenu(true)
    set_description("Debugger adapters: individual, none, all, or an exact comma-separated language list. An explicit list overrides saved individual switches.")
    after_check(function(option)
        local _, reason = parse_language_debug_options(option:value() or "individual", {})
        if reason then
            raise(reason)
        end
    end)
end)

for _, entry in ipairs(uwvm_language_debug_options) do
    local name, macro = entry[1], entry[2]
    option("debug-language-" .. name, function()
        set_default(true)
        set_showmenu(true)
        set_description("Compile the " .. name .. " debugger adapter (" .. macro .. ") in individual selection mode.")
    end)
end

function uwvm_selected_language_debug_options()
    local individual = {}
    for _, entry in ipairs(uwvm_language_debug_options) do
        individual[entry[1]] = not not get_config("debug-language-" .. entry[1])
    end
    return parse_language_debug_options(get_config("debug-languages") or "individual", individual)
end

function uwvm_language_debug_enabled(name)
    return uwvm_selected_language_debug_options()[name] == true
end

function uwvm_apply_language_debug_options()
    local selected = uwvm_selected_language_debug_options()
    for _, entry in ipairs(uwvm_language_debug_options) do
        add_defines(entry[2] .. "=" .. (selected[entry[1]] and "1" or "0"))
    end
end
