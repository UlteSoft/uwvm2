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
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/impl.h>
# include "ref.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::object::global
{
    enum class global_type : unsigned
    {
        wasm_i32,
        wasm_i64,
        wasm_f32,
        wasm_f64,
        wasm_v128,
#if 0
        /// @todo wasm3.0
        wasm_ref
#endif
    };

    inline constexpr ::uwvm2::utils::container::u8string_view get_global_type_name(global_type type) noexcept
    {
        switch(type)
        {
            case global_type::wasm_i32:
            {
                return u8"i32";
            }
            case global_type::wasm_i64:
            {
                return u8"i64";
            }
            case global_type::wasm_f32:
            {
                return u8"f32";
            }
            case global_type::wasm_f64:
            {
                return u8"f64";
            }
            case global_type::wasm_v128:
            {
                return u8"v128";
            }
            /// @todo wasm3.0
            [[unlikely]] default:
            {
                return u8"unknown";
            }
        }
    }

    union wasm_global_storage_u
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 i32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 i64;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 f32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 f64;
        ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128 v128;
#if 0
        /// @todo wasm3.0
        ::uwvm2::object::global::wasm_global_ref_t ref;
#endif
    };

    struct wasm_global_storage_t
    {
        wasm_global_storage_u storage;
        global_type kind;
        bool is_mutable;
    };

}  // namespace uwvm2::object::global

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
