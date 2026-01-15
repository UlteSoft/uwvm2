if is_plat("wasm-wasi", "wasm-wasip1", "wasm-wasip2", "wasm-wasip3","wasm-wasi-threads", "wasm-wasip1-threads", "wasm-wasip2-threads", "wasm-wasip3-threads") then
    -- Limited to i686, extended instruction set via march settings
    set_allowedarchs("wasm32", "wasm64")
end

function wasm_wasi_target()
    set_extension(".wasm")

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
    add_syslinks("dl")
    --if use_llvm_toolchain then	
    --    add_syslinks("c++abi")
    --end

    if is_arch("wasm32") then
		if is_plat("wasm-wasi") then
			add_cxflags("--target=wasm32-wasi", {force = true})
			add_ldflags("--target=wasm32-wasi", {force = true})
		elseif is_plat("wasm-wasip1") then
			add_cxflags("--target=wasm32-wasip1", {force = true})
			add_ldflags("--target=wasm32-wasip1", {force = true})
		elseif is_plat("wasm-wasip2") then
			add_cxflags("--target=wasm32-wasip2", {force = true})
			add_ldflags("--target=wasm32-wasip2", {force = true})
		elseif is_plat("wasm-wasi-threads") then
			add_cxflags("--target=wasm32-wasi-threads", {force = true})
			add_ldflags("--target=wasm32-wasi-threads", {force = true})
			--add_defines("UWVM_ENABLE_WASI_THREADS")
		elseif is_plat("wasm-wasip1-threads") then
			add_cxflags("--target=wasm32-wasip1-threads", {force = true})
			add_ldflags("--target=wasm32-wasip1-threads", {force = true})
			--add_defines("UWVM_ENABLE_WASI_THREADS")
		elseif is_plat("wasm-wasip2-threads") then
			add_cxflags("--target=wasm32-wasip2-threads", {force = true})
			add_ldflags("--target=wasm32-wasip2-threads", {force = true})
			--add_defines("UWVM_ENABLE_WASI_THREADS")
		end
	elseif is_arch("wasm64") then
		if is_plat("wasm-wasi") then
			add_cxflags("--target=wasm64-wasi", {force = true})
			add_ldflags("--target=wasm64-wasi", {force = true})
		elseif is_plat("wasm-wasip1") then
			add_cxflags("--target=wasm64-wasip1", {force = true})
			add_ldflags("--target=wasm64-wasip1", {force = true})
		elseif is_plat("wasm-wasip2") then
			add_cxflags("--target=wasm64-wasip2", {force = true})
			add_ldflags("--target=wasm64-wasip2", {force = true})
		elseif is_plat("wasm-wasi-threads") then
			add_cxflags("--target=wasm64-wasi-threads", {force = true})
			add_ldflags("--target=wasm64-wasi-threads", {force = true})
			--add_defines("UWVM_ENABLE_WASI_THREADS")
		elseif is_plat("wasm-wasip1-threads") then
			add_cxflags("--target=wasm64-wasip1-threads", {force = true})
			add_ldflags("--target=wasm64-wasip1-threads", {force = true})
			--add_defines("UWVM_ENABLE_WASI_THREADS")
		elseif is_plat("wasm-wasip2-threads") then
			add_cxflags("--target=wasm64-wasip2-threads", {force = true})
			add_ldflags("--target=wasm64-wasip2-threads", {force = true})
			--add_defines("UWVM_ENABLE_WASI_THREADS")
		end
	end

	-- The default WASM memory allocation is too small; here we set it to 8MB.
	add_ldflags("-Wl,-z,stack-size=8388608", {force = true})

end