/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-05-31
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
# include <cstdint>
# include <cstddef>
# include <climits>
# include <cstring>
# include <concepts>
# include <memory>
# include <bit>
# include <limits>
# include <type_traits>
# if __has_include(<stdfloat>)
#  include <stdfloat>
# endif
// macro
# include <uwvm2/utils/macro/push_macros.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::precfloat
{
    struct error_float_t
    {
    };

    // =============================
    // float16_t
    // =============================

    namespace details
    {
#if defined(__STDCPP_FLOAT16_T__)
        using float16_t_need_check = _Float16;  // C23
#elif defined(__SIZEOF_FLOAT16__)
        using float16_t_need_check = __float16;  // GCC / Clang intrinsic
#elif defined(__fp16)
        using float16_t_need_check = __fp16;  // ARM / Clang half float
#else
        using float16_t_need_check = error_float_t;
#endif

    }  // namespace details

    inline constexpr bool supports_float16_t{::std::numeric_limits<details::float16_t_need_check>::is_specialized &&
                                             (::std::numeric_limits<details::float16_t_need_check>::is_iec559 &&
                                              ::std::numeric_limits<details::float16_t_need_check>::digits == 11 &&
                                              ::std::numeric_limits<details::float16_t_need_check>::max_exponent == 16 &&
                                              ::std::numeric_limits<details::float16_t_need_check>::min_exponent == -13)};

    using float16_t = ::std::conditional_t<supports_float16_t, details::float16_t_need_check, error_float_t>;

    // =============================
    // bfloat16_t
    // =============================

    namespace details
    {
#if defined(__STDCPP_BFLOAT16_T__)
        using bfloat16_t_need_check = decltype(0.0bf16);
#elif defined(__bf16)
        using bfloat16_t_need_check = __bf16;  // GCC/Clang extension
#else
        using bfloat16_t_need_check = error_float_t;
#endif
    }  // namespace details

    inline constexpr bool supports_bfloat16_t{::std::numeric_limits<details::bfloat16_t_need_check>::is_specialized &&
                                              (::std::numeric_limits<details::bfloat16_t_need_check>::digits == 8 &&
                                               ::std::numeric_limits<details::bfloat16_t_need_check>::max_exponent == 128 &&
                                               ::std::numeric_limits<details::bfloat16_t_need_check>::min_exponent == -125)};

    using bfloat16_t = ::std::conditional_t<supports_bfloat16_t, details::bfloat16_t_need_check, error_float_t>;

    // =============================
    // float32_t
    // =============================

#ifdef __STDCPP_FLOAT32_T__
    using float32_t = _Float32;  // C23 IEEE 754-2008
#elif defined(__FLT32_MANT_DIG__) && __FLT32_MANT_DIG__ == 24
    using float32_t = _Float32;  // GCC/Clang extension before C23
#else
    using float32_t = float;  // The C++ Standard doesn't specify it. Gotta check.
#endif

    // always support float32_t
    inline constexpr bool supports_float32_t{true};

    // always check
    static_assert(::std::numeric_limits<float32_t>::is_specialized &&
                      (::std::numeric_limits<float32_t>::is_iec559 && ::std::numeric_limits<float32_t>::digits == 24 &&
                       ::std::numeric_limits<float32_t>::max_exponent == 128 && ::std::numeric_limits<float32_t>::min_exponent == -125),
                  "float32_t ain't of the IEC 559/IEEE 754 floating-point types");

    // =============================
    // float64_t
    // =============================

#ifdef __STDCPP_FLOAT64_T__
    using float64_t = _Float64;  // C23 IEEE 754-2008
#elif defined(__FLT64_MANT_DIG__) && __FLT64_MANT_DIG__ == 53
    using float64_t = _Float64;  // GCC/Clang extension before C23
#else
    using float64_t = double;  // The C++ Standard doesn't specify it. Gotta check.
#endif

    // always support float64_t
    inline constexpr bool supports_float64_t{true};

    // always check
    static_assert(::std::numeric_limits<float64_t>::is_specialized &&
                      (::std::numeric_limits<float64_t>::is_iec559 && ::std::numeric_limits<float64_t>::digits == 53 &&
                       ::std::numeric_limits<float64_t>::max_exponent == 1024 && ::std::numeric_limits<float64_t>::min_exponent == -1021),
                  "float64_t ain't of the IEC 559/IEEE 754 floating-point types");

    // =============================
    // float80_t
    // =============================

    namespace details
    {
#if defined(__SIZEOF_FLOAT80__)
        using float80_t_need_check = __float80;
#else
        using float80_t_need_check = long double;
#endif
    }  // namespace details

    inline constexpr bool supports_float80_t{::std::numeric_limits<details::float80_t_need_check>::is_specialized &&
                                             (::std::numeric_limits<details::float80_t_need_check>::is_iec559 &&
                                              ::std::numeric_limits<details::float80_t_need_check>::digits == 64 &&
                                              ::std::numeric_limits<details::float80_t_need_check>::max_exponent == 16384 &&
                                              ::std::numeric_limits<details::float80_t_need_check>::min_exponent == -16381)};

    using float80_t = ::std::conditional_t<supports_float80_t, details::float80_t_need_check, error_float_t>;

    // =============================
    // float128_t
    // =============================

    namespace details
    {
#if defined(__STDCPP_FLOAT128_T__)
        using float128_t_need_check = _Float128;
#elif defined(__SIZEOF_FLOAT128__)
        using float128_t_need_check = __float128;
#else
        using float128_t_need_check = long double;
#endif
    }  // namespace details

    inline constexpr bool supports_float128_t{::std::numeric_limits<details::float128_t_need_check>::is_specialized &&
                                              (::std::numeric_limits<details::float128_t_need_check>::is_iec559 &&
                                               ::std::numeric_limits<details::float128_t_need_check>::digits == 113 &&
                                               ::std::numeric_limits<details::float128_t_need_check>::max_exponent == 16384 &&
                                               ::std::numeric_limits<details::float128_t_need_check>::min_exponent == -16381)};

    using float128_t = ::std::conditional_t<supports_float128_t, details::float128_t_need_check, error_float_t>;
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
