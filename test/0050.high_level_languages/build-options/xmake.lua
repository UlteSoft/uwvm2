-- Standalone header-mode adapter checks; this is not the full UWVM product.
set_project("uwvm-language-adapter-check")
set_languages("cxx23")
set_allowedmodes("debug", "release")
set_defaultmode("debug")
add_rules("mode.debug", "mode.release")
includes("../../../xmake/language_debug.lua")

option("matrix-expected-mask", function()
    -- A literal sentinel avoids Xmake's reserved auto/nil handling for
    -- explicitly supplied readonly command-line configurations.
    set_default("none")
    set_showmenu(true)
    set_description("Expected language bit mask: none or a decimal integer 0..32767.")
    after_check(function(option)
        local value = option:value()
        if value ~= "none" and
            (type(value) ~= "string" or not value:match("^%d+$") or
             not tonumber(value) or tonumber(value) > 32767) then
            raise("--matrix-expected-mask requires none or decimal 0..32767")
        end
    end)
end)

target("language_debug_probe")
    set_kind("binary")
    set_warnings("all", "extra", "error")
    add_files("language_debug_probe.cc")
    add_includedirs("../../../src")
    add_sysincludedirs("../../../third-parties/fast_io/include")
    uwvm_apply_language_debug_options()
    local expected = get_config("matrix-expected-mask")
    if expected and expected ~= "none" then
        add_defines("UWVM_TEST_EXPECT_MASK=" .. tostring(tonumber(expected)))
    end
target_end()
