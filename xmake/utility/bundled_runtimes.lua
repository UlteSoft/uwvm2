-- One source-owned C++ ABI/unwinder provider for ROS LLVM and its consumers.
-- This builds no compiler, C library or sysroot; those remain bootstrap inputs.
import("core.project.config")
import("core.cache.memcache")
import("core.base.json")
import("lib.detect.find_tool")

local get_config = config.get
local release = "23.1.1"
local recipe_version = "uwvm-ros-runtimes.1"

local function selected(name)
    local value = get_config(name)
    if value and value ~= "none" and value ~= "no" and value ~= "detect" and value ~= "default" then
        return value
    end
end

function enabled()
    local provider = get_config("llvm-runtimes-provider") or "auto"
    assert(provider == "auto" or provider == "bundled" or provider == "platform", "Invalid LLVM runtimes provider")
    if provider == "platform" then return false end
    local plat = get_config("plat") or os.host()
    local supported = plat == "linux" or plat == "macosx" or plat == "mingw"
        or plat == "bsd" or plat == "freebsd" or plat == "openbsd"
        or plat == "netbsd" or plat == "dragonflybsd"
    -- Windows/MSVC, ARM64EC and other ABIs retain their existing explicit
    -- platform-provider contract. Do not substitute an Itanium runtime there.
    supported = supported and get_config("arch") ~= "arm64ec"
    if provider == "bundled" then
        assert(supported, "Bundled ROS runtimes require a supported GNU Clang ABI, not MSVC/ARM64EC")
    elseif not supported or not get_config("use-llvm-compiler") then
        return false
    end
    assert(not selected("stdlib") or selected("stdlib") == "libc++", "Bundled ROS runtimes conflict with --stdlib=libstdc++")
    local unwind = selected("unwindlib")
    assert(not unwind or unwind == "libunwind" or unwind == "force-libunwind", "Bundled ROS runtimes cannot mix the platform/libgcc unwinder")
    assert(not os.getenv("UWVM_ROS_QUALIFIED_PREBUILT_LLVM"),
        "A platform-runtime prebuilt LLVM cannot be used with newly bundled runtimes; use --llvm-runtimes-provider=platform or rebuild LLVM")
    return true
end

local function verify_provider_flags(value)
    for _, flag in ipairs(os.argv(value or "")) do
        local name = path.filename(flag)
        assert(not flag:match("^%-stdlib=libstdc%+%+$")
            and not flag:match("^%-l(c%+%+.*)$") and flag ~= "-lstdc++"
            and flag ~= "-lunwind" and flag ~= "-lgcc_s" and flag ~= "-lgcc_eh"
            and not name:match("^libc%+%+.*%.[asd]") and not name:match("^libunwind%.[asd]")
            and not flag:find("_LIBCPP_ABI_", 1, true) and not flag:find("_LIBCPP_HARDENING_MODE", 1, true),
            "Bundled ROS runtimes reject an external C++/unwinder or header ABI override: " .. flag)
        local unwinder = flag:match("^%-+unwindlib=(.*)$")
        assert(not unwinder or unwinder == "none" or unwinder == "libunwind",
            "Bundled ROS runtimes reject a second unwinder: " .. flag)
    end
end

local function verify_source(root)
    local manifest = path.join(root, "sources.sha256")
    assert(os.isfile(manifest), "Missing ROS runtimes source manifest")
    local recorded = {}
    for line in io.lines(manifest) do
        local digest, relative = line:match("^([0-9a-f]+)  (.+)$")
        assert(digest and #digest == 64 and not path.is_absolute(relative)
            and not relative:find("..", 1, true), "Malformed ROS runtimes manifest")
        assert(not recorded[relative], "Duplicate ROS runtimes source")
        local file = path.join(root, relative)
        assert(os.isfile(file) and hash.sha256(file):lower() == digest, "Changed ROS runtimes source: " .. relative)
        recorded[relative] = true
    end
    for _, file in ipairs(os.files(path.join(root, "upstream", "**"))) do
        local relative = path.relative(file, root):gsub("\\", "/")
        assert(recorded[relative], "Unmanifested ROS runtimes source: " .. relative)
    end
    local version = io.readfile(path.join(root, "upstream/cmake/Modules/LLVMVersion.cmake"))
    for name, value in pairs({MAJOR="23", MINOR="1", PATCH="1"}) do
        assert(version:find("set%(LLVM_VERSION_" .. name .. " " .. value .. "%)"), "ROS runtimes release mismatch")
    end
    return hash.sha256(manifest):lower()
end

local function read_contract(build)
    local replies = os.files(path.join(build, ".cmake/api/v1/reply/index-*.json"))
    table.sort(replies)
    assert(#replies > 0, "Missing ROS runtimes CMake file API")
    local directory = path.directory(replies[#replies])
    local index = json.decode(io.readfile(replies[#replies]))
    local reply = assert(index.reply and index.reply["codemodel-v2"], "Missing runtimes codemodel")
    local model = json.decode(io.readfile(path.join(directory, reply.jsonFile)))
    assert(#model.configurations == 1 and model.configurations[1].name == "Release", "ROS runtimes must be Release")
    local consumer
    for _, target in ipairs(model.configurations[1].targets) do
        if target.name == "uwvm_ros_runtimes_contract" then
            consumer = json.decode(io.readfile(path.join(directory, target.jsonFile)))
        end
    end
    assert(consumer and consumer.link and consumer.link.language == "CXX" and #consumer.compileGroups == 1,
        "Missing ROS runtimes consumer contract")
    local metadata = json.decode(io.readfile(path.join(build, "uwvm-runtimes.json")))
    assert(metadata.version == release and metadata.abi_version == "1" and metadata.provider == "llvm-static",
        "ROS runtimes resolved ABI contract mismatch")
    metadata.build = build
    metadata.compile = consumer.compileGroups[1]
    metadata.link = consumer.link.commandFragments
    return metadata
end

local function ordered_link(result)
    local arguments, found = {}, {}
    for _, fragment in ipairs(result.link) do
        if fragment.role ~= "flags" or fragment.backtrace then
            for _, argument in ipairs(os.argv(fragment.fragment)) do
                local name = path.filename(argument)
                local runtime = name == "libc++.a" or name == "libc++abi.a" or name == "libunwind.a"
                assert(not argument:match("^%-lc%+%+") and not argument:match("^%-lunwind")
                    and not argument:match("^%-lgcc_s") and not argument:match("^%-lgcc_eh"),
                    "ROS runtimes contract attempted an external C++ ABI/unwinder")
                if runtime then
                    argument = path.absolute(argument, result.build)
                    local relative = path.relative(argument, result.build):gsub("\\", "/")
                    assert(not relative:startswith("../") and not path.is_absolute(relative) and os.isfile(argument),
                        "ROS runtime archive escapes its verified build")
                    found[name] = argument
                elseif name:match("^libc%+%+.*%.so") or name:match("^libunwind.*%.so")
                    or name:match("^libc%+%+.*%.dylib") or name:match("^libunwind.*%.dylib") then
                    raise("ROS runtimes contract attempted a shared provider")
                end
                table.insert(arguments, argument)
            end
        end
    end
    for _, name in ipairs({"libc++.a", "libc++abi.a", "libunwind.a"}) do
        assert(found[name], "Missing paired ROS runtime archive: " .. name)
    end
    result.archives = found
    result.link_arguments = arguments
    result.link_flags = os.args(arguments)
end

function ensure(input)
    if not enabled() then return nil end
    local memo_key = os.projectdir() .. ":" .. config.builddir()
    local cached = memcache.get("uwvm-ros.bundled-runtimes", memo_key)
    if cached then return cached end
    local root = path.join(os.projectdir(), "third-parties/llvm-runtimes")
    local source_id = verify_source(root)
    verify_provider_flags(input.cflags)
    verify_provider_flags(input.cxxflags)
    verify_provider_flags(input.linkflags)
    local cmake = assert(find_tool("cmake"), "ROS runtimes require CMake >= 3.20")
    -- libc++ header generators import modules from the verified source tree.
    -- Keep Python bytecode in memory so our own build does not add unmanifested
    -- __pycache__ files and make the next strict source verification fail.
    local cmake_environment = {envs = {PYTHONDONTWRITEBYTECODE = "1"}}
    local ninja = assert(find_tool("ninja"), "ROS runtimes require Ninja")
    local cross_file = input.cross_file
    local compiler_rt = selected("rtlib") == "compiler-rt"
    local target = selected("target") or selected("llvm-target") or ""
    if selected("target") and selected("llvm-target") then
        assert(selected("target") == selected("llvm-target"), "ROS runtimes C/C++ target disagreement")
    end
    local contract = path.join(os.projectdir(), "xmake/llvm/runtimes_contract.cmake")
    local stub = path.join(os.projectdir(), "xmake/llvm/contract.cpp")
    local profile = path.join(root, "paired-static.cmake")
    assert(os.isfile(profile), "Missing ROS paired-static runtime profile")
    local settings = {source_id, release, recipe_version, input.cc, input.cxx,
        hash.sha256(input.cc), hash.sha256(input.cxx), input.plat, input.arch, target,
        input.cflags, input.cxxflags, input.linkflags, selected("sysroot") or "",
        cross_file or "", cross_file and hash.sha256(cross_file) or "",
        compiler_rt and "compiler-rt" or "libgcc-builtins", "ABI=1;unstable=OFF;hardening=none;static=ON;exceptions=ON",
        os.getenv("SDKROOT") or "", os.getenv("MACOSX_DEPLOYMENT_TARGET") or "",
        hash.sha256(path.join(os.projectdir(), "xmake/utility/bundled_runtimes.lua")),
        hash.sha256(contract), hash.sha256(stub), hash.sha256(profile)}
    local text = table.concat(settings, "\n") .. "\n"
    local temporary = os.tmpfile()
    io.writefile(temporary, text)
    local key = hash.sha256(temporary):lower()
    os.rm(temporary)
    local base = path.absolute(path.join(config.builddir(), "bundled-runtimes"), os.projectdir())
    local build = path.join(base, key)
    os.mkdir(build)
    local llvm = import("utility.bundled_llvm", {anonymous = true})
    build = llvm.canonical_build_directory(build, cmake.program)
    local lock = io.openlock(path.join(build, "build.lock"))
    lock:lock()
    local result
    try {
        function()
            local cache = path.join(build, "CMakeCache.txt")
            if os.isfile(cache) then
                local previous = io.readfile(cache):match("\nCMAKE_HOME_DIRECTORY:INTERNAL=([^\r\n]+)")
                assert(previous and path.normalize(previous) == path.normalize(path.join(root, "upstream/runtimes")),
                    "ROS runtimes build directory was copied from a different source")
            end
            local query = path.join(build, ".cmake/api/v1/query/codemodel-v2")
            os.mkdir(path.directory(query))
            if not os.isfile(query) then io.writefile(query, "") end
            local stamp = path.join(build, "complete.txt")
            local args = {"-C", profile, "-S", path.join(root, "upstream/runtimes"), "-B", build, "-G", "Ninja",
                "-DCMAKE_MAKE_PROGRAM=" .. ninja.program, "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_PROJECT_Runtimes_INCLUDE=" .. contract,
                "-DUWVM_ROS_RUNTIME_REQUIRE_EXPLICIT_TARGET=" .. ((cross_file or target ~= "") and "ON" or "OFF"),
                "-DCMAKE_C_COMPILER=" .. input.cc, "-DCMAKE_CXX_COMPILER=" .. input.cxx,
                "-DCMAKE_ASM_COMPILER=" .. input.cc,
                "-DCMAKE_C_FLAGS=" .. input.cflags, "-DCMAKE_CXX_FLAGS=" .. input.cxxflags,
                "-DCMAKE_ASM_FLAGS=" .. input.cflags,
                "-DCMAKE_EXE_LINKER_FLAGS=" .. input.linkflags,
                "-DCMAKE_POSITION_INDEPENDENT_CODE=ON", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                "-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=ON", "-DCMAKE_DISABLE_FIND_PACKAGE_Clang=ON",
                "-DLLVM_ENABLE_RUNTIMES=libunwind;libcxxabi;libcxx", "-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF",
                "-DLLVM_INCLUDE_TESTS=OFF", "-DLLVM_INCLUDE_DOCS=OFF", "-DLLVM_ENABLE_ASSERTIONS=OFF",
                "-DLLVM_ENABLE_EH=ON", "-DLLVM_ENABLE_RTTI=ON", "-DLLVM_ENABLE_LTO=OFF",
                "-DLIBCXX_ENABLE_SHARED=OFF", "-DLIBCXX_ENABLE_STATIC=ON", "-DLIBCXX_CXX_ABI=libcxxabi",
                "-DLIBCXX_ABI_VERSION=1", "-DLIBCXX_ABI_NAMESPACE=__1", "-DLIBCXX_ABI_UNSTABLE=OFF", "-DLIBCXX_HARDENING_MODE=none",
                "-DLIBCXX_ENABLE_EXCEPTIONS=ON", "-DLIBCXX_ENABLE_RTTI=ON", "-DLIBCXX_ENABLE_THREADS=ON",
                "-DLIBCXX_STATICALLY_LINK_ABI_IN_STATIC_LIBRARY=OFF", "-DLIBCXX_INCLUDE_TESTS=OFF",
                "-DLIBCXX_INCLUDE_BENCHMARKS=OFF", "-DLIBCXX_INCLUDE_DOCS=OFF",
                "-DLIBCXXABI_ENABLE_SHARED=OFF", "-DLIBCXXABI_ENABLE_STATIC=ON",
                "-DLIBCXXABI_USE_LLVM_UNWINDER=ON", "-DLIBCXXABI_ENABLE_STATIC_UNWINDER=ON",
                "-DLIBCXXABI_STATICALLY_LINK_UNWINDER_IN_STATIC_LIBRARY=OFF",
                "-DLIBCXXABI_ENABLE_EXCEPTIONS=ON", "-DLIBCXXABI_ENABLE_ASSERTIONS=OFF", "-DLIBCXXABI_ENABLE_THREADS=ON",
                "-DLIBCXXABI_INCLUDE_TESTS=OFF", "-DLIBUNWIND_ENABLE_SHARED=OFF", "-DLIBUNWIND_ENABLE_STATIC=ON",
                "-DLIBUNWIND_ENABLE_ASSERTIONS=OFF", "-DLIBUNWIND_ENABLE_THREADS=ON", "-DLIBUNWIND_ENABLE_CROSS_UNWINDING=OFF",
                "-DLIBUNWIND_INCLUDE_TESTS=OFF", "-DLIBUNWIND_INCLUDE_DOCS=OFF",
                "-DLIBCXX_USE_COMPILER_RT=" .. (compiler_rt and "ON" or "OFF"),
                "-DLIBCXXABI_USE_COMPILER_RT=" .. (compiler_rt and "ON" or "OFF"),
                "-DLIBUNWIND_USE_COMPILER_RT=" .. (compiler_rt and "ON" or "OFF")}
            if cross_file then table.insert(args, "-DCMAKE_TOOLCHAIN_FILE=" .. cross_file) end
            if target ~= "" then
                for _, language in ipairs({"C", "CXX", "ASM"}) do
                    table.insert(args, "-DCMAKE_" .. language .. "_COMPILER_TARGET=" .. target)
                end
                table.insert(args, "-DLLVM_DEFAULT_TARGET_TRIPLE=" .. target)
            end
            if selected("sysroot") then
                local sysroot = path.absolute(selected("sysroot"))
                table.insert(args, "-DCMAKE_SYSROOT=" .. sysroot)
                if input.plat == "macosx" then table.insert(args, "-DCMAKE_OSX_SYSROOT=" .. sysroot) end
            end
            if not os.isfile(cache) or not os.isfile(stamp) or io.readfile(stamp) ~= key .. "\n" then
                io.writefile(path.join(build, "settings.txt"), text)
                os.vrunv(cmake.program, args, cmake_environment)
            end
            os.vrunv(cmake.program, {"--build", build, "--parallel", tostring(input.jobs), "--target", "build.ninja"}, cmake_environment)
            local llvm_flags = import("utility.bundled_llvm", {anonymous = true})
            for line in io.lines(cache) do
                local value = line:match("^CMAKE_C[_X]*_FLAGS[^:]*:STRING=(.*)$")
                if value then llvm_flags.verify_fp_flags(value); verify_provider_flags(value) end
            end
            result = read_contract(build)
            for _, fragment in ipairs(result.compile.compileCommandFragments or {}) do
                llvm_flags.verify_fp_flags(fragment.fragment)
                verify_provider_flags(fragment.fragment)
            end
            os.vrunv(cmake.program, {"--build", build, "--parallel", tostring(input.jobs), "--target", "cxx_static", "cxxabi_static", "unwind_static"}, cmake_environment)
            result = read_contract(build)
            result.identity = key
            result.source_id = source_id
            result.stdlib = "libc++"
            result.include = path.join(build, "include/c++/v1")
            result.abi_include = path.join(root, "upstream/libcxxabi/include")
            result.unwind_include = path.join(root, "upstream/libunwind/include")
            result.config_site = path.join(result.include, "__config_site")
            assert(os.isfile(result.config_site) and os.isfile(path.join(result.include, "cstddef")), "Missing generated ROS libc++ headers")
            result.config_site_sha256 = hash.sha256(result.config_site):lower()
            ordered_link(result)
            io.writefile(path.join(build, "runtime-inputs.json"), json.encode({schema=1, source_id=source_id, identity=key,
                config_site_sha256=result.config_site_sha256, archives={
                    libcxx=hash.sha256(result.archives["libc++.a"]), libcxxabi=hash.sha256(result.archives["libc++abi.a"]),
                    libunwind=hash.sha256(result.archives["libunwind.a"])}}))
            io.writefile(stamp, key .. "\n")
        end,
        finally {function(ok, err)
            lock:unlock()
            lock:close()
            if not ok then raise(err) end
        end}
    }
    memcache.set("uwvm-ros.bundled-runtimes", memo_key, result)
    return result
end
