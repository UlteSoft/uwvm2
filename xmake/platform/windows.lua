
-- There is no need to set up an allowed arch that comes with the msvc

function windows_target()

    local use_llvm_toolchain = get_config("use-llvm")
    if use_llvm_toolchain then	
        set_toolchains("clang")

        add_ldflags("-fuse-ld=lld", {force = true})

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
        
        local static_link = get_config("static")

        if is_mode("debug") then    
            if static_link then	
                set_runtimes("MTd")
            else
                set_runtimes("MDd")
            end
        else
            if static_link then	
                set_runtimes("MT")
            else
                set_runtimes("MD")
            end
        end

        add_cxflags("-fno-rtti") -- disable rtti
        
        if not is_mode("debug") then
            add_cxflags("-fno-unwind-tables") -- disable unwind tables
            add_cxflags("-fno-asynchronous-unwind-tables") -- disable asynchronous unwind tables
        end
    
        if is_kind("binary") then
            set_extension(".exe")
        end
    
        if static_link then	
            add_ldflags("-static", {force = true})
        end

        -- support all platforms
        add_syslinks("ws2_32")

        -- The windows-msvc toolchain defines its supported platforms and API levels through the Windows SDK. Currently, it only supports WINNT 10.0 (Windows 10 / Windows 11).
        add_defines("_WIN32_WINNT=0x0A00")
        add_defines("WINVER=0x0A00")
        add_ldflags("-Wl,/STACK:8388608", "-Wl,/SUBSYSTEM:CONSOLE", {force = true})
        add_syslinks("ntdll")
        add_syslinks("shell32")

        local march = get_config("march")
        if march == "default" then
            -- use all native instruction
            add_cxflags("-march=native")
        end
        if not (not march or march == "none" or march == "default") then
            local march_target = "-march=" .. march
            add_cxflags(march_target)
        end    
    else -- msvc

        error("uwvm2 does not currently support the msvc compiler.")

        if is_kind("binary") then
            set_extension(".exe")
        end
    
        if is_mode("debug") then
            add_cxflags("/guard:cf") -- enable Control Flow Guard
        end
    
        add_cxflags("-GR-") -- disable rtti
    
        local static_link = get_config("static")
    
        if is_mode("debug") then
            add_cxflags("-GS") -- enable buffer overflow check
    
            if static_link then	
                set_runtimes("MTd")
            else
                set_runtimes("MDd")
            end
        else
            add_cxflags("-GS-") -- disable buffer overflow check
            add_cxflags("-GL") -- Whole Program Optimization
            --add_ldflags("-LTCG") -- Link Time Code Generation
    
            if static_link then	
                set_runtimes("MT")
            else
                set_runtimes("MD")
            end
        end
    
        -- support all platforms
        add_syslinks("ws2_32")

        -- The windows-msvc toolchain defines its supported platforms and API levels through the Windows SDK. Currently, it only supports WINNT 10.0 (Windows 10 / Windows 11).
        add_defines("_WIN32_WINNT=0x0A00")
        add_defines("WINVER=0x0A00")
        add_ldflags("-STACK:8388608", "-SUBSYSTEM:CONSOLE", {force = true})
        add_syslinks("ntdll")
        add_syslinks("shell32")
        -- Do not set console version=10.00.
        -- This option may be changed in a future update of the NT kernel major version.
        
        local march = get_config("march")
        if not (march == "no" or march == "default") then
            error("windows (unknown-windows-msvc) does not support custom march!")
        end    
    end
end