-- Run: xmake lua test/0018.build/check_bundled_llvm_manifest.lua
-- Fast packaging/negative tests; these do not build LLVM or alter vendor files.
function main()
    local project = os.projectdir()
    local bundled = import("utility.bundled_llvm", {rootdir = path.join(project, "xmake"), anonymous = true})
    local vendor = path.join(project, "third-parties/llvm")
    assert(#bundled.verify_source(vendor) == 64)
    local fixture = os.tmpdir() .. "/uwvm-llvm-manifest-" .. hash.uuid()
    os.mkdir(path.join(fixture, "cmake/Modules"))
    local version = path.join(fixture, "cmake/Modules/LLVMVersion.cmake")
    io.writefile(version, "set(LLVM_VERSION_MAJOR 23)\nset(LLVM_VERSION_MINOR 1)\nset(LLVM_VERSION_PATCH 1)\n")
    io.writefile(path.join(fixture, "sources.sha256"), hash.sha256(version) .. "  cmake/Modules/LLVMVersion.cmake\n")
    assert(#bundled.verify_source(fixture) == 64)
    local failures = 0
    local function must_fail(fragment, check)
        local rejected = false
        try {
            function()
                if check then check() else bundled.verify_source(fixture) end
            end,
            catch {function(err)
                rejected = tostring(err):find(fragment, 1, true) ~= nil
            end}
        }
        assert(rejected, "Expected failure: " .. fragment)
        failures = failures + 1
    end
    io.writefile(version, "wrong source\n")
    must_fail("source mismatch")
    io.writefile(path.join(fixture, "sources.sha256"), hash.sha256(version) .. "  cmake/Modules/LLVMVersion.cmake\n")
    must_fail("requires LLVM 23.1.1")
    io.writefile(path.join(fixture, "cmake/injected.cmake"), "unexpected source\n")
    must_fail("Unrecorded")
    io.writefile(path.join(fixture, "sources.sha256"), string.rep("0", 64) .. "  ../escape\n")
    must_fail("Malformed")
    local build = path.join(fixture, "build")
    os.mkdir(build)
    io.writefile(path.join(build, "CMakeCache.txt"), "CMAKE_HOME_DIRECTORY:INTERNAL=" .. path.join(fixture, "llvm") .. "\n")
    bundled.verify_build_directory(fixture, build)
    io.writefile(path.join(build, "CMakeCache.txt"), "CMAKE_HOME_DIRECTORY:INTERNAL=/another/checkout/llvm\n")
    must_fail("copied", function() bundled.verify_build_directory(fixture, build) end)
    bundled.verify_fp_flags("-O3 -DNDEBUG -ffp-contract=off")
    must_fail("Unsafe floating-point", function() bundled.verify_fp_flags("-O3 -ffast-math") end)
    must_fail("Unsafe floating-point", function() bundled.verify_fp_flags("/O2 /fp:fast") end)
    must_fail("Unsafe floating-point", function() bundled.verify_fp_flags("/clang:-ffast-math") end)
    must_fail("Unsafe floating-point", function() bundled.verify_fp_flags("-clang:-Ofast") end)
    must_fail("Unsafe floating-point", function() bundled.verify_fp_flags("-Xclang=-fno-signed-zeros") end)
    -- Only this uniquely created test fixture is removed. No build/source tree
    -- is passed to recursive deletion, including on malformed-manifest cases.
    os.rm(fixture)
    print("PASS: pinned source inventory and " .. failures .. " negative cases")
end
