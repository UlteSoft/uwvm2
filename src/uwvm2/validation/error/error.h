/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-07-07
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
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
# include <limits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/intrinsics/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/utils/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::validation::error
{
    enum class code_validation_error_code : ::std::uint_least32_t
    {
        ok = 0u,
        missing_end,
        missing_block_type,
        illegal_block_type,
        illegal_opbase,
        operand_stack_underflow,
        select_type_mismatch,
        select_cond_type_not_i32,
        if_cond_type_not_i32,
        illegal_else,
        if_then_result_mismatch,
        if_missing_else,
        end_result_mismatch,
        trailing_code_after_end,
        invalid_label_index,
        illegal_label_index,
        br_value_type_mismatch,
        br_cond_type_not_i32,
        br_table_target_type_mismatch,
        invalid_function_index_encoding,
        invalid_type_index,
        illegal_type_index,
        invalid_table_index,
        illegal_table_index,
        invalid_memory_index,
        illegal_memory_index,
        local_set_type_mismatch,
        local_tee_type_mismatch,
        invalid_global_index,
        illegal_global_index,
        immutable_global_set,
        global_set_type_mismatch,
        no_memory,
        invalid_memarg_align,
        invalid_memarg_offset,
        illegal_memarg_alignment,
        memarg_address_type_not_i32,
        not_local_function,
        invalid_function_index,
        invalid_local_index,
        illegal_local_index,
        store_value_type_mismatch,
        memory_grow_delta_type_not_i32,
        invalid_const_immediate,
        numeric_operand_type_mismatch,
    };

    /// @brief define IEEE 754 F32 and F64
    using error_f32 = ::uwvm2::utils::precfloat::float32_t;
    using error_f64 = ::uwvm2::utils::precfloat::float64_t;

    /// @brief used for duplicate_exports_of_the_same_export_type
    struct operand_stack_underflow_err_t
    {
        ::uwvm2::utils::container::u8string_view op_code_name;
        ::std::size_t stack_size_actual;
        ::std::size_t stack_size_required;
    };

    struct select_type_mismatch_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type type_v1;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type type_v2;
    };

    struct select_cond_type_not_i32_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type cond_type;
    };

    struct if_cond_type_not_i32_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type cond_type;
    };

    struct if_then_result_mismatch_err_t
    {
        ::std::size_t expected_count;
        ::std::size_t actual_count;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type expected_type;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type actual_type;
    };

    struct if_missing_else_err_t
    {
        ::std::size_t expected_count;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type expected_type;
    };

    struct end_result_mismatch_err_t
    {
        ::uwvm2::utils::container::u8string_view block_kind;
        ::std::size_t expected_count;
        ::std::size_t actual_count;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type expected_type;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type actual_type;
    };

    struct illegal_label_index_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 label_index;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_label_count;
    };

    struct br_value_type_mismatch_err_t
    {
        ::uwvm2::utils::container::u8string_view op_code_name;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type expected_type;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type actual_type;
    };

    struct br_cond_type_not_i32_err_t
    {
        ::uwvm2::utils::container::u8string_view op_code_name;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type cond_type;
    };

    struct br_table_target_type_mismatch_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 expected_label_index;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 mismatched_label_index;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 expected_arity;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 actual_arity;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type expected_type;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type actual_type;
    };

    struct illegal_type_index_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 type_index;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_type_count;
    };

    struct illegal_table_index_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_index;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_table_count;
    };

    struct illegal_memory_index_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memory_index;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_memory_count;
    };

    struct local_variable_type_mismatch_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_index;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type expected_type;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type actual_type;
    };

    struct immutable_global_set_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 global_index;
    };

    struct global_variable_type_mismatch_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 global_index;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type expected_type;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type actual_type;
    };

    struct illegal_memarg_alignment_err_t
    {
        ::uwvm2::utils::container::u8string_view op_code_name;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 max_align;
    };

    struct no_memory_err_t
    {
        ::uwvm2::utils::container::u8string_view op_code_name;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;
    };

    struct memarg_address_type_not_i32_err_t
    {
        ::uwvm2::utils::container::u8string_view op_code_name;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type addr_type;
    };

    struct not_local_function_err_t
    {
        ::std::size_t function_index;
    };

    struct invalid_function_index_err_t
    {
        ::std::size_t function_index;
        ::std::size_t all_function_size;
    };

    struct illegal_local_index_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_index;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_local_count;
    };

    struct illegal_global_index_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 global_index;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_global_count;
    };

    struct store_value_type_mismatch_err_t
    {
        ::uwvm2::utils::container::u8string_view op_code_name;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type expected_type;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type actual_type;
    };

    struct memory_grow_delta_type_not_i32_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type delta_type;
    };

    struct invalid_const_immediate_err_t
    {
        ::uwvm2::utils::container::u8string_view op_code_name;
    };

    struct numeric_operand_type_mismatch_err_t
    {
        ::uwvm2::utils::container::u8string_view op_code_name;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type expected_type;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type actual_type;
    };

    /// @brief Additional information provided by wasm error
    union code_validation_error_selectable_t
    {
        operand_stack_underflow_err_t operand_stack_underflow;
        static_assert(::std::is_trivially_copyable_v<operand_stack_underflow_err_t> && ::std::is_trivially_destructible_v<operand_stack_underflow_err_t>);

        select_type_mismatch_err_t select_type_mismatch;
        static_assert(::std::is_trivially_copyable_v<select_type_mismatch_err_t> && ::std::is_trivially_destructible_v<select_type_mismatch_err_t>);

        select_cond_type_not_i32_err_t select_cond_type_not_i32;
        static_assert(::std::is_trivially_copyable_v<select_cond_type_not_i32_err_t> && ::std::is_trivially_destructible_v<select_cond_type_not_i32_err_t>);

        if_cond_type_not_i32_err_t if_cond_type_not_i32;
        static_assert(::std::is_trivially_copyable_v<if_cond_type_not_i32_err_t> && ::std::is_trivially_destructible_v<if_cond_type_not_i32_err_t>);

        if_then_result_mismatch_err_t if_then_result_mismatch;
        static_assert(::std::is_trivially_copyable_v<if_then_result_mismatch_err_t> && ::std::is_trivially_destructible_v<if_then_result_mismatch_err_t>);

        if_missing_else_err_t if_missing_else;
        static_assert(::std::is_trivially_copyable_v<if_missing_else_err_t> && ::std::is_trivially_destructible_v<if_missing_else_err_t>);

        end_result_mismatch_err_t end_result_mismatch;
        static_assert(::std::is_trivially_copyable_v<end_result_mismatch_err_t> && ::std::is_trivially_destructible_v<end_result_mismatch_err_t>);

        illegal_label_index_err_t illegal_label_index;
        static_assert(::std::is_trivially_copyable_v<illegal_label_index_err_t> && ::std::is_trivially_destructible_v<illegal_label_index_err_t>);

        br_value_type_mismatch_err_t br_value_type_mismatch;
        static_assert(::std::is_trivially_copyable_v<br_value_type_mismatch_err_t> && ::std::is_trivially_destructible_v<br_value_type_mismatch_err_t>);

        br_cond_type_not_i32_err_t br_cond_type_not_i32;
        static_assert(::std::is_trivially_copyable_v<br_cond_type_not_i32_err_t> && ::std::is_trivially_destructible_v<br_cond_type_not_i32_err_t>);

        br_table_target_type_mismatch_err_t br_table_target_type_mismatch;
        static_assert(::std::is_trivially_copyable_v<br_table_target_type_mismatch_err_t> &&
                      ::std::is_trivially_destructible_v<br_table_target_type_mismatch_err_t>);

        illegal_type_index_err_t illegal_type_index;
        static_assert(::std::is_trivially_copyable_v<illegal_type_index_err_t> && ::std::is_trivially_destructible_v<illegal_type_index_err_t>);

        illegal_table_index_err_t illegal_table_index;
        static_assert(::std::is_trivially_copyable_v<illegal_table_index_err_t> && ::std::is_trivially_destructible_v<illegal_table_index_err_t>);

        illegal_memory_index_err_t illegal_memory_index;
        static_assert(::std::is_trivially_copyable_v<illegal_memory_index_err_t> && ::std::is_trivially_destructible_v<illegal_memory_index_err_t>);

        local_variable_type_mismatch_err_t local_variable_type_mismatch;
        static_assert(::std::is_trivially_copyable_v<local_variable_type_mismatch_err_t> &&
                      ::std::is_trivially_destructible_v<local_variable_type_mismatch_err_t>);

        not_local_function_err_t not_local_function;
        static_assert(::std::is_trivially_copyable_v<not_local_function_err_t> && ::std::is_trivially_destructible_v<not_local_function_err_t>);

        invalid_function_index_err_t invalid_function_index;
        static_assert(::std::is_trivially_copyable_v<invalid_function_index_err_t> && ::std::is_trivially_destructible_v<invalid_function_index_err_t>);

        illegal_local_index_err_t illegal_local_index;
        static_assert(::std::is_trivially_copyable_v<illegal_local_index_err_t> && ::std::is_trivially_destructible_v<illegal_local_index_err_t>);

        illegal_global_index_err_t illegal_global_index;
        static_assert(::std::is_trivially_copyable_v<illegal_global_index_err_t> && ::std::is_trivially_destructible_v<illegal_global_index_err_t>);

        immutable_global_set_err_t immutable_global_set;
        static_assert(::std::is_trivially_copyable_v<immutable_global_set_err_t> && ::std::is_trivially_destructible_v<immutable_global_set_err_t>);

        global_variable_type_mismatch_err_t global_variable_type_mismatch;
        static_assert(::std::is_trivially_copyable_v<global_variable_type_mismatch_err_t> &&
                      ::std::is_trivially_destructible_v<global_variable_type_mismatch_err_t>);

        illegal_memarg_alignment_err_t illegal_memarg_alignment;
        static_assert(::std::is_trivially_copyable_v<illegal_memarg_alignment_err_t> && ::std::is_trivially_destructible_v<illegal_memarg_alignment_err_t>);

        no_memory_err_t no_memory;
        static_assert(::std::is_trivially_copyable_v<no_memory_err_t> && ::std::is_trivially_destructible_v<no_memory_err_t>);

        memarg_address_type_not_i32_err_t memarg_address_type_not_i32;
        static_assert(::std::is_trivially_copyable_v<memarg_address_type_not_i32_err_t> &&
                      ::std::is_trivially_destructible_v<memarg_address_type_not_i32_err_t>);

        store_value_type_mismatch_err_t store_value_type_mismatch;
        static_assert(::std::is_trivially_copyable_v<store_value_type_mismatch_err_t> && ::std::is_trivially_destructible_v<store_value_type_mismatch_err_t>);

        memory_grow_delta_type_not_i32_err_t memory_grow_delta_type_not_i32;
        static_assert(::std::is_trivially_copyable_v<memory_grow_delta_type_not_i32_err_t> &&
                      ::std::is_trivially_destructible_v<memory_grow_delta_type_not_i32_err_t>);

        invalid_const_immediate_err_t invalid_const_immediate;
        static_assert(::std::is_trivially_copyable_v<invalid_const_immediate_err_t> && ::std::is_trivially_destructible_v<invalid_const_immediate_err_t>);

        numeric_operand_type_mismatch_err_t numeric_operand_type_mismatch;
        static_assert(::std::is_trivially_copyable_v<numeric_operand_type_mismatch_err_t> &&
                      ::std::is_trivially_destructible_v<numeric_operand_type_mismatch_err_t>);

        ::std::byte const* err_end;
        ::std::size_t err_uz;
        ::std::ptrdiff_t err_pdt;

        ::std::uint_least64_t u64;
        ::std::int_least64_t i64;
        ::std::uint_least32_t u32;
        ::std::int_least32_t i32;
        ::std::uint_least16_t u16;
        ::std::int_least16_t i16;
        ::std::uint_least8_t u8;
        ::std::int_least8_t i8;

        error_f64 f64;
        error_f32 f32;
        bool boolean;

        ::std::uint_least64_t u64arr[1];
        ::std::int_least64_t i64arr[1];
        ::std::uint_least32_t u32arr[2];
        ::std::int_least32_t i32arr[2];
        ::std::uint_least16_t u16arr[4];
        ::std::int_least16_t i16arr[4];
        ::std::uint_least8_t u8arr[8];
        ::std::int_least8_t i8arr[8];

        error_f64 f64arr[1];
        error_f32 f32arr[2];
        bool booleanarr[8];
    };

    /// @brief Structured structure, with reference form as formal parameter
    struct code_validation_error_impl
    {
        code_validation_error_selectable_t err_selectable{};
        ::std::byte const* err_curr{};
        code_validation_error_code err_code{};
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
