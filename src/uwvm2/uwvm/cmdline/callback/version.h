/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <memory>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// platform
# if defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
#  include <linux/version.h>
# endif
# if defined(__APPLE__) || defined(__DARWIN_C_LEVEL)
#  include <TargetConditionals.h>
#  include <Availability.h>
# endif
# if (defined(__MIPS__) || defined(__mips__) || defined(_MIPS_ARCH))
#  include <sgidefs.h>
# endif
// imported
# include <fast_io.h>
# include <fast_io_crypto.h>
# include <uwvm2/utils/container/impl.h>
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

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
    template <typename Stm>
    inline constexpr void logo_u8print_not_rst_impl(Stm && stm) noexcept
    {
        // Preventing the wasm32-wasi-uwvm stack from being too small and causing an overflow requires limiting the size of the contents of each print
        // Platforms like GitHub CI lose color information when line breaks occur.
        ::fast_io::io::perr(::std::forward<Stm>(stm),
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
                            u8"|\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                            u8"|",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(189, 37, 206)),
                            u8" | | | | ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(152, 37, 206)),
                            u8"\\ \\      / / ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(113, 37, 206)),
                            u8"\\ \\   / / ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(70, 37, 206)),
                            u8"|  \\/  | ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                            u8"|\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                            u8"|",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(189, 37, 206)),
                            u8" | | | | ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(152, 37, 206)),
                            u8" \\ \\ /\\ / /  ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(113, 37, 206)),
                            u8" \\ \\ / /  ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(70, 37, 206)),
                            u8"| |\\/| | ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                            u8"|\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                            u8"|",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(189, 37, 206)),
                            u8" | |_| | ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(152, 37, 206)),
                            u8"  \\ V  V /     ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(113, 37, 206)),
                            u8"\\ V /   ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(70, 37, 206)),
                            u8"| |  | | ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                            u8"|\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                            u8"|",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(189, 37, 206)),
                            u8"  \\___/ ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(152, 37, 206)),
                            u8"    \\_/\\_/    ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(113, 37, 206)),
                            u8"   \\_/    ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(70, 37, 206)),
                            u8"|_|  |_| ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                            u8"|\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                            u8"|                                         |\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                            u8"|",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(189, 37, 206)),
                            u8" Ultimate ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(152, 37, 206)),
                            u8"WebAssembly ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(113, 37, 206)),
                            u8" Virtual ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(70, 37, 206)),
                            u8"  Machine",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                            u8" |\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RGB(72, 61, 139)),
                            u8" ----------------------------------------- \n\n");

        // color == UWVM_COLOR_U8_RGB(72, 61, 139), not reset
    }

    template <typename Stm, ::uwvm2::parser::wasm::concepts::has_feature_name F0, ::uwvm2::parser::wasm::concepts::has_feature_name... Fs>
    inline constexpr void version_u8print_wasm_feature_impl(Stm && stm) noexcept
    {
        ::fast_io::io::perrln(::std::forward<Stm>(stm), u8"      - ", F0::feature_name);
        if constexpr(sizeof...(Fs) != 0) { version_u8print_wasm_feature_impl<Stm, Fs...>(::std::forward<Stm>(stm)); }
    }

    template <typename Stm, ::uwvm2::parser::wasm::concepts::has_feature_name... Fs>
    inline constexpr void version_u8print_wasm_feature_from_tuple(Stm && stm, ::uwvm2::utils::container::tuple<Fs...>) noexcept
    { version_u8print_wasm_feature_impl<Stm, Fs...>(::std::forward<Stm>(stm)); }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        ::uwvm2::utils::cmdline::parameter_return_type version_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                        ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                        ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept
    {
        // No copies will be made here.
        auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
        // Add raii locks while unlocking operations
        ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
            ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
        // No copies will be made here.
        auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

        // print logo
        logo_u8print_not_rst_impl(u8log_output_ul);

        // print version_info
        // Preventing the wasm32-wasi-uwvm stack from being too small and causing an overflow requires limiting the size of the contents of each print
        ::fast_io::io::perr(u8log_output_ul,
                                // uwvm
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_PURPLE),
                                u8"Ultimate WebAssembly Virtual Machine 2",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                // mode
                                u8"\nMode: "
#if defined(UWVM_MODE_RELEASE)
                                u8"Release "
#elif defined(UWVM_MODE_RELEASEDBG)
                                u8"Releasedbg "
#elif defined(UWVM_MODE_MINSIZEREL)
                                u8"Minsizerel "
#elif defined(UWVM_MODE_DEBUG)
                                u8"Debug "
#else
                                u8"Unknown "
#endif
#if defined(UWVM_MODULE)
                                u8"(CXX-Module Build) "
#endif
                                // Copyright
                                u8"\nCopyright (c) 2025-present UlteSoft. All rights reserved.  "
        // Install Path
#if defined(UWVM_SUPPORT_INSTALL_PATH)
                                u8"\nInstall Path: ",
                                ::uwvm2::uwvm::utils::install_path::install_path.path_name,
#endif
                                // Version
                                u8"\nVersion: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                ::uwvm2::uwvm::custom::uwvm_version,
        // flags
#if defined(UWVM_VERSION_DEV)
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_PURPLE),
                                u8" [dev]",
#elif defined(UWVM_VERSION_ALPHA)
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_PURPLE),
                                u8" [alpha]",
#elif defined(UWVM_VERSION_BETA)
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_PURPLE),
                                u8" [beta]",
#elif defined(UWVM_VERSION_RELEASE_CANDIDATE)
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_PURPLE),
                                u8" [rc]",
#elif defined(UWVM_VERSION_STABLE_RELEASE)
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_PURPLE),
                                u8" [sr]",
#endif
                                // git commit data
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
#if defined(__INTEL_LLVM_COMPILER)
                                u8"Intel LLVM Compiler "
#elif defined(__clang__)
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
        // ARM64EC is a Windows hybrid binary ABI that supports little-endian only.
#elif defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
                                u8"AArch64"
# if defined(__AARCH64EB__) || defined(__AARCH_BIG_ENDIAN)
                                u8"EB"
# endif
#elif defined(__arm__) || defined(_M_ARM)
                                u8"ARM"
# if defined(__ARMEB__) || defined(__ARM_BIG_ENDIAN)
                                u8"EB"
# endif
#elif (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
                                u8"x86_64"
#elif defined(__i386__) || defined(_M_IX86)
# if defined(_MSC_VER)
                                u8"i686"
# elif defined(__i686__)
                                u8"i686"
# elif defined(__i586__)
                                u8"i586"
# elif defined(__i486__)
                                u8"i486"
# else
                                u8"i386"
# endif
#elif defined(__BFIN__)
                                u8"Blackfin"
#elif defined(__convex__)
                                u8"Convex Computer"
#elif defined(__e2k__)
                                u8"E2K"
#elif defined(__XTENSA__)
                                u8"Xtensa"
# if defined(__XTENSA_EL__)
                                u8"EL"
# elif defined(__XTENSA_EB__)
                                u8"EB"
# endif
#elif defined(__IA64__) || defined(_M_IA64) || defined(__ia64__) || defined(__itanium__)
                                u8"Intel Itanium 64"
#elif defined(__loongarch__)
# if defined(__loongarch64)
                                u8"LoongArch64"
# else
                                u8"LoongArch32"
# endif
        // loongarch no big-endian mode
#elif defined(__m68k__) || defined(__mc68000__)
                                u8"Motorola 68k"
#elif defined(__MIPS64__) || defined(__mips64__)
                                u8"MIPS64"
# if defined(__MIPSEL__) || defined(__MIPSEL) || defined(_MIPSEL)
                                u8"EL"
# endif
# if defined(_MIPS_SIM)
#  if _MIPS_SIM == _ABIO32
                                u8"O32"
#  elif _MIPS_SIM == _ABIN32
                                u8"N32"
#  elif _MIPS_SIM == _ABI64
                                u8"N64"
#  endif
# endif
# if defined(_MIPS_ARCH_MIPS64R2)
                                u8" (R2)"
# endif
#elif defined(__MIPS__) || defined(__mips__) || defined(_MIPS_ARCH)
                                u8"MIPS"
# if defined(__MIPSEL__) || defined(__MIPSEL) || defined(_MIPSEL)
                                u8"EL"
# endif
# if defined(_MIPS_SIM)
#  if _MIPS_SIM == _ABIO32
                                u8"O32"
#  elif _MIPS_SIM == _ABIN32
                                u8"N32"
#  elif _MIPS_SIM == _ABI64
                                u8"N64"
#  endif
# endif
# if defined(_MIPS_ARCH_MIPS32R2)
                                u8" (R2)"
# endif
#elif defined(__HPPA__)
                                u8"HP/PA RISC"
#elif defined(__riscv)
                                u8"RISC-V"
# if defined(__riscv_xlen)
#  if __riscv_xlen == 64
                                u8" 64"
#  endif
# endif
#elif defined(__s390x__)
                                u8"System/390 Extended"
#elif defined(__s390__)
                                u8"System/390"
#elif defined(__370__) || defined(__THW_370__)
                                u8"System/370"
#elif defined(__pdp11)
                                u8"PDP11"
#elif defined(__pdp10)
                                u8"PDP10"
#elif defined(__pdp7)
                                u8"PDP7"
#elif defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || defined(_ARCH_PPC64)
                                u8"PowerPC64"
# if defined(_LITTLE_ENDIAN) || defined(__LITTLE_ENDIAN__)
                                u8"LE"
# endif
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
                                u8"PowerPC"
# if defined(_LITTLE_ENDIAN) || defined(__LITTLE_ENDIAN__)
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
        // ISA, SIMD
#if (defined(_MSC_VER) && !defined(__clang__)) && !defined(_KERNEL_MODE)
# if defined(_M_ARM64EC) || defined(_M_ARM64)
                                u8"\nISA support: "
#  if defined(USE_SOFT_INTRINSICS)
                                u8"CRC32 "
#  endif
                                u8"NEON "
# elif defined(_M_AMD64) && !defined(_M_ARM64EC)
                                u8"\nISA support: CRC32 MMX SSE SSE2 "
# elif defined(_M_IX86_FP)
#  if _M_IX86_FP == 2
                                u8"\nISA support: CRC32 MMX SSE SSE2 "
#  endif
# endif
#elif defined(__wasm_simd128__)
                                u8"\nISA support: SIMD128"
#elif defined(__loongarch__) && defined(__loongarch_sx)
                                u8"\nISA support: LoongSX "
# if defined(__loongarch_asx)
                                u8"LoongASX "
# endif
#elif ((defined(__arm64__) || defined(__aarch64__)) || (defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)) || (defined(__arm64ec__) || defined(_M_ARM64EC))) &&                                              \
    (defined(__ARM_NEON) || defined(__ARM_FEATURE_CRC32)) 
                                /*
                                 * https://arm-software.github.io/acle/main/acle.html
                                 */
                                u8"\nISA support: "
# if defined(__ARM_FEATURE_CRC32)
                                u8"CRC32 "
# endif
# if defined(__ARM_NEON)
                                u8"NEON "
# endif
# if defined(__ARM_FEATURE_SVE)
                                u8"SVE "
# endif
# if defined(__ARM_FEATURE_SVE2)
                                u8"SVE2 "
# endif
# if defined(__ARM_FEATURE_SME)
                                u8"SME"
#  if !defined(UWVM_ENABLE_SME_SVE_STREAM_MODE)
                                u8"(mno-SVESM)"
#  endif
                                u8" "
# endif
# if defined(__ARM_FEATURE_SME2)
                                u8"SME2 "
# endif
#elif ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) && \
    (defined(__MMX__) || defined(__FMA__) || defined(__BMI__) || defined(__PRFCHW__) || defined(__CRC32__))
                                u8"\nISA support: "
# if defined(__CRC32__)
                                u8"CRC32 "
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
        // 'POPCNT' instruction is a subset of sse4
# if defined(__PRFCHW__)
                                u8"PRFCHW "
# endif
# if defined(__PREFETCHI__)
                                u8"PREFETCHI "
# endif
# if 0  // uwvm don't need these instructions
#  if defined(__RDSEED__)
                                u8"RDSEED "
#  endif
#  if defined(__RDRND__)
                                u8"RDRND "
#  endif
# endif
# if defined(__RTM__)
                                u8"RTM "
# endif
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
# if defined(__AVX512CD__)
                                u8"AVX512CD "
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
# if defined(__AVX512VBMI__)
                                u8"AVX512VBMI "
# endif
# if defined(__AVX512VBMI2__)
                                u8"AVX512VBMI2 "
# endif
# if defined(__AVX512BITALG__)
                                u8"AVX512BITALG "
# endif
# if defined(__AVX512IFMA__)
                                u8"AVX512IFMA "
# endif
# if defined(__AVX512VNNI__)
                                u8"AVX512VNNI "
# endif
# if defined(__AVX512FP16__)
                                u8"AVX512FP16 "
# endif
# if defined(__AVX512BF16__)
                                u8"AVX512BF16 "
# endif
# if defined(__AVX512VPOPCNTDQ__)
                                u8"AVX512VPOPCNTDQ "
# endif
# if defined(__AVX512VP2INTERSECT__)
                                u8"AVX512VP2INTERSECT "
# endif
# if defined(__AVXIFMA__)
                                u8"AVXIFMA "
# endif
# if defined(__AVXNECONVERT__)
                                u8"AVXNECONVERT "
# endif
# if defined(__AVXVNNIINT16__)
                                u8"AVXVNNIINT16 "
# endif
# if defined(__AVXVNNIINT8__)
                                u8"AVXVNNIINT8 "
# endif
# if defined(__AVXVNNI__)
                                u8"AVXVNNI "
# endif
# if defined(__AVX10_1__)
                                u8"AVX10.1 "
# endif
# if defined(__AVX10_1_512__)
                                u8"AVX10.1-512 "
# endif
# if defined(__AVX10_2__)
                                u8"AVX10.2 "
# endif
# if defined(__AVX10_2_512__)
                                u8"AVX10.2-512 "
# endif
# if defined(__APX_F__)
                                u8"APX "
# endif
#elif ((defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || defined(_ARCH_PPC64)) ||                                                         \
       (defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC))) &&                                                                \
    (defined(__ALTIVEC__) || defined(__VEC__))
                                u8"\nISA support: ALTIVEC "
# if defined(__VSX__)
                                u8"VSX "
# endif
#elif defined(__riscv) && defined(__riscv_vector)
                                u8"\nISA support: RVV "
#endif

                                // OS
                                u8"\nOS: "
#if defined(__CYGWIN__)
                                u8"Cygwin"
#elif defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__TOS_WIN__) || defined(__WINDOWS__) || defined(_WIN32_WINNT) ||                     \
    defined(_WIN32_WINDOWS) || defined(_WINDOWS)
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
                                u8"Windows NT 5.2 (WS03, WINXP64)"
#  elif _WIN32_WINNT >= 0x0501
                                u8"Windows NT 5.1 (WINXP)"
#  elif _WIN32_WINNT >= 0x0500
                                u8"Windows NT 5.0 (WS2K, WIN2K)"
#  elif _WIN32_WINNT >= 0x0400
                                u8"Windows NT 4.0"
#  elif _WIN32_WINNT >= 0x0351
                                u8"Windows NT 3.51"
#  elif _WIN32_WINNT >= 0x0350
                                u8"Windows NT 3.50"
#  elif _WIN32_WINNT >= 0x0310
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
                                u8"Linux "
# if defined(LINUX_VERSION_CODE)
                                ,
                                static_cast<::std::make_unsigned_t<decltype(LINUX_VERSION_CODE)>>(LINUX_VERSION_CODE) >> 16u,
                                u8".",
                                (static_cast<::std::make_unsigned_t<decltype(LINUX_VERSION_CODE)>>(LINUX_VERSION_CODE) >> 8u) & 0xFFu,
                                u8".",
                                static_cast<::std::make_unsigned_t<decltype(LINUX_VERSION_CODE)>>(LINUX_VERSION_CODE) & 0xFFu,
# endif
#elif defined(__APPLE__)
        /*
         *  TARGET_OS_*
         *
         *  These conditionals specify in which Operating System the generated code will
         *  run.  Indention is used to show which conditionals are evolutionary subclasses.
         *
         *  The MAC/WIN32/UNIX conditionals are mutually exclusive.
         *  The IOS/TV/WATCH/VISION conditionals are mutually exclusive.
         *
         *    TARGET_OS_WIN32              - Generated code will run on WIN32 API
         *    TARGET_OS_WINDOWS            - Generated code will run on Windows
         *    TARGET_OS_UNIX               - Generated code will run on some Unix (not macOS)
         *    TARGET_OS_LINUX              - Generated code will run on Linux
         *    TARGET_OS_MAC                - Generated code will run on a variant of macOS
         *      TARGET_OS_OSX                - Generated code will run on macOS
         *      TARGET_OS_IPHONE             - Generated code will run on a variant of iOS (firmware, devices, simulator)
         *        TARGET_OS_IOS                - Generated code will run on iOS
         *          TARGET_OS_MACCATALYST        - Generated code will run on macOS
         *        TARGET_OS_TV                 - Generated code will run on tvOS
         *        TARGET_OS_WATCH              - Generated code will run on watchOS
         *        TARGET_OS_VISION             - Generated code will run on visionOS
         *        TARGET_OS_BRIDGE             - Generated code will run on bridge devices
         *      TARGET_OS_SIMULATOR          - Generated code will run on an iOS, tvOS, watchOS, or visionOS simulator
         *      TARGET_OS_DRIVERKIT          - Generated code will run on macOS, iOS, tvOS, watchOS, or visionOS
         *
         *    TARGET_OS_EMBEDDED           - DEPRECATED: Use TARGET_OS_IPHONE and/or TARGET_OS_SIMULATOR instead
         *    TARGET_IPHONE_SIMULATOR      - DEPRECATED: Same as TARGET_OS_SIMULATOR
         *    TARGET_OS_NANO               - DEPRECATED: Same as TARGET_OS_WATCH
         *
         *    +--------------------------------------------------------------------------------------+
         *    |                                    TARGET_OS_MAC                                     |
         *    | +-----+ +------------------------------------------------------------+ +-----------+ |
         *    | |     | |                  TARGET_OS_IPHONE                          | |           | |
         *    | |     | | +-----------------+ +----+ +-------+ +--------+ +--------+ | |           | |
         *    | |     | | |       IOS       | |    | |       | |        | |        | | |           | |
         *    | | OSX | | | +-------------+ | | TV | | WATCH | | BRIDGE | | VISION | | | DRIVERKIT | |
         *    | |     | | | | MACCATALYST | | |    | |       | |        | |        | | |           | |
         *    | |     | | | +-------------+ | |    | |       | |        | |        | | |           | |
         *    | |     | | +-----------------+ +----+ +-------+ +--------+ +--------+ | |           | |
         *    | +-----+ +------------------------------------------------------------+ +-----------+ |
         *    +--------------------------------------------------------------------------------------+
         */

# if TARGET_OS_OSX
                                u8"Apple macOS "
# elif TARGET_OS_MACCATALYST
                                u8"Apple Mac Catalyst "
# elif TARGET_OS_IOS
                                u8"Apple iOS "
# elif TARGET_OS_TV
                                u8"Apple tvOS "
# elif TARGET_OS_WATCH
                                u8"Apple watchOS "
# elif TARGET_OS_VISION
                                u8"Apple visionOS "
# elif TARGET_OS_BRIDGE
                                u8"Apple Bridge Devices "
# elif TARGET_OS_DRIVERKIT
                                u8"Apple DriverKit "
# elif TARGET_OS_SIMULATOR
                                u8"Apple Simulator "
# elif TARGET_OS_MAC  // Reserved for future use
                                u8"Apple Platform "
# elif defined(__MACH__)
                                u8"Mach Kernel "
# else
                                u8"Unknown Apple Platform "
# endif

# if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
                                ,
                                static_cast<::std::make_unsigned_t<decltype(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)>>(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) / 10000u,
                                u8".",
                                static_cast<::std::make_unsigned_t<decltype(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)>>(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) / 100u % 100u,
                                u8".",      
                                static_cast<::std::make_unsigned_t<decltype(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)>>(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) % 100u,
# elif defined(__ENVIRONMENT_OS_VERSION_MIN_REQUIRED__)
                                ,   
                                static_cast<::std::make_unsigned_t<decltype(__ENVIRONMENT_OS_VERSION_MIN_REQUIRED__)>>(__ENVIRONMENT_OS_VERSION_MIN_REQUIRED__) / 10000u,
                                u8".",
                                static_cast<::std::make_unsigned_t<decltype(__ENVIRONMENT_OS_VERSION_MIN_REQUIRED__)>>(__ENVIRONMENT_OS_VERSION_MIN_REQUIRED__) / 100u % 100u,
                                u8".",
                                static_cast<::std::make_unsigned_t<decltype(__ENVIRONMENT_OS_VERSION_MIN_REQUIRED__)>>(__ENVIRONMENT_OS_VERSION_MIN_REQUIRED__) % 100u,
# endif

#elif defined(__DragonFly__)
                                u8"DragonFlyBSD"
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
                                u8"FreeBSD"
#elif defined(__NetBSD__)
                                u8"NetBSD"
#elif defined(__OpenBSD__)
                                u8"OpenBSD"
#elif defined(BSD) || defined(_SYSTYPE_BSD) || defined(__BSD_VISIBLE)
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
#elif defined(__TANDEM) || defined(__NONSTOP)
                                u8"HPE NonStop"
#elif defined(__hpux)
                                u8"HP-UX"
#elif defined(__HAIKU__)
                                u8"Haiku"
#elif defined(__AROS__)
                                u8"AROS"
#elif defined(__OS2__)
                                u8"OS/2"
#elif defined(__MVS__) || defined(__HOS_MVS__)
                                u8"z/OS"
#elif defined(__unix) || defined(_XOPEN_SOURCE) || defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE)
                                u8"Unix Environment"
#else
                                u8"Unknown OS"
#endif
                // C Library
                                u8"\nC Library: "
#if defined(__EMSCRIPTEN__)
                                u8"Emscripten"
#elif defined(__wasi__)
                                u8"WASI"
#elif defined(__MINGW32__) && !defined(_UCRT) && !defined(__BIONIC__)
                                u8"MSVCRT"
#elif (defined(_MSC_VER) || defined(_UCRT)) && !defined(__WINE__) && !defined(__CYGWIN__) && !defined(__BIONIC__)
                                u8"UCRT"
#elif defined(__DARWIN_C_LEVEL)
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
                                u8"Open Harmony"
#elif defined(__CRTL_VER)
                                u8"VMS"
#elif defined(__LIBREL__)
                                u8"z/OS"
#elif defined(__NEWLIB__)
                                u8"Newlib"
#elif defined(__PICOLIBC__)
                                u8"picolibc"
#elif defined(__LLVM_LIBC__) || defined(__LLVM_LIBC_TYPES_FILE_H__)
                                u8"LLVM LIBC "
#elif defined(__MLIBC_O_CLOEXEC)
                                u8"MLIBC"
#elif defined(__NEED___isoc_va_list) || defined(__musl__)
                                u8"MUSL"
#elif defined(__serenity__)
                                u8"Serenity"
#elif defined(__DJGPP__)
                                u8"DJGPP"
#elif defined(__BSD_VISIBLE) || (defined(__NEWLIB__) && !defined(__CUSTOM_FILE_IO__)) || defined(__MSDOS__)
                                u8"UNIX"
#else
                                u8"Unknown"
#endif
#ifdef _POSIX_C_SOURCE
                                u8"\nPOSIX: ",
                                _POSIX_C_SOURCE,
#endif
                                u8"\nAllocator: "
#if defined(FAST_IO_USE_CUSTOM_GLOBAL_ALLOCATOR)
                                u8"Custom Allocator"
#elif defined(FAST_IO_USE_MIMALLOC)
                                u8"mimalloc"
#elif (defined(__linux__) && defined(__KERNEL__)) || defined(FAST_IO_USE_LINUX_KERNEL_ALLOCATOR)
                                u8"Linux kmalloc"
#elif ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && !defined(_LIBCPP_FREESTANDING)) ||                                      \
       defined(FAST_IO_ENABLE_HOSTED_FEATURES))
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__) && !defined(FAST_IO_USE_C_MALLOC)
#  if defined(_DEBUG) && defined(_MSC_VER)
                                u8"WinCRT malloc DBG"
#  else
#   if defined(_WIN32_WINDOWS)
                                u8"Win32 Heapalloc"
#   else
                                u8"NT Rtlallocateheap"
#   endif
#  endif
# else
#  if defined(_DEBUG) && defined(_MSC_VER)
                                u8"WinCRT malloc DBG"
#  else
                                u8"C's malloc"
#  endif
# endif
#else
                                u8"Custom Allocator"
#endif
                                u8"\n"
                                // Feature
                                u8"Feature:\n"
        // detailed checker
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                // Perform additional checks during debugging
                                u8"  * Detailed Debug Check Mode\n"
#endif
        // fno-exceptions
#if !(defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE))
                                // Error Direct Crash Mode
                                u8"  * Error Direct Crash Mode (fno-exceptions)\n"
#endif
                                // command line hash
                                u8"  * CMDLHash: "
                                u8"HT=",
                                ::uwvm2::uwvm::cmdline::hash_table_size.hash_table_size,
                                u8", EX=",
                                ::uwvm2::uwvm::cmdline::hash_table_size.extra_size,
                                u8", RC=",
                                ::uwvm2::uwvm::cmdline::hash_table_size.real_max_conflict_size,
                                u8", SZ=",
                                ::uwvm2::uwvm::cmdline::hash_table_byte_sz,
        // sanitizer
#if UWVM_HAS_FEATURE(address_sanitizer) || UWVM_HAS_FEATURE(thread_sanitizer) || UWVM_HAS_FEATURE(memory_sanitizer) || UWVM_HAS_FEATURE(leak_sanitizer) ||     \
    UWVM_HAS_FEATURE(undefined_sanitizer)
                                u8"\n  * Sanitizer: "
# if UWVM_HAS_FEATURE(address_sanitizer)
                                u8"Address "
# elif UWVM_HAS_FEATURE(thread_sanitizer)
                                u8"Thread "
# elif UWVM_HAS_FEATURE(memory_sanitizer)
                                u8"Memory "
# elif UWVM_HAS_FEATURE(leak_sanitizer)
                                u8"Leak "
# elif UWVM_HAS_FEATURE(undefined_sanitizer)
                                u8"Undefined "
# endif
#endif
                                // wasm feature
                                u8"\n"
                                u8"  * WebAssembly Features Supported: "
                                u8"\n"
            );
        // wasm feature
        version_u8print_wasm_feature_from_tuple(u8log_output_ul, ::uwvm2::uwvm::wasm::feature::all_features);

        // other features
        ::fast_io::io::perr(u8log_output_ul,
        // version_u8print_wasm_feature_from_tuple already print '\n'
#ifdef UWVM_SUPPORT_PRELOAD_DL
                            // Support Preload Dynamic Linking Module (--wasm-register-dl)
                            u8"  * Support Preload Dynamic Linking Module\n"
#endif
#ifdef UWVM_SUPPORT_WEAK_SYMBOL
                            // Support Weak Symbol Module (via link)
                            u8"  * Support Weak Symbol Module\n"
#endif

        // Local Imported
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# ifdef UWVM_IMPORT_WASI_WASIP1
                            // Support WASI WASIP1
                            u8"  * Support Imported WASI Preview 1"
#  ifdef UWVM_IMPORT_WASI_WASIP1_WASM64
                            u8" (With WASM64 Extension)"
#  endif
                            u8"\n"
#  ifdef UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET
                            // Support WASI WASIP1 Socket: The wasip1 socket appears as an extension.
                            u8"      - Support Imported WASI Preview 1 Socket"
#   ifdef UWVM_IMPORT_WASI_WASIP1_SUPPORT_WASIX_SOCKET
                            u8" (WASIX Version)"
#   endif
                            u8"\n"
#  endif
# endif
#endif

        // runtime compiler
                            u8"  * Runtime Compiler:\n"
#ifdef UWVM_RUNTIME_UWVM_INTERPRETER
                            u8"      - UWVM Interpreter\n"
#endif
#ifdef UWVM_RUNTIME_LLVM_JIT
                            u8"      - LLVM JIT\n"
#endif
#ifdef UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED
                            u8"      - UWVM Interpreter + LLVM JIT (Tiered)\n"
#endif
#ifdef UWVM_RUNTIME_DEBUG_INTERPRETER
                            u8"      - Debug Interpreter\n"
#endif

        // ENDL
                            u8"\n");

        // Here, guard will perform destructors.
        return ::uwvm2::utils::cmdline::parameter_return_type::return_imme;
    }

}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
