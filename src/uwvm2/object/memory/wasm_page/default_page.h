/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
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
# include <cstddef>
# include <cstdint>
# include <limits>
# include <concepts>
# include <bit>
// import
# include <fast_io.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/binfmt/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::wasm_page
{
    using wasm32_page_size_t = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
    using wasm64_page_size_t = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;

    inline constexpr wasm32_page_size_t wasm32_page_size_add_or_overflow_die_chain(wasm32_page_size_t a, wasm32_page_size_t b) noexcept
    {
        constexpr auto wasm32_page_size_max{::std::numeric_limits<wasm32_page_size_t>::max()};
        if(a > wasm32_page_size_max - b) [[unlikely]] { ::fast_io::fast_terminate(); }
        return a + b;
    }

    inline constexpr wasm64_page_size_t wasm64_page_size_add_or_overflow_die_chain(wasm64_page_size_t a, wasm64_page_size_t b) noexcept
    {
        constexpr auto wasm64_page_size_max{::std::numeric_limits<wasm64_page_size_t>::max()};
        if(a > wasm64_page_size_max - b) [[unlikely]] { ::fast_io::fast_terminate(); }
        return a + b;
    }

    inline constexpr auto default_wasm32_page_size{static_cast<wasm32_page_size_t>(65536u)};
    inline constexpr auto default_wasm64_page_size{static_cast<wasm64_page_size_t>(65536u)};

    template <::std::unsigned_integral T>
    inline constexpr T floor_log2(T value) noexcept
    {
        constexpr auto dgt{::std::numeric_limits<T>::digits - 1u};
        return value == 0u ? ::std::numeric_limits<T>::max() : static_cast<T>(dgt - ::std::countl_zero(value));
    }

    inline constexpr auto default_wasm32_page_size_log2{floor_log2(default_wasm32_page_size)};
    inline constexpr auto default_wasm64_page_size_log2{floor_log2(default_wasm64_page_size)};

}  // namespace uwvm2::object::memory::wasm_page

