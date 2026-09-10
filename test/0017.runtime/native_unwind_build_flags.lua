-- Run with: lua test/0017.runtime/native_unwind_build_flags.lua [repository-root]
-- This exercises the build policy only; it neither configures nor compiles UWVM.
local root = arg[1] or "."
local mode, backend, recorded
function get_config(name)
    if name == "execution-jit" then return backend end
    return nil
end
function is_mode(value) return mode == value end
function includes(_) end
function add_cxflags(value, options)
    recorded[#recorded + 1] = {value, options}
end
dofile(root .. "/xmake/platform/impl.lua")
for _, selected_mode in ipairs({"debug", "release", "releasedbg", "minsizerel"}) do
    for _, selected_backend in ipairs({"none", "default", "llvm"}) do
        mode, backend, recorded = selected_mode, selected_backend, {}
        uwvm_add_native_unwind_cxflags()
        if backend ~= "none" then
            assert(#recorded == 1 and recorded[1][1] == "-fasynchronous-unwind-tables")
            assert(recorded[1][2].force == true)
        elseif mode == "debug" then
            assert(#recorded == 0)
        else
            assert(#recorded == 2)
            assert(recorded[1][1] == "-fno-unwind-tables")
            assert(recorded[2][1] == "-fno-asynchronous-unwind-tables")
        end
    end
end
-- Native platform setup must not undo the policy later. Bare/wasm targets are
-- deliberately outside this policy and keep their existing build flags.
for _, platform in ipairs({"linux", "darwin", "bsd", "cygwin", "mingw", "windows"}) do
    local file = root .. "/xmake/platform/" .. platform .. ".lua"
    assert(loadfile(file))
    local stream = assert(io.open(file, "r"))
    local source = stream:read("*a")
    stream:close()
    local _, uses = source:gsub("uwvm_add_native_unwind_cxflags%(%)", "")
    assert(uses == 1, platform .. ": missing/duplicate shared unwind policy")
    assert(not source:find("-fno%-unwind%-tables"), platform .. ": overrides CFI")
    assert(not source:find("-fno%-asynchronous%-unwind%-tables"), platform .. ": overrides async CFI")
end
print("native unwind build flags: PASS (12 mode/backend combinations; 6 native platforms)")
