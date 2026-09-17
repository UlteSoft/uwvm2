-- A linked, pinned LLVM library does not repair the compiler used to build
-- the C++ VM itself. Some Clang 23 snapshots lose non-inline internal dynamic
-- initialization in named modules (llvm/llvm-project#212170, fixed by #218304).
-- Testing an exported variable misses this bug; [[gnu::used]] is not a cure.
-- Fail closed on the actual BMI -> IR path. No target program is executed, so
-- this check works for cross compilers and needs neither target libc nor QEMU.
import("core.cache.memcache")

function verify_clang(program)
    local cache = "uwvm.module-initializers"
    if memcache.get(cache, program) then return end
    local directory = path.join(os.tmpdir(), "uwvm-module-initializers-" .. hash.uuid())
    os.mkdir(directory)
    local source = path.join(directory, "probe.cppm")
    local bmi = path.join(directory, "probe.pcm")
    local ir = path.join(directory, "probe.ll")
    io.writefile(source, [[module;
extern "C" void uwvm_module_initializer_probe() noexcept;
export module uwvm_module_initializer_check;
namespace { int const marker = (uwvm_module_initializer_probe(), 0); }
export int anchor() { return 42; }
]])
    -- Select the GNU-style driver grammar even for clang-cl. This source has
    -- no headers, target library dependencies or architecture-specific code;
    -- the failure concerns Sema/module serialization, not target execution.
    local ok, failure = false, nil
    try { function()
        os.vrunv(program, {"--driver-mode=g++", "-std=c++20", "--precompile", source, "-o", bmi})
        os.vrunv(program, {"--driver-mode=g++", "-std=c++20", "-O0", "-S", "-emit-llvm", bmi, "-o", ir})
        local code = io.readfile(ir)
        -- A declaration alone proves nothing: require a call in generated IR.
        assert(code and code:match("call[^\n]*@uwvm_module_initializer_probe%("),
            "bootstrap Clang dropped a non-inline module initializer")
        ok = true
    end, catch {function(err) failure = err end} }
    if not ok then
        raise("Named-module compiler check failed: %s\nUse a bootstrap Clang containing " ..
              "https://github.com/llvm/llvm-project/pull/218304, or build with --use-cxx-module=n. " ..
              "The bundled LLVM library cannot fix the bootstrap compiler. Probe retained at %s",
              tostring(failure), directory)
    end
    -- Successful probe artifacts are disposable and confined to this new dir.
    os.rm(directory)
    -- Process-only: a compiler upgraded in place must be checked next time.
    memcache.set(cache, program, true)
end

function check_target(target)
    if target:has_tool("cxx", "clang", "clangxx", "clang_cl") then
        verify_clang(target:compiler("cxx"):program())
    end
end
