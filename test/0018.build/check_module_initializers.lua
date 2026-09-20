-- xmake lua test/0018.build/check_module_initializers.lua /path/to/clang++ [reject]
-- "reject" is only a diagnostic negative control for a known-broken compiler;
-- normal builds call the same verifier without any rejection escape hatch.
function main(program, expected)
    assert(program, "Provide the bootstrap Clang executable")
    assert(not expected or expected == "reject", "Expected optional 'reject'")
    local checker = import("utility.module_initializers", {rootdir = path.join(os.projectdir(), "xmake"), anonymous = true})
    local rejected = false
    try {function() checker.verify_clang(program) end, catch {function(err)
        rejected = tostring(err):find("bootstrap Clang dropped a non-inline module initializer", 1, true) ~= nil
        if not rejected then raise(err) end
        print(tostring(err))
    end}}
    assert(rejected == (expected == "reject"), "Unexpected bootstrap module-initializer result")
    print(rejected and "PASS: known-broken bootstrap rejected" or "PASS: bootstrap preserves non-inline module initialization")
end
