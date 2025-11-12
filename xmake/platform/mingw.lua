
if is_plat("mingw") then
    -- Limited to i686, extended instruction set via march settings
    set_allowedarchs("x86_64", "i686", "aarch64", "arm", "arm64ec")
end

function mingw_target()

    local use_llvm_toolchain = get_config("use-llvm")
    if use_llvm_toolchain then	
        set_toolchains("clang")
        add_ldflags("-fuse-ld=lld", {force = true})
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

    local debug_strip = get_config("debug-strip")
    if debug_strip == "all" or is_mode("release", "releasedbg", "minsizerel") then
        add_cxflags("-fno-ident") -- also strip ident data
    end

    add_cxflags("-fno-rtti") -- disable rtti
    
    if not is_mode("debug") then
        add_cxflags("-fno-unwind-tables") -- disable unwind tables
        add_cxflags("-fno-asynchronous-unwind-tables") -- disable asynchronous unwind tables
    end

    if is_kind("binary") then
        set_extension(".exe")
    end

    local static_link = get_config("static")
    if static_link then	
        add_ldflags("-static", {force = true})
    end

    -- support all platforms
    add_syslinks("ws2_32")

    local opt_name = get_config("mingw-min")
    if opt_name == "default" then	
        -- same as WIN10
        add_defines("_WIN32_WINNT=0x0A00")
        add_defines("WINVER=0x0A00")
        -- MinGW NT 10.0 continues to use the FF from NT 6.0
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:6.00", {force = true})
        add_syslinks("ntdll")
        add_syslinks("shell32")
        local march = get_config("march")
        if march == "default" then
            -- use all native instruction
            add_cxflags("-march=native")
        end
    -- Windows NT (with win32 api)

    elseif 
        opt_name == "WS25" or 
        opt_name == "WIN11" or 
        opt_name == "WS22" or
        opt_name == "WS19" or
        opt_name == "WS16" or
        opt_name == "WIN10"
            then
        add_defines("_WIN32_WINNT=0x0A00")
        add_defines("WINVER=0x0A00")
        -- MinGW NT 10.0 continues to use the FF from NT 6.0
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:6.00", {force = true})
        add_syslinks("ntdll")
        add_syslinks("shell32")
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end
    elseif 
        opt_name == "WS12R2" or
        opt_name == "WINBLUE"
            then
        add_defines("_WIN32_WINNT=0x0603")
        add_defines("WINVER=0x0603")
        -- MinGW NT 6.3 continues to use the FF from NT 6.0.
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:6.00", {force = true})
        add_syslinks("ntdll")
        add_syslinks("shell32")
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end
    elseif 
        opt_name == "WS12" or 
        opt_name == "WIN8" 
            then
        add_defines("_WIN32_WINNT=0x0602")
        add_defines("WINVER=0x0602")
        -- MinGW NT 6.2 continues to use the FF from NT 6.0.
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:6.00", {force = true})
        add_syslinks("ntdll")
        add_syslinks("shell32")
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end
    elseif 
        opt_name == "WS08R2" or
        opt_name == "WIN7"
            then
        add_defines("_WIN32_WINNT=0x0601")
        add_defines("WINVER=0x0601")
        -- MinGW NT 6.1 continues to use the FF from NT 6.0.
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:6.00", {force = true})
        add_syslinks("ntdll")
        add_syslinks("shell32")
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end
    elseif 
        opt_name == "WS08" or
        opt_name == "VISTA" 
            then
        add_defines("_WIN32_WINNT=0x0600")
        add_defines("WINVER=0x0600")
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:6.00", {force = true})
        add_syslinks("ntdll")
        add_syslinks("shell32")
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end
    elseif 
        opt_name == "WS03" or
        opt_name == "WINXP64"
            then
        add_defines("_WIN32_WINNT=0x0502")
        add_defines("WINVER=0x0502")
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:5.02", {force = true})
        add_syslinks("ntdll")
        add_syslinks("shell32")
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end
    elseif 
        opt_name == "WINXP"
            then
        add_defines("_WIN32_WINNT=0x0501")
        add_defines("WINVER=0x0501")
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:5.01", {force = true})
        add_syslinks("ntdll")
        add_syslinks("shell32")
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end
    elseif 
        opt_name == "WS2K" or
        opt_name == "WIN2K"
            then
        add_defines("_WIN32_WINNT=0x0500")
        add_defines("WINVER=0x0500")
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:5.00", {force = true})
        add_syslinks("ntdll")
        add_syslinks("shell32")
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end

    -- Windows 9x (with win32 api).

    elseif opt_name == "WINME" then
        add_undefines("_WIN32_WINNT")
        add_defines("_WIN32_WINDOWS=0x0490")
        add_defines("WINVER=0x0490")
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:4.90", {force = true})
        add_syslinks("msvcrt")
        add_ldflags("-static", {force = true}) -- Forced static linking on win9x
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end
    elseif opt_name == "WIN98" then
        add_undefines("_WIN32_WINNT")
        add_defines("_WIN32_WINDOWS=0x0410")
        add_defines("WINVER=0x0410")
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:4.10", {force = true})
        add_syslinks("msvcrt")
        add_ldflags("-static", {force = true}) -- Forced static linking on win9x
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end
    elseif opt_name == "WIN95" then
        add_undefines("_WIN32_WINNT")
        add_defines("_WIN32_WINDOWS=0x0400")
        add_defines("WINVER=0x0400")
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:4.00", {force = true})
        add_syslinks("msvcrt")
        add_ldflags("-static", {force = true}) -- Forced static linking on win9x
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end

    -- Windows NT (without win32 api)，unverified.

    elseif opt_name == "NT400" then -- unverified
        add_defines("_WIN32_WINNT=0x0400")
        add_defines("WINVER=0x0400")
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:4.00", {force = true})
        add_syslinks("ntdll")
        add_syslinks("msvcrt")
        add_ldflags("-static", {force = true}) -- Forced static linking on win9x
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end
    elseif opt_name == "NT351" then -- unverified
        add_defines("_WIN32_WINNT=0x0351")
        add_defines("WINVER=0x0351")
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:3.51", {force = true})
        add_syslinks("ntdll")
        add_ldflags("-static", {force = true}) -- Forced static linking on win9x
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end
    elseif opt_name == "NT350" then -- unverified
        add_defines("_WIN32_WINNT=0x0350")
        add_defines("WINVER=0x0350")
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:3.50", {force = true})
        add_syslinks("ntdll")
        add_ldflags("-static", {force = true}) -- Forced static linking on win9x
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end
    elseif opt_name == "NT310" then -- unverified
        add_defines("_WIN32_WINNT=0x0310")
        add_defines("WINVER=0x0310")
        add_ldflags("-Wl,--stack,8388608", "-Wl,--subsystem,console:3.10", {force = true})
        add_syslinks("ntdll")
        add_ldflags("-static", {force = true}) -- Forced static linking on win9x
        local march = get_config("march")
        if march == "default" and is_arch("i686") then
            -- clang will automatically generate sse2 code on ix86 series, here use MinGW minimum support isa: i686
            -- MinGW only supports 32-bit processors of the x86 family.
            add_cxflags("-march=i686")
        end

    -- Windows 1.0 2.0 3.0 are not supported because it only supports x86 (16-bit).

    else
        error("invalid value")
    end

    local march = get_config("march")
    if not (not march or march == "none" or march == "default") then
        local march_target = "-march=" .. march
        add_cxflags(march_target)
    end
end