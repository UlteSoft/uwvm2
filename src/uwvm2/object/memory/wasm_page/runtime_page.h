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
// import
# include <fast_io.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/binfmt/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include "default_page.h"
# include "custom_page.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::wasm_page
{
    // page_size must be 2^n
    union wasm32_wasm64_page_size_storage_u
    {
        ::uwvm2::object::memory::wasm_page::wasm32_page_size_t wasm32_page_size;
        ::uwvm2::object::memory::wasm_page::wasm64_page_size_t wasm64_page_size;
    };

    struct runtime_wasm_page
    {
        wasm32_wasm64_page_size_storage_u page_size{};
        bool is_wasm64{};
    };

    inline constexpr runtime_wasm_page get_default_wasm32_runtime_wasm_page() noexcept
    { return {wasm32_wasm64_page_size_storage_u{.wasm32_page_size = ::uwvm2::object::memory::wasm_page::default_wasm32_page_size}, false}; }

    inline constexpr runtime_wasm_page get_default_wasm64_runtime_wasm_page() noexcept
    { return {wasm32_wasm64_page_size_storage_u{.wasm64_page_size = ::uwvm2::object::memory::wasm_page::default_wasm64_page_size}, true}; }

    inline constexpr runtime_wasm_page get_custom_wasm32_runtime_wasm_page(unsigned page_size_log2) noexcept
    { return {wasm32_wasm64_page_size_storage_u{.wasm32_page_size = ::uwvm2::object::memory::wasm_page::set_custom_wasm32_page_size(page_size_log2)}, false}; }

    inline constexpr runtime_wasm_page get_custom_wasm64_runtime_wasm_page(unsigned page_size_log2) noexcept
    { return {wasm32_wasm64_page_size_storage_u{.wasm64_page_size = ::uwvm2::object::memory::wasm_page::set_custom_wasm64_page_size(page_size_log2)}, true}; }
}  // namespace uwvm2::object::memory::wasm_page

