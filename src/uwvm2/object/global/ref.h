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
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::object::global
{
    /// @warning Extension point: new reference kinds require parser validation, runtime global/table storage, local_imported wrappers, and GC/host integration.
    enum class wasm_ref_kind : unsigned
    {
        wasm_null,
        wasm_func,
        wasm_extern,
#if 0
        /// @warning Extension point: wasm3 GC reference kinds live here once the runtime representation is ready.
        /// @todo wasm3.0
        wasm_struct,
        wasm_array,
        wasm_exn,
        wasm_i31
#endif
    };

    union wasm_global_ref_storage_u
    {
        void* ptr;
#if 0
        /// @warning Extension point: wasm3 immediate reference payloads must be kept synchronized with wasm_ref_kind.
        /// @todo wasm3.0
        ::uwvm2::parser::wasm::standard::wasm3::type::wasm_i31 wasm_i31;
#endif
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_idx;
    };

    struct wasm_global_ref_t
    {
        wasm_global_ref_storage_u storage;
        ::uwvm2::object::global::wasm_ref_kind kind;
    };

    struct wasm_funcref_t
    {
        ::uwvm2::object::global::wasm_global_ref_t ref;
    };

    struct wasm_externref_t
    {
        ::uwvm2::object::global::wasm_global_ref_t ref;
    };

    static_assert(sizeof(wasm_funcref_t) == sizeof(wasm_global_ref_t));
    static_assert(sizeof(wasm_externref_t) == sizeof(wasm_global_ref_t));
    static_assert(alignof(wasm_funcref_t) == alignof(wasm_global_ref_t));
    static_assert(alignof(wasm_externref_t) == alignof(wasm_global_ref_t));
}  // namespace uwvm2::object::global

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
