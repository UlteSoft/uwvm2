
if is_plat("linux") then
    set_allowedarchs("i386", "x86_64", "arm", "arm64", "riscv", "riscv64", "loong64", "s390x",
        "mips", "mipsel", "mips64", "mips64el", "ppc", "ppc64", "sparc", "sparc64", "hppa", "hppa64", "alpha",
        "sh", "sh4", "m68k", "xtensa", "csky", "hexagon", "arc", "arceb", "or1k", "nios2",
        "microblaze")
end

function linux_target()

    local function triple_is_powerpc(triple)
        if not triple or triple == "detect" then
            return false
        end
        return triple:find("^powerpc") ~= nil or triple:find("^ppc") ~= nil
    end

    local function triple_is_powerpc32(triple)
        if not triple or triple == "detect" then
            return false
        end
        return triple:find("^powerpc%-") ~= nil or triple:find("^ppc%-") ~= nil
    end

    local function triple_is_powerpc64le(triple)
        if not triple or triple == "detect" then
            return false
        end
        triple = triple:lower()
        return triple:find("^powerpc64le") ~= nil or triple:find("^ppc64le") ~= nil
    end

    local function triple_is_sparc(triple)
        if not triple or triple == "detect" then
            return false
        end
        return triple:find("^sparc") ~= nil
    end

    local function is_powerpc_family()
        if is_arch("ppc") or is_arch("ppc64") then
            return true
        end

        if triple_is_powerpc(get_config("target")) or triple_is_powerpc(get_config("llvm-target")) then
            return true
        end

        return false
    end

    local function is_powerpc32_family()
        if is_arch("ppc") then
            return true
        end

        if triple_is_powerpc32(get_config("target")) or triple_is_powerpc32(get_config("llvm-target")) then
            return true
        end

        return false
    end

    local function is_sparc_family()
        if is_arch("sparc") or is_arch("sparc64") then
            return true
        end

        if triple_is_sparc(get_config("target")) or triple_is_sparc(get_config("llvm-target")) then
            return true
        end

        return false
    end

    local use_llvm_compiler = get_config("use-llvm-compiler")
    if use_llvm_compiler then
        set_toolchains("clang")

        -- LLVM's MIPS backend disables tail calls by default, independently of
        -- Clang accepting [[clang::musttail]]. The interpreter requires bounded
        -- host stack depth even at -O0; never work around this by dropping that
        -- attribute. Internal opfunc visibility is handled in their macros.
        local mips_target = get_config("target") or ""
        local mips_llvm_target = get_config("llvm-target") or ""
        if is_arch("mips", "mipsel", "mips64", "mips64el") or mips_target:lower():find("^mips") or
            mips_llvm_target:lower():find("^mips") then
            add_cxflags("-mllvm -mips-tail-calls", {force = true})
            -- Full LLVM static consumers exceed the signed 16-bit GOT window.
            add_cxflags("-mxgot", {force = true})
            -- Keep the large runtime translation unit's unused local text and
            -- data out of the final GOT when the linker collects sections.
            add_cxflags("-ffunction-sections", "-fdata-sections", {force = true})
        end

        -- lld does not support the PPC64 ELFv1 ABI used by big-endian Linux, and
        -- 32-bit PowerPC glibc linker scripts use absolute paths that bfd resolves
        -- through sysroot correctly. SPARC64 also uses relocations in GCC startup
        -- objects that current lld rejects. Use the system linker for these families.
        local function mips64_linker_arch_from_triple(triple)
            if triple == "" or triple == "detect" then
                return nil
            end
            triple = triple:lower()
            if triple:find("^mips64el%-") then
                return "mips64el"
            elseif triple:find("^mips64%-") then
                return "mips64"
            end
            return false
        end

        -- The actual compiler target takes precedence over the LLVM target and
        -- architecture aliases when choosing the N64 linker's byte order.
        local mips64_linker_arch = mips64_linker_arch_from_triple(mips_target)
        if mips64_linker_arch == nil then
            mips64_linker_arch = mips64_linker_arch_from_triple(mips_llvm_target)
        end
        if mips64_linker_arch == nil then
            if is_arch("mips64el") then
                mips64_linker_arch = "mips64el"
            elseif is_arch("mips64") then
                mips64_linker_arch = "mips64"
            end
        end
        local mips64_native_debug = (get_config("linux-native-debug") or get_config("linux-x64-native-debug")) and
            mips64_linker_arch
        if mips64_native_debug then
            -- N64 static LLVM consumers still overflow BFD's local GOT_PAGE
            -- range with -mxgot. Current glibc startup notes also conflict with
            -- LLD's noexecstack policy. Use the qualified MIPS gold linker;
            -- keep the final stack non-executable. This mixed linker profile
            -- disables LTO in the release-family rules.
            add_ldflags("-fuse-ld=gold", "--ld-path=" .. mips64_linker_arch .. "-linux-gnuabi64-ld.gold",
                "-Wl,-z,noexecstack", "-Wl,--gc-sections", {force = true})
        elseif not is_powerpc_family() and not is_sparc_family() then
            add_ldflags("-fuse-ld=lld", {force = true})
        end
    end

    local sysroot_para = get_config("sysroot")
    if sysroot_para ~= "detect" and sysroot_para ~= "none" and sysroot_para ~= "no" and sysroot_para then
        local sysroot_cvt = "--sysroot=" .. sysroot_para
        add_cxflags(sysroot_cvt, {force = true})
        add_ldflags(sysroot_cvt, {force = true})
    end

    local target_para = get_config("target")
    if target_para ~= "detect" and target_para then
        local target_cvt = "--target=" .. target_para
        add_cxflags(target_cvt, {force = true})
        add_ldflags(target_cvt, {force = true})
    end

    local strip_cfg = get_config("strip") or "default"
    local is_ident_strip = strip_cfg == "ident" or (strip_cfg == "default" and is_mode("minsizerel"))
    if is_ident_strip then
        add_cxflags("-fno-ident") -- also strip ident data
    end

    if uwvm_static_mode_is_compiler() then
        add_ldflags("-static", {force = true})
    end

    add_cxflags("-fno-rtti") -- disable rtti

    if triple_is_powerpc64le(get_config("cross")) or triple_is_powerpc64le(get_config("target")) or
        triple_is_powerpc64le(get_config("llvm-target")) then
        -- GCC's IEEE long-double ABI makes `__float128` and `long double` the same C++ type on
        -- powerpc64le.  Hide only the duplicate compiler spelling from header feature detection;
        -- the ordinary long-double path retains the complete binary128 formatting support.
        add_cxflags("-U__SIZEOF_FLOAT128__", "-U__FLOAT128__", {force = true})
    end
    
    uwvm_add_native_unwind_cxflags()

    local march = get_config("march")
    if not march or march == "none" then
    elseif march == "default" then
        add_cxflags("-march=native")
    else
        local march_target = "-march=" .. march
        add_cxflags(march_target)
    end

    -- dynamic libary loader
    add_syslinks("dl")
    if is_powerpc32_family() then
        add_syslinks("atomic")
    end
    --if use_llvm_compiler then	
    --    add_syslinks("c++abi")
    --end

end
