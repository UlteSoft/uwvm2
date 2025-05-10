/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-30
 * @copyright   APL-2 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#include <memory>
#include <utility>

#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>

#ifdef UWVM_MODULE
import fast_io;
import fast_io_crypto;
import uwvm2.utils.ansies;
import uwvm2.utils.cmdline;
import uwvm2.parser.wasm.concepts;
import uwvm2.uwvm.io;
import uwvm2.uwvm.utils.ansies;
import uwvm2.uwvm.custom;
import uwvm2.uwvm.cmdline;
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.utils.install_path;
#else
# include <fast_io.h>
# include <fast_io_crypto.h>
# include <fast_io_dsal/tuple.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/custom/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/utils/install_path/impl.h>
#endif

namespace uwvm2::uwvm::cmdline::paras::details
{
    template <typename Stm, ::uwvm2::parser::wasm::concepts::has_feature_name F0, ::uwvm2::parser::wasm::concepts::has_feature_name... Fs>
    inline void version_print_wasm_feature_impl(Stm&& stm) noexcept
    {
        ::fast_io::io::perrln(::std::forward<Stm>(stm), u8"        ", F0::feature_name);
        if constexpr(sizeof...(Fs) != 0) { version_print_wasm_feature_impl<Stm, Fs...>(::std::forward<Stm>(stm)); }
    }

    template <typename Stm, ::uwvm2::parser::wasm::concepts::has_feature_name... Fs>
    inline void version_print_wasm_feature_from_tuple(Stm&& stm, ::fast_io::tuple<Fs...>) noexcept
    {
        version_print_wasm_feature_impl<Stm, Fs...>(::std::forward<Stm>(stm));
    }

    UWVM_GNU_COLD extern ::uwvm2::utils::cmdline::parameter_return_type version_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                         ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                         ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept
    {
        // No copies will be made here.
        auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::u8log_output)};
        // Add raii locks while unlocking operations
        ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
            ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
        // No copies will be made here.
        auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

        ::fast_io::io::perr(u8log_output_ul,
                                // logo
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL), 
                                u8"\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)), 
                                u8" ----------------------------------------- \n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                                u8"|",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(189, 37, 206)),
                                u8"  _   _  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(152, 37, 206)),
                                u8"__        __ ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(113, 37, 206)),
                                u8"__     __  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(70, 37, 206)),
                                u8"__  __  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                                u8"|\n|",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(189, 37, 206)),
                                u8" | | | | ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(152, 37, 206)),
                                u8"\\ \\      / / ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(113, 37, 206)),
                                u8"\\ \\   / / ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(70, 37, 206)),
                                u8"|  \\/  | ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                                u8"|\n|",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(189, 37, 206)),
                                u8" | | | | ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(152, 37, 206)),
                                u8" \\ \\ /\\ / /  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(113, 37, 206)),
                                u8" \\ \\ / /  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(70, 37, 206)),
                                u8"| |\\/| | ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                                u8"|\n|",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(189, 37, 206)),
                                u8" | |_| | ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(152, 37, 206)),
                                u8"  \\ V  V /     ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(113, 37, 206)),
                                u8"\\ V /   ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(70, 37, 206)),
                                u8"| |  | | ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                                u8"|\n|",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(189, 37, 206)),
                                u8"  \\___/ ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(152, 37, 206)),
                                u8"    \\_/\\_/    ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(113, 37, 206)),
                                u8"   \\_/    ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(70, 37, 206)),
                                u8"|_|  |_| ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                                u8"|\n|                                         |\n|",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(189, 37, 206)),
                                u8" Ultimate ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(152, 37, 206)),
                                u8"WebAssembly ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(113, 37, 206)),
                                u8" Virtual ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(70, 37, 206)),
                                u8"  Machine",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                                u8" |\n ----------------------------------------- \n\n",                        
                                // uwvm
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"Ultimate WebAssembly Virtual Machine\n",
        // Debug Mode
#ifdef _DEBUG
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                u8"(Debug Mode)\n",
#endif
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                // Copyright
                                u8"Copyright (c) 2025-present UlteSoft. All rights reserved.  "
        // Install Path
#if defined(UWVM_SUPPORT_INSTALL_PATH)
                                u8"\nInstall Path: ",
                                ::uwvm2::uwvm::utils::install_path::install_path.path_name,
#endif
                                // Version
                                u8"\nVersion: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                ::uwvm2::uwvm::custom::uwvm_version,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
#if defined(UWVM_GIT_COMMIT_DATA)
                                u8" (",
                                ::uwvm2::uwvm::custom::git_commit_data,
                                u8")"
#endif
#if defined(UWVM_GIT_COMMIT_DATA) || defined(UWVM_GIT_REMOTE_URL) || defined(UWVM_GIT_COMMIT_ID)
                                u8"\nGit: "
# ifdef UWVM_GIT_HAS_UNCOMMITTED_MODIFICATIONS
                                u8"(M) "
# endif
                                ,
                                ::uwvm2::uwvm::custom::git_commit_id,
                                u8" branch \'",
                                ::uwvm2::uwvm::custom::git_upstream_branch,
                                u8"\' of ",
                                ::uwvm2::uwvm::custom::git_remote_url,
#endif
                                // Compiler
                                u8"\nCompiler: "
#if defined(__clang__)
                                u8"LLVM clang "
# if defined(__clang_version__)
                                __clang_version__
# endif
#elif defined(__GNUC__)
                                u8"GCC "
# if defined(__VERSION__)
                                __VERSION__
# endif
#elif defined(_MSC_VER)
                                u8"Microsoft Visual C++ ",
                                _MSC_VER,
#elif defined(__INTELLISENSE__)
                                u8"Intel C++ Compiler"
#else
                                u8"Unknown C++ compiler"
#endif
                // std Lib
                                u8"\nC++ STD Library: "
#if defined(_LIBCPP_VERSION)
                                u8"LLVM libc++ ",
                                _LIBCPP_VERSION,
#elif defined(__GLIBCXX__)
                                u8"GNU C++ Library ",
                                _GLIBCXX_RELEASE,
                                u8" ",
                                __GLIBCXX__,
#elif defined(_MSVC_STL_UPDATE)
                                u8"Microsoft Visual C++ STL ",
                                _MSVC_STL_UPDATE,
#else
                                u8"Unknown C++ standard library",
#endif

                // architecture
                                u8"\nArchitecture: "
#if defined(__wasm__)
                                u8"WASM"
# if defined(__wasm32__)
                                u8"32"
# elif defined(__wasm64__)
                                u8"64"
# endif
#elif defined(__alpha__)
                                u8"DEC Alpha"
#elif defined(__arm64ec__) || defined(_M_ARM64EC)
                                u8"ARM64EC"
# if defined(__AARCH_BIG_ENDIAN)
                                u8"_BE"
# endif
#elif defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
                                u8"AArch64"
# if defined(__AARCH_BIG_ENDIAN)
                                u8"_BE"
# endif
#elif defined(__arm__) || defined(_M_ARM)
                                u8"ARM"
# if defined(__ARM_BIG_ENDIAN)
                                u8"EB"
# endif
#elif defined(__x86_64__) || defined(_M_AMD64)
                                u8"x86_64"
# if defined(__APX_F__)
                                u8" (APX)"
# endif
#elif defined(__i386__) || defined(_M_IX86)
# if defined(__MINGW32__) || defined(_MSC_VER)
                                u8"i686"
# elif defined(__DJGPP__)
                                u8"i586"
# else
                                u8"i386"
# endif
#elif defined(__BFIN__)
                                u8"Blackfin"
#elif defined(__convex__)
                                u8"Convex Computer"
#elif defined(__e2k__)
                                u8"E2K"
#elif defined(__IA64__) || defined(_M_IA64)
                                u8"Intel Itanium 64"
#elif defined(__loongarch64__)
                                u8"LoongArch64"
#elif defined(__loongarch__)
                                u8"LoongArch"
#elif defined(__m68k__) || defined(__mc68000__)
                                u8"Motorola 68k"
#elif defined(__MIPS64__) || defined(__mips64__)
                                u8"MIPS64"
# if defined(_MIPS_ARCH_MIPS64R2)
                                u8"R2"
# endif
#elif defined(__MIPS__) || defined(__mips__) || defined(_MIPS_ARCH)
                                u8"MIPS"
#elif defined(__HPPA__)
                                u8"HP/PA RISC"
#elif defined(__riscv)
                                u8"RISC-V"
# if defined(__riscv_xlen) && __riscv_xlen == 64
                                u8" 64"
# endif
#elif defined(__370__) || defined(__THW_370__)
                                u8"System/370"
#elif defined(__s390__) || defined(__s390x__)
                                u8"System/390"
#elif defined(__pdp11)
                                u8"PDP11"
#elif defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || defined(_ARCH_PPC64)
                                u8"PowerPC64"
# if defined(_LITTLE_ENDIAN)
                                u8"LE"
# endif
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
                                u8"PowerPC"
# if defined(_LITTLE_ENDIAN)
                                u8"LE"
# endif
# if defined(__SPE__)
                                u8"SPE"
# endif
#elif defined(__THW_RS6000) || defined(_IBMR2) || defined(_POWER) || defined(_ARCH_PWR) || defined(_ARCH_PWR2)
                                u8"RS/6000"
#elif defined(__CUDA_ARCH__)
                                u8"PTX"
#elif defined(__sparc__)
                                u8"SPARC"
# if defined(__sparcv8__)
                                u8"v8"
# endif
#elif defined(__sh__)
                                u8"SuperH"
#elif defined(__SYSC_ZARCH__)
                                u8"z/Architecture"
#elif defined(__AVR__)
                                u8"AVR"
#else
                                u8"Unknown Arch"
#endif

// ARM PROFILE
#if defined(__ARM_ARCH)
                                u8"-V",
                                __ARM_ARCH,
# if defined(__ARM_ARCH_PROFILE)
#  if __ARM_ARCH_PROFILE == 'A'
                                u8"A"
#  elif __ARM_ARCH_PROFILE == 'R'
                                u8"R"
#  else
                                u8"M"
#  endif
# endif
#endif
// SIMD
#if defined(__wasm_simd128__)
                                u8"\nSIMD support: WebAssembly SIMD 128"
#elif defined(__loongarch__) && (defined(__loongarch_sx) || UWVM_HAS_BUILTIN(__builtin_loongarch_crc32c_b))
                                u8"\nSIMD support: LoongSX "
# if defined(__loongarch_asx)
                                u8"LoongASX "
# endif
#elif defined(__ARM_NEON) || ((defined(_MSC_VER) && !defined(__clang__)) && defined(_M_ARM64) && !defined(_KERNEL_MODE))
                                /*
                                 * https://arm-software.github.io/acle/main/acle.html
                                 */
                                u8"\nSIMD support: NEON "
# if defined(__ARM_FEATURE_SVE)
                                u8"SVE "
# endif
# if defined(__ARM_FEATURE_SVE2)
                                u8"SVE2 "
# endif
#elif (defined(__x86_64__) || defined(_M_AMD64) || defined(__i386__) || defined(_M_IX86)) && defined(__MMX__)
                                u8"\nSIMD support: "
# if defined(__MMX__)
                                u8"MMX "
# endif
# if defined(__SSE__)
                                u8"SSE "
# endif
# if defined(__SSE2__)
                                u8"SSE2 "
# endif
# if defined(__SSE3__)
                                u8"SSE3 "
# endif
# if defined(__SSSE3__)
                                u8"SSSE3 "
# endif
# if defined(__SSE4_1__)
                                u8"SSE4.1 "
# endif
# if defined(__SSE4_2__)
                                u8"SSE4.2 "
# endif
# if defined(__FMA__)
                                u8"FMA "
# endif
# if defined(__BMI__)
                                u8"BMI "
# endif
# if defined(__BMI2__)
                                u8"BMI2 "
# endif
# if defined(__AVX__)
                                u8"AVX "
# endif
# if defined(__AVX2__)
                                u8"AVX2 "
# endif
# if defined(__MIC__)
                                u8"MIC "
# endif
# if defined(__AVX512F__)
                                u8"AVX512F "
# endif
# if defined(__AVX512BW__)
                                u8"AVX512BW "
# endif
# if defined(__AVX512VL__)
                                u8"AVX512VL "
# endif
# if defined(__AVX512DQ__)
                                u8"AVX512DQ "
# endif
# if 0
#  if defined(__AVX512BF16__)
                                u8"AVX512BF16 "
#  endif
#  if defined(__AVX512FP16__)
                                u8"AVX512FP16 "
#  endif
# endif
# if defined(__AVX512VBMI__)
                                u8"AVX512VBMI "
# endif
#elif defined(__VECTOR4DOUBLE__) || defined(__VSX__) || (defined(__ALTIVEC__) || defined(__VEC__))
                                u8"\nSIMD support: PPC SIMD"
#endif

                                // OS
                                u8"\nOS: "
#if defined(__CYGWIN__)
                                u8"Cygwin"
#elif defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__TOS_WIN__) || defined(__WINDOWS__) || defined(_WIN32_WINNT) ||                     \
    defined(_WIN32_WINDOWS) || defined(_WINNT) || defined(_WINDOWS)
                                u8"Microsoft Windows"
# if defined(_WIN32_WINDOWS)
                                u8" - "
#  if _WIN32_WINDOWS >= 0x0490
                                u8"Windows 9x 4.9 (WINME)"
#  elif _WIN32_WINDOWS >= 0x0410
                                u8"Windows 9x 4.1 (WIN98)"
#  elif _WIN32_WINDOWS >= 0x0400
                                u8"Windows 9x 4.0 (WIN95)"
#  endif
# elif defined(_WIN32_WINNT)
                                u8" - "
#  if _WIN32_WINNT >= 0x0A00
                                u8"Windows NT 10.0 (WS25, WIN11, WS22, WS19, WS16, WIN10)"
#  elif _WIN32_WINNT >= 0x0603
                                u8"Windows NT 6.3 (WS12R2, WINBLUE)"
#  elif _WIN32_WINNT >= 0x0602
                                u8"Windows NT 6.2 (WS12, WIN8)"
#  elif _WIN32_WINNT >= 0x0601
                                u8"Windows NT 6.1 (WS08R2, WIN7)"
#  elif _WIN32_WINNT >= 0x0600
                                u8"Windows NT 6.0 (WS08, VISTA)"
#  elif _WIN32_WINNT >= 0x0502
                                u8"Windows NT 5.2 (WS03SP1, WINXPSP2)"
#  elif _WIN32_WINNT >= 0x0501
                                u8"Windows NT 5.1 (WS03, WINXP)"
#  elif _WIN32_WINNT >= 0x0500
                                u8"Windows NT 5.0 (WS2K, WIN2K)"
#  endif
# elif defined(_WINNT)
#  if _WINNT >= 0x0400
                                u8"Windows NT 4.0"
#  elif _WINNT >= 0x0351
                                u8"Windows NT 3.51"
#  elif _WINNT >= 0x0350
                                u8"Windows NT 3.50"
#  elif _WINNT >= 0x0310
                                u8"Windows NT 3.10"
#  endif
# elif defined(_WINDOWS)
#  if _WINDOWS >= 0x0300
                                u8"Windows 3.0"
#  elif _WINDOWS >= 0x0200
                                u8"Windows 2.0"
#  elif _WINDOWS >= 0x0310
                                u8"Windows 1.0"
#  endif
# endif
#elif defined(__MSDOS__)
                                u8"Microsoft Dos"

#elif defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
                                u8"Linux"
#elif defined(__APPLE__) && defined(__MACH__)
# if defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
                                u8"iOS"
# else
                                u8"Mac OS"
# endif
#elif defined(__DragonFly__)
                                u8"DragonFlyBSD"
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
                                u8"FreeBSD"
#elif defined(__NetBSD__)
                                u8"NetBSD"
#elif defined(__OpenBSD__)
                                u8"OpenBSD"
#elif defined(BSD) || defined(_SYSTYPE_BSD)
                                u8"BSD"
#elif defined(__VMS)
                                u8"VMS"
#elif defined(__sun)
                                u8"Solaris (SunOS)"
#elif defined(__QNX__) || defined(__QNXNTO__)
                                u8"QNX"
#elif defined(__BEOS__)
                                u8"BeOS"
#elif defined(AMIGA) || defined(__amigaos__)
                                u8"AmigaOS"
#elif defined(_AIX) || defined(__TOS_AIX__)
                                u8"IBM AIX"
#elif defined(__OS400__)
                                u8"IBM OS/400"
#elif defined(__sgi)
                                u8"IRIX"
#elif defined(__hpux)
                                u8"HP-UX"
#elif defined(__HAIKU__)
                                u8"Haiku"
#elif defined(__unix) || defined(_XOPEN_SOURCE) || defined(_POSIX_SOURCE)
                                u8"Unix Environment"
#else
                                u8"Unknown OS"
#endif
                // C Library
                                u8"\nC Library: "
#if defined(__wasi__)
                                u8"WASI"
#elif defined(__MINGW32__) && !defined(_UCRT) && !defined(__BIONIC__)
                                u8"MSVCRT"
#elif (defined(_MSC_VER) || defined(_UCRT)) && !defined(__WINE__) && !defined(__CYGWIN__) && !defined(__BIONIC__)
                                u8"UCRT"
#elif defined(__APPLE__) || defined(__DARWIN_C_LEVEL)
                                u8"Darwin"
#elif defined(__GLIBC__)
                                u8"GNU LIBC ",
                                __GLIBC__,
                                u8".",
                                __GLIBC_MINOR__,
#elif defined(__BIONIC__)
                                u8"Bionic"
#elif defined(__cloudlibc__)
                                u8"cloudlibc"
#elif defined(__UCLIBC__)
                                u8"uClibc"
#elif defined(__OHOS__)
                                u8"OpenHarmony"
#elif defined(__CRTL_VER)
                                u8"VMS"
#elif defined(__LIBREL__)
                                u8"z/OS"
#elif defined(__AVR__) || defined(_PICOLIBC__)
                                u8"AVR LIBC"
#elif defined(__LLVM_LIBC__) || defined(__LLVM_LIBC_TYPES_FILE_H__)
                                u8"LLVM LIBC "
#elif defined(__MLIBC_O_CLOEXEC)
                                u8"MLIBC"
#elif defined(__wasi__)
                                u8"WASI"
#elif defined(__NEED___isoc_va_list) || defined(__musl__)
                                u8"MUSL"
#elif defined(__serenity__)
                                u8"serenity"
#elif defined(__DJGPP__)
                                u8"djgpp"
#elif defined(__BSD_VISIBLE) || (defined(__NEWLIB__) && !defined(__CUSTOM_FILE_IO__)) || defined(__MSDOS__)
                                u8"unix"
#else
                                u8"Unknown"
#endif
#ifdef _POSIX_C_SOURCE
                                u8"\nPOSIX: ",
                                _POSIX_C_SOURCE,
#endif
                                u8"\nAllocator: "
#if defined(FAST_IO_USE_CUSTOM_GLOBAL_ALLOCATOR)
                                u8"custom global"
#elif defined(FAST_IO_USE_MIMALLOC)
                                u8"mimalloc"
#elif (defined(__linux__) && defined(__KERNEL__)) || defined(FAST_IO_USE_LINUX_KERNEL_ALLOCATOR)
                                u8"linux kmalloc"
#elif ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && !defined(_LIBCPP_FREESTANDING)) ||                                      \
       defined(FAST_IO_ENABLE_HOSTED_FEATURES))
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__) && !defined(FAST_IO_USE_C_MALLOC)
#  if defined(_DEBUG) && defined(_MSC_VER)
                                u8"wincrt malloc dbg"
#  else
#   if defined(_WIN32_WINDOWS)
                                u8"win32 heapalloc"
#   else
                                u8"nt rtlallocateheap"
#   endif
#  endif
# else
#  if defined(_DEBUG) && defined(_MSC_VER)
                                u8"wincrt malloc dbg"
#  else
                                u8"c malloc"
#  endif
# endif
#else
                                u8"custom global"
#endif
                                u8"\n"
                                // Feature
                                u8"Feature:\n"
                                u8"    HWcrc32c: ",
                                ::fast_io::mnp::boolalpha(::fast_io::details::support_hardware_crc32c),
                                u8"\n"
                                u8"    CMDLHash: "
                                u8"HT=",
                                ::uwvm2::uwvm::cmdline::hash_table_size.hash_table_size,
                                u8", EX=",
                                ::uwvm2::uwvm::cmdline::hash_table_size.extra_size,
                                u8", RC=",
                                ::uwvm2::uwvm::cmdline::hash_table_size.real_max_conflict_size,
                                u8", SZ=",
                                ::uwvm2::uwvm::cmdline::hash_table_byte_sz,
                                u8"\n"
                                u8"    WebAssembly Features Supported: "
                                u8"\n"
            );
        // wasm feature
        version_print_wasm_feature_from_tuple(u8log_output_ul, ::uwvm2::uwvm::wasm::feature::all_features);
        // end ln
        ::fast_io::io::perrln(u8log_output_ul);

        return ::uwvm2::utils::cmdline::parameter_return_type::return_imme;
    }

}  // namespace uwvm2::uwvm::cmdline::paras::details

// macro
#include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
#include <uwvm2/utils/macro/pop_macros.h>
