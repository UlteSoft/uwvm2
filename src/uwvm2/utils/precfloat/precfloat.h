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
    inline consteval auto get_float32_t() noexcept
    {
        if constexpr(requires { typename ::std::float32_t; })
        {
            return ::std::float32_t{};  // C++23 IEEE 754-2008
        }
        else
        {
#ifdef __STDCPP_FLOAT32_T__
            return _Float32{};  // C23 IEEE 754-2008
#else
            return float{};  // The C++ Standard doesn't specify it. Gotta check.
#endif
        }
    }

    inline consteval auto get_float64_t() noexcept
    {
        if constexpr(requires { typename ::std::float64_t; })
        {
            return ::std::float64_t{};  // C++23 IEEE 754-2008
        }
        else
        {
#ifdef __STDCPP_FLOAT64_T__
            return _Float64{};  // C23 IEEE 754-2008
#else
            return double{};  // The C++ Standard doesn't specify it. Gotta check.
#endif
        }
    }

    using float32_t = decltype(get_float32_t());
    static_assert(::std::numeric_limits<float32_t>::is_iec559 && ::std::numeric_limits<float32_t>::digits == 24 &&
                      ::std::numeric_limits<float32_t>::max_exponent == 128 && ::std::numeric_limits<float32_t>::min_exponent == -125,
                  "float32_t ain't of the IEC 559/IEEE 754 floating-point types");

    using float64_t = decltype(get_float64_t());
    static_assert(::std::numeric_limits<float64_t>::is_iec559 && ::std::numeric_limits<float64_t>::digits == 53 &&
                      ::std::numeric_limits<float64_t>::max_exponent == 1024 && ::std::numeric_limits<float64_t>::min_exponent == -1021,
                  "float64_t ain't of the IEC 559/IEEE 754 floating-point types");
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
