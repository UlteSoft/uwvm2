-- ROS owns its LLVM dependency. A system llvm-config with a matching version
-- is NOT equivalent: the MIPS repair changes register allocation/spill semantics.
-- Keep compiler selection separate from dependency selection: an installed C++
-- compiler may bootstrap LLVM, but no installed LLVM headers/libraries are used.
import("core.project.config")
import("core.base.option")
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
local version = release .. "-uwvm-ros.13"

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
    if kind == "cxx" and path.filename(program) == "clang" then
        -- xmake's generic Clang toolchain may report the C driver for both
        -- languages during configuration, before the saved cxx override is
        -- visible. clang compiles .cpp files but does not link libc++ for
        -- LLVM's host TableGen tools; use the sibling C++ driver instead.
        local cxx_driver = find_tool("c++", {program = program .. "++"})
        assert(cxx_driver, "Cannot locate the C++ driver beside ROS bootstrap clang: " .. program)
        program = cxx_driver.program
    end
    local tool = assert(find_tool(kind == "cc" and "cc" or "c++", {program = program}),
        "Cannot locate the ROS bootstrap compiler: " .. program)
    return tool.program
end

local function flags(name)
    local value = get_config(name)
    return type(value) == "table" and table.concat(value, " ") or value or ""
end

function resolve_build_jobs()
    local value = get_config("llvm-build-jobs")
    if value == nil then value = "inherit" end
    if value == "inherit" then
        value = option.get("jobs")
        if value == nil then
            -- xmake build enters a nested config task without inheriting jobs.
            -- Reparse the original build command with Xmake's own menu/parser;
            -- do not read private option contexts or mistake another flag's value for -j.
            local argv = table.clone(xmake.argv())
            local explicit_build = argv[1] == "build" or argv[1] == "b"
            if explicit_build then table.remove(argv, 1) end
            if explicit_build or not argv[1] or argv[1]:startswith("-") then
                local menu = assert(option.taskmenu("build"), "Missing Xmake build menu")
                value = option.raw_parse(argv, menu.options, {populate_defaults = true}).jobs
            end
        end
        -- Standalone xmake f has no outer build concurrency to inherit.
        if value == nil then value = 1 end
    end
    local jobs = tonumber(value)
    assert(jobs and jobs >= 1 and jobs <= 2147483647 and jobs == math.floor(jobs),
        "--llvm-build-jobs / xmake --jobs must be a positive integer (or llvm-build-jobs=inherit)")
    return jobs
end

function build_targets(cmake_program, build, jobs, targets, environment)
    local args = {"--build", build, "--parallel", tostring(jobs), "--target"}
    table.join2(args, targets)
    os.vrunv(cmake_program, args, environment)
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
    local generated_header = path.join(build, "include/llvm/Config/llvm-config.h")
    local generated_version = false
    if os.isfile(generated_header) then
        for line in io.readfile(generated_header):gmatch("[^\r\n]+") do
            if line == '#define LLVM_VERSION_STRING "' .. version .. '"' then
                generated_version = true
                break
            end
        end
    end
    assert(generated_version, "Bundled LLVM generated version header does not match " .. version)
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
    -- A Windows qualification run may reuse a separately built ros.10 target
    -- LLVM. This explicit path is never inferred from PATH or llvm-config.
    -- The normal source-owned CMake/Ninja path below remains the default.
    local prebuilt = os.getenv("UWVM_ROS_QUALIFIED_PREBUILT_LLVM")
    if prebuilt and prebuilt ~= "" then
        -- Existing certificates bind a bootstrap runtime, not the new paired
        -- source provider. Require an explicit platform selection for them.
        local runtimes_builder = import("utility.bundled_runtimes", {anonymous = true})
        assert(not runtimes_builder.enabled(), "Prebuilt LLVM has no paired ROS runtimes qualification")
        assert(get_config("plat") == "mingw" and get_config("arch") == "x86_64",
            "Qualified prebuilt ROS LLVM is supported only for Windows x64")
        prebuilt = path.absolute(prebuilt)
        local source_id = verify_source(root)
        local certificate_path = path.join(prebuilt, "qualified-llvm.json")
        assert(os.isfile(certificate_path),
            "Missing qualified Windows LLVM archive manifest; run tools/ci/qualify_windows_ros_llvm_prebuilt.py")
        local certificate = json.loadfile(certificate_path)
        local target = "x86_64-w64-windows-gnu"
        assert(certificate.schema == 1 and certificate.llvm_version == version and
            certificate.host_target == target and certificate.source_manifest_sha256 == source_id,
            "Qualified Windows LLVM manifest does not match this ROS source/target/version")
        local cache_path = path.join(prebuilt, "CMakeCache.txt")
        assert(os.isfile(cache_path) and hash.sha256(cache_path):lower() == certificate.cmake_cache_sha256,
            "Qualified Windows LLVM CMake cache changed")
        local cache = io.readfile(cache_path)
        local function cache_value(name)
            return cache:match("\n" .. name .. ":[^=]*=([^\r\n]+)")
                or cache:match("^" .. name .. ":[^=]*=([^\r\n]+)")
        end
        assert(cache_value("CMAKE_BUILD_TYPE") == "Release" and
            cache_value("CMAKE_SYSTEM_NAME") == "Windows" and
            cache_value("CMAKE_C_COMPILER_TARGET") == target and
            cache_value("CMAKE_CXX_COMPILER_TARGET") == target and
            cache_value("CMAKE_ASM_COMPILER_TARGET") == target and
            cache_value("LLVM_HOST_TRIPLE") == target and
            cache_value("LLVM_VERSION_SUFFIX") == "-uwvm-ros.13" and
            cache_value("BUILD_SHARED_LIBS") == "OFF" and
            cache_value("LLVM_ENABLE_EH") == "OFF" and
            cache_value("LLVM_ENABLE_RTTI") == "OFF" and
            (";" .. (cache_value("LLVM_TARGETS_TO_BUILD") or "") .. ";"):find(";X86;", 1, true),
            "Qualified Windows LLVM CMake target/ABI differs from ROS x64")
        local recorded = cache_value("CMAKE_HOME_DIRECTORY")
        assert(recorded and path.filename(recorded) == "llvm", "Missing qualified LLVM source identity")
        local original_root = path.directory(recorded)
        assert(verify_source(original_root) == source_id,
            "Qualified Windows LLVM was built from different or modified ROS sources")
        local version_header = path.join(prebuilt, "include/llvm/Config/llvm-config.h")
        assert(os.isfile(version_header) and
            hash.sha256(version_header):lower() == certificate.generated_version_header_sha256 and
            io.readfile(version_header):find('#define LLVM_VERSION_STRING "' .. version .. '"', 1, true),
            "Qualified Windows LLVM generated version header changed or is not ros.13")
        local response = path.join(prebuilt, "consumer-link.rsp")
        assert(os.isfile(response) and hash.sha256(response):lower() == certificate.consumer_link_sha256,
            "Qualified Windows LLVM link response changed")
        local result = read_contract(prebuilt)
        assert(result.version == version and result.host_target == target,
            "Qualified Windows LLVM CMake contract has the wrong target/version")
        assert(type(certificate.link_fragments) == "table" and
            #certificate.link_fragments == #result.link and type(certificate.archives) == "table",
            "Qualified Windows LLVM link contract changed")
        local linked = {}
        local count = 0
        for index, fragment in ipairs(result.link) do
            local recorded_fragment = certificate.link_fragments[index]
            assert(type(recorded_fragment) == "table" and
                recorded_fragment.role == (fragment.role or "") and
                recorded_fragment.fragment == fragment.fragment,
                "Qualified Windows LLVM CMake link fragment changed")
            local name = fragment.fragment
            if fragment.role == "libraries" and name:match("%.a$") and not linked[name] then
                assert(name:match("^lib/libLLVM[%w_]+%.a$"),
                    "Qualified Windows LLVM archive escapes its library directory")
                local archive = path.join(prebuilt, name)
                local entry = certificate.archives[name]
                assert(type(entry) == "table" and os.isfile(archive) and
                    hash.sha256(archive):lower() == entry.sha256,
                    "Qualified Windows LLVM archive changed: " .. name)
                linked[name] = true
                count = count + 1
            end
        end
        assert(count == certificate.archive_count and count >= 40 and
            linked["lib/libLLVMX86CodeGen.a"], "Qualified Windows LLVM archive closure is incomplete")
        assert(linked["lib/libLLVMDebugInfoDWARF.a"],
            "Qualified Windows LLVM lacks the embedded source debugger's DWARF library; regenerate its CMake consumer contract and qualification")
        for name in pairs(certificate.archives) do
            assert(linked[name], "Qualified Windows LLVM manifest contains an unlinked archive")
        end
        result.identity = source_id .. ":" .. hash.sha256(certificate_path):lower()
        result.bootstrap_prefix = path.directory(path.directory(compiler("cxx")))
        result.stdlib = "libc++"
        memcache.set("uwvm-ros.bundled-llvm", memo_key, result)
        return result
    end
    local source_id = verify_source(root)
    local cmake = assert(find_tool("cmake"), "Bundled LLVM requires CMake >= 3.20")
    local cmake_environment = {envs = {PYTHONDONTWRITEBYTECODE = "1"}}
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
    local jobs = resolve_build_jobs()
    local targets = get_config("llvm-build-targets") or "Native"
    assert(targets:match("^[%w_;]+$"), "Invalid --llvm-build-targets")
    local cflags = flags("cxflags") .. " " .. flags("cflags")
    local cxxflags = flags("cxflags") .. " " .. flags("cxxflags")
    local runtimes_builder = import("utility.bundled_runtimes", {anonymous = true})
    local bundled_runtime_selected = runtimes_builder.enabled()
    local macos_libcxx, macos_libcxxabi
    if not bundled_runtime_selected and plat == "macosx" and selected("sysroot") then
        local sdk = path.absolute(selected("sysroot"))
        local libcxx = path.join(sdk, "usr", "lib", "libc++.a")
        local libcxxabi = path.join(sdk, "usr", "lib", "libc++abi.a")
        if os.isfile(libcxx) and os.isfile(libcxxabi) then
            macos_libcxx, macos_libcxxabi = libcxx, libcxxabi
            -- This paired static libc++abi aborts if typed operator new is
            -- called by LLVM's global constructors before libc++ initialization.
            -- Keep this compiler option in the settings hash so host tools and
            -- archives cannot reuse objects built with the unsafe ABI mode.
            cxxflags = cxxflags .. " -fno-typed-cxx-new-delete"
        end
    end
    verify_fp_flags(cflags)
    verify_fp_flags(cxxflags)
    local linkflags = flags("ldflags")
    if plat == "macosx" and get_config("use-llvm-compiler") and
       not linkflags:find("-fuse-ld=lld", 1, true) then
        -- xmake/platform/darwin.lua selects lld for the ROS executable, but
        -- the bundled LLVM CMake build does not inherit target link flags.
        -- Apple's ld cannot read newer compiler-rt bitcode from the selected
        -- LLVM bootstrap toolchain. Keep host tools on the same linker.
        linkflags = linkflags .. " -fuse-ld=lld"
    end
    if not bundled_runtime_selected and selected("stdlib") then
        cxxflags = cxxflags .. " -stdlib=" .. selected("stdlib")
        linkflags = linkflags .. " -stdlib=" .. selected("stdlib")
    end
    if selected("rtlib") then linkflags = linkflags .. " -rtlib=" .. selected("rtlib") end
    if not bundled_runtime_selected and selected("unwindlib") then
        linkflags = linkflags .. " -unwindlib=" .. selected("unwindlib"):gsub("^force%-", "")
    end
    -- Build one paired static C++ ABI/unwinder before LLVM. LLVM's generated
    -- host tools and ROS then consume identical generated headers and archives;
    -- bootstrap compiler/libc/sysroot discovery stays independent.
    local runtimes = runtimes_builder.ensure({cc=cc, cxx=cxx, plat=plat, arch=arch,
        cflags=cflags, cxxflags=cxxflags, linkflags=linkflags, jobs=jobs, cross_file=cross_file})
    if runtimes then
        cflags = cflags .. " " .. os.args({"-isystem", runtimes.unwind_include})
        cxxflags = cxxflags .. " -nostdinc++ " .. os.args({"-isystem", runtimes.include,
            "-isystem", runtimes.abi_include, "-isystem", runtimes.unwind_include})
        linkflags = linkflags .. " -nostdlib++ --unwindlib=none"
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
    if runtimes then
        table.insert(settings, runtimes.identity)
        table.insert(settings, runtimes.config_site_sha256)
        table.insert(settings, runtimes.link_flags)
        for _, name in ipairs({"libc++.a", "libc++abi.a", "libunwind.a"}) do
            table.insert(settings, hash.sha256(runtimes.archives[name]))
        end
    end
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
            -- for reproducibility. A supported bundled-runtime provider is
            -- separately built and paired static; libc/sysroot stays external.
            local args = {"-S", path.join(root, "llvm"), "-B", build, "-G", "Ninja",
                "-DCMAKE_MAKE_PROGRAM=" .. ninja.program, "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_PROJECT_LLVM_INCLUDE=" .. contract_source,
                "-DUWVM_ROS_REQUIRE_EXPLICIT_HOST_TRIPLE=" .. (cross_file and "ON" or "OFF"),
                "-DUWVM_ROS_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT=" .. (get_config("macos-x64-native-step") and "ON" or "OFF"),
                "-DCMAKE_C_COMPILER=" .. cc, "-DCMAKE_CXX_COMPILER=" .. cxx,
                "-DCMAKE_C_FLAGS=" .. cflags, "-DCMAKE_CXX_FLAGS=" .. cxxflags,
                "-DCMAKE_EXE_LINKER_FLAGS=" .. linkflags, "-DCMAKE_SHARED_LINKER_FLAGS=" .. linkflags,
                "-DCMAKE_POSITION_INDEPENDENT_CODE=ON", "-DLLVM_TARGETS_TO_BUILD=" .. targets,
                "-DLLVM_VERSION_SUFFIX=-uwvm-ros.13", "-DLLVM_APPEND_VC_REV=OFF",
                "-DLLVM_ENABLE_PROJECTS=", "-DLLVM_ENABLE_RUNTIMES=",
                "-DLLVM_INCLUDE_TESTS=OFF", "-DLLVM_INCLUDE_BENCHMARKS=OFF", "-DLLVM_INCLUDE_EXAMPLES=OFF",
                "-DLLVM_INCLUDE_DOCS=OFF", "-DLLVM_ENABLE_BINDINGS=OFF", "-DLLVM_BUILD_TOOLS=OFF",
                "-DLLVM_TOOL_LLVM_CONFIG_BUILD=OFF",
                "-DLLVM_BUILD_UTILS=OFF", "-DLLVM_BUILD_LLVM_DYLIB=OFF", "-DLLVM_LINK_LLVM_DYLIB=OFF",
                "-DBUILD_SHARED_LIBS=OFF", "-DLLVM_ENABLE_ZLIB=OFF", "-DLLVM_ENABLE_ZSTD=OFF",
                "-DLLVM_ENABLE_LIBXML2=OFF", "-DLLVM_ENABLE_CURL=OFF", "-DLLVM_ENABLE_LIBEDIT=OFF",
                "-DLLVM_ENABLE_ASSERTIONS=OFF", "-DLLVM_ENABLE_EH=OFF", "-DLLVM_ENABLE_RTTI=OFF",
                -- Keep the pinned source's namespace lookup independent of
                -- bootstrap PCH serialization (some Clang builds lose the
                -- PatternMatchHelpers using-directive from LLVMCore's PCH).
                -- This also bounds disk/memory use during serial builds.
                "-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON",
                "-DLLVM_ENABLE_LTO=OFF", "-DLLVM_PARALLEL_LINK_JOBS=1",
                -- The invocation's --parallel owns compile concurrency. A
                -- cached Ninja compile pool must not pin later -j changes.
                "-DLLVM_PARALLEL_COMPILE_JOBS="}
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
                if plat == "mingw" and arch == "x86_64" then
                    -- CMake detects ASM separately from C/C++. LLVM's
                    -- BLAKE3 Windows .S files otherwise become host ELF
                    -- objects inside an otherwise COFF static archive.
                    -- Supply the target on every clean or incremental
                    -- configure; the cache check below catches a toolchain
                    -- that overrides this with another architecture.
                    table.insert(args, "-DCMAKE_ASM_COMPILER_TARGET=x86_64-w64-windows-gnu")
                end
                table.insert(args, "-DCMAKE_TOOLCHAIN_FILE=" .. cross_file)
            end
            if selected("sysroot") then
                local sysroot = path.absolute(selected("sysroot"))
                table.insert(args, "-DCMAKE_SYSROOT=" .. sysroot)
                if plat == "macosx" then table.insert(args, "-DCMAKE_OSX_SYSROOT=" .. sysroot) end
            end
            if runtimes then
                -- CMake puts standard libraries after LLVM host-tool objects.
                -- A driver implicit library or an early global flag cannot
                -- substitute a dynamic libc++abi/unwinder into this closure.
                table.insert(args, "-DCMAKE_CXX_STANDARD_LIBRARIES=" .. runtimes.link_flags)
                -- LLVM probes EH registration with C, not C++. Link those real
                -- probes to this same provider instead of accidentally detecting
                -- libgcc/libSystem and later resolving another dynamic unwinder.
                -- Linker forwarding options have positional operands. Passing
                -- them as REQUIRED_LIBRARIES converts symbols to -l names and
                -- de-duplicates -Xlinker. Preserve the ordered driver group as
                -- link options, with actual runtime archives after probe objects.
                local required = {}
                for _, name in ipairs({"libc++.a", "libc++abi.a", "libunwind.a"}) do
                    table.insert(required, runtimes.archives[name])
                end
                table.insert(args, "-DCMAKE_REQUIRED_LIBRARIES=" .. table.concat(required, ";"))
                table.insert(args, "-DCMAKE_REQUIRED_LINK_OPTIONS=SHELL:" .. runtimes.link_flags)
                table.insert(args, "-DUWVM_ROS_PAIRED_RUNTIME_PROVIDER=ON")
            elseif macos_libcxx then
                -- The standalone LLVM 23 Darwin SDK may expose libc++23
                -- headers while its libSystem/SDK stub lacks their out-of-line
                -- symbols. LLVM's host tools (notably llvm-tblgen) must link
                -- the paired runtime after all object/archive arguments.
                -- CMAKE_EXE_LINKER_FLAGS is placed before them and cannot
                -- reliably resolve static libc++/libc++abi references.
                table.insert(args, "-DCMAKE_CXX_STANDARD_LIBRARIES=" .. macos_libcxx .. " " .. macos_libcxxabi)
            end
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
                os.vrunv(cmake.program, args, cmake_environment)
            end
            if cross_file and plat == "mingw" and arch == "x86_64" then
                -- Clang/C++ target flags do not configure CMake's independent
                -- ASM language. Without this check BLAKE3's Windows .S files
                -- can be archived as host ELF and fail only at PE link time.
                local cross_cache = io.readfile(path.join(build, "CMakeCache.txt"))
                assert(cross_cache:match("\nCMAKE_ASM_COMPILER_TARGET:[^=]*=x86_64%-w64%-windows%-gnu[\r\n]"),
                    "Windows x64 bundled LLVM requires CMAKE_ASM_COMPILER_TARGET in the toolchain file")
            end
            -- Ninja may need to rerun CMake after an included toolchain file
            -- changes, even when our own recipe/stamp is unchanged. Refresh
            -- only its build-system target before checking the resolved flags;
            -- otherwise the check can inspect an old reply and the subsequent
            -- library build can regenerate an unsafe configuration behind it.
            -- The generator is explicitly Ninja, so build.ninja is available;
            -- this step builds no LLVM archive or metadata consumer executable.
            build_targets(cmake.program, build, jobs, {"build.ninja"}, cmake_environment)
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
            build_targets(cmake.program, build, jobs, {"llvm-libraries"}, cmake_environment)
            -- Read again after the build as well; never publish a stale reply
            -- if the build system regenerated during its dependency checks.
            result = read_contract(build)
            result.identity = key
            result.bootstrap_prefix = path.directory(path.directory(cxx))
            result.stdlib = runtimes and "libc++" or selected("stdlib")
            result.runtimes = runtimes
            if runtimes then
                assert(result.host_target == runtimes.host_target,
                    "LLVM and bundled ROS runtimes target/ABI disagree")
            end
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
