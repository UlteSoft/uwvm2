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
# include <cstring>
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/object/impl.h>
# include "define.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t unreachable_func{};  // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_call_func_t call_func{};    // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_call_indirect_func_t call_indirect_func{};  // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t trap_invalid_conversion_to_integer_func{};  // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t trap_integer_divide_by_zero_func{};         // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t trap_integer_overflow_func{};               // [global]
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
