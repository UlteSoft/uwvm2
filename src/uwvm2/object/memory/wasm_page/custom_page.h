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
# include "default_page.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::wasm_page
{
    inline constexpr ::uwvm2::object::memory::wasm_page::wasm32_page_size_t set_custom_wasm32_page_size(unsigned page_size_log2) noexcept
    {
        constexpr unsigned max_page_size_log2{::std::numeric_limits<::uwvm2::object::memory::wasm_page::wasm32_page_size_t>::digits};

        if(page_size_log2 >= max_page_size_log2) [[unlikely]] { ::fast_io::fast_terminate(); }

        return static_cast<::uwvm2::object::memory::wasm_page::wasm32_page_size_t>(static_cast<::uwvm2::object::memory::wasm_page::wasm32_page_size_t>(1u)
                                                                                   << page_size_log2);
    }

    inline constexpr ::uwvm2::object::memory::wasm_page::wasm64_page_size_t set_custom_wasm64_page_size(unsigned page_size_log2) noexcept
    {
        constexpr unsigned max_page_size_log2{::std::numeric_limits<::uwvm2::object::memory::wasm_page::wasm64_page_size_t>::digits};

        if(page_size_log2 >= max_page_size_log2) [[unlikely]] { ::fast_io::fast_terminate(); }

        return static_cast<::uwvm2::object::memory::wasm_page::wasm64_page_size_t>(static_cast<::uwvm2::object::memory::wasm_page::wasm64_page_size_t>(1u)
                                                                                   << page_size_log2);
    }

    inline constexpr unsigned get_wasm32_page_size_log2(::uwvm2::object::memory::wasm_page::wasm32_page_size_t page_size) noexcept
    { return ::uwvm2::object::memory::wasm_page::floor_log2(page_size); }

    inline constexpr unsigned get_wasm64_page_size_log2(::uwvm2::object::memory::wasm_page::wasm64_page_size_t page_size) noexcept
    { return ::uwvm2::object::memory::wasm_page::floor_log2(page_size); }

}  // namespace uwvm2::object::memory::wasm_page

