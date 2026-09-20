-- Run: xmake lua test/0018.build/check_bundled_llvm_contract.lua
-- Synthetic CMake replies exercise the production reader without building or
-- executing LLVM. Keep archive ordering/repetition: static linkers need both.
function main()
    import("core.base.json")
    local modules = {rootdir = path.join(os.projectdir(), "xmake"), anonymous = true}
    local bundled = import("utility.bundled_llvm", modules)
    for crt, expected in pairs({MT = "MultiThreaded", MD = "MultiThreadedDLL",
        MTd = "MultiThreadedDebug", MDd = "MultiThreadedDebugDLL"}) do
        assert(bundled.msvc_runtime_library(crt) == expected)
    end
    local utility = import("utility.utility", modules)
    local fixture = path.join(os.tmpdir(), "uwvm-llvm-contract-" .. hash.uuid())
    local reply = path.join(fixture, ".cmake/api/v1/reply")
    os.mkdir(reply)
    local failures = 0
    local function must_fail(fragment, check)
        local rejected = false
        try {check, catch {function(err)
            rejected = tostring(err):find(fragment, 1, true) ~= nil
        end}}
        assert(rejected, "Expected failure: " .. fragment)
        failures = failures + 1
    end
    local function write(name, value) json.savefile(path.join(reply, name), value) end
    local function read() return bundled.read_contract(fixture) end
    must_fail("Missing bundled LLVM CMake", read)
    write("index-1.json", {reply = {["codemodel-v2"] = {jsonFile = "model.json"}}})
    local model = {configurations = {{name = "Release", targets = {{name = "uwvm_ros_llvm_contract", jsonFile = "contract.json"}}}}}
    write("model.json", model)
    local contract = {compileGroups = {{includes = {{path = fixture}}, defines = {{define = "PINNED"}}}},
        link = {language = "CXX", commandFragments = {{role = "libraries", fragment = "lib/libLLVMCore.a"}}}}
    write("contract.json", contract)
    local metadata = path.join(fixture, "uwvm-llvm-version.json")
    json.savefile(metadata, {version = "23.0.0", host_target = "wrong"})
    must_fail("version mismatch", read)
    json.savefile(metadata, {version = "23.1.1-uwvm-ros.6", host_target = "x86_64-unknown-linux-gnu"})
    assert(read().compile.defines[1].define == "PINNED")
    -- Directory/target compile options need not appear in CMAKE_CXX_FLAGS.
    -- Inspect CMake's resolved consumer command before any library build.
    contract.compileGroups[1].compileCommandFragments = {{fragment = "-O3 -ffast-math"}}
    write("contract.json", contract)
    must_fail("Unsafe floating-point", read)
    contract.compileGroups[1].compileCommandFragments = {{fragment = "-O3 -ffp-contract=off"}}
    write("contract.json", contract)
    assert(read().compile.defines[1].define == "PINNED")
    model.configurations[1].name = "Debug"
    write("model.json", model)
    must_fail("single Release configuration", read)
    model.configurations[1].name = "Release"
    table.insert(model.configurations, {targets = {}})
    write("model.json", model)
    must_fail("single Release configuration", read)
    table.remove(model.configurations)
    write("model.json", model)
    contract.link.language = "C"
    write("contract.json", contract)
    must_fail("consumer link contract", read)
    contract.link.language = "CXX"
    contract.link.commandFragments = nil
    write("contract.json", contract)
    must_fail("ordered link fragments", read)

    os.mkdir(path.join(fixture, "lib with spaces"))
    local archive = path.join(fixture, "lib with spaces/libLLVMCore.a")
    io.writefile(archive, "fixture, not an executable archive")
    local dependency = {build = fixture, link = {
        {role = "flags", fragment = "-O3 -DNDEBUG"},
        {role = "flags", fragment = "-pthread", backtrace = 1},
        {role = "libraries", fragment = "-Wl,--start-group"},
        {role = "libraries", fragment = os.args({path.relative(archive, fixture)})},
        {role = "libraries", fragment = os.args({path.relative(archive, fixture)})},
        {role = "libraries", fragment = "-Wl,--end-group -lm"}}}
    local args, count = utility.llvm_link_arguments(dependency)
    assert(count == 2 and #args == 6 and args[1] == "-pthread"
        and args[2] == "-Wl,--start-group" and args[3] == archive and args[4] == archive
        and args[5] == "-Wl,--end-group" and args[6] == "-lm")
    local roundtrip = os.argv(os.args(args))
    for i, arg in ipairs(args) do assert(roundtrip[i] == arg) end
    local function bad(fragment, expected)
        dependency.link = {{role = "libraries", fragment = fragment}}
        must_fail(expected, function() utility.llvm_link_arguments(dependency) end)
    end
    bad("-lLLVMCore", "never library search names")
    bad("-l:libLLVMCore.a", "never library search names")
    bad("libLLVM.so.23.1", "shared-library substitution")
    bad("../libLLVMCore.a", "escapes")
    bad("lib/libLLVMAbsent.a", "Missing bundled LLVM archive")
    bad("-lm", "Empty bundled LLVM")
    -- Delete only this uniquely created fixture, never a source/build tree.
    os.rm(fixture)
    print("PASS: CMake metadata, ordered static link contract and " .. failures .. " negative cases")
end
