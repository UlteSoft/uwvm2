function bsd_target()

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

    -- dynamic libary loader
    add_syslinks("dl")
    --if use_llvm_toolchain then	
    --    add_syslinks("c++abi")
    --end

end
