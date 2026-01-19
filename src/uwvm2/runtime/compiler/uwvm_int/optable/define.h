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
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/object/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    using wasm1_code = ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic;
    using wasm1_code_version_type = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version;

    struct uwvm_interpreter_function_operands_t
    {
        ::uwvm2::utils::container::vector<::std::byte> operands{};
    };

    struct local_func_storage_t
    {
        ::std::size_t local_count{};
        ::std::size_t operand_stack_max{};

        uwvm_interpreter_function_operands_t op{};
    };

    struct uwvm_interpreter_full_function_symbol_t
    {
        ::std::size_t local_count{};
        ::std::size_t operand_stack_max{};

        ::uwvm2::utils::container::vector<local_func_storage_t const*> imported_func_operands_ptrs{};
        ::uwvm2::utils::container::vector<local_func_storage_t> local_funcs{};
    };

    union wasm_stack_top_i32_with_f32_u
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 i32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 f32;
    };

    union wasm_stack_top_i32_i64_f32_f64_u
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 i32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 i64;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 f32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 f64;
    };

    struct uwvm_interpreter_translate_option_t
    {
        /// @brief    tail_call
        /// @details  The `is_tail_call` parameter provides a feature that allows a function to generate a tail call rather than returning directly to the
        ///           higher‑level loop interpreter.
        ///
        ///           tail_call(::std::byte const* opcurr)
        ///           {
        ///               auto const opcurr_tail{opcurr};
        ///               opcurr += sizeof(void*);
        ///               ptr_t next_func;
        ///               ::std::memcpy(::std::addressof(next_func), opcurr_tail, sizeof(void*));
        ///               if constexpr(is_tail_call) [[gnu::musttail]] return next_func(opcurr);
        ///               // else == return
        ///           }
        bool is_tail_call{};

        /// @brief    local_stack_ptr_pos
        /// @details  Represents the position within a variadic template parameter.
        ///           SIZE_MAX = unknown
        ::std::size_t local_stack_ptr_pos{SIZE_MAX};

        /// @brief    operand_stack_ptr_pos
        /// @details  Represents the position within a variadic template parameter.
        ///           SIZE_MAX = unknown
        ::std::size_t operand_stack_ptr_pos{SIZE_MAX};

        /// @details  `local_stack_ptr_pos` and `operand_stack_ptr_pos` can be merged; set one of them to `SIZE_MAX`, and the other will be used as the base
        ///           address. If both are set to `SIZE_MAX`, a compile‑time error will occur.

        /// @brief   Because the target platform bitness and ABI may differ, the “stack-top optimization” must explicitly distinguish and track the cached
        ///          top-of-stack ranges for each value type.
        /// @details The begin/end fields describe the register/slot range occupied by a given type in top-of-stack caching. The idea is to map the top few
        ///          operand-stack values into registers (or register pairs) to reduce memory stack loads/stores.
        ///
        ///          1) On 32-bit targets, 64-bit values (i64/f64) typically cannot be cached in a single general-purpose register the way 32-bit values can
        ///             (they may require a register pair or must fall back to memory), so they must be tracked separately from i32/f32.
        ///
        ///          2) Whether floating-point values use a separate FP register file depends on the ABI. Some ABIs (typical hard-float) place f32/f64 in
        ///             dedicated FP registers, while others (e.g. MS ABI or some soft-float/no-hardfpu cases) may co-locate float values with integers in
        ///             GPRs or otherwise share the same caching slot layout.
        ///
        ///          3) If the f32/f64 top begin/end ranges fully coincide with the corresponding i32/i64 ranges, they are sharing the same registers/slots.
        ///             In that case a union can be used for same-location reinterpretation to reuse the stack-top optimization. If the ranges do not overlap,
        ///             floats and integers are allocated in different register files/slots and must be handled separately.
        ::std::size_t i32_stack_top_begin_pos{SIZE_MAX};
        ::std::size_t i32_stack_top_end_pos{SIZE_MAX};

        ::std::size_t i64_stack_top_begin_pos{SIZE_MAX};
        ::std::size_t i64_stack_top_end_pos{SIZE_MAX};

        ::std::size_t f32_stack_top_begin_pos{SIZE_MAX};
        ::std::size_t f32_stack_top_end_pos{SIZE_MAX};

        ::std::size_t f64_stack_top_begin_pos{SIZE_MAX};
        ::std::size_t f64_stack_top_end_pos{SIZE_MAX};

#if 0
        /// @brief   v128 stack-top caching range (SIMD).
        /// @details v128 is a 128-bit SIMD value. On many mainstream ABIs/ISAs, SIMD registers share the same physical register file as floating-point
        ///          registers (e.g. x86 SSE XMM, AArch64/ARM NEON V registers). In such cases, v128 can be co-located with f32/f64 in the same top-of-stack
        ///          cache slots/registers, and the overlap can be detected the same way as described above:
        ///          if `v128_stack_top_begin_pos/end_pos` fully coincide with `f32_stack_top_*` / `f64_stack_top_*`, they are sharing the same slots.
        ///
        ///          If the ranges do not overlap, it indicates that v128 is allocated in a distinct register class or uses a different caching layout, and
        ///          must be handled separately.
        ::std::size_t v128_stack_top_begin_pos{SIZE_MAX};
        ::std::size_t v128_stack_top_end_pos{SIZE_MAX};
#endif
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
