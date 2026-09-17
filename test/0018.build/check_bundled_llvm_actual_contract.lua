-- Read an actual configure-only CMake reply with the production parser.
-- No LLVM build or target program is run; an optional second argument is the
-- required rejection diagnostic, not permission to accept arbitrary failures.
function main(build, expected_error)
    assert(build and os.isdir(build), "Provide a real CMake build directory")
    local bundled = import("utility.bundled_llvm", {rootdir = path.join(os.projectdir(), "xmake"), anonymous = true})
    if not expected_error then
        assert(bundled.read_contract(build).version == "23.1.1-uwvm-ros.6")
    else
        local rejected = false
        try {function() bundled.read_contract(build) end, catch {function(err)
            rejected = tostring(err):find(expected_error, 1, true) ~= nil
        end}}
        assert(rejected, "Expected actual CMake contract rejection: " .. expected_error)
    end
    print("PASS actual CMake contract: " .. (expected_error or "valid Release"))
end
