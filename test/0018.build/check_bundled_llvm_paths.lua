-- Run: xmake lua test/0018.build/check_bundled_llvm_paths.lua
-- Exercise the real CMake path resolver, including spaces and relative links.
-- No LLVM compilation or filesystem-wide cleanup is performed.
function main()
    import("lib.detect.find_tool")
    local bundled = import("utility.bundled_llvm", {rootdir = path.join(os.projectdir(), "xmake"), anonymous = true})
    local cmake = assert(find_tool("cmake"), "CMake is required")
    local fixture = path.join(os.tmpdir(), "uwvm-llvm-paths-" .. hash.uuid())
    local real = path.join(fixture, "real cache with spaces")
    os.mkdir(path.join(real, "key"))
    os.mkdir(path.join(fixture, "aliases"))
    os.ln(real, path.join(fixture, "absolute alias"))
    os.ln("../real cache with spaces", path.join(fixture, "aliases/relative alias"))
    local cwd = os.curdir()
    local function resolve(directory) return bundled.canonical_build_directory(directory, cmake.program) end
    local expected = resolve(path.join(real, "key"))
    assert(resolve(path.join(fixture, "absolute alias/key")) == expected)
    assert(resolve(path.join(fixture, "aliases/relative alias/key")) == expected)
    assert(resolve(path.join(real, "key/../key")) == expected)
    assert(os.curdir() == cwd, "Path resolver changed xmake's working directory")
    local rejected = false
    try {function() resolve(path.join(fixture, "missing")) end, catch {function(err)
        rejected = tostring(err):find("Missing bundled LLVM build directory", 1, true) ~= nil
    end}}
    assert(rejected, "Missing cache directory must not be silently created")
    -- Only this uniquely created test fixture is removed; aliases point inside it.
    os.rm(fixture)
    print("PASS: canonical cache paths, space/relative aliases, unchanged CWD and missing-directory rejection")
end
