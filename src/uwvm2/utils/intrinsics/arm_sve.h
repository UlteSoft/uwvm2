/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-05-16
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
# include <cstddef>
# include <cstdint>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(__ARM_FEATURE_SVE) || defined(__ARM_FEATURE_SME)
UWVM_MODULE_EXPORT namespace uwvm2::utils::intrinsics::arm_sve
{
# if defined(__clang__)

    // Try to avoid the need for [[clang::__overloadable__]] code

    // clang-format off

    using svbool_t = __SVBool_t;
    using svuint8_t = __SVUint8_t;
    using svuint16_t = __SVUint16_t;
    using svuint32_t = __SVUint32_t;
    using svuint64_t = __SVUint64_t;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svptrue_b8)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svbool_t svptrue_b8() noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svdup_n_u8)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint8_t svdup_n_u8(::std::uint8_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svptest_any)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern bool svptest_any(svbool_t, svbool_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svwhilelt_b8_u64)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svbool_t svwhilelt_b8_u64(::std::uint64_t, ::std::uint64_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svwhilelt_b8_u32)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svbool_t svwhilelt_b8_u32(::std::uint32_t, ::std::uint32_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svld1_u8)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint8_t svld1_u8(svbool_t, ::std::uint8_t const *) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svcmpge_n_u8)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svbool_t svcmpge_n_u8(svbool_t, svuint8_t, ::std::uint8_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svcntp_b8)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern ::std::uint64_t svcntp_b8(svbool_t, svbool_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svcntb)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern ::std::uint64_t svcntb() noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svld1_u64)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint64_t svld1_u64(svbool_t, ::std::uint64_t const *) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_sveor_u64_x)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint64_t sveor_u64_x(svbool_t, svuint64_t, svuint64_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svtbl_u64)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint64_t svtbl_u64(svuint64_t, svuint64_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svextw_u64_x)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint64_t svextw_u64_x(svbool_t, svuint64_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svlsr_n_u64_x)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint64_t svlsr_n_u64_x(svbool_t, svuint64_t, ::std::uint64_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svmad_u64_x)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint64_t svmad_u64_x(svbool_t, svuint64_t, svuint64_t, svuint64_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svadd_u64_x)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint64_t svadd_u64_x(svbool_t, svuint64_t, svuint64_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_sveor_n_u64_z)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint64_t sveor_n_u64_z(svbool_t, svuint64_t, ::std::uint64_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svptrue_b64)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svbool_t svptrue_b64() noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svindex_u64)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint64_t svindex_u64(::std::uint64_t, ::std::uint64_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svcntd)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern ::std::uint64_t svcntd() noexcept;

    enum svpattern
    {
        SV_POW2 = 0,
        SV_VL1 = 1,
        SV_VL2 = 2,
        SV_VL3 = 3,
        SV_VL4 = 4,
        SV_VL5 = 5,
        SV_VL6 = 6,
        SV_VL7 = 7,
        SV_VL8 = 8,
        SV_VL16 = 9,
        SV_VL32 = 10,
        SV_VL64 = 11,
        SV_VL128 = 12,
        SV_VL256 = 13,
        SV_MUL4 = 29,
        SV_MUL3 = 30,
        SV_ALL = 31
    };

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svptrue_pat_b64)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svbool_t svptrue_pat_b64(enum svpattern) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svst1_u64)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern void svst1_u64(svbool_t, ::std::uint64_t *, svuint64_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svdup_n_u32)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint32_t svdup_n_u32(::std::uint32_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svdup_n_u64)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint64_t svdup_n_u64(::std::uint64_t) noexcept;

    [[clang::__clang_arm_builtin_alias(__builtin_sve_svlsl_n_u64_x)]]
    [[__gnu__::__always_inline__]]
    [[__gnu__::__nodebug__]]
    extern svuint64_t svlsl_n_u64_x(svbool_t, svuint64_t, ::std::uint64_t) noexcept;
    // clang-format on

# elif defined(__GNUC__)

    /* NOTE: This implementation of arm_sve.h is intentionally short.  It does
       not define the SVE types and intrinsic functions directly in C and C++
       code, but instead uses the following pragma to tell GCC to insert the
       necessary type and function definitions itself.  The net effect is the
       same, and the file is a complete implementation of arm_sve.h.  */

#  pragma GCC aarch64 "arm_sve.h"

# else

#  include "arm_sve.h"

# endif
}
#endif
