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
    /// @warning Extension point: new runtime global categories require storage, initializer, local_imported, compiler, and printable-name updates.
    enum class global_type : unsigned
    {
        wasm_i32,
        wasm_i64,
        wasm_f32,
        wasm_f64,
        wasm_v128,
        wasm_ref
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
            case global_type::wasm_ref:
            {
                return u8"ref";
            }
            [[unlikely]] default:
            {
                return u8"unknown";
            }
        }
    }

    /// @warning Extension point: keep this union synchronized with global_type and every parser value type that can appear in a global.
    union wasm_global_storage_u
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 i32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 i64;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 f32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 f64;
        ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128 v128;
        ::uwvm2::object::global::wasm_global_ref_t ref;
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
