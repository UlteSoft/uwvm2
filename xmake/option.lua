option("march", function()
    set_description
    (
        [[Set the "-march" option for gcc and clang.]],
        "The option is automatically added if using our toolchain option.",
        [[    none: Don't set the "-march" option, use the default march of the toolchain.]],
        [[    default: Set the "-march" option as "-march=native" if possible, otherwise don't set the "-march" option and use the default march of the toolchain.]],
        [[    arch: Set the "-march" option as "-march=arch". Note that "arch" is any value other than "no" and "default".]]
    )
    set_default("default")
    after_check(function(option)
        import("utility.utility")
        option:add("cxflags", utility.get_march_option())
    end)
end)

option("sysroot", function()
    set_description
    (
        "Set the `--sysroot` option for gcc and clang.",
        "The option is automatically added if using our toolchain option.",
        [[    none: Don't set the "--sysroot" option, use the default sysroot of the toolchain.]],
        [[    detect: Detect and set the sysroot for clang, use the default sysroot for gcc.]],
        [[    path: Set the "--sysroot" option as "--sysroot=path". Note that "path" is an absolute path or a relative path other than "no" and "detect".]]
    )
    set_default("detect")
    after_check(function(option)
        import("utility.utility")
        local sysroot_option = utility.get_sysroot_option()
        for flag, opt in pairs(sysroot_option) do
            option:add(flag, opt)
        end
    end)
end)

option("target", function()
    set_description
    (
        "Set the `--target` option for gcc and clang.",
        "The option is automatically added if using our toolchain option.",
        [[    none: Don't set the "--target" option, use the default target of the toolchain.]],
        [[    detect: Detect and set the target for clang, use the default target for gcc.]],
        [[    triplet: Set the "--target" option as "--target=triplet".]]
    )
    set_default("detect")
end)

option("rtlib", function()
    set_description
    (
        [[Set the "-rtlib" option for clang.]],
        "The option is automatically added if using our toolchain option.",
        [[    default: Don't set the "rtlib" option, use the default rtlib of clang.]],
        [[    libgcc/compiler-rt/platform: Set the "rtlib" option.]]
    )
    set_default("default")
    set_values("default", "libgcc", "compiler-rt", "platform")
    after_check(function(option)
        import("utility.utility")
        local rtlib_option = utility.get_rtlib_option()
        for _, flag in ipairs({ "ldflags", "shflags" }) do
            option:add(flag, rtlib_option)
        end
    end)
end)

option("unwindlib", function()
    set_description
    (
        [[Set the "-unwindlib" option for clang."]],
        "The option is automatically added if using our toolchain option.",
        [[    default: Don't set the "unwindlib" option, use the default unwindlib of clang.]],
        [[    libgcc/libunwind/platform: Set the option "unwindlib" if rtlib is "compiler-rt".]],
        [[    force-libgcc/force-libunwind/force-platform: Always set the "unwindlib" option.]]
        )
    set_default("default")
    set_values("default", "libgcc", "libunwind", "platform", "force-libgcc", "force-libunwind", "force-platform")
    add_deps("rtlib")
    after_check(function(option)
        import("utility.utility")
        local unwind_option = utility.get_unwindlib_option()
        for _, flag in ipairs({ "ldflags", "shflags" }) do
            option:add(flag, unwind_option)
        end
    end)
end)

option("debug-strip", function()
    set_description
    (
        "Whether to strip the symbols while building with debug information.",
        [[    none: Don't strip the symbols if possible.]],
        [[    debug: Strip the debug symbols to a independent symbol file while keeping other symbols in the target file.]],
        [[    all: Strip the debug symbols to a independent symbol file then strip all symbols from the target file.]]
    )
    set_default("none")
    set_values("none", "debug", "all")
end)

option("enable-lto", function()
    set_description
    (
        "Whether to enable LTO while building in release/minsizerel/releasedbg mode.",
        "LTO is enabled by default, but when use different compiler and linker, " ..
        "for example, compiling with clang while linking with bfd, LTO should be disabled.",
        [[Errors may be reported as "file not recognized: file format not recognized".]]
    )
    set_default(true)
end)

option("static", function()
    set_description
    (
        "The static flag is used to enable static linking of libraries instead of dynamic linking.",
        "static linking is disable by default"
    )
    set_default(false)
end)

option("use-llvm", function()
    set_description
    (
        "Use Clang-CL under windows and clang on the rest of the platforms.",
        "default = false"
    )
    set_default(false)
end)

option("use-cxx-module", function()
    set_description
    (
        "Use CXX Module to compile.",
        "default = false"
    )
    set_default(false)
end)

option("enable-int", function()
    set_description
    (
        "enable interpreter",
        [[    none: disable interpreter.]],
        [[    defualt: use default interpreter.]],
        [[    uwvm-int: use uwvm interpreter.]]
    )
    set_default("default")
    set_values("none", "default", "uwvm-int")
end)

option("enable-jit", function()
    set_description
    (
        "enable just-in-time compilation",
        [[    none: disable jit.]],
        [[    defualt: use default jit engine.]],
        [[    llvm: use llvm jit engine.]]
    )
    set_default("default")
    set_values("none", "default", "llvm")
end)

-- WINDOWS

option("winmin", function()
    set_description
    (
        "Minimum (x86_64, i686, aarch64, arm) windows system required. Minimum value for _WIN32_WINNT, _WIN32_WINDOWS and WINVER.",
        [[    default: Use the default compiler macro definition. In general, MinGW is 0x0A00, MSVC is not defined.]],
        [[    WS25: Windows Server 2025, -D_WIN32_WINNT=0x0A00]],
        [[    WIN11: Windows 11, -D_WIN32_WINNT=0x0A00]],
        [[    WS22: Windows Server 2022, -D_WIN32_WINNT=0x0A00]],    
        [[    WS19: Windows Server 2019, -D_WIN32_WINNT=0x0A00]],    
        [[    WS16: Windows Server 2016, -D_WIN32_WINNT=0x0A00]],    
        [[    WIN10: Windows 10, -D_WIN32_WINNT=0x0A00]],
        [[    WS12R2: Windows Server 2012 R2, -D_WIN32_WINNT=0x0603]],    
        [[    WINBLUE: Windows 8.1, -D_WIN32_WINNT=0x0603]],
        [[    WS12: Windows Server 2012, -D_WIN32_WINNT=0x0602]],    
        [[    WIN8: Windows 8, -D_WIN32_WINNT=0x0602]],
        [[    WS08R2: Windows Server 2008 R2, -D_WIN32_WINNT=0x0601]],
        [[    WIN7: Windows 7, -D_WIN32_WINNT=0x0601]],
        [[    WS08: Windows Server 2008, -D_WIN32_WINNT=0x0600]],
        [[    VISTA: Windows Vista, -D_WIN32_WINNT=0x0600]],
        [[    WS03SP1: Windows Server 2003 with SP1, -D_WIN32_WINNT=0x0502]],
        [[    WINXPSP2: Windows XP with SP2, -D_WIN32_WINNT=0x0502]],
        [[    WS03: Windows Server 2003, -D_WIN32_WINNT=0x0501]],
        [[    WINXP: Windows XP, -D_WIN32_WINNT=0x0501]],
        [[    WS2K: Windows Server 2000, -D_WIN32_WINNT=0x0500]],
        [[    WIN2K: Windows 2000, -D_WIN32_WINNT=0x0500]],
        [[    WINME: Windows ME, -D_WIN32_WINDOWS=0x0490]],
        [[    WIN98: Windows 98, -D_WIN32_WINDOWS=0x0410]],
        [[    WIN95: Windows 95, -D_WIN32_WINDOWS=0x0400]],
        [[    NT400: Windows NT 4.0, -D_WINNT=0x0400]],
        [[    NT351: Windows NT 3.51, -D_WINNT=0x0351]],
        [[    NT350: Windows NT 3.5, -D_WINNT=0x0350]],
        [[    NT310: Windows NT 3.1, -D_WINNT=0x0310]],
        [[Errors may be reported as "version not recognized: Windows Version not recognized".]]
    )
    set_default("default")
    set_values
    (
        "default",
        "WS25", 
        "WIN11", 
        "WS22", 
        "WS19", 
        "WS16", 
        "WIN10",
        "WS12R2", 
        "WINBLUE",
        "WS12",
        "WIN8", 
        "WS08R2", 
        "WIN7",         
        "WS08", 
        "VISTA", 
        "WS03SP1", 
        "WINXPSP2",
        "WS03", 
        "WINXP", 
        "WS2K",
        "WIN2K",
        "WINME", 
        "WIN98", 
        "WIN95", 
        "NT400",
        "NT351",
        "NT350",
        "NT310"
    )
end)

-- uwvm Debug Option

option("debug-timer", function()
    set_description
    (
        "Add timer functionality to each module.",
        "default = false"
    )
    set_default(false)
end)

-- uwvm special build options

option("fno-exceptions", function()
    set_description
    (
        "The uwvm's wasi relies on the cpp exception. Turn off the cpp exception with this parameter so that it can be used in an environment where exceptions are forbidden. " ..
        "(This option causes the program to crash if some system calls fail, and it will be removed after herbception (P0709) enters the ios c++ standard)",
        "default = false"
    )
    set_default(false)
end)
