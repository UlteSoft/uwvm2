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
    // float32_t
    // =============================

#ifdef __STDCPP_FLOAT32_T__
    using float32_t = _Float32;  // C23 IEEE 754-2008
#else
    using float32_t = float;  // The C++ Standard doesn't specify it. Gotta check.

    static_assert(::std::numeric_limits<float>::is_specialized &&
                      (::std::numeric_limits<float>::is_iec559 && ::std::numeric_limits<float>::digits == 24 &&
                       ::std::numeric_limits<float>::max_exponent == 128 && ::std::numeric_limits<float>::min_exponent == -125),
                  "float ain't of the IEC 559/IEEE 754 floating-point types");
#endif

    // =============================
    // float64_t
    // =============================

#ifdef __STDCPP_FLOAT64_T__
    using float64_t = _Float64;  // C23 IEEE 754-2008
#else
    using float64_t = double;  // The C++ Standard doesn't specify it. Gotta check.

    static_assert(::std::numeric_limits<double>::is_specialized &&
                      (::std::numeric_limits<double>::is_iec559 && ::std::numeric_limits<double>::digits == 53 &&
                       ::std::numeric_limits<double>::max_exponent == 1024 && ::std::numeric_limits<double>::min_exponent == -1021),
                  "double ain't of the IEC 559/IEEE 754 floating-point types");
#endif

    // Not Supported Yet
    /// @TODO: support float16_t, bfloat16_t, float80_t, float128_t

#if 0
    // =============================
    // float16_t
    // =============================

# if defined(__STDCPP_FLOAT16_T__)
    using float16_t = _Float16;  // C23 (IEEE 754, no necessary to check)
# else
    using float16_t = error_float_t;
# endif

    // =============================
    // bfloat16_t
    // =============================

    // bf16 still requires inspection

# if defined(__STDCPP_BFLOAT16_T__)
    using bfloat16_t = decltype(0.0bf16);  // C23 (IEEE 754, no necessary to check)
# else
    using bfloat16_t = error_float_t;
# endif

    // =============================
    // float80_t
    // =============================

    namespace details
    {
# if defined(__SIZEOF_FLOAT80__)
        using float80_t_unchecked = __float80;
# else
        using float80_t_unchecked = long double;
# endif
    }  // namespace details

    inline constexpr bool supports_float80_t{::std::numeric_limits<details::float80_t_unchecked>::is_specialized &&
                                             (::std::numeric_limits<details::float80_t_unchecked>::is_iec559 &&
                                              ::std::numeric_limits<details::float80_t_unchecked>::digits == 64 &&
                                              ::std::numeric_limits<details::float80_t_unchecked>::max_exponent == 16384 &&
                                              ::std::numeric_limits<details::float80_t_unchecked>::min_exponent == -16381)};

    using float80_t = ::std::conditional_t<supports_float80_t, details::float80_t_unchecked, error_float_t>;

    // =============================
    // float128_t
    // =============================

    namespace details
    {
# if defined(__STDCPP_FLOAT128_T__)
        using float128_t_unchecked = _Float128;
# elif defined(__SIZEOF_FLOAT128__)
        using float128_t_unchecked = __float128;
# else
        using float128_t_unchecked = long double;
# endif
    }  // namespace details

    inline constexpr bool supports_float128_t{::std::numeric_limits<details::float128_t_unchecked>::is_specialized &&
                                              (::std::numeric_limits<details::float128_t_unchecked>::is_iec559 &&
                                               ::std::numeric_limits<details::float128_t_unchecked>::digits == 113 &&
                                               ::std::numeric_limits<details::float128_t_unchecked>::max_exponent == 16384 &&
                                               ::std::numeric_limits<details::float128_t_unchecked>::min_exponent == -16381)};

    using float128_t = ::std::conditional_t<supports_float128_t, details::float128_t_unchecked, error_float_t>;
#endif
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
