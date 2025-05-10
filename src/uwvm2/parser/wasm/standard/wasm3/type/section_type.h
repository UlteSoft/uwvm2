/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-04
 * @copyright   APL-2 License
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

#ifdef UWVM_MODULE
import uwvm2.parser.wasm.standard.wasm2;
#else
// std
# include <cstdint>
# include <cstddef>
# include <concepts>
# include <bit>
// macro
# include <uwvm2/parser/wasm/feature/feature_push_macro.h>
# include <uwvm2/parser/wasm/standard/wasm2/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm3::type
{
    /// @brief      Limits
    /// @details    Limits classify the size range of resizeable storage associated with memory types and table types.
    /// @details    Modified limits structure in "WebAssembly Release 1.0 (2019-07-20) § 2.3.4", content forward compatible, structure cannot be inherited.
    /// @see        WebAssembly Release 3.0 (Draft 2024-09-21) § 2.3.12
    struct limits
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64 min{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64 max{};
        bool present_max{};
    };

}  // namespace uwvm2::parser::wasm::standard::wasm3::type

#ifndef UWVM_MODULE
// macro
# include <uwvm2/parser/wasm/feature/feature_pop_macro.h>
#endif