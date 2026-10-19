-- OpenSSL remains a separate dependency from the selected C/C++ compiler.
-- Build only the complete pinned tree, with target ABI checks before Configure.
import("core.project.config")
import("core.base.option")
import("core.base.json")
import("lib.detect.find_tool")

local function append_flags(out, value)
    if type(value) == "string" then value = os.argv(value) end
    for _, flag in ipairs(value or {}) do
        if type(flag) == "table" then append_flags(out, flag) else table.insert(out, flag) end
    end
end

local function abi_flags(target)
    -- Keep toolchain/SDK/ABI flags; UWVM's C++ language, warning and source
    -- definitions are not an OpenSSL C-library build contract.
    local all = {}
    append_flags(all, target:toolconfig("cxflags"))
    append_flags(all, target:toolconfig("cflags"))
    append_flags(all, target:get("cxflags"))
    append_flags(all, target:get("cflags"))
    append_flags(all, config.get("cxflags"))
    append_flags(all, config.get("cflags"))
    local out, take_next, skip_next = {"-O2", "-g0", "-fPIC"}, false, false
    for _, flag in ipairs(all) do
        if skip_next then
            skip_next = false
        elseif flag == "-mllvm" then
            skip_next = true
        elseif take_next then
            table.insert(out, flag)
            take_next = false
        elseif flag == "-arch" or flag == "-isysroot" or flag == "--sysroot" or
               flag == "-target" or flag == "--target" or flag == "-B" then
            table.insert(out, flag)
            take_next = true
        elseif (flag:startswith("-m") and not flag:startswith("-mllvm")) or flag:startswith("--target=") or flag:startswith("--sysroot=") or
               flag:startswith("--gcc-toolchain=") or flag:startswith("--gcc-install-dir=") or
               flag:startswith("-B") or flag:startswith("-fuse-ld=") or flag:startswith("--ld-path=") or
               flag == "-fPIC" or flag == "-fpic" or flag == "-pthread" or
               flag:startswith("-D_FILE_OFFSET_BITS=") or flag:startswith("-D_TIME_BITS=") then
            table.insert(out, flag)
        end
    end
    assert(not take_next, "Incomplete OpenSSL target/SDK compiler flag")
    -- Xmake's explicit LLVM triple and sysroot options can be injected by
    -- option hooks after target flags. Record them here as well.
    local triple = config.get("llvm-target")
    if config.get("use-llvm-compiler") and triple and triple ~= "detect" and triple ~= "none" and triple ~= "no" then
        table.insert(out, "--target=" .. triple)
    end
    local sysroot = config.get("sysroot")
    if sysroot and sysroot ~= "detect" and sysroot ~= "none" and sysroot ~= "no" then
        table.insert(out, "--sysroot=" .. path.absolute(sysroot))
    end
    append_flags(out, config.get("openssl-cflags"))
    return out
end

local function tool_argv(target, kind, fallback)
    local program = target:tool(kind)
    if not program then
        local tool = find_tool(fallback or kind)
        assert(tool, "Bundled OpenSSL requires " .. kind)
        program = tool.program
    end
    return os.argv(program)
end

function prepare(target)
    local source = path.join(os.projectdir(), "third-parties", "openssl")
    local manifest = path.join(source, "sources.sha256")
    assert(os.isfile(manifest), "Missing full bundled OpenSSL source; restore third-parties/openssl")
    local jobs = tonumber(config.get("openssl-build-jobs") or 1)
    assert(jobs and jobs >= 1 and jobs == math.floor(jobs), "openssl-build-jobs must be a positive integer")
    local configure_target = config.get("openssl-configure-target")
    if configure_target == "auto" then configure_target = nil end
    local spec = {platform = target:plat(), arch = target:arch(),
        cc = tool_argv(target, "cc"), ar = tool_argv(target, "ar"),
        ranlib = tool_argv(target, "ranlib"), cflags = abi_flags(target),
        ldflags = {}, configure_target = configure_target, jobs = jobs,
        float_abi = config.get("openssl-float-abi") or "auto",
        no_asm = config.get("openssl-no-asm") or false}
    local provider_receipt = config.get("openssl-provider-receipt")
    if provider_receipt and provider_receipt ~= "none" then
        spec.provider_receipt = path.absolute(provider_receipt)
        assert(os.isfile(spec.provider_receipt), "Missing OpenSSL target provider receipt")
        spec.provider_receipt_sha256 = hash.sha256(spec.provider_receipt):lower()
    end
    -- Linker flags used while Configure builds its library objects, not UWVM's
    -- own archive/link inputs. The compiler receives the same target ABI flags.
    append_flags(spec.ldflags, config.get("openssl-ldflags"))
    json.mark_as_array(spec.ldflags)
    local identity = {recipe = "uwvm-openssl-source.1", source = hash.sha256(manifest),
        builder = hash.sha256(path.join(os.projectdir(), "tools/dependencies/build_openssl.py")), spec = spec}
    -- Xmake hashes strings as filenames. Hash an explicit file containing the
    -- serialized identity, then retain the same bytes beside the build spec.
    -- Ordered arrays avoid Lua map iteration changing the output directory on
    -- each fresh xmake process and forcing an unnecessary dependency rebuild.
    local serialized = json.encode({identity.recipe, identity.source, identity.builder,
        spec.platform, spec.arch, spec.cc, spec.ar, spec.ranlib, spec.cflags,
        spec.ldflags, spec.configure_target or "", spec.jobs, spec.float_abi,
        spec.no_asm, spec.provider_receipt or "", spec.provider_receipt_sha256 or ""})
    local identity_file = os.tmpfile() .. ".openssl-identity.json"
    io.writefile(identity_file, serialized)
    local key = hash.sha256(identity_file):lower():sub(1, 24)
    os.tryrm(identity_file)
    local output = path.absolute(path.join(config.builddir(), ".openssl", target:plat(), target:arch(), key))
    local specfile = path.join(output, "spec.json")
    os.mkdir(output)
    io.writefile(specfile, json.encode(spec))
    io.writefile(path.join(output, "identity.json"), serialized)
    target:data_set("uwvm.openssl", {source = source, output = output, specfile = specfile})
    target:add("includedirs", path.join(output, "include"))
    -- Absolute archive arguments prevent a missing cross artifact from silently
    -- resolving /usr/lib/libcrypto on the build host.
    target:add("links", path.join(output, "libssl.a"), path.join(output, "libcrypto.a"))
    if target:is_plat("linux", "unix", "bsd", "freebsd", "netbsd", "openbsd", "dragonflybsd", "sun", "cross") then
        target:add("syslinks", "pthread", "dl")
        if target:arch():startswith("arm") or target:arch():startswith("mips") then target:add("syslinks", "atomic") end
    elseif target:is_plat("mingw", "windows") then
        target:add("syslinks", "crypt32", "gdi32", "advapi32", "user32", "ws2_32")
    end
end

function build(target)
    local settings = assert(target:data("uwvm.openssl"), "Missing bundled OpenSSL target contract")
    local python = assert(find_tool("python3"), "Bundled OpenSSL requires Python 3.9+, Perl 5 and make")
    os.vrunv(python.program, {path.join(os.projectdir(), "tools/dependencies/build_openssl.py"),
        "--source-root", settings.source, "--spec", settings.specfile, "--output", settings.output})
end


-- File IO belongs to a target callback; xmake's project-description sandbox
-- deliberately has no global io table. Exact paths also prevent host fallback.
function prepare_prebuilt(target)
    local root = path.absolute(assert(config.get("openssl-root")))
    local header = path.join(root, "include/openssl/opensslv.h")
    assert(os.isfile(header), "Missing OpenSSL version header in --openssl-root")
    local version = io.readfile(header)
    local major = tonumber(version:match("#%s*define%s+OPENSSL_VERSION_MAJOR%s+(%d+)"))
    assert(major and major >= 3, "--openssl-root requires OpenSSL >= 3.0")
    local mode = config.get("static")
    local prefer_static = mode == "non-system" or mode == "compiler"
    local function resolve(directory, name)
        local names = prefer_static and {"lib" .. name .. ".a", "lib" .. name .. ".dll.a",
            "lib" .. name .. ".so", "lib" .. name .. ".dylib", name .. ".lib", "lib" .. name .. ".lib"} or
            {"lib" .. name .. ".so", "lib" .. name .. ".dylib", "lib" .. name .. ".dll.a",
            name .. ".lib", "lib" .. name .. ".lib", "lib" .. name .. ".a"}
        for _, name in ipairs(names) do
            local file = path.join(directory, name)
            if os.isfile(file) then return path.absolute(file) end
        end
    end
    local ssl, crypto
    for _, directory in ipairs({path.join(root, "lib"), path.join(root, "lib64"), root}) do
        ssl, crypto = resolve(directory, "ssl"), resolve(directory, "crypto")
        if ssl and crypto then break end
    end
    assert(ssl and crypto, "Missing target ssl/crypto libraries in --openssl-root (host fallback is forbidden)")
    target:add("includedirs", path.join(root, "include"))
    target:add("links", ssl, crypto)
    if target:is_plat("windows", "mingw") then
        target:add("syslinks", "crypt32", "gdi32", "advapi32", "user32", "ws2_32")
    elseif target:is_plat("linux", "unix", "bsd", "freebsd", "netbsd", "openbsd", "dragonflybsd", "sun", "cross") then
        target:add("syslinks", "pthread", "dl")
        if target:arch():startswith("arm") or target:arch():startswith("mips") then target:add("syslinks", "atomic") end
    end
end
