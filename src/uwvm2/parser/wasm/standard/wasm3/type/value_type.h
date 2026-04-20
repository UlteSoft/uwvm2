/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-31
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
# include <climits>
# include <concepts>
# include <bit>
// macro
# include <uwvm2/parser/wasm/feature/feature_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm3::type
{
/// @brief      The abstract type i31 denotes unboxed scalars, that is, integers injected into references. Their observable value range is limited to 31 bits
/// @see        WebAssembly Release 3.0 (Draft 2024-09-21) § 2.2.3
/// @note       The i31 type specification in Wasm does not mandate whether tags must reside in the high or low byte. Therefore, bit fields are used here for
///             implementation, with access optimizations applied for certain big-endian systems.
#if defined(__BIG_ENDIAN__)
    // GCC, LLVM: Big Endian

    struct wasm_i31
    {
        bool tag : 1;
        ::std::int_least32_t value : 31;
    };
#else
    // GCC, LLVM: Little Endian
    // GCC: PDP11 Endian
    // MSVC: MS Platform (msabi: sizeof(wasm_i31) == 2uz * sizeof(::std::int_least32_t))

    struct wasm_i31
    {
        ::std::int_least32_t value : 31;
        bool tag : 1;
    };
#endif
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/parser/wasm/feature/feature_pop_macro.h>
#endif
