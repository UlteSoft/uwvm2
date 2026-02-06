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

option("llvm-target", function()
    set_description
    (
        "Set the `--llvm-target` option for clang.",
        "The option is automatically added if using our toolchain option.",
        [[    none: Don't set the "--llvm-target" option, use the default target of the toolchain.]],
        [[    detect: Detect and set the target for clang.]],
        [[    triplet: Set the "--llvm-target" option as "--llvm-target=triplet".]]
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

option("stdlib", function()
    set_description
    (
        "Select the C++ standard library implementation.",
        "The option is automatically added if using our toolchain option.",
        [[    default: Use the toolchain's default C++ standard library.]],
        [[    libstdc++: Prefer GNU libstdc++.]],
        [[    libc++: Prefer LLVM libc++.]]
    )
    set_default("default")
    set_values("default", "libstdc++", "libc++")
end)

option("strip", function()
    set_description
    (
        "Strip level.",
        [[    default: Use per-mode defaults.]],
        [[             debug/release/releasedbg: none]],
        [[             minsizerel: ident]],
        [[    none: Don't strip symbols.]],
        [[    symbol: Strip symbols.]],
        [[    ident: Strip symbols + strip ident strings (e.g. -fno-ident).]]
    )
    set_default("default")
    set_values("default", "none", "symbol", "ident")
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

option("enable-debug-int", function()
    set_description
    (
        "enable debug interpreter",
        "default = true"
    )
    set_default(true)
end)

option("enable-uwvm-int-heavy-combine-ops", function()
    set_description
    (
        "Enable heavy combined opcodes for uwvm-int (dense_compute / rare fusion patterns).",
        "default = true"
    )
    set_default(true)
end)

option("detailed-debug-check", function()
    set_description
    (
        "Initiates a more detailed debug checking mode in debug mode",
        "default = true"
    )
    set_default(true)
end)

-- WINDOWS

option("mingw-min", function()
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
        [[    WS03: Windows Server 2003, -D_WIN32_WINNT=0x0502]],
        [[    WINXP64: Windows XP 64bit, -D_WIN32_WINNT=0x0502]],
        [[    WINXP: Windows XP, -D_WIN32_WINNT=0x0501]],
        [[    WS2K: Windows Server 2000, -D_WIN32_WINNT=0x0500]],
        [[    WIN2K: Windows 2000, -D_WIN32_WINNT=0x0500]],
        [[    WINME: Windows ME, -D_WIN32_WINDOWS=0x0490]],
        [[    WIN98: Windows 98, -D_WIN32_WINDOWS=0x0410]],
        [[    WIN95: Windows 95, -D_WIN32_WINDOWS=0x0400]],
        [[    NT400: Windows NT 4.0, -D_WIN32_WINNT=0x0400]],
        [[    NT351: Windows NT 3.51, -D_WIN32_WINNT=0x0351]],
        [[    NT350: Windows NT 3.5, -D_WIN32_WINNT=0x0350]],
        [[    NT310: Windows NT 3.1, -D_WIN32_WINNT=0x0310]],
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
        "WS03", 
        "WINXP64",
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

-- APPLE PLATFORM

option("apple-platform", function()
    set_description
    (
        "Apple platform target and minimum version. Sets -mtargetos and -m*-version-min options.",
        [[    default:]],
        [[        Use the default compiler behavior.]],
        [[]],
        [[    macOS versions:]],
        [[        MACOS_SEQUOIA: macOS 15.0 Sequoia]],
        [[        MACOS_SONOMA: macOS 14.0 Sonoma]],
        [[        MACOS_VENTURA: macOS 13.0 Ventura]],
        [[        MACOS_MONTEREY: macOS 12.0 Monterey]],
        [[        MACOS_BIG_SUR: macOS 11.0 Big Sur]],
        [[        MACOS_CATALINA: macOS 10.15 Catalina]],
        [[        MACOS_MOJAVE: macOS 10.14 Mojave]],
        [[        MACOS_HIGH_SIERRA: macOS 10.13 High Sierra]],
        [[        MACOS_SIERRA: macOS 10.12 Sierra]],
        [[        MACOS_EL_CAPITAN: macOS 10.11 El Capitan]],
        [[        MACOS_YOSEMITE: macOS 10.10 Yosemite]],
        [[]],
        [[    iOS versions:]],
        [[        IOS_18: iOS 18.0]],
        [[        IOS_17: iOS 17.0]],
        [[        IOS_16: iOS 16.0]],
        [[        IOS_15: iOS 15.0]],
        [[        IOS_14: iOS 14.0]],
        [[        IOS_13: iOS 13.0]],
        [[        IOS_12: iOS 12.0]],
        [[        IOS_11: iOS 11.0]],
        [[]],
        [[    tvOS versions:]],
        [[        TVOS_18: tvOS 18.0]],
        [[        TVOS_17: tvOS 17.0]],
        [[        TVOS_16: tvOS 16.0]],
        [[        TVOS_15: tvOS 15.0]],
        [[        TVOS_14: tvOS 14.0]],
        [[        TVOS_13: tvOS 13.0]],
        [[]],
        [[    watchOS versions:]],
        [[        WATCHOS_11: watchOS 11.0]],
        [[        WATCHOS_10: watchOS 10.0]],
        [[        WATCHOS_9: watchOS 9.0]],
        [[        WATCHOS_8: watchOS 8.0]],
        [[        WATCHOS_7: watchOS 7.0]],
        [[]],
        [[    visionOS versions:]],
        [[        VISIONOS_2: visionOS 2.0]],
        [[        VISIONOS_1: visionOS 1.0]],
        [[]],
        [[    Custom format:]],
        [[        "platform:version" (e.g., "macos:10.15", "ios:13.0")]]
    )
    set_default("default")
    set_values
    (
        "default",
        -- macOS
        "MACOS_SEQUOIA", "MACOS_SONOMA", "MACOS_VENTURA", "MACOS_MONTEREY", 
        "MACOS_BIG_SUR", "MACOS_CATALINA", "MACOS_MOJAVE", "MACOS_HIGH_SIERRA",
        "MACOS_SIERRA", "MACOS_EL_CAPITAN", "MACOS_YOSEMITE",
        -- iOS
        "IOS_18", "IOS_17", "IOS_16", "IOS_15", "IOS_14", "IOS_13", "IOS_12", "IOS_11",
        -- tvOS
        "TVOS_18", "TVOS_17", "TVOS_16", "TVOS_15", "TVOS_14", "TVOS_13",
        -- watchOS
        "WATCHOS_11", "WATCHOS_10", "WATCHOS_9", "WATCHOS_8", "WATCHOS_7",
        -- visionOS
        "VISIONOS_2", "VISIONOS_1"
    )
end)

-- uwvm test option

option("test-libfuzzer", function()
    set_description
    (
        "Test libfuzzer with fuzzing mode. (Currently only supports the LLVM environment.)",
        "default = false",
        [[    true: Test libfuzzer with fuzzing mode.]],
        [[    false: Don't test libfuzzer with fuzzing mode.]]
    )
    set_default(false)
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

option("use-multithread-allocator-memory", function()
    set_description
    (
        "Enable on platforms that do not support mmap but support multithreading.",
        "default = false"
    )
    set_default(false)
end)

option("disable-local-imported-wasip1", function()
    set_description
    (
        "Disable the import of wasip1 during compilation. For platforms that do not support wasip1, this option is ignored.",
        "default = false"
    )
    set_default(false)
end)
