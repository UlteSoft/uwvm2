function uwvm_static_mode_is_compiler()
    return get_config("static") == "compiler"
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
