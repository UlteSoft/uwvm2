/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     Parser limits for wasm1.1-only syntax
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
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
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <uwvm2/parser/wasm/standard/wasm1/features/parser_limit.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
    inline constexpr ::std::size_t default_max_data_count_sec_count{::uwvm2::parser::wasm::standard::wasm1::features::default_max_data_sec_entries};
    inline constexpr ::std::size_t default_max_elem_sec_expr{::uwvm2::parser::wasm::standard::wasm1::features::default_max_elem_sec_funcidx};

    static_assert(default_max_data_count_sec_count == ::uwvm2::parser::wasm::standard::wasm1::features::default_max_data_sec_entries);
    static_assert(default_max_elem_sec_expr == ::uwvm2::parser::wasm::standard::wasm1::features::default_max_elem_sec_funcidx);

    struct wasm1p1_parser_limit_t
    {
        ::std::size_t max_data_count_sec_count{default_max_data_count_sec_count};
        ::std::size_t max_elem_sec_expr{default_max_elem_sec_expr};
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
