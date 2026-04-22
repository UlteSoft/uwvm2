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
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm_custom/customs/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::storage
{
    inline ::uwvm2::utils::container::unordered_flat_map<::uwvm2::utils::container::u8string_view, ::uwvm2::uwvm::wasm::type::all_module_t>
        all_module{};  // [global]

    using wasm_module_name_t = ::uwvm2::utils::container::u8string_view;
    using wasm_import_export_name_t = ::uwvm2::utils::container::u8string_view;

    inline ::uwvm2::utils::container::unordered_flat_map<
        wasm_module_name_t,
        ::uwvm2::utils::container::unordered_flat_map<wasm_import_export_name_t, ::uwvm2::uwvm::wasm::type::all_module_export_t>>
        all_module_export{};  // [global]
}  // namespace uwvm2::uwvm::wasm::storage
