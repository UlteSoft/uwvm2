import("common")
import("lib.detect.find_tool")


local function _endswith(str, suffix)
    return suffix == "" or str:sub(-#suffix) == suffix
end

local function _append_unique(result, seen, field, value)
    if not value then
        return
    end
    value = string.trim(value)
    if value == "" then
        return
    end

    local field_seen = seen[field]
    if not field_seen then
        field_seen = {}
        seen[field] = field_seen
    end
    if field_seen[value] then
        return
    end
    field_seen[value] = true

    local values = result[field]
    if not values then
        values = {}
        result[field] = values
    end
    table.insert(values, value)
end

local function _extract_prefixed_value(argv, index, prefix)
    local flag = argv[index]
    if flag == prefix then
        return argv[index + 1], 1
    end
    if flag:startswith(prefix) and #flag > #prefix then
        return flag:sub(#prefix + 1), 0
    end
    return nil, 0
end

local function _normalize_dir(dir)
    if not dir or dir == "" then
        return dir
    end
    return os.isdir(dir) and path.normalize(dir) or dir
end

local function _normalize_static_link_mode(mode)
    if mode == nil then
        return "none"
    end
    mode = tostring(mode)
    if mode == "none" or mode == "non-system" or mode == "compiler" then
        return mode
    end

    raise([[Invalid --static value "%s"; expected one of: none, non-system, compiler.]], tostring(mode))
end

---Get the normalized --static mode.
---@return string -- "none", "non-system", or "compiler"
function get_static_link_mode()
    return _normalize_static_link_mode(get_config("static"))
end

---Whether --static selects the compiler/toolchain -static strategy.
---@return boolean
function is_static_compiler_mode()
    return get_static_link_mode() == "compiler"
end

---Whether --static selects explicit non-system static libraries.
---@return boolean
function is_static_non_system_mode()
    return get_static_link_mode() == "non-system"
end

local function _is_default_system_include_dir(dir)
    local normalized = _normalize_dir(dir)
    return normalized == "/usr/include" or normalized == "/usr/local/include"
end

local function _is_library_path(flag)
    if not flag or flag == "" or not flag:find("[/\\]") then
        return false
    end

    local lower = flag:lower()
    return _endswith(lower, ".a")
        or _endswith(lower, ".so")
        or _endswith(lower, ".dylib")
        or _endswith(lower, ".tbd")
        or _endswith(lower, ".lib")
end

---Find a static archive for a logical library name.
---@param link string
---@param linkdirs string[] | string | nil
---@return string | nil
function find_static_library(link, linkdirs)
    if not link or link == "" then
        return nil
    end

    local name = link
    if name:startswith("-l:") then
        name = name:sub(4)
    elseif name:startswith("-l") and #name > 2 then
        name = name:sub(3)
    end

    local lower = name:lower()
    if _is_library_path(name) then
        if os.isfile(name) and (_endswith(lower, ".a") or _endswith(lower, ".lib")) then
            return path.normalize(name)
        end
        return nil
    end

    local patterns = {}
    if _endswith(lower, ".a") or _endswith(lower, ".lib") then
        table.insert(patterns, name)
    else
        if is_plat("windows") then
            table.insert(patterns, name .. ".lib")
            table.insert(patterns, "lib" .. name .. ".lib")
        else
            table.insert(patterns, "lib" .. name .. ".a")
            table.insert(patterns, name .. ".a")
        end
    end

    for _, linkdir in ipairs(table.wrap(linkdirs)) do
        if linkdir and linkdir ~= "" then
            for _, pattern in ipairs(patterns) do
                local libfile = path.join(linkdir, pattern)
                if os.isfile(libfile) then
                    return path.normalize(libfile)
                end
            end
        end
    end
end

local function _append_existing_libcxx_include_dir(result, seen, dir)
    if not dir or dir == "" or not os.isdir(dir) then
        return false
    end

    if not os.isfile(path.join(dir, "cstddef")) then
        return false
    end

    _append_unique(result, seen, "sysincludedirs", path.normalize(dir))
    return true
end

local function _append_existing_libcxx_link_dir(result, seen, dir)
    if not dir or dir == "" or not os.isdir(dir) then
        return false
    end

    if not (os.isfile(path.join(dir, "libc++.a"))
        or os.isfile(path.join(dir, "libc++.dll.a"))
        or os.isfile(path.join(dir, "c++.lib"))
        or os.isfile(path.join(dir, "libc++.lib"))) then
        return false
    end

    _append_unique(result, seen, "linkdirs", path.normalize(dir))
    return true
end

local function _link_dir_has_library(dir, name)
    return os.isfile(path.join(dir, "lib" .. name .. ".a"))
        or os.isfile(path.join(dir, "lib" .. name .. ".dll.a"))
        or os.isfile(path.join(dir, name .. ".lib"))
        or os.isfile(path.join(dir, "lib" .. name .. ".lib"))
end

local function _append_unique_string(values, seen, value)
    if not value or value == "" or seen[value] then
        return
    end

    seen[value] = true
    table.insert(values, value)
end

local function _add_llvm_libcxx_runtime_paths(result, seen, dependency, libdir)
    local prefix = dependency.build
    local host_target = dependency.host_target

    local roots = {}
    local root_seen = {}
    local function add_root(root)
        if root and root ~= "" then
            _append_unique_string(roots, root_seen, path.normalize(root))
        end
    end

    add_root(prefix)
    -- The LLVM library is vendored, but libc++ still belongs to the selected
    -- bootstrap compiler/SDK. Do not search for a system llvm-config to find it.
    add_root(dependency.bootstrap_prefix)
    if prefix and prefix ~= "" then
        add_root(path.join(prefix, ".."))
        add_root(path.join(prefix, "runtimes"))
        add_root(path.join(prefix, "..", "runtimes"))
    end
    if libdir and libdir ~= "" then
        add_root(path.join(libdir, ".."))
        add_root(path.join(libdir, "..", "runtimes"))
    end

    local targets = {}
    local target_seen = {}
    local function add_target(target)
        if target and target ~= "" and target ~= "detect" then
            _append_unique_string(targets, target_seen, target)
            _append_unique_string(targets, target_seen, target:gsub("%-unknown%-", "-"))
        end
    end

    add_target(host_target)
    add_target(get_config("llvm-target"))

    local has_libcxx_include_dir = false
    local function try_add_libcxx_include_dir(dir)
        if not has_libcxx_include_dir and _append_existing_libcxx_include_dir(result, seen, dir) then
            has_libcxx_include_dir = true
        end
    end

    local libcxx_link_dirs = {}
    local libcxx_link_dir_seen = {}
    local function try_add_libcxx_link_dir(dir)
        if _append_existing_libcxx_link_dir(result, seen, dir) then
            local normalized = path.normalize(dir)
            _append_unique_string(libcxx_link_dirs, libcxx_link_dir_seen, normalized)
        end
    end

    for _, root in ipairs(roots) do
        try_add_libcxx_include_dir(path.join(root, "include", "c++", "v1"))
        try_add_libcxx_include_dir(path.join(root, "runtimes", "include", "c++", "v1"))
        for _, target in ipairs(targets) do
            try_add_libcxx_include_dir(path.join(root, target, "include", "c++", "v1"))
        end

        try_add_libcxx_link_dir(path.join(root, "lib"))
        try_add_libcxx_link_dir(path.join(root, "runtimes", "lib"))
        for _, target in ipairs(targets) do
            try_add_libcxx_link_dir(path.join(root, "lib", target))
            try_add_libcxx_link_dir(path.join(root, "runtimes", "lib", target))
            try_add_libcxx_link_dir(path.join(root, target, "lib"))
        end
    end

    if #libcxx_link_dirs ~= 0 then
        _append_unique(result, seen, "ldflags", "-nostdlib++")
        _append_unique(result, seen, "shflags", "-nostdlib++")
        _append_unique(result, seen, "syslinks", "c++")
        for _, dir in ipairs(libcxx_link_dirs) do
            if _link_dir_has_library(dir, "c++abi") then
                _append_unique(result, seen, "syslinks", "c++abi")
                break
            end
        end
        for _, dir in ipairs(libcxx_link_dirs) do
            if _link_dir_has_library(dir, "unwind") then
                _append_unique(result, seen, "syslinks", "unwind")
                break
            end
        end
    end
end


local function _parse_llvm_linkflags(flags, result, seen, link_field)
    local argv = os.argv(flags or "")
    local i = 1
    while i <= #argv do
        local flag = argv[i]
        local consumed = 0
        local value

        value, consumed = _extract_prefixed_value(argv, i, "-L")
        if not value then
            value, consumed = _extract_prefixed_value(argv, i, "/LIBPATH:")
        end
        if value then
            _append_unique(result, seen, "linkdirs", _normalize_dir(value))
            i = i + 1 + consumed
            goto continue
        end

        value, consumed = _extract_prefixed_value(argv, i, "-F")
        if value then
            _append_unique(result, seen, "frameworkdirs", _normalize_dir(value))
            i = i + 1 + consumed
            goto continue
        end

        if flag == "-framework" then
            value = argv[i + 1]
            if value then
                _append_unique(result, seen, "frameworks", value)
                consumed = 1
            end
            i = i + 1 + consumed
            goto continue
        end

        if flag == "-Xlinker" then
            value = argv[i + 1]
            _append_unique(result, seen, "ldflags", flag)
            _append_unique(result, seen, "shflags", flag)
            if value then
                _append_unique(result, seen, "ldflags", value)
                _append_unique(result, seen, "shflags", value)
                consumed = 1
            end
            i = i + 1 + consumed
            goto continue
        end

        if flag:startswith("-l") and #flag > 2 then
            local link = flag:startswith("-l:") and flag:sub(4) or flag:sub(3)
            _append_unique(result, seen, link_field, link)
            i = i + 1
            goto continue
        end

        if not flag:find("[/\\]") and _endswith(flag:lower(), ".lib") then
            _append_unique(result, seen, link_field, flag:sub(1, -5))
            i = i + 1
            goto continue
        end

        if _is_library_path(flag) then
            _append_unique(result, seen, "ldflags", flag)
            _append_unique(result, seen, "shflags", flag)
            i = i + 1
            goto continue
        end

        _append_unique(result, seen, "ldflags", flag)
        _append_unique(result, seen, "shflags", flag)
        i = i + 1

        ::continue::
    end
end

---Parse linker flags in the same way as LLVM JIT detection and add them to a target.
---@param target any
---@param flags string | nil
---@param link_field string | nil
function add_linkflags_to_target(target, flags, link_field)
    if not target or not flags or flags == "" then
        return
    end

    local result = {}
    local seen = {}
    _parse_llvm_linkflags(flags, result, seen, link_field or "links")

    for _, field in ipairs({
        "linkdirs",
        "frameworkdirs",
        "frameworks",
        "links",
        "syslinks",
        "ldflags",
        "shflags"
    }) do
        local values = result[field]
        if values then
            for _, value in ipairs(values) do
                if field == "ldflags" or field == "shflags" then
                    target:add(field, value, { force = true })
                else
                    target:add(field, value)
                end
            end
        end
    end
end

-- LLVM's own CMake file API is the dependency contract. ROS has one pinned
-- source/build, so neither executable discovery nor component-name fallbacks
-- are appropriate. Keep CMake's complete link order, including repeated archives.
function llvm_link_arguments(dependency)
    local link_arguments = {}
    local archives = 0
    for _, item in ipairs(dependency.link) do
        -- Unattributed 'flags' fragments are CMake's global compiler/link flags:
        -- ROS already owns these (dialect, optimization, warnings, stdlib, etc.).
        -- Target/interface link options have a backtrace and must be retained.
        -- Importing every flag would overwrite ROS's own compile/link policy;
        -- dropping all flags could lose a platform dependency's required option.
        if item.role ~= "flags" or item.backtrace then
            for _, argument in ipairs(os.argv(item.fragment)) do
                local name = path.filename(argument)
                local llvm_archive = name:startswith("libLLVM") or name:startswith("LLVM")
                if argument:startswith("-lLLVM") or argument:startswith("-l:libLLVM") then
                    raise("Bundled LLVM must be linked by absolute archive paths, never library search names")
                elseif llvm_archive or _is_library_path(argument) then
                    argument = path.absolute(argument, dependency.build)
                    if llvm_archive then
                        -- Matching a version or SONAME is not enough to prove
                        -- the downstream MIPS patch is present. Absolute static
                        -- archives from the verified build avoid both -L/PATH
                        -- substitution at link time and loader replacement later.
                        assert(name:endswith(".a") or name:endswith(".lib"),
                            "Bundled LLVM contract attempted shared-library substitution")
                        local relative = path.relative(argument, dependency.build):gsub("\\", "/")
                        assert(not relative:startswith("../") and not path.is_absolute(relative),
                            "Bundled LLVM archive escapes its verified build")
                        assert(os.isfile(argument), "Missing bundled LLVM archive: " .. argument)
                        archives = archives + 1
                    end
                end
                table.insert(link_arguments, argument)
            end
        end
    end
    assert(archives > 0, "Empty bundled LLVM static link contract")
    return link_arguments, archives
end

function get_llvm_jit_options()
    local bundled = import("utility.bundled_llvm", {anonymous = true})
    local dependency = bundled.ensure()
    if dependency.options then return dependency.options end
    local result = {llvm_version = dependency.version, static_mode = get_static_link_mode(),
        llvm_stdlib = dependency.stdlib}
    local seen = {}
    for _, item in ipairs(dependency.compile.includes or {}) do
        _append_unique(result, seen, "sysincludedirs", item.path)
    end
    for _, item in ipairs(dependency.compile.defines or {}) do
        _append_unique(result, seen, "defines", item.define)
    end
    if result.llvm_stdlib == "libc++" then
        _add_llvm_libcxx_runtime_paths(result, seen, dependency, path.join(dependency.build, "lib"))
    end
    local link_arguments, archives = llvm_link_arguments(dependency)
    -- A single ordered fragment prevents xmake's individual-flag de-duplication
    -- from destroying CMake's archive repetitions/link groups on non-lld hosts.
    local ordered = os.args(link_arguments)
    result.ldflags = table.join(result.ldflags or {}, ordered)
    result.shflags = table.join(result.shflags or {}, ordered)
    cprint("using ROS bundled LLVM ... ${color.success}%s (%s), %d archive entries",
        dependency.build, dependency.version, archives)
    dependency.options = result
    return result
end

---Deriving target and modifier from arch and plat
---@param target string --Target platform
---@param toolchain string --Toolchain name
---@return string --Target platform
---@return modifier_t --adjustment function
function get_target_modifier(target, toolchain)
    ---@type modifier_table_t
    local target_list = import("target", { anonymous = true }).get_target_list()
    if target ~= "target" then
        return target, target_list[target]
    end

    ---@type table<string, any>
    local cache_info = common.get_cache()
    ---@type string, modifier_t
    local target, modifier = table.unpack(cache_info["target"] or {})
    if target and modifier then -- Already detected, returns target and modifier directly.
        return target, modifier
    end

    ---@type string | nil
    local arch = get_config("arch")
    ---@type string | nil
    local plat = get_config("plat")
    ---@type string
    local target_os = get_config("target_os") or "none"
    local message = [[Unsupported %s "%s". Please select a specific toolchain.]]

    ---Mapping xmake style arch to triplet style
    ---@type map_t
    local arch_table = {
        x86 = "i686",
        i386 = "i686",
        i686 = "i686",
        x64 = "x86_64",
        x86_64 = "x86_64",
        loong64 = "loongarch64",
        riscv64 = "riscv64",
        arm = "arm",
        armv7 = "arm",
        armv7s = "arm",
        ["arm64-v8a"] = "aarch64",
        arm64 = "aarch64",
        arm64ec = "aarch64"
    }
    local old_arch = arch
    arch = arch_table[arch]
    assert(arch, format(message, "arch", old_arch))

    if plat == "windows" and toolchain == "gcc" then
        plat = "mingw" -- gcc doesn't support msvc targets, but clang does!
    end
    ---@type map_t
    local plat_table = {
        mingw = "w64",
        msys = "w64",
        linux = "linux",
        windows = "windows",
        cross = target_os
    }
    local old_plat = plat
    plat = plat_table[plat]
    assert(plat, format(message, "plat", old_plat))

    ---@type map_t
    local x86_abi_table = {
        windows = "msvc",
        w64 = "mingw32",
        linux = "gnu",
        none = "elf"
    }
    ---@type map_t
    local linux_abi_table = { linux = "gnu" }
    ---@type table<string, map_t>
    local abi_table = {
        i686 = x86_abi_table,
        x86_64 = x86_abi_table,
        arm = {
            linux = "gnueabihf",
            none = "eabi"
        },
        aarch64 = linux_abi_table,
        riscv64 = linux_abi_table,
        loongarch64 = linux_abi_table
    }
    ---@type string
    local abi = (abi_table[arch] or {})[plat] or "unknown"

    ---@type string[]
    local field = { arch, plat, abi }
    -- Special treatment for arch-elf
    if plat == "none" and abi == "elf" then
        table.remove(field, 2)
    end
    target = table.concat(field, "-")

    modifier = target_list[target]
    cprint("detecting for target .. " .. (modifier and "${color.success}" or "${color.failure}") .. target)
    assert(modifier, format(message, "target", target))

    cache_info["target"] = { target, modifier }
    common.update_cache(cache_info)

    return target, modifier
end

---Get a list of sysroot options based on options or probing results
---@return table<string, string | string[]> | nil --Options list
function get_sysroot_option()
    local cache_info = common.get_cache()
    ---sysroot cache
    ---@type string | nil
    local sysroot = cache_info["sysroot"]
    ---Get a list of options based on sysroot
    ---@return table<string, string | string[]> --Options list
    local function get_option_list()
        local sysroot_option = "--sysroot=" .. sysroot
        -- Determine if it is libc++
        local is_libcxx = (get_config("runtimes") or ""):startswith("c++")
        local libcxx_option = is_libcxx and "-isystem" .. path.join(sysroot, "include", "c++", "v1") or nil
        return { cxflags = { sysroot_option, libcxx_option }, ldflags = sysroot_option, shflags = sysroot_option }
    end
    if sysroot == "" then
        return nil               -- Detected, no sysroot available.
    elseif sysroot then
        return get_option_list() -- Already probed, using cached sysroot
    end

    -- Check for a given sysroot or auto-detect sysroot.
    sysroot = get_config("sysroot")
    local detect = sysroot == "detect"
    -- The specified sysroot if sysroot is not “none”/“no” or “detect”.
    sysroot = (sysroot ~= "none" and sysroot ~= "no" and not detect) and sysroot or nil
    -- If the clang toolchain is used and sysroot is not specified, then autoprobe is attempted.
    detect = detect and common.is_clang()
    cache_info["sysroot_set_by_user"] = sysroot and true or false
    if sysroot then    -- Check legitimacy if sysroot is specified
        assert(os.isdir(sysroot), string.format([[The sysroot "%s" is not a directory.]], sysroot))
    elseif detect then -- Attempted detection
        ---@type string | nil
        local prefix
        if get_config("bin") then
            prefix = path.join(string.trim(get_config("bin")), "..")
        else
            -- Bootstrap compiler discovery must not reintroduce system LLVM
            -- dependency discovery through an unrelated llvm-config on PATH.
            local clang = find_tool("clang")
            prefix = clang and path.directory(path.directory(clang.program)) or nil
        end
        if prefix then
            -- Try the following directories: 1. prefix/sysroot 2. prefix/... /sysroot Prefer a more localized directory
            for _, v in ipairs({ "sysroot", "../sysroot" }) do
                local dir = path.join(prefix, v)
                if os.isdir(dir) then
                    sysroot = path.normalize(dir)
                    cprint("detecting for sysroot ... ${color.success}%s", sysroot)
                    break
                end
            end
        end
    end

    -- Updating the cache
    cache_info["sysroot"] = sysroot or ""
    common.update_cache(cache_info)

    if sysroot then
        return get_option_list()
    else
        if detect then
            cprint("detecting for sysroot ... ${color.failure}no")
        end
        return nil
    end
end

---Get march options
---@param target string --Target platform
---@param toolchain string --Type of toolchain
---@note Check option legitimacy only if target and toolchain exist.
---@return string | nil --march option
function get_march_option(target, toolchain)
    local cache_info = common.get_cache()
    local option = cache_info["march"]
    if option == "" then
        return nil    -- Already detected, -march is not supported
    elseif option then
        return option -- Already probed to support the -march option
    end

    ---Detect if march is supported
    ---@type string
    local arch = get_config("march")
    if arch ~= "no" and arch ~= "none" then
        local march = (arch ~= "default" and arch or "native")
        option = { "-march=" .. march }
        -- Check option legitimacy only if target and toolchain exist.
        if target and toolchain then
            import("core.tool.compiler")
            if toolchain == "clang" and target ~= "native" then
                table.insert(option, "--target=" .. target)
            end
            ---@type boolean
            local support = compiler.has_flags("cxx", table.concat(option, " "))
            local message = "checking for march ... "
            cprint(message .. (support and "${color.success}" or "${color.failure}") .. march)
            if not support then
                if arch ~= "default" then
                    raise(string.format([[The toolchain doesn't support the arch "%s"]], march))
                end
                option = ""
            else
                option = option[1]
            end
        else
            option = nil -- 未探测，设置为nil在下次进行探测
        end
    else
        option = ""
    end

    -- Updating the cache
    cache_info["march"] = option
    common.update_cache(cache_info)
    return option
end

---Getting the rtlib option
---@return string | nil --rtlib option
function get_rtlib_option()
    local config = get_config("rtlib")
    return (common.is_clang() and config ~= "default") and "-rtlib=" .. config or nil
end

---Getting the unwindlib option
---@return string | nil --unwindlib option
function get_unwindlib_option()
    local config = get_config("unwindlib")
    local force = config:startswith("force")
    local lib = force and string.sub(config, 7, #config) or config
    local option = config ~= "default" and "-unwindlib=" .. lib or nil
    return (common.is_clang() and (force or get_config("rtlib") == "compiler-rt")) and option or nil
end

---Getting Apple platform options
---@return table<string, string | string[]> | nil --Apple platform options
function get_apple_platform_options()
    local config = get_config("apple-platform")
    if config == "default" or not config then
        return nil -- Use default compiler behavior
    end
    
    ---@type map_t
    local version_table = {
        -- macOS versions
        MACOS_SEQUOIA = "15.0",
        MACOS_SONOMA = "14.0", 
        MACOS_VENTURA = "13.0",
        MACOS_MONTEREY = "12.0",
        MACOS_BIG_SUR = "11.0",
        MACOS_CATALINA = "10.15",
        MACOS_MOJAVE = "10.14",
        MACOS_HIGH_SIERRA = "10.13",
        MACOS_SIERRA = "10.12",
        MACOS_EL_CAPITAN = "10.11",
        MACOS_YOSEMITE = "10.10",
        -- iOS versions
        IOS_18 = "18.0",
        IOS_17 = "17.0",
        IOS_16 = "16.0",
        IOS_15 = "15.0",
        IOS_14 = "14.0",
        IOS_13 = "13.0",
        IOS_12 = "12.0",
        IOS_11 = "11.0",
        -- tvOS versions
        TVOS_18 = "18.0",
        TVOS_17 = "17.0",
        TVOS_16 = "16.0",
        TVOS_15 = "15.0",
        TVOS_14 = "14.0",
        TVOS_13 = "13.0",
        -- watchOS versions
        WATCHOS_11 = "11.0",
        WATCHOS_10 = "10.0",
        WATCHOS_9 = "9.0",
        WATCHOS_8 = "8.0",
        WATCHOS_7 = "7.0",
        -- visionOS versions
        VISIONOS_2 = "2.0",
        VISIONOS_1 = "1.0"
    }
    
    local platform, version = config:match("^([^_]+)_(.+)$")
    if not platform or not version then
        -- Custom version format like "macos:10.15" or "ios:13.0"
        platform, version = config:match("^([^:]+):(.+)$")
        if not platform or not version then
            return nil
        end
    end
    
    -- Resolve version from predefined constants
    local resolved_version = version_table[config] or version
    
    local options = {}
    
    -- Set target OS and minimum version based on platform
    if platform:upper() == "MACOS" then
        options.cxflags = { "-mtargetos=macos", "-mmacos-version-min=" .. resolved_version }
        options.ldflags = "-mmacos-version-min=" .. resolved_version
    elseif platform:upper() == "IOS" then
        options.cxflags = { "-mtargetos=ios", "-mios-version-min=" .. resolved_version }
        options.ldflags = "-mios-version-min=" .. resolved_version
    elseif platform:upper() == "TVOS" then
        options.cxflags = { "-mtargetos=tvos", "-mtvos-version-min=" .. resolved_version }
        options.ldflags = "-mtvos-version-min=" .. resolved_version
    elseif platform:upper() == "WATCHOS" then
        options.cxflags = { "-mtargetos=watchos", "-mwatchos-version-min=" .. resolved_version }
        options.ldflags = "-mwatchos-version-min=" .. resolved_version
    elseif platform:upper() == "VISIONOS" then
        options.cxflags = { "-mtargetos=visionos", "-mvisionos-version-min=" .. resolved_version }
        options.ldflags = "-mvisionos-version-min=" .. resolved_version
    else
        return nil
    end
    
    return options
end

---Mapping mode to cmake style
---@param mode string --xmake style compilation mode
---@return string | nil --cmake style compilation mode
function get_cmake_mode(mode)
    ---@type map_t
    local table = { debug = "Debug", release = "Release", minsizerel = "MinSizeRel", releasedbg = "RelWithDebInfo" }
    return table[mode]
end
