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
# include <cstdint>
# include <cstddef>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm_custom/customs/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include "para.h"
# include "cwrapper.h"
# include "dl.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::type
{
    using wasm_wws_storage_t = ::uwvm2::uwvm::wasm::type::wasm_dl_storage_t;

    struct wasm_weak_symbol_t
    {
        // Accurate module names that must work
        ::uwvm2::utils::container::u8string_view module_name{};
        // week symbol handler (same as DL handler)
        wasm_wws_storage_t wasm_wws_storage{};
        // wasm_parameter_u
        ::uwvm2::uwvm::wasm::type::wasm_parameter_u wasm_parameter{};
    };

    // Conversion from CAPI
    struct uwvm_weak_symbol_module_t
    {
        char8_t const* module_name_ptr;
        ::std::size_t module_name_length;
        ::uwvm2::uwvm::wasm::type::capi_custom_handler_vec_t custom_handler_vec;
        ::uwvm2::uwvm::wasm::type::capi_function_vec_t function_vec;
    };
}  // namespace uwvm2::uwvm::wasm::storage

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
