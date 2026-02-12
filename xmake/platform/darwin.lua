
function darwin_target()

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

    local strip_cfg = get_config("strip") or "default"
    local is_ident_strip = strip_cfg == "ident" or (strip_cfg == "default" and is_mode("minsizerel"))
    if is_ident_strip then
        add_cxflags("-fno-ident") -- also strip ident data
    end

    local static_link = get_config("static")
    if static_link then	
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

    -- Apple platform options (target OS and minimum version)
    local apple_platform = get_config("apple-platform")
    if apple_platform and apple_platform ~= "default" then
        -- Version mapping table
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
        
        local platform, version = apple_platform:match("^([^_]+)_(.+)$")
        if not platform or not version then
            -- Custom version format like "macos:10.15" or "ios:13.0"
            platform, version = apple_platform:match("^([^:]+):(.+)$")
            if not platform or not version then
                return
            end
        end
        
        -- Resolve version from predefined constants
        local resolved_version = version_table[apple_platform] or version
        
        -- Set target OS and minimum version based on platform
        if platform:upper() == "MACOS" then
            add_cxflags("-mtargetos=macos" .. resolved_version, {force = true})
            add_ldflags("-mtargetos=macos" .. resolved_version, {force = true})
        elseif platform:upper() == "IOS" then
            add_cxflags("-mtargetos=ios" .. resolved_version, {force = true})
            add_ldflags("-mtargetos=ios" .. resolved_version, {force = true})
        elseif platform:upper() == "TVOS" then
            add_cxflags("-mtargetos=tvos" .. resolved_version, {force = true})
            add_ldflags("-mtargetos=tvos" .. resolved_version, {force = true})
        elseif platform:upper() == "WATCHOS" then
            add_cxflags("-mtargetos=watchos" .. resolved_version, {force = true})
            add_ldflags("-mtargetos=watchos" .. resolved_version, {force = true})
        elseif platform:upper() == "VISIONOS" then
            add_cxflags("-mtargetos=visionos" .. resolved_version, {force = true})
            add_ldflags("-mtargetos=visionos" .. resolved_version, {force = true})
        end
    end

    -- dynamic libary loader
    add_syslinks("dl")
    --if use_llvm_toolchain then	
    --    add_syslinks("c++abi")
    --end

end
