function uwvm_static_mode_is_compiler()
    return get_config("static") == "compiler"
end

function uwvm_add_native_unwind_cxflags()
    local execution_jit = get_config("execution-jit")
    if execution_jit == "llvm" or execution_jit == "default" then
        -- Native Wasm backtraces cross C++ trap/import/host-API frames, including
        -- helpers inlined or moved across translation units by LTO. JIT uwtable
        -- attributes alone cannot describe those host frames. Keep asynchronous
        -- CFI throughout native JIT targets even with C++ exceptions disabled;
        -- do not append the opposite -fno-* flags later in the platform setup.
        add_cxflags("-fasynchronous-unwind-tables", {force = true})
    elseif not is_mode("debug") then
        add_cxflags("-fno-unwind-tables")
        add_cxflags("-fno-asynchronous-unwind-tables")
    end
end

includes("windows.lua")
includes("mingw.lua")
includes("linux.lua")
includes("darwin.lua")
includes("djgpp.lua")
includes("bsd.lua")
includes("wasm-wasi.lua")
includes("wasm-emscripten.lua")
includes("cygwin.lua")
includes("none.lua")
