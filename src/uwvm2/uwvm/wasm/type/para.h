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
# include <cstring>
# include <new>
# include <memory>
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm_custom/customs/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::type
{
    struct wasm_parameter_t
    {
        ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t binfmt1_para{};
        static_assert(::std::is_trivially_copyable_v<decltype(binfmt1_para)> && ::std::is_trivially_destructible_v<decltype(binfmt1_para)>);
    };

    static_assert(::std::is_trivially_copyable_v<wasm_parameter_t> && ::std::is_trivially_destructible_v<wasm_parameter_t>);
}  // namespace uwvm2::uwvm::wasm::storage

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
