
if is_plat("linux") then
    set_allowedarchs("i386", "x86_64", "arm", "arm64", "riscv", "riscv64", "loong64", "s390x",
        "mips", "mips64", "ppc", "ppc64", "sparc", "sparc64", "hppa", "hppa64", "alpha",
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

        -- lld does not support the PPC64 ELFv1 ABI used by big-endian Linux, and
        -- 32-bit PowerPC glibc linker scripts use absolute paths that bfd resolves
        -- through sysroot correctly. SPARC64 also uses relocations in GCC startup
        -- objects that current lld rejects. Use the system linker for these families.
        if not is_powerpc_family() and not is_sparc_family() then
            add_ldflags("-fuse-ld=lld", {force = true})
        end
    end

    local sysroot_para = get_config("sysroot")
    if sysroot_para ~= "detect" and sysroot_para then
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
    
    if not is_mode("debug") then
        add_cxflags("-fno-unwind-tables") -- disable unwind tables
        add_cxflags("-fno-asynchronous-unwind-tables") -- disable asynchronous unwind tables
    end

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
