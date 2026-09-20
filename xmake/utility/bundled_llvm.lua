-- ROS owns its LLVM dependency. A system llvm-config with a matching version
-- is NOT equivalent: the MIPS repair changes register allocation/spill semantics.
-- Keep compiler selection separate from dependency selection: an installed C++
-- compiler may bootstrap LLVM, but no installed LLVM headers/libraries are used.
import("core.project.config")
import("core.platform.platform")
import("core.tool.toolchain")
import("core.cache.memcache")
import("core.base.json")
import("lib.detect.find_tool")

local get_config = config.get

-- This is an exact official release, not a moving major-version constraint.
-- Keep the version header, CMake contract, provenance and regression runner in
-- sync when upgrading; see documents/toolchain/ros-llvm-maintenance.md. The ROS
-- suffix distinguishes generated native-object caches from unpatched upstream
-- builds. A new code-generation patch must not silently reuse the old suffix.
local release = "23.1.1"
local version = release .. "-uwvm-ros.6"

function msvc_runtime_library(crt)
    -- LLVM 23 removed LLVM_USE_CRT_RELEASE. Passing that unused option can
    -- silently leave Release LLVM on /MD while ROS uses /MDd or /MT[d]. The
    -- official CMake abstraction sets every newly created target's CRT/ABI.
    local values = {MT = "MultiThreaded", MD = "MultiThreadedDLL",
        MTd = "MultiThreadedDebug", MDd = "MultiThreadedDebugDLL"}
    return assert(values[crt], "Unsupported ROS MSVC runtime: " .. tostring(crt))
end

local function selected(name)
    local value = get_config(name)
    if value and value ~= "none" and value ~= "no" and value ~= "detect" and value ~= "default" then
        return value
    end
end

local function compiler(kind)
    local program = get_config(kind)
    if not program then
        if get_config("use-llvm-compiler") then
            local tc = toolchain.load("clang", {plat = get_config("plat"), arch = get_config("arch")})
            assert(tc:check(), "Cannot initialize the selected Clang bootstrap toolchain")
            program = tc:tool(kind)
        else
            program = platform.tool(kind)
        end
    end
    assert(program, "Cannot resolve the ROS bootstrap " .. kind .. " compiler")
    -- xmake permits toolname@/path syntax; CMake expects just the executable.
    program = program:gsub("^[%w_+%-]+@", "")
    local tool = assert(find_tool(kind == "cc" and "cc" or "c++", {program = program}),
        "Cannot locate the ROS bootstrap compiler: " .. program)
    return tool.program
end

local function flags(name)
    local value = get_config(name)
    return type(value) == "table" and table.concat(value, " ") or value or ""
end

function verify_source(root)
    -- Verify every retained upstream file, not just LLVMVersion.cmake. Without
    -- this, a stale/unpatched checkout can reuse a trusted build-cache identity.
    -- The manifest covers retained upstream subtrees and LICENSE.TXT; our
    -- provenance documents, inventory helpers and the manifest are not LLVM
    -- compilation inputs and are documented separately.
    local manifest = path.join(root, "sources.sha256")
    assert(os.isfile(manifest), "Missing bundled LLVM source manifest")
    local recorded = {}
    for line in io.lines(manifest) do
        local digest, relative = line:match("^([0-9a-f]+)  (.+)$")
        assert(digest and #digest == 64 and not relative:find("..", 1, true)
            and not path.is_absolute(relative), "Malformed bundled LLVM source manifest")
        local file = path.join(root, relative)
        assert(not recorded[relative], "Duplicate bundled LLVM source: " .. relative)
        recorded[relative] = true
        assert(os.isfile(file) and hash.sha256(file):lower() == digest,
            "Bundled LLVM source mismatch: " .. relative .. "; restore the pinned source/patch, never substitute system LLVM")
    end
    for _, subtree in ipairs({"cmake", "libc", "llvm", "third-party"}) do
        for _, file in ipairs(os.files(path.join(root, subtree, "**"))) do
            local relative = path.relative(file, root):gsub("\\", "/")
            assert(recorded[relative], "Unrecorded bundled LLVM source: " .. relative)
        end
    end
    local cmake_version = io.readfile(path.join(root, "cmake/Modules/LLVMVersion.cmake"))
    assert(cmake_version:find("set(LLVM_VERSION_MAJOR 23)", 1, true)
        and cmake_version:find("set(LLVM_VERSION_MINOR 1)", 1, true)
        and cmake_version:find("set(LLVM_VERSION_PATCH 1)", 1, true), "ROS requires LLVM 23.1.1")
    return hash.sha256(manifest):lower()
end

function read_contract(build)
    -- Use CMake's actual consumer target, not an inferred list of libLLVM*.a.
    -- Static dependencies have ordering/cycle requirements and vary by target.
    -- Querying a cross-built llvm-config would also require executing target
    -- code on the build host; the file API requires no target executable.
    local reply = path.join(build, ".cmake/api/v1/reply")
    local indexes = os.files(path.join(reply, "index-*.json"))
    table.sort(indexes)
    assert(#indexes > 0, "Missing bundled LLVM CMake file-API reply")
    local index = json.loadfile(indexes[#indexes])
    local entry = assert(index.reply["codemodel-v2"], "Missing LLVM codemodel-v2 reply")
    local model = json.loadfile(path.join(reply, entry.jsonFile))
    -- Our archive cache/Windows CRT policy describes Release LLVM even when
    -- ROS is a debug application. A cross toolchain can override CMake cache
    -- variables: one configuration is not sufficient if that one is Debug.
    assert(#model.configurations == 1 and model.configurations[1].name == "Release",
        "Bundled LLVM requires a single Release configuration")
    local contract
    for _, target in ipairs(model.configurations[1].targets) do
        if target.name == "uwvm_ros_llvm_contract" then
            contract = json.loadfile(path.join(reply, target.jsonFile))
            break
        end
    end
    assert(contract and contract.link and contract.link.language == "CXX", "Missing LLVM consumer link contract")
    assert(#contract.compileGroups == 1, "Unexpected LLVM consumer compile contract")
    -- add_compile_options()/target options may bypass CMAKE_CXX_FLAGS entirely.
    -- Check the resolved command too: LLVM's own floating-point computations
    -- must not inherit assumptions that NaNs/infinities/signed zero are absent.
    for _, fragment in ipairs(contract.compileGroups[1].compileCommandFragments or {}) do
        verify_fp_flags(fragment.fragment)
    end
    assert(contract.link.commandFragments and #contract.link.commandFragments > 0,
        "Missing LLVM ordered link fragments")
    local metadata = json.loadfile(path.join(build, "uwvm-llvm-version.json"))
    assert(metadata.version == version, "Bundled LLVM CMake version mismatch")
    metadata.compile = contract.compileGroups[1]
    metadata.link = contract.link.commandFragments
    metadata.build = build
    return metadata
end

function verify_build_directory(root, build)
    local cache = path.join(build, "CMakeCache.txt")
    if os.isfile(cache) then
        local recorded = io.readfile(cache):match("CMAKE_HOME_DIRECTORY:INTERNAL=([^\r\n]+)")
        -- A copied build tree can contain absolute paths to another checkout.
        -- Its sources are not the tree we just verified. Do not execute its
        -- Ninja rules or read its dependency contract, even if versions match.
        assert(recorded and path.normalize(recorded) == path.normalize(path.join(root, "llvm")),
            "Bundled LLVM build directory was copied from another source path; choose a fresh output directory with -o <builddir>")
    end
end

function canonical_build_directory(directory, cmake_program)
    -- The same cache can be reached through a symlink (e.g. header/module
    -- output directories sharing LLVM). Passing different -B spellings to
    -- CMake changes absolute include paths and Ninja command hashes, needlessly
    -- recompiling LLVM and making the exported contract depend on the caller.
    -- Resolve the existing directory before locking/configuring it. CMake's
    -- REAL_PATH handles platform path rules; do not change xmake's process CWD
    -- or depend on a Unix-only realpath executable during configuration.
    local helper = path.join(os.projectdir(), "xmake/llvm/real_build_directory.cmake")
    local physical = os.iorunv(cmake_program, {"-DUWVM_ROS_BUILD_DIRECTORY=" .. directory, "-P", helper})
    physical = physical:gsub("[\r\n]+$", "")
    assert(#physical > 0 and os.isdir(physical), "Cannot resolve bundled LLVM build directory")
    return path.normalize(physical)
end

function verify_fp_flags(value)
    -- LLVM 23's APFloat uses shared libc math helpers. Building the compiler
    -- itself under fast-math can change constant folding before Wasm's runtime
    -- FP guards ever run. Performance must come from legal optimizations, not
    -- assumptions that NaNs/infinities/signed zero cannot occur.
    local unsafe = { ["-ofast"] = true, ["-ffast-math"] = true,
        ["-ffp-model=fast"] = true, ["-ffp-model=aggressive"] = true,
        ["-ffinite-math-only"] = true, ["-funsafe-math-optimizations"] = true,
        ["-fassociative-math"] = true, ["-fno-signed-zeros"] = true,
        ["-freciprocal-math"] = true, ["-fno-honor-nans"] = true,
        ["-fno-honor-infinities"] = true, ["-fapprox-func"] = true, ["/fp:fast"] = true }
    for _, flag in ipairs(os.argv(value)) do
        -- clang-cl can forward a driver option with /clang: or -clang:.
        -- Inspect that option too; checking only the outer spelling would
        -- accept the same unsafe mode that the direct spelling rejects.
        local option = flag:lower():gsub("^/clang:", ""):gsub("^-clang:", ""):gsub("^-xclang=", "")
        assert(not unsafe[option], "Unsafe floating-point flag for bundled LLVM: " .. flag)
    end
end

function ensure()
    -- Configuration cannot change halfway through one xmake invocation. Still
    -- verify/rebuild on the next invocation; never persist this shortcut.
    -- Anonymous xmake imports can have separate Lua closures. Use process-only
    -- memcache so each target does not rehash 200+ MiB of LLVM and rerun Ninja.
    -- detectcache is deliberately NOT used here: it survives xmake invocations.
    local memo_key = "uwvm-ros.bundled-llvm:" .. os.projectdir() .. ":" .. config.builddir()
    local cached_tool = memcache.get("uwvm-ros.bundled-llvm", memo_key)
    if cached_tool then return cached_tool end
    local root = path.join(os.projectdir(), "third-parties/llvm")
    local source_id = verify_source(root)
    local cmake = assert(find_tool("cmake"), "Bundled LLVM requires CMake >= 3.20")
    local ninja = assert(find_tool("ninja"), "Bundled LLVM requires Ninja")
    local cc, cxx = compiler("cc"), compiler("cxx")
    local plat, arch = get_config("plat") or os.host(), get_config("arch") or os.arch()
    local cross_file = selected("llvm-cmake-toolchain")
    if cross_file then
        cross_file = path.absolute(cross_file, os.projectdir())
        assert(os.isfile(cross_file), "Missing --llvm-cmake-toolchain file")
    elseif plat ~= os.host() or arch ~= os.arch() or selected("target") or selected("llvm-target") then
        raise("Cross LLVM requires --llvm-cmake-toolchain; refusing to link a host/system LLVM into a different target")
    end
    local jobs = tonumber(get_config("llvm-build-jobs") or "2")
    assert(jobs and jobs >= 1 and jobs <= 16 and jobs == math.floor(jobs), "--llvm-build-jobs must be an integer in 1..16")
    local targets = get_config("llvm-build-targets") or "Native"
    assert(targets:match("^[%w_;]+$"), "Invalid --llvm-build-targets")
    local cflags = flags("cxflags") .. " " .. flags("cflags")
    local cxxflags = flags("cxflags") .. " " .. flags("cxxflags")
    verify_fp_flags(cflags)
    verify_fp_flags(cxxflags)
    local linkflags = flags("ldflags")
    if selected("stdlib") then
        cxxflags = cxxflags .. " -stdlib=" .. selected("stdlib")
        linkflags = linkflags .. " -stdlib=" .. selected("stdlib")
    end
    if selected("rtlib") then linkflags = linkflags .. " -rtlib=" .. selected("rtlib") end
    if selected("unwindlib") then
        linkflags = linkflags .. " -unwindlib=" .. selected("unwindlib"):gsub("^force%-", "")
    end
    -- This disk cache is for building the LLVM library, not the runtime's
    -- signed Wasm native-object cache. Both need compatible identities, but
    -- their lifetimes and trust boundaries are different. In particular these
    -- hashes detect accidental stale/substituted files, not hostile build tools.
    -- Avoid mixing compiler ABIs when the compiler is upgraded in place. The
    -- executable digest also covers wrappers whose version text is unchanged.
    local settings = {source_id, version, cc, cxx, hash.sha256(cc), hash.sha256(cxx),
        plat, arch, targets, cflags, cxxflags, linkflags, selected("sysroot") or "",
        -- Retain the empty former runner slot so removing llvm-config does not
        -- invalidate otherwise identical, verified native archive directories.
        cross_file or "", cross_file and hash.sha256(cross_file) or "", "",
        os.getenv("SDKROOT") or "", os.getenv("MACOSX_DEPLOYMENT_TARGET") or ""}
    local crt
    if plat == "windows" then
        -- The LLVM C++ API crosses allocator/standard-library boundaries.
        -- Match ROS's Windows CRT even though LLVM itself is optimized Release;
        -- MD/MT and debug iterator ABIs must not share an archive cache entry.
        crt = get_config("static") == "compiler" and "MT" or "MD"
        if get_config("mode") == "debug" then crt = crt .. "d" end
        table.insert(settings, crt)
    end
    -- hash.sha256 accepts a file or bytes; write an auditable configuration file
    -- under the build directory instead of treating a string as a file name.
    local base = path.absolute(path.join(config.builddir(), "bundled-llvm"), os.projectdir())
    os.mkdir(base)
    local settings_text = table.concat(settings, "\n") .. "\n"
    local temp = os.tmpfile()
    io.writefile(temp, settings_text)
    local key = hash.sha256(temp):lower()
    os.rm(temp)
    local build = path.join(base, key)
    os.mkdir(build)
    build = canonical_build_directory(build, cmake.program)
    local lock = io.openlock(path.join(build, "build.lock"))
    -- Configure/build/metadata publication are one transaction. Independent
    -- xmake processes must not observe a half-written CMake reply or race Ninja
    -- in this directory. Process-local memcache alone cannot serialize them.
    lock:lock()
    local result
    try {
        function()
            verify_build_directory(root, build)
            local stamp = path.join(build, "complete.txt")
            local contract_source = path.join(os.projectdir(), "xmake/llvm/contract.cmake")
            local contract_stub = path.join(os.projectdir(), "xmake/llvm/contract.cpp")
            local query = path.join(build, ".cmake/api/v1/query/codemodel-v2")
            os.mkdir(path.directory(query))
            -- Touching the query every invocation would force CMake to
            -- regenerate its reply and undo the cheap incremental build path.
            if not os.isfile(query) then io.writefile(query, "") end
            -- on_load/after_check call us more than once. Ninja remains the
            -- authority on missing outputs; a stamp alone must not hide deletion
            -- of an archive or generated header. No --clean-first or source writes.
            -- LLVM Release optimizes the host compiler implementation; it does
            -- NOT select the guest Wasm optimization policy. ROS still chooses
            -- O0/O1/O2/O3 per compilation. Disable optional host-package probes
            -- for reproducibility, and keep LLVM static without forcing libc++,
            -- libunwind or the application's other system libraries static.
            local args = {"-S", path.join(root, "llvm"), "-B", build, "-G", "Ninja",
                "-DCMAKE_MAKE_PROGRAM=" .. ninja.program, "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_PROJECT_LLVM_INCLUDE=" .. contract_source,
                "-DUWVM_ROS_REQUIRE_EXPLICIT_HOST_TRIPLE=" .. (cross_file and "ON" or "OFF"),
                "-DCMAKE_C_COMPILER=" .. cc, "-DCMAKE_CXX_COMPILER=" .. cxx,
                "-DCMAKE_C_FLAGS=" .. cflags, "-DCMAKE_CXX_FLAGS=" .. cxxflags,
                "-DCMAKE_EXE_LINKER_FLAGS=" .. linkflags, "-DCMAKE_SHARED_LINKER_FLAGS=" .. linkflags,
                "-DCMAKE_POSITION_INDEPENDENT_CODE=ON", "-DLLVM_TARGETS_TO_BUILD=" .. targets,
                "-DLLVM_VERSION_SUFFIX=-uwvm-ros.6", "-DLLVM_APPEND_VC_REV=OFF",
                "-DLLVM_ENABLE_PROJECTS=", "-DLLVM_ENABLE_RUNTIMES=",
                "-DLLVM_INCLUDE_TESTS=OFF", "-DLLVM_INCLUDE_BENCHMARKS=OFF", "-DLLVM_INCLUDE_EXAMPLES=OFF",
                "-DLLVM_INCLUDE_DOCS=OFF", "-DLLVM_ENABLE_BINDINGS=OFF", "-DLLVM_BUILD_TOOLS=OFF",
                "-DLLVM_TOOL_LLVM_CONFIG_BUILD=OFF",
                "-DLLVM_BUILD_UTILS=OFF", "-DLLVM_BUILD_LLVM_DYLIB=OFF", "-DLLVM_LINK_LLVM_DYLIB=OFF",
                "-DBUILD_SHARED_LIBS=OFF", "-DLLVM_ENABLE_ZLIB=OFF", "-DLLVM_ENABLE_ZSTD=OFF",
                "-DLLVM_ENABLE_LIBXML2=OFF", "-DLLVM_ENABLE_CURL=OFF", "-DLLVM_ENABLE_LIBEDIT=OFF",
                "-DLLVM_ENABLE_ASSERTIONS=OFF", "-DLLVM_ENABLE_EH=OFF", "-DLLVM_ENABLE_RTTI=OFF",
                "-DLLVM_ENABLE_LTO=OFF", "-DLLVM_PARALLEL_LINK_JOBS=1",
                "-DLLVM_PARALLEL_COMPILE_JOBS=" .. tostring(jobs)}
            if crt then
                local runtime = msvc_runtime_library(crt)
                -- Remove the obsolete cached request too; its mere presence
                -- is not evidence of the CRT used by the compiled archives.
                table.insert(args, "-ULLVM_USE_CRT_RELEASE")
                table.insert(args, "-DCMAKE_MSVC_RUNTIME_LIBRARY=" .. runtime)
                -- The deferred hook verifies actual library target properties,
                -- so a toolchain override cannot silently select another CRT.
                table.insert(args, "-DUWVM_ROS_MSVC_RUNTIME=" .. runtime)
            end
            if cross_file then
                -- The explicit requirement above also covers toolchains which
                -- omit CMAKE_SYSTEM_NAME: CMake may then report a native build
                -- even though the selected compiler emits another ISA.
                -- LLVM's default host triple comes from config.guess, which
                -- observes the build host even with a genuine cross compiler.
                -- Clear old guesses before the toolchain is read; merely
                -- checking DEFINED would trust a stale auto-populated cache.
                -- The toolchain must set LLVM_HOST_TRIPLE itself. A missing
                -- LLVM_DEFAULT_TARGET_TRIPLE then derives from that value.
                table.insert(args, "-ULLVM_HOST_TRIPLE")
                table.insert(args, "-ULLVM_DEFAULT_TARGET_TRIPLE")
                table.insert(args, "-DCMAKE_TOOLCHAIN_FILE=" .. cross_file)
            end
            if selected("sysroot") then table.insert(args, "-DCMAKE_SYSROOT=" .. path.absolute(selected("sysroot"))) end
            local recipe = hash.sha256(path.join(os.projectdir(), "xmake/utility/bundled_llvm.lua")) .. "\n"
                .. hash.sha256(contract_source) .. "\n" .. hash.sha256(contract_stub) .. "\n"
                .. hash.sha256(path.join(os.projectdir(), "xmake/llvm/real_build_directory.cmake"))
            local stamp_text = key .. "\n" .. recipe .. "\n"
            -- Source/compiler/ABI changes select a different directory. Recipe
            -- changes reconfigure this one, allowing Ninja to reuse unaffected
            -- objects; neither stamp replaces source verification or Ninja.
            if not os.isfile(path.join(build, "CMakeCache.txt")) or not os.isfile(stamp)
                or io.readfile(stamp) ~= stamp_text or not os.isfile(path.join(build, "uwvm-llvm-version.json"))
                or #os.files(path.join(build, ".cmake/api/v1/reply/index-*.json")) == 0 then
                cprint("Building ROS bundled LLVM %s from verified sources (%s)", version, key:sub(1, 12))
                io.writefile(path.join(build, "settings.txt"), settings_text)
                os.vrunv(cmake.program, args)
            end
            -- Ninja may need to rerun CMake after an included toolchain file
            -- changes, even when our own recipe/stamp is unchanged. Refresh
            -- only its build-system target before checking the resolved flags;
            -- otherwise the check can inspect an old reply and the subsequent
            -- library build can regenerate an unsafe configuration behind it.
            -- The generator is explicitly Ninja, so build.ninja is available;
            -- this step builds no LLVM archive or metadata consumer executable.
            os.vrunv(cmake.program, {"--build", build, "--parallel", tostring(jobs), "--target", "build.ninja"})
            -- Also inspect CMake's resolved flags: a cross toolchain can add
            -- flags independently of xmake's command-line configuration.
            for line in io.lines(path.join(build, "CMakeCache.txt")) do
                local value = line:match("^CMAKE_C[_X]*_FLAGS[^:]*:STRING=(.*)$")
                if value then verify_fp_flags(value) end
            end
            -- Configuration already produced the file-API contract. Reject
            -- unsafe resolved flags before spending resources building LLVM,
            -- not only after potentially miscompiled archives have been made.
            result = read_contract(build)
            -- Only real library targets are built. The metadata-only consumer
            -- target and llvm-config executable are never built or executed.
            os.vrunv(cmake.program, {"--build", build, "--parallel", tostring(jobs), "--target", "llvm-libraries"})
            -- Read again after the build as well; never publish a stale reply
            -- if the build system regenerated during its dependency checks.
            result = read_contract(build)
            result.identity = key
            result.bootstrap_prefix = path.directory(path.directory(cxx))
            result.stdlib = selected("stdlib")
            for _, fragment in ipairs(result.compile.compileCommandFragments or {}) do
                for _, flag in ipairs(os.argv(fragment.fragment)) do
                    if flag:startswith("-stdlib=") then
                        local actual = flag:sub(9)
                        assert(not result.stdlib or result.stdlib == actual, "Bundled LLVM C++ standard library mismatch")
                        result.stdlib = actual
                    end
                end
            end
            io.writefile(stamp, stamp_text)
        end,
        finally {function(ok, err)
            lock:unlock()
            lock:close()
            if not ok then raise(err) end
        end}
    }
    memcache.set("uwvm-ros.bundled-llvm", memo_key, result)
    return result
end
