if is_plat("wasm-emscripten") then
    -- Limited to i686, extended instruction set via march settings
    set_allowedarchs("wasm32", "wasm64")
end

function wasm_emscripten_target()
    set_extension(".wasm")

    set_toolchains("emcc")
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
    if not (not march or march == "none" or march == "default") then
        local march_target = "-march=" .. march
        add_cxflags(march_target)
    end

    -- dynamic libary loader
    --if use_llvm_toolchain then	
    --    add_syslinks("c++abi")
    --end

    if is_arch("wasm32") then
        add_cxflags("--target=wasm32-unknown-emscripten", {force = true})
        add_ldflags("--target=wasm32-unknown-emscripten", {force = true})
    elseif is_arch("wasm64") then
        add_cxflags("--target=wasm64-unknown-emscripten", {force = true})
        add_ldflags("--target=wasm64-unknown-emscripten", {force = true})
    end

    add_ldflags("-s STACK_SIZE=8388608", {force = true})

end
