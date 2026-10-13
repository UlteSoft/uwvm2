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
# include <cstring>
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/object/impl.h>
# include "define.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    inline interpreter_indirect_tail_transfer_func_t indirect_tail_transfer_func{}; // [global]
    inline interpreter_ref_tail_transfer_func_t ref_tail_transfer_func{}; // [global]
    inline interpreter_tail_transfer_func_t tail_transfer_func{}; // [global] installed before execution admission
    inline atomic_wait_trap_func_t trap_atomic_wait_func{}; // [global] installed before execution admission
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t trap_unaligned_atomic_func{}; // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t unreachable_func{};                  // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_call_func_t call_func{};                    // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_call_indirect_func_t call_indirect_func{};  // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_call_ref_func_t call_ref_func{};  // [global]
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_loop_osr_func_t tiered_loop_osr_func{};  // [global]
# endif
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t trap_invalid_conversion_to_integer_func{};  // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t trap_integer_divide_by_zero_func{};         // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t trap_integer_overflow_func{};               // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t trap_null_reference_func{}; // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t trap_array_out_of_bounds_func{}; // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t trap_gc_allocation_failure_func{}; // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t trap_cast_failure_func{}; // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func_t trap_table_out_of_bounds_func{};            // [global]
    inline ::uwvm2::runtime::compiler::uwvm_int::optable::memory_out_of_bounds_func_t trap_memory_out_of_bounds_func{};  // [global]
}
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
