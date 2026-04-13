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
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::object::global
{
#if 0
    /// @todo wasm3.0

    enum class wasm_ref_kind : unsigned
    {
        wasm_null,
        wasm_func,
        wasm_extern,
        wasm_struct,
        wasm_array,
        wasm_exn,
        wasm_i31
    };

    union wasm_global_ref_storage_u
    {
        void* ptr;
        ::uwvm2::parser::wasm::standard::wasm3::type::wasm_i31 wasm_i31;
    };

    struct wasm_global_ref_t
    {
        wasm_global_ref_storage_u storage;
        ::uwvm2::object::global::wasm_ref_kind kind;
    };

#endif
}  // namespace uwvm2::object::global

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
