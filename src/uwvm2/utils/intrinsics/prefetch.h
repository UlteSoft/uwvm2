/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        prefetch.cppm
 * @brief       cpu prefetch instruction
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-21
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

// platfrom
/// @brief      Including intrin.h in the absence of __builtin_prefetch
#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/push_macros.h>
// platform
# if !UWVM_HAS_BUILTIN(__builtin_prefetch)
#  include <intrin.h>
# endif
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::intrinsics::universal
{
    enum class pfc_mode
    {
        read = 0,
        write = 1,
        instruction = 2
    };

    enum class pfc_level
    {
        nta = 0,
        L3 = 1,
        L2 = 2,
        L1 = 3
    };

    enum class ret_policy
    {
        keep = 0,
        strm = 1,
    };

    /// @brief      Direct conversion to cpu prefetch instructions
    /// @details    write: write or read sensitive
    ///             level == 0 -> nta (Non-Temporal Access)
    ///             level == 1 -> L3
    ///             level == 2 -> L2, L3
    ///             level == 3 -> L1, L2, L3
    /// @param      address address to be prefetched
    /// @see        __builtin_prefetch and _mm_prefetch
    template <pfc_mode curr_prefetch_mode = pfc_mode::write, pfc_level prefetch_level = pfc_level::nta, ret_policy retention_policy = ret_policy::keep>
        requires ((0 <= static_cast<int>(curr_prefetch_mode) && static_cast<int>(curr_prefetch_mode) < 3) &&
                  (0 <= static_cast<int>(prefetch_level) && static_cast<int>(prefetch_level) < 4) &&
                  (0 <= static_cast<int>(retention_policy) && static_cast<int>(retention_policy) < 2))
    UWVM_GNU_ALWAYS_INLINE_ARTIFICIAL UWVM_GNU_NODEBUG UWVM_NO_SANITIZE inline void prefetch(void const* address) noexcept
    {
        if constexpr(curr_prefetch_mode == pfc_mode::instruction)
        {
#if ((defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)) || (defined(__arm64ec__) || defined(_M_ARM64EC))) &&                                    \
    UWVM_HAS_BUILTIN(__builtin_arm_prefetch)  // Only Clang
            // __builtin_arm_prefetch has different function prototypes between aarch64 and arm
            if constexpr(prefetch_level == pfc_level::nta) { __builtin_arm_prefetch(address, 0, 0, 1, 0); }
            else
            {
                __builtin_arm_prefetch(address, 0, 3 - static_cast<int>(prefetch_level), static_cast<int>(retention_policy), 0);
            }
#elif UWVM_HAS_BUILTIN(__builtin_ia32_prefetchi)  // Clang + GCC
            // llvm: have prefetchit0 and prefetchit1 instructions which map to _MM_HINT_T0(3) and _MM_HINT_T1(2).
            //       Other arguments are not supported.
            // GCC:  instruction prefetch applies when in 64-bit mode with RIP-relative addressing and option '-mprefetchi';
            //       they stay NOPs otherwise
            constexpr int actual_prefetch_level{static_cast<int>(prefetch_level) < 2 ? 2 : static_cast<int>(prefetch_level)};
            __builtin_ia32_prefetchi(address, static_cast<int>(actual_prefetch_level));
#endif
            // Instructions that do not do generic processing to ensure that the correct prefetch instructions are generated.
        }
        else
        {
#if ((defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)) || (defined(__arm64ec__) || defined(_M_ARM64EC))) &&                                    \
    UWVM_HAS_BUILTIN(__builtin_arm_prefetch)  // Only Clang
            // __builtin_arm_prefetch has different function prototypes between aarch64 and arm
            if constexpr(prefetch_level == pfc_level::nta)
            {
                // same as __builtin_prefetch(address, static_cast<int>(curr_prefetch_mode), 0)
                __builtin_arm_prefetch(address, static_cast<int>(curr_prefetch_mode), 0, 1, 1);
            }
            else
            {
                __builtin_arm_prefetch(address,
                                       static_cast<int>(curr_prefetch_mode),
                                       3 - static_cast<int>(prefetch_level),
                                       static_cast<int>(retention_policy),
                                       1);
            }
#elif UWVM_HAS_BUILTIN(__builtin_prefetch)  // Clang + GCC
            __builtin_prefetch(address, static_cast<int>(curr_prefetch_mode), static_cast<int>(prefetch_level));
#else                                       // MSVC
            ::_mm_prefetch(reinterpret_cast<char const*>(address), (static_cast<int>(curr_prefetch_mode) << 2u) | static_cast<int>(prefetch_level));
#endif
        }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
