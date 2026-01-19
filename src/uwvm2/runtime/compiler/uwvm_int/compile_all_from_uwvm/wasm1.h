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
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/validation/error/impl.h>
# include <uwvm2/object/impl.h>
/// @note This requires a dependency after uwvm2.uwvm.runtime.storage.
# include <uwvm2/uwvm/wasm/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/optable/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm
{
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t compile_option>
    inline constexpr ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t compile_all_from_uwvm_single_func(
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t storage{};

        // Note: This is a compiler-side, standalone copy of the wasm1 validator logic.
        // It validates decayed `wasm_module_storage_t` (not parser storage) and must not depend on the standard validator implementation.

        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_byte = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte;
        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        using code_validation_error_code = ::uwvm2::validation::error::code_validation_error_code;

        // no necessary to set err to default

        enum class block_type : unsigned
        {
            function,
            block,
            loop,
            if_,
            else_
        };

        struct block_result_type
        {
            wasm_value_type const* begin{};
            wasm_value_type const* end{};
        };

        struct block_t
        {
            block_result_type result{};
            ::std::size_t operand_stack_base{};
            block_type type{};
            bool polymorphic_base{};
            bool then_polymorphic_end{};  // only meaningful for if/else frames
        };

        struct operand_stack_storage_t
        {
            wasm_value_type type{};
        };

        auto const import_func_count{curr_module.imported_function_vec_storage.size()};
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};

        auto const imported_global_count{static_cast<wasm_u32>(curr_module.imported_global_vec_storage.size())};
        auto const local_global_count{static_cast<wasm_u32>(curr_module.local_defined_global_vec_storage.size())};
        auto const all_global_count{static_cast<wasm_u32>(imported_global_count + local_global_count)};

        auto const imported_table_count{static_cast<wasm_u32>(curr_module.imported_table_vec_storage.size())};
        auto const local_table_count{static_cast<wasm_u32>(curr_module.local_defined_table_vec_storage.size())};
        auto const all_table_count{static_cast<wasm_u32>(imported_table_count + local_table_count)};

        auto const imported_memory_count{static_cast<wasm_u32>(curr_module.imported_memory_vec_storage.size())};
        auto const local_memory_count{static_cast<wasm_u32>(curr_module.local_defined_memory_vec_storage.size())};
        auto const all_memory_count{static_cast<wasm_u32>(imported_memory_count + local_memory_count)};

        for(::std::size_t local_function_idx{}; local_function_idx < local_func_count; ++local_function_idx)
        {
            ::std::size_t const function_index{import_func_count + local_function_idx};

            auto const& curr_local_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_idx)};
            auto const& curr_func_type{*curr_local_func.function_type_ptr};
            auto const& curr_code{*curr_local_func.wasm_code_ptr};

            auto const* const code_begin{reinterpret_cast<::std::byte const*>(curr_code.body.expr_begin)};
            auto const* const code_end{reinterpret_cast<::std::byte const*>(curr_code.body.code_end)};

            // check
            if(function_index < import_func_count) [[unlikely]]
            {
                err.err_curr = code_begin;
                err.err_selectable.not_local_function.function_index = function_index;
                err.err_code = code_validation_error_code::not_local_function;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const local_func_idx{function_index - import_func_count};
            if(local_func_idx >= local_func_count) [[unlikely]]
            {
                err.err_curr = code_begin;
                err.err_selectable.invalid_function_index.function_index = function_index;
                err.err_selectable.invalid_function_index.all_function_size = import_func_count + local_func_count;
                err.err_code = code_validation_error_code::invalid_function_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const func_parameter_begin{curr_func_type.parameter.begin};
            auto const func_parameter_end{curr_func_type.parameter.end};
            auto const func_parameter_count_uz{static_cast<::std::size_t>(func_parameter_end - func_parameter_begin)};
            auto const func_parameter_count_u32{static_cast<wasm_u32>(func_parameter_count_uz)};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(func_parameter_count_u32 != func_parameter_count_uz) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

            auto const& curr_code_locals{curr_code.locals};

            // all local count = parameter + local defined local count
            wasm_u32 all_local_count{func_parameter_count_u32};
            for(auto const& local_part: curr_code_locals)
            {
                // all_local_count never overflow and never exceed the max of u32 (validated by parser limits)
                all_local_count += local_part.count;
            }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if constexpr(::std::numeric_limits<wasm_u32>::max() > ::std::numeric_limits<::std::size_t>::max())
            {
                if(all_local_count > ::std::numeric_limits<::std::size_t>::max()) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
            }
#endif

            // control-flow stack
            using curr_block_type = block_t;
            ::uwvm2::utils::container::vector<curr_block_type> control_flow_stack{};

            // operand stack
            using curr_operand_stack_value_type = wasm_value_type;
            using curr_operand_stack_type = ::uwvm2::utils::container::vector<operand_stack_storage_t>;
            curr_operand_stack_type operand_stack{};
            bool is_polymorphic{};

            ::uwvm2::runtime::compiler::uwvm_int::optable::local_func_storage_t local_func_symbol{};
            local_func_symbol.local_count = static_cast<::std::size_t>(all_local_count);

            // block type
            using value_type_enum = curr_operand_stack_value_type;
            static constexpr value_type_enum i32_result_arr[1u]{value_type_enum::i32};
            static constexpr value_type_enum i64_result_arr[1u]{value_type_enum::i64};
            static constexpr value_type_enum f32_result_arr[1u]{value_type_enum::f32};
            static constexpr value_type_enum f64_result_arr[1u]{value_type_enum::f64};

            // function block (label/result type is the function result)
            control_flow_stack.push_back({
                .result = {.begin = curr_func_type.result.begin, .end = curr_func_type.result.end},
                .operand_stack_base = 0uz,
                .type = block_type::function,
                .polymorphic_base = false,
                .then_polymorphic_end = false
            });

            // start parse the code
            auto code_curr{code_begin};

            using wasm_value_type_u = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;

            auto const validate_numeric_unary{
                [&](::uwvm2::utils::container::u8string_view op_name,
                    curr_operand_stack_value_type expected_operand_type,
                    curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
                {
                    // op_name ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // op_name ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // op_name ...
                    // [safe]  unsafe (could be the section_end)
                    //         ^^ code_curr

                    if(!is_polymorphic && operand_stack.empty()) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.operand_stack_underflow.op_code_name = op_name;
                        err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                        err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                        err.err_code = code_validation_error_code::operand_stack_underflow;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    bool operand_from_stack{};
                    curr_operand_stack_value_type operand_type{};
                    if(!operand_stack.empty())
                    {
                        operand_from_stack = true;
                        operand_type = operand_stack.back_unchecked().type;
                        operand_stack.pop_back_unchecked();
                    }

                    if(!is_polymorphic && operand_from_stack && operand_type != expected_operand_type) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                        err.err_selectable.numeric_operand_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_operand_type);
                        err.err_selectable.numeric_operand_type_mismatch.actual_type = static_cast<wasm_value_type_u>(operand_type);
                        err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    operand_stack.push_back({result_type});
                }};

            auto const validate_numeric_binary{
                [&](::uwvm2::utils::container::u8string_view op_name,
                    curr_operand_stack_value_type expected_operand_type,
                    curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
                {
                    // op_name ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // op_name ...
                    // [safe] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // op_name ...
                    // [safe ] unsafe (could be the section_end)
                    //         ^^ code_curr

                    if(!is_polymorphic && operand_stack.size() < 2uz) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.operand_stack_underflow.op_code_name = op_name;
                        err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                        err.err_selectable.operand_stack_underflow.stack_size_required = 2uz;
                        err.err_code = code_validation_error_code::operand_stack_underflow;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // rhs
                    curr_operand_stack_value_type rhs_type{};
                    bool rhs_from_stack{};
                    if(!operand_stack.empty())
                    {
                        rhs_from_stack = true;
                        rhs_type = operand_stack.back_unchecked().type;
                        operand_stack.pop_back_unchecked();
                    }

                    if(!is_polymorphic && rhs_from_stack && rhs_type != expected_operand_type) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                        err.err_selectable.numeric_operand_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_operand_type);
                        err.err_selectable.numeric_operand_type_mismatch.actual_type = static_cast<wasm_value_type_u>(rhs_type);
                        err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // lhs
                    curr_operand_stack_value_type lhs_type{};
                    bool lhs_from_stack{};
                    if(!operand_stack.empty())
                    {
                        lhs_from_stack = true;
                        lhs_type = operand_stack.back_unchecked().type;
                        operand_stack.pop_back_unchecked();
                    }

                    if(!is_polymorphic && lhs_from_stack && lhs_type != expected_operand_type) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                        err.err_selectable.numeric_operand_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_operand_type);
                        err.err_selectable.numeric_operand_type_mismatch.actual_type = static_cast<wasm_value_type_u>(lhs_type);
                        err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    operand_stack.push_back({result_type});
                }};

            auto const validate_mem_load{
                [&](::uwvm2::utils::container::u8string_view op_name,
                    wasm_u32 const max_align,
                    curr_operand_stack_value_type const result_type) constexpr UWVM_THROWS
                {
                    // i32.load align offset ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i32.load align offset ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i32.load align offset ...
                    // [ safe ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    wasm_u32 align;   // No initialization necessary
                    wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // i32.load align offset ...
                    // [    safe    ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // i32.load align offset ...
                    // [    safe    ] unsafe (could be the section_end)
                    //                ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // i32.load align offset ...
                    // [        safe       ] unsafe (could be the section_end)
                    //                ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // i32.load align offset ...
                    // [        safe       ] unsafe (could be the section_end)
                    //                       ^^ code_curr

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = op_name;
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(align > max_align) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = op_name;
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = max_align;
                        err.err_code = code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = op_name;
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != wasm_value_type_u::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = op_name;
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({result_type});
                }};

            auto const validate_mem_store{
                [&](::uwvm2::utils::container::u8string_view op_name,
                    wasm_u32 const max_align,
                    curr_operand_stack_value_type const expected_value_type) constexpr UWVM_THROWS
                {
                    // i32.store align offset ...
                    // [ safe  ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i32.store align offset ...
                    // [ safe  ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i32.store align offset ...
                    // [ safe  ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    wasm_u32 align;   // No initialization necessary
                    wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // i32.store align offset ...
                    // [      safe   ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // i32.store align offset ...
                    // [      safe   ] unsafe (could be the section_end)
                    //                 ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // i32.store align offset ...
                    // [      safe          ] unsafe (could be the section_end)
                    //                 ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // i32.store align offset ...
                    // [      safe          ] unsafe (could be the section_end)
                    //                        ^^ code_curr

                    // MVP memory instructions implicitly target memory 0. If the module has no imported/defined memory, any load/store is invalid.
                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = op_name;
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(align > max_align) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = op_name;
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = max_align;
                        err.err_code = code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(!is_polymorphic)
                    {
                        if(operand_stack.size() < 2uz) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = op_name;
                            err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                            err.err_selectable.operand_stack_underflow.stack_size_required = 2uz;
                            err.err_code = code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const value{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();
                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != wasm_value_type_u::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = op_name;
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(value.type != expected_value_type) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.store_value_type_mismatch.op_code_name = op_name;
                            err.err_selectable.store_value_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_value_type);
                            err.err_selectable.store_value_type_mismatch.actual_type = value.type;
                            err.err_code = code_validation_error_code::store_value_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }
                }};

            // [before_section ... ] | opbase opextent
            // [        safe       ] | unsafe (could be the section_end)
            //                         ^^ code_curr

            // a WebAssembly function with type '() -> ()' (often written as returning “nil”) can have no meaningful code, but it still must have a valid
            // instruction sequence—at minimum an end.

            ::std::size_t runtime_operand_stack_max{};

            for(;;)
            {
                if(code_curr == code_end) [[unlikely]]
                {
                    // [... ] | (end)
                    // [safe] | unsafe (could be the section_end)
                    //          ^^ code_curr

                    // Validation completes when the end is reached, so this condition can never be met. If it were met, it would indicate a missing end.

                    err.err_curr = code_curr;
                    err.err_code = code_validation_error_code::missing_end;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // opbase ...
                // [safe] unsafe (could be the section_end)
                // ^^ code_curr

                using wasm1_code = ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code;

                wasm1_code curr_opbase;  // no initialize necessary
                ::std::memcpy(::std::addressof(curr_opbase), code_curr, sizeof(wasm1_code));

                switch(curr_opbase)
                {
                    case wasm1_code::unreachable:
                    {
                        // unreachable ...
                        // [   safe  ] unsafe (could be the section_end)
                        // ^^ code_curr

                        ++code_curr;

                        // unreachable ...
                        // [   safe  ] unsafe (could be the section_end)
                        //             ^^ code_curr

                        if(!control_flow_stack.empty())
                        {
                            auto const base{control_flow_stack.back_unchecked().operand_stack_base};
                            while(operand_stack.size() > base) { operand_stack.pop_back_unchecked(); }
                        }

                        is_polymorphic = true;

                        break;
                    }
                    case wasm1_code::nop:
                    {
                        // nop    ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        ++code_curr;

                        // nop    ...
                        // [safe] unsafe (could be the section_end)
                        //        ^^ code_curr

                        break;
                    }
                    case wasm1_code::block:
                    {
                        // block  blocktype ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // block  blocktype ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // block  blocktype ...
                        // [safe] unsafe (could be the section_end)
                        //        ^^ op_begin

                        if(code_curr == code_end) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::missing_block_type;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
                        }

                        // block blocktype ...
                        // [     safe    ] unsafe (could be the section_end)
                        //       ^^ op_begin

                        wasm_byte blocktype_byte;
                        ::std::memcpy(::std::addressof(blocktype_byte), code_curr, sizeof(blocktype_byte));

                        ++code_curr;

                        // block blocktype ...
                        // [     safe    ] unsafe (could be the section_end)
                        //                 ^^ op_begin

                        block_result_type block_result{};
                        switch(blocktype_byte)
                        {
                            case 0x40u:
                            {
                                block_result = {};
                                break;
                            }
                            case static_cast<wasm_byte>(wasm_value_type_u::i32):
                            {
                                block_result.begin = i32_result_arr;
                                block_result.end = i32_result_arr + 1u;
                                break;
                            }
                            case static_cast<wasm_byte>(wasm_value_type_u::i64):
                            {
                                block_result.begin = i64_result_arr;
                                block_result.end = i64_result_arr + 1u;
                                break;
                            }
                            case static_cast<wasm_byte>(wasm_value_type_u::f32):
                            {
                                block_result.begin = f32_result_arr;
                                block_result.end = f32_result_arr + 1u;
                                break;
                            }
                            case static_cast<wasm_byte>(wasm_value_type_u::f64):
                            {
                                block_result.begin = f64_result_arr;
                                block_result.end = f64_result_arr + 1u;
                                break;
                            }
                            [[unlikely]] default:
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.u8 = blocktype_byte;
                                err.err_code = code_validation_error_code::illegal_block_type;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        control_flow_stack.push_back({.result = block_result,
                                                      .operand_stack_base = operand_stack.size(),
                                                      .type = block_type::block,
                                                      .polymorphic_base = is_polymorphic,
                                                      .then_polymorphic_end = false});

                        break;
                    }
                    case wasm1_code::loop:
                    {
                        // loop   blocktype ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // loop   blocktype ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // loop   blocktype ...
                        // [safe] unsafe (could be the section_end)
                        //        ^^ code_curr

                        if(code_curr == code_end) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::missing_block_type;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
                        }

                        // loop blocktype ...
                        // [    safe    ] unsafe (could be the section_end)
                        //      ^^ code_curr

                        wasm_byte blocktype_byte;
                        ::std::memcpy(::std::addressof(blocktype_byte), code_curr, sizeof(blocktype_byte));

                        ++code_curr;

                        // loop blocktype ...
                        // [    safe    ] unsafe (could be the section_end)
                        //                ^^ code_curr

                        block_result_type block_result{};
                        switch(blocktype_byte)
                        {
                            case 0x40u:
                            {
                                block_result = {};
                                break;
                            }
                            case static_cast<wasm_byte>(wasm_value_type_u::i32):
                            {
                                block_result.begin = i32_result_arr;
                                block_result.end = i32_result_arr + 1u;
                                break;
                            }
                            case static_cast<wasm_byte>(wasm_value_type_u::i64):
                            {
                                block_result.begin = i64_result_arr;
                                block_result.end = i64_result_arr + 1u;
                                break;
                            }
                            case static_cast<wasm_byte>(wasm_value_type_u::f32):
                            {
                                block_result.begin = f32_result_arr;
                                block_result.end = f32_result_arr + 1u;
                                break;
                            }
                            case static_cast<wasm_byte>(wasm_value_type_u::f64):
                            {
                                block_result.begin = f64_result_arr;
                                block_result.end = f64_result_arr + 1u;
                                break;
                            }
                            [[unlikely]] default:
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.u8 = blocktype_byte;
                                err.err_code = code_validation_error_code::illegal_block_type;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        control_flow_stack.push_back({.result = block_result,
                                                      .operand_stack_base = operand_stack.size(),
                                                      .type = block_type::loop,
                                                      .polymorphic_base = is_polymorphic,
                                                      .then_polymorphic_end = false});

                        break;
                    }
                    case wasm1_code::if_:
                    {
                        // if     blocktype ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // if     blocktype ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // if     blocktype ...
                        // [safe] unsafe (could be the section_end)
                        //        ^^ code_curr

                        if(code_curr == code_end) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::missing_block_type;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
                        }

                        // if blocktype ...
                        // [   safe   ] unsafe (could be the section_end)
                        //    ^^ code_curr

                        wasm_byte blocktype_byte;
                        ::std::memcpy(::std::addressof(blocktype_byte), code_curr, sizeof(blocktype_byte));

                        ++code_curr;

                        // if blocktype ...
                        // [   safe   ] unsafe (could be the section_end)
                        //              ^^ code_curr

                        block_result_type block_result{};
                        switch(blocktype_byte)
                        {
                            case 0x40u:
                            {
                                block_result = {};
                                break;
                            }
                            case static_cast<wasm_byte>(wasm_value_type_u::i32):
                            {
                                block_result.begin = i32_result_arr;
                                block_result.end = i32_result_arr + 1u;
                                break;
                            }
                            case static_cast<wasm_byte>(wasm_value_type_u::i64):
                            {
                                block_result.begin = i64_result_arr;
                                block_result.end = i64_result_arr + 1u;
                                break;
                            }
                            case static_cast<wasm_byte>(wasm_value_type_u::f32):
                            {
                                block_result.begin = f32_result_arr;
                                block_result.end = f32_result_arr + 1u;
                                break;
                            }
                            case static_cast<wasm_byte>(wasm_value_type_u::f64):
                            {
                                block_result.begin = f64_result_arr;
                                block_result.end = f64_result_arr + 1u;
                                break;
                            }
                            [[unlikely]] default:
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.u8 = blocktype_byte;
                                err.err_code = code_validation_error_code::illegal_block_type;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        if(!is_polymorphic && operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"if";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(!operand_stack.empty())
                        {
                            auto const cond{operand_stack.back_unchecked()};
                            operand_stack.pop_back_unchecked();
                            if(cond.type != curr_operand_stack_value_type::i32) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.if_cond_type_not_i32.cond_type = cond.type;
                                err.err_code = code_validation_error_code::if_cond_type_not_i32;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        control_flow_stack.push_back({.result = block_result,
                                                      .operand_stack_base = operand_stack.size(),
                                                      .type = block_type::if_,
                                                      .polymorphic_base = is_polymorphic,
                                                      .then_polymorphic_end = false});
                        break;
                    }
                    case wasm1_code::else_:
                    {
                        // else   ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // else   ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // else   ...
                        // [safe] unsafe (could be the section_end)
                        //        ^^ code_curr

                        if(control_flow_stack.empty() || control_flow_stack.back_unchecked().type != block_type::if_) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::illegal_else;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto& if_frame{control_flow_stack.back_unchecked()};

                        if(!is_polymorphic)
                        {
                            auto const expected_count{static_cast<::std::size_t>(if_frame.result.end - if_frame.result.begin)};
                            auto const actual_count{operand_stack.size() - if_frame.operand_stack_base};

                            bool mismatch{expected_count != actual_count};

                            wasm_value_type_u expected_type{};
                            wasm_value_type_u actual_type{};

                            bool const expected_single{expected_count == 1uz};
                            bool const actual_single{actual_count == 1uz};

                            if(expected_single) { expected_type = static_cast<wasm_value_type_u>(*if_frame.result.begin); }
                            if(actual_single) { actual_type = static_cast<wasm_value_type_u>(operand_stack.back_unchecked().type); }

                            if(!mismatch && expected_single && actual_single && expected_type != actual_type) { mismatch = true; }

                            if(mismatch) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.if_then_result_mismatch.expected_count = expected_count;
                                err.err_selectable.if_then_result_mismatch.actual_count = actual_count;
                                err.err_selectable.if_then_result_mismatch.expected_type = expected_type;
                                err.err_selectable.if_then_result_mismatch.actual_type = actual_type;
                                err.err_code = code_validation_error_code::if_then_result_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        if_frame.then_polymorphic_end = is_polymorphic;

                        while(operand_stack.size() > if_frame.operand_stack_base) { operand_stack.pop_back_unchecked(); }
                        is_polymorphic = if_frame.polymorphic_base;
                        if_frame.type = block_type::else_;

                        break;
                    }
                    case wasm1_code::end:
                    {
                        // end    ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // end    ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // end    ...
                        // [safe] unsafe (could be the section_end)
                        //        ^^ code_curr

                        if(control_flow_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(curr_opbase);
                            err.err_code = code_validation_error_code::illegal_opbase;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const frame{control_flow_stack.back_unchecked()};
                        bool const is_function_frame{frame.type == block_type::function};

                        ::uwvm2::utils::container::u8string_view block_kind;
                        switch(frame.type)
                        {
                            case block_type::function:
                            {
                                block_kind = u8"function";
                                break;
                            }
                            case block_type::block:
                            {
                                block_kind = u8"block";
                                break;
                            }
                            case block_type::loop:
                            {
                                block_kind = u8"loop";
                                break;
                            }
                            case block_type::if_:
                            {
                                block_kind = u8"if";
                                break;
                            }
                            case block_type::else_:
                            {
                                block_kind = u8"if-else";
                                break;
                            }
                            [[unlikely]] default:
                            {
                                block_kind = u8"block";
                                break;
                            }
                        }

                        auto const expected_count{static_cast<::std::size_t>(frame.result.end - frame.result.begin)};

                        if(frame.type == block_type::if_ && expected_count != 0uz) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.if_missing_else.expected_count = expected_count;
                            err.err_selectable.if_missing_else.expected_type = static_cast<wasm_value_type_u>(*frame.result.begin);
                            err.err_code = code_validation_error_code::if_missing_else;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const base{frame.operand_stack_base};
                        auto const stack_size{operand_stack.size()};
                        auto const actual_count{stack_size >= base ? stack_size - base : 0uz};

                        if(!is_polymorphic ? (actual_count != expected_count) : (actual_count > expected_count))
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.end_result_mismatch.block_kind = block_kind;
                            err.err_selectable.end_result_mismatch.expected_count = expected_count;
                            err.err_selectable.end_result_mismatch.actual_count = actual_count;

                            if(expected_count == 1uz)
                            {
                                err.err_selectable.end_result_mismatch.expected_type = static_cast<wasm_value_type_u>(*frame.result.begin);
                            }
                            else
                            {
                                err.err_selectable.end_result_mismatch.expected_type = {};
                            }

                            if(actual_count == 1uz && stack_size != 0uz)
                            {
                                err.err_selectable.end_result_mismatch.actual_type = static_cast<wasm_value_type_u>(operand_stack.back_unchecked().type);
                            }
                            else
                            {
                                err.err_selectable.end_result_mismatch.actual_type = {};
                            }

                            err.err_code = code_validation_error_code::end_result_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(expected_count != 0uz && actual_count >= expected_count)
                        {
                            for(::std::size_t i{}; i != expected_count; ++i)
                            {
                                auto const expected_type{frame.result.begin[expected_count - 1uz - i]};
                                auto const actual_type{operand_stack.index_unchecked(stack_size - 1uz - i).type};
                                if(actual_type != expected_type) [[unlikely]]
                                {
                                    err.err_curr = op_begin;
                                    err.err_selectable.end_result_mismatch.block_kind = block_kind;
                                    err.err_selectable.end_result_mismatch.expected_count = expected_count;
                                    err.err_selectable.end_result_mismatch.actual_count = actual_count;
                                    err.err_selectable.end_result_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                                    err.err_selectable.end_result_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
                                    err.err_code = code_validation_error_code::end_result_mismatch;
                                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                }
                            }
                        }

                        while(operand_stack.size() > base) { operand_stack.pop_back_unchecked(); }
                        for(::std::size_t i{}; i != expected_count; ++i) { operand_stack.push_back({frame.result.begin[i]}); }

                        if(frame.type == block_type::else_) { is_polymorphic = frame.polymorphic_base || (frame.then_polymorphic_end && is_polymorphic); }
                        else
                        {
                            is_polymorphic = frame.polymorphic_base;
                        }

                        control_flow_stack.pop_back_unchecked();

                        if(is_function_frame)
                        {
                            if(code_curr != code_end) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_code = code_validation_error_code::trailing_code_after_end;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            local_func_symbol.operand_stack_max = runtime_operand_stack_max;
                            storage.local_count = local_func_symbol.local_count;
                            storage.operand_stack_max = runtime_operand_stack_max;
                            storage.local_funcs.push_back(local_func_symbol);

                            return storage;
                        }

                        break;
                    }
                    case wasm1_code::br:
                    {

                        // br     label_index ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // br     label_index ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // br     label_index ...
                        // [safe] unsafe (could be the section_end)
                        //        ^^ code_curr

                        wasm_u32 label_index;
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [label_next, label_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                    ::fast_io::mnp::leb128_get(label_index))};
                        if(label_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_label_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(label_err);
                        }

                        // br     label_index ...
                        // [     safe       ] unsafe (could be the section_end)
                        //        ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(label_next);

                        // br     label_index ...
                        // [     safe       ] unsafe (could be the section_end)
                        //                    ^^ code_curr

                        auto const all_label_count_uz{control_flow_stack.size()};
                        auto const label_index_uz{static_cast<::std::size_t>(label_index)};
                        if(label_index_uz >= all_label_count_uz) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_label_index.label_index = label_index;
                            err.err_selectable.illegal_label_index.all_label_count = static_cast<wasm_u32>(all_label_count_uz);
                            err.err_code = code_validation_error_code::illegal_label_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const& target_frame{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - label_index_uz)};
                        auto const target_arity{static_cast<::std::size_t>(target_frame.result.end - target_frame.result.begin)};

                        if(!is_polymorphic && operand_stack.size() < target_arity) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"br";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                            err.err_selectable.operand_stack_underflow.stack_size_required = target_arity;
                            err.err_code = code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(!is_polymorphic && target_arity != 0uz && operand_stack.size() >= target_arity)
                        {
                            auto const expected_type{*target_frame.result.begin};
                            auto const actual_type{operand_stack.back_unchecked().type};
                            if(actual_type != expected_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"br";
                                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
                                err.err_code = code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        if(target_arity != 0uz)
                        {
                            auto n{target_arity};
                            while(!operand_stack.empty() && n-- != 0uz) { operand_stack.pop_back_unchecked(); }
                        }
                        auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
                        while(operand_stack.size() > curr_frame_base) { operand_stack.pop_back_unchecked(); }
                        is_polymorphic = true;

                        break;
                    }
                    case wasm1_code::br_if:
                    {
                        // br_if  label_index ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // br_if  label_index ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // br_if  label_index ...
                        // [safe] unsafe (could be the section_end)
                        //        ^^ code_curr

                        wasm_u32 label_index;
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [label_next, label_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                    ::fast_io::mnp::leb128_get(label_index))};
                        if(label_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_label_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(label_err);
                        }

                        // br_if  label_index ...
                        // [      safe      ] unsafe (could be the section_end)
                        //        ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(label_next);

                        // br_if  label_index ...
                        // [      safe      ] unsafe (could be the section_end)
                        //                    ^^ code_curr

                        auto const all_label_count_uz{control_flow_stack.size()};
                        auto const label_index_uz{static_cast<::std::size_t>(label_index)};
                        if(label_index_uz >= all_label_count_uz) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_label_index.label_index = label_index;
                            err.err_selectable.illegal_label_index.all_label_count = static_cast<wasm_u32>(all_label_count_uz);
                            err.err_code = code_validation_error_code::illegal_label_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const& target_frame{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - label_index_uz)};
                        auto const target_arity{static_cast<::std::size_t>(target_frame.result.end - target_frame.result.begin)};

                        if(!is_polymorphic && operand_stack.size() < target_arity + 1uz) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"br_if";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                            err.err_selectable.operand_stack_underflow.stack_size_required = target_arity + 1uz;
                            err.err_code = code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(!operand_stack.empty())
                        {
                            auto const cond{operand_stack.back_unchecked()};
                            operand_stack.pop_back_unchecked();
                            if(!is_polymorphic && cond.type != curr_operand_stack_value_type::i32) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_cond_type_not_i32.op_code_name = u8"br_if";
                                err.err_selectable.br_cond_type_not_i32.cond_type = static_cast<wasm_value_type_u>(cond.type);
                                err.err_code = code_validation_error_code::br_cond_type_not_i32;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        if(!is_polymorphic && target_arity != 0uz && operand_stack.size() >= target_arity)
                        {
                            auto const expected_type{*target_frame.result.begin};
                            auto const actual_type{operand_stack.back_unchecked().type};
                            if(actual_type != expected_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"br_if";
                                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
                                err.err_code = code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        break;
                    }
                    case wasm1_code::br_table:
                    {
                        // br_table  target_count ...
                        // [ safe ] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // br_table  target_count ...
                        // [ safe ] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // br_table  target_count ...
                        // [ safe ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                        wasm_u32 target_count;
                        auto const [cnt_next, cnt_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(target_count))};
                        if(cnt_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_label_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(cnt_err);
                        }

                        // br_table  target_count ...
                        // [       safe         ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(cnt_next);

                        // br_table  target_count ...
                        // [       safe         ] unsafe (could be the section_end)
                        //                       ^^ code_curr

                        auto const all_label_count_uz{control_flow_stack.size()};
                        auto const validate_label{[&](wasm_u32 li) constexpr UWVM_THROWS
                                                  {
                                                      if(static_cast<::std::size_t>(li) >= all_label_count_uz) [[unlikely]]
                                                      {
                                                          err.err_curr = op_begin;
                                                          err.err_selectable.illegal_label_index.label_index = li;
                                                          err.err_selectable.illegal_label_index.all_label_count = static_cast<wasm_u32>(all_label_count_uz);
                                                          err.err_code = code_validation_error_code::illegal_label_index;
                                                          ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                                      }
                                                  }};

                        struct get_sig_result_t
                        {
                            ::std::size_t arity{};
                            curr_operand_stack_value_type type{};
                        };

                        auto const get_sig{[&](wasm_u32 li) constexpr noexcept
                                           {
                                               auto const& frame{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - static_cast<::std::size_t>(li))};
                                               ::std::size_t const arity{static_cast<::std::size_t>(frame.result.end - frame.result.begin)};
                                               curr_operand_stack_value_type type{};
                                               if(arity != 0uz) { type = frame.result.begin[0]; }
                                               return get_sig_result_t{arity, type};
                                           }};

                        bool have_expected_sig{};
                        wasm_u32 expected_label{};
                        ::std::size_t expected_arity{};
                        curr_operand_stack_value_type expected_type{};

                        for(wasm_u32 i{}; i != target_count; ++i)
                        {
                            // ...    | curr_target ...
                            // [safe] | unsafe (could be the section_end)
                            //          ^^ code_curr

                            wasm_u32 li;
                            auto const [li_next, li_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(li))};
                            if(li_err != ::fast_io::parse_code::ok) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_code = code_validation_error_code::invalid_label_index;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(li_err);
                            }

                            // ...   | curr_target ...
                            // [safe | safe      ] unsafe (could be the section_end)
                            //         ^^ code_curr

                            code_curr = reinterpret_cast<::std::byte const*>(li_next);

                            // ...   | curr_target ...
                            // [safe | safe      ] unsafe (could be the section_end)
                            //                     ^^ code_curr

                            validate_label(li);

                            auto const [arity, type]{get_sig(li)};
                            if(!have_expected_sig)
                            {
                                have_expected_sig = true;
                                expected_label = li;
                                expected_arity = arity;
                                expected_type = type;
                            }
                            else if(arity != expected_arity || (expected_arity != 0uz && type != expected_type)) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_table_target_type_mismatch.expected_label_index = expected_label;
                                err.err_selectable.br_table_target_type_mismatch.mismatched_label_index = li;
                                err.err_selectable.br_table_target_type_mismatch.expected_arity = static_cast<wasm_u32>(expected_arity);
                                err.err_selectable.br_table_target_type_mismatch.actual_arity = static_cast<wasm_u32>(arity);
                                err.err_selectable.br_table_target_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                                err.err_selectable.br_table_target_type_mismatch.actual_type = static_cast<wasm_value_type_u>(type);
                                err.err_code = code_validation_error_code::br_table_target_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        // ... last_target | default_label ...
                        // [   safe      ]   unsafe (could be the section_end)
                        //                   ^^ code_curr

                        wasm_u32 default_label;
                        auto const [def_next, def_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(default_label))};
                        if(def_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_label_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(def_err);
                        }

                        // ... last_target | default_label ...
                        // [         safe  |      safe   ] unsafe (could be the section_end)
                        //                   ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(def_next);

                        // ... last_target | default_label ...
                        // [         safe  |      safe   ] unsafe (could be the section_end)
                        //                                 ^^ code_curr

                        validate_label(default_label);

                        auto const [default_arity, default_type]{get_sig(default_label)};
                        if(!have_expected_sig)
                        {
                            have_expected_sig = true;
                            expected_label = default_label;
                            expected_arity = default_arity;
                            expected_type = default_type;
                        }
                        else if(default_arity != expected_arity || (expected_arity != 0uz && default_type != expected_type)) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.br_table_target_type_mismatch.expected_label_index = expected_label;
                            err.err_selectable.br_table_target_type_mismatch.mismatched_label_index = default_label;
                            err.err_selectable.br_table_target_type_mismatch.expected_arity = static_cast<wasm_u32>(expected_arity);
                            err.err_selectable.br_table_target_type_mismatch.actual_arity = static_cast<wasm_u32>(default_arity);
                            err.err_selectable.br_table_target_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                            err.err_selectable.br_table_target_type_mismatch.actual_type = static_cast<wasm_value_type_u>(default_type);
                            err.err_code = code_validation_error_code::br_table_target_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(!is_polymorphic && operand_stack.size() < expected_arity + 1uz) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"br_table";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                            err.err_selectable.operand_stack_underflow.stack_size_required = expected_arity + 1uz;
                            err.err_code = code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(!operand_stack.empty())
                        {
                            auto const idx{operand_stack.back_unchecked()};
                            operand_stack.pop_back_unchecked();
                            if(!is_polymorphic && idx.type != curr_operand_stack_value_type::i32) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_cond_type_not_i32.op_code_name = u8"br_table";
                                err.err_selectable.br_cond_type_not_i32.cond_type = static_cast<wasm_value_type_u>(idx.type);
                                err.err_code = code_validation_error_code::br_cond_type_not_i32;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        if(!is_polymorphic && expected_arity != 0uz && operand_stack.size() >= expected_arity)
                        {
                            auto const actual_type{operand_stack.back_unchecked().type};
                            if(actual_type != expected_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_value_type_mismatch.op_code_name = u8"br_table";
                                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
                                err.err_code = code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        if(expected_arity != 0uz)
                        {
                            auto n{expected_arity};
                            while(!operand_stack.empty() && n-- != 0uz) { operand_stack.pop_back_unchecked(); }
                        }
                        auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
                        while(operand_stack.size() > curr_frame_base) { operand_stack.pop_back_unchecked(); }
                        is_polymorphic = true;

                        break;
                    }
                    case wasm1_code::return_:
                    {
                        // return ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // return ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // return ...
                        // [safe] unsafe (could be the section_end)
                        //        ^^ code_curr

                        auto const& func_frame{control_flow_stack.index_unchecked(0u)};
                        ::std::size_t const return_arity{static_cast<::std::size_t>(func_frame.result.end - func_frame.result.begin)};

                        if(!is_polymorphic && operand_stack.size() < return_arity) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"return";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                            err.err_selectable.operand_stack_underflow.stack_size_required = return_arity;
                            err.err_code = code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const operator_stack_size{operand_stack.size()};
                        if(!is_polymorphic && return_arity != 0uz && operator_stack_size >= return_arity)
                        {
                            for(::std::size_t i{}; i != return_arity; ++i)
                            {
                                auto const expected_type{func_frame.result.begin[return_arity - 1uz - i]};
                                auto const actual_type{operand_stack.index_unchecked(operator_stack_size - 1uz - i).type};
                                if(actual_type != expected_type) [[unlikely]]
                                {
                                    err.err_curr = op_begin;
                                    err.err_selectable.br_value_type_mismatch.op_code_name = u8"return";
                                    err.err_selectable.br_value_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                                    err.err_selectable.br_value_type_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
                                    err.err_code = code_validation_error_code::br_value_type_mismatch;
                                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                }
                            }
                        }

                        if(return_arity != 0uz)
                        {
                            auto n{return_arity};
                            while(!operand_stack.empty() && n-- != 0uz) { operand_stack.pop_back_unchecked(); }
                        }

                        auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
                        while(operand_stack.size() > curr_frame_base) { operand_stack.pop_back_unchecked(); }
                        is_polymorphic = true;

                        break;
                    }
                    case wasm1_code::call:
                    {
                        // call     func_index ...
                        // [ safe ] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // call     func_index ...
                        // [ safe ] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // call     func_index ...
                        // [ safe ] unsafe (could be the section_end)
                        //          ^^ code_curr

                        wasm_u32 func_index;
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [func_next, func_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(func_index))};
                        if(func_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_function_index_encoding;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(func_err);
                        }

                        // call func_index ...
                        // [      safe   ] unsafe (could be the section_end)
                        //      ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(func_next);

                        // call func_index ...
                        // [      safe   ] unsafe (could be the section_end)
                        //                ^^ code_curr

                        auto const all_function_size{import_func_count + local_func_count};
                        if(static_cast<::std::size_t>(func_index) >= all_function_size) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.invalid_function_index.function_index = static_cast<::std::size_t>(func_index);
                            err.err_selectable.invalid_function_index.all_function_size = all_function_size;
                            err.err_code = code_validation_error_code::invalid_function_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* callee_type_ptr{};
                        if(static_cast<::std::size_t>(func_index) < import_func_count)
                        {
                            auto const& imported_rec{curr_module.imported_function_vec_storage.index_unchecked(static_cast<::std::size_t>(func_index))};
                            auto const imported_func_ptr{imported_rec.import_type_ptr};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            if(imported_func_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                            callee_type_ptr = imported_func_ptr->imports.storage.function;
                        }
                        else
                        {
                            auto const local_idx{static_cast<::std::size_t>(func_index) - import_func_count};
                            callee_type_ptr = curr_module.local_defined_function_vec_storage.index_unchecked(local_idx).function_type_ptr;
                        }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(callee_type_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

                        auto const& callee_type{*callee_type_ptr};
                        auto const param_count{static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
                        auto const result_count{static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};

                        if(!is_polymorphic && operand_stack.size() < param_count) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"call";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                            err.err_selectable.operand_stack_underflow.stack_size_required = param_count;
                            err.err_code = code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const stack_size{operand_stack.size()};
                        if(!is_polymorphic && param_count != 0uz && stack_size >= param_count)
                        {
                            for(::std::size_t i{}; i != param_count; ++i)
                            {
                                auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
                                auto const actual_type{operand_stack.index_unchecked(stack_size - 1uz - i).type};
                                if(actual_type != expected_type) [[unlikely]]
                                {
                                    err.err_curr = op_begin;
                                    err.err_selectable.br_value_type_mismatch.op_code_name = u8"call";
                                    err.err_selectable.br_value_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                                    err.err_selectable.br_value_type_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
                                    err.err_code = code_validation_error_code::br_value_type_mismatch;
                                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                }
                            }
                        }

                        if(param_count != 0uz)
                        {
                            auto n{param_count};
                            while(!operand_stack.empty() && n-- != 0uz) { operand_stack.pop_back_unchecked(); }
                        }

                        if(result_count != 0uz)
                        {
                            for(::std::size_t i{}; i != result_count; ++i) { operand_stack.push_back({callee_type.result.begin[i]}); }
                        }

                        break;
                    }
                    case wasm1_code::call_indirect:
                    {
                        // call_indirect  type_index table_index ...
                        // [ safe      ] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // call_indirect  type_index table_index ...
                        // [ safe      ] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // call_indirect type_index table_index ...
                        // [    safe   ] unsafe (could be the section_end)
                        //               ^^ code_curr

                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                        wasm_u32 type_index;
                        auto const [type_next, type_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(type_index))};
                        if(type_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_type_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(type_err);
                        }

                        // call_indirect type_index table_index ...
                        // [          safe        ] unsafe (could be the section_end)
                        //               ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(type_next);

                        // call_indirect type_index table_index ...
                        // [          safe        ] unsafe (could be the section_end)
                        //                          ^^ code_curr

                        wasm_u32 table_index;
                        auto const [table_next, table_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                    ::fast_io::mnp::leb128_get(table_index))};
                        if(table_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_table_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(table_err);
                        }

                        // call_indirect type_index table_index ...
                        // [                safe              ] unsafe (could be the section_end)
                        //                          ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(table_next);

                        // call_indirect type_index table_index ...
                        // [                safe              ] unsafe (could be the section_end)
                        //                                      ^^ code_curr

                        if(table_index >= all_table_count) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_table_index.table_index = table_index;
                            err.err_selectable.illegal_table_index.all_table_count = all_table_count;
                            err.err_code = code_validation_error_code::illegal_table_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto types_begin{curr_module.type_section_storage.type_section_begin};
                        auto types_end{curr_module.type_section_storage.type_section_end};

                        auto const all_type_count_uz{static_cast<::std::size_t>(types_end - types_begin)};
                        if(static_cast<::std::size_t>(type_index) >= all_type_count_uz) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_type_index.type_index = type_index;
                            err.err_selectable.illegal_type_index.all_type_count = static_cast<wasm_u32>(all_type_count_uz);
                            err.err_code = code_validation_error_code::illegal_type_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const& callee_type{types_begin[static_cast<::std::size_t>(type_index)]};
                        auto const param_count{static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
                        auto const result_count{static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};

                        if(!is_polymorphic && operand_stack.size() < param_count + 1uz) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"call_indirect";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                            err.err_selectable.operand_stack_underflow.stack_size_required = param_count + 1uz;
                            err.err_code = code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(!operand_stack.empty())
                        {
                            auto const idx{operand_stack.back_unchecked()};
                            operand_stack.pop_back_unchecked();
                            if(!is_polymorphic && idx.type != curr_operand_stack_value_type::i32) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.br_cond_type_not_i32.op_code_name = u8"call_indirect";
                                err.err_selectable.br_cond_type_not_i32.cond_type = static_cast<wasm_value_type_u>(idx.type);
                                err.err_code = code_validation_error_code::br_cond_type_not_i32;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        auto const stack_size{operand_stack.size()};
                        if(!is_polymorphic && param_count != 0uz && stack_size >= param_count)
                        {
                            for(::std::size_t i{}; i != param_count; ++i)
                            {
                                auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
                                auto const actual_type{operand_stack.index_unchecked(stack_size - 1uz - i).type};
                                if(actual_type != expected_type) [[unlikely]]
                                {
                                    err.err_curr = op_begin;
                                    err.err_selectable.br_value_type_mismatch.op_code_name = u8"call_indirect";
                                    err.err_selectable.br_value_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                                    err.err_selectable.br_value_type_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
                                    err.err_code = code_validation_error_code::br_value_type_mismatch;
                                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                }
                            }
                        }

                        if(param_count != 0uz)
                        {
                            auto n{param_count};
                            while(!operand_stack.empty() && n-- != 0uz) { operand_stack.pop_back_unchecked(); }
                        }

                        if(result_count != 0uz)
                        {
                            for(::std::size_t i{}; i != result_count; ++i) { operand_stack.push_back({callee_type.result.begin[i]}); }
                        }

                        break;
                    }
                    case wasm1_code::drop:
                    {
                        // drop   ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // drop   ...
                        // [safe] unsafe (could be the section_end)
                        //        ^^ op_begin

                        ++code_curr;

                        // drop   ...
                        // [safe] unsafe (could be the section_end)
                        //        ^^ code_curr

                        if(operand_stack.empty()) [[unlikely]]
                        {
                            if(!is_polymorphic)
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.operand_stack_underflow.op_code_name = u8"drop";
                                err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                                err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                                err.err_code = code_validation_error_code::operand_stack_underflow;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                        else
                        {
                            operand_stack.pop_back_unchecked();
                        }

                        break;
                    }
                    case wasm1_code::select:
                    {
                        // select ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // select ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // select ...
                        // [safe] unsafe (could be the section_end)
                        //        ^^ code_curr

                        if(!is_polymorphic && operand_stack.size() < 3uz) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"select";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                            err.err_selectable.operand_stack_underflow.stack_size_required = 3uz;
                            err.err_code = code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        bool cond_from_stack{};
                        curr_operand_stack_value_type cond_type{};
                        if(!operand_stack.empty())
                        {
                            auto const cond{operand_stack.back_unchecked()};
                            operand_stack.pop_back_unchecked();
                            cond_from_stack = true;
                            cond_type = cond.type;
                        }

                        if(cond_from_stack && cond_type != curr_operand_stack_value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.select_cond_type_not_i32.cond_type = cond_type;
                            err.err_code = code_validation_error_code::select_cond_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        bool v2_from_stack{};
                        curr_operand_stack_value_type v2_type{};
                        if(!operand_stack.empty())
                        {
                            auto const v2{operand_stack.back_unchecked()};
                            operand_stack.pop_back_unchecked();
                            v2_from_stack = true;
                            v2_type = v2.type;
                        }

                        bool v1_from_stack{};
                        curr_operand_stack_value_type v1_type{};
                        if(!operand_stack.empty())
                        {
                            auto const v1{operand_stack.back_unchecked()};
                            v1_from_stack = true;
                            v1_type = v1.type;
                        }

                        if(v1_from_stack && v2_from_stack && v1_type != v2_type) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.select_type_mismatch.type_v1 = v1_type;
                            err.err_selectable.select_type_mismatch.type_v2 = v2_type;
                            err.err_code = code_validation_error_code::select_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(!v1_from_stack && v2_from_stack) { operand_stack.push_back({v2_type}); }

                        break;
                    }
                    case wasm1_code::local_get:
                    {
                        // local.get ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // local.get ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // local.get local_index ...
                        // [ safe  ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        wasm_u32 local_index;
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                ::fast_io::mnp::leb128_get(local_index))};
                        if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
                        }

                        // local.get local_index ...
                        // [     safe          ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

                        // local.get local_index ...
                        // [     safe          ] unsafe (could be the section_end)
                        //                       ^^ code_curr

                        if(local_index >= all_local_count) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_local_index.local_index = local_index;
                            err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                            err.err_code = code_validation_error_code::illegal_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        curr_operand_stack_value_type curr_local_type{};
                        if(local_index < func_parameter_count_u32) { curr_local_type = func_parameter_begin[local_index]; }
                        else
                        {
                            auto tem_local_index{local_index - func_parameter_count_u32};

                            bool found_local{};
                            for(auto const& local_part: curr_code_locals)
                            {
                                if(tem_local_index < local_part.count)
                                {
                                    curr_local_type = local_part.type;
                                    found_local = true;
                                    break;
                                }

                                tem_local_index -= local_part.count;
                            }

                            if(!found_local) [[unlikely]]
                            {
                                // Inconsistency between `all_local_count` and the locals vector; treat as invalid code.
                                err.err_curr = op_begin;
                                err.err_selectable.illegal_local_index.local_index = local_index;
                                err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                                err.err_code = code_validation_error_code::illegal_local_index;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        operand_stack.push_back({curr_local_type});
                        break;
                    }
                    case wasm1_code::local_set:
                    {
                        // local.set ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // local.set ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // local.set local_index ...
                        // [ safe  ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        wasm_u32 local_index;
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                ::fast_io::mnp::leb128_get(local_index))};
                        if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
                        }

                        // local.set local_index ...
                        // [     safe          ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

                        // local.set local_index ...
                        // [     safe          ] unsafe (could be the section_end)
                        //                       ^^ code_curr

                        if(local_index >= all_local_count) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_local_index.local_index = local_index;
                            err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                            err.err_code = code_validation_error_code::illegal_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        curr_operand_stack_value_type curr_local_type{};
                        if(local_index < func_parameter_count_u32) { curr_local_type = func_parameter_begin[local_index]; }
                        else
                        {
                            auto tem_local_index{local_index - func_parameter_count_u32};

                            bool found_local{};
                            for(auto const& local_part: curr_code_locals)
                            {
                                if(tem_local_index < local_part.count)
                                {
                                    curr_local_type = local_part.type;
                                    found_local = true;
                                    break;
                                }

                                tem_local_index -= local_part.count;
                            }

                            if(!found_local) [[unlikely]]
                            {
                                // Inconsistency between `all_local_count` and the locals vector; treat as invalid code.
                                err.err_curr = op_begin;
                                err.err_selectable.illegal_local_index.local_index = local_index;
                                err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                                err.err_code = code_validation_error_code::illegal_local_index;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        if(operand_stack.empty()) [[unlikely]]
                        {
                            if(!is_polymorphic)
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.operand_stack_underflow.op_code_name = u8"local.set";
                                err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                                err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                                err.err_code = code_validation_error_code::operand_stack_underflow;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                        else
                        {
                            auto const value{operand_stack.back_unchecked()};
                            if(value.type != curr_local_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.local_variable_type_mismatch.local_index = local_index;
                                err.err_selectable.local_variable_type_mismatch.expected_type = curr_local_type;
                                err.err_selectable.local_variable_type_mismatch.actual_type = value.type;
                                err.err_code = code_validation_error_code::local_set_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            operand_stack.pop_back_unchecked();
                        }

                        break;
                    }
                    case wasm1_code::local_tee:
                    {
                        // local.tee ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // local.tee ...
                        // [safe] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // local.tee local_index ...
                        // [ safe  ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        wasm_u32 local_index;
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                ::fast_io::mnp::leb128_get(local_index))};
                        if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
                        }

                        // local.tee local_index ...
                        // [     safe          ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

                        // local.tee local_index ...
                        // [     safe          ] unsafe (could be the section_end)
                        //                       ^^ code_curr

                        if(local_index >= all_local_count) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_local_index.local_index = local_index;
                            err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                            err.err_code = code_validation_error_code::illegal_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        curr_operand_stack_value_type curr_local_type{};
                        if(local_index < func_parameter_count_u32) { curr_local_type = func_parameter_begin[local_index]; }
                        else
                        {
                            auto tem_local_index{local_index - func_parameter_count_u32};

                            bool found_local{};
                            for(auto const& local_part: curr_code_locals)
                            {
                                if(tem_local_index < local_part.count)
                                {
                                    curr_local_type = local_part.type;
                                    found_local = true;
                                    break;
                                }

                                tem_local_index -= local_part.count;
                            }

                            if(!found_local) [[unlikely]]
                            {
                                // Inconsistency between `all_local_count` and the locals vector; treat as invalid code.
                                err.err_curr = op_begin;
                                err.err_selectable.illegal_local_index.local_index = local_index;
                                err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                                err.err_code = code_validation_error_code::illegal_local_index;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        if(operand_stack.empty()) [[unlikely]]
                        {
                            if(!is_polymorphic)
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.operand_stack_underflow.op_code_name = u8"local.tee";
                                err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                                err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                                err.err_code = code_validation_error_code::operand_stack_underflow;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                            else
                            {
                                operand_stack.push_back({curr_local_type});
                            }
                        }
                        else
                        {
                            auto const value{operand_stack.back_unchecked()};
                            if(value.type != curr_local_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.local_variable_type_mismatch.local_index = local_index;
                                err.err_selectable.local_variable_type_mismatch.expected_type = curr_local_type;
                                err.err_selectable.local_variable_type_mismatch.actual_type = value.type;
                                err.err_code = code_validation_error_code::local_tee_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        break;
                    }
                    case wasm1_code::global_get:
                    {
                        // global.get ...
                        // [  safe  ] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // global.get ...
                        // [ safe   ] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // global.get global_index ...
                        // [ safe   ] unsafe (could be the section_end)
                        //            ^^ code_curr

                        wasm_u32 global_index;
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [global_index_next, global_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                  ::fast_io::mnp::leb128_get(global_index))};
                        if(global_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_global_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(global_index_err);
                        }

                        // global.get global_index ...
                        // [     safe            ] unsafe (could be the section_end)
                        //            ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(global_index_next);

                        // global.get global_index ...
                        // [      safe           ] unsafe (could be the section_end)
                        //                         ^^ code_curr

                        if(global_index >= all_global_count) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_global_index.global_index = global_index;
                            err.err_selectable.illegal_global_index.all_global_count = all_global_count;
                            err.err_code = code_validation_error_code::illegal_global_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        curr_operand_stack_value_type curr_global_type{};
                        if(global_index < imported_global_count)
                        {
                            auto const& imported_global_rec{curr_module.imported_global_vec_storage.index_unchecked(static_cast<::std::size_t>(global_index))};
                            auto const imported_global_ptr{imported_global_rec.import_type_ptr};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            if(imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                            curr_global_type = imported_global_ptr->imports.storage.global.type;
                        }
                        else
                        {
                            auto const local_global_index{static_cast<::std::size_t>(global_index - imported_global_count)};
                            auto const& local_global_rec{curr_module.local_defined_global_vec_storage.index_unchecked(local_global_index)};
                            auto const local_global_ptr{local_global_rec.global_type_ptr};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            if(local_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                            curr_global_type = local_global_ptr->type;
                        }

                        operand_stack.push_back({curr_global_type});
                        break;
                    }
                    case wasm1_code::global_set:
                    {
                        // global.set ...
                        // [  safe  ] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // global.set ...
                        // [ safe   ] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // global.set global_index ...
                        // [ safe   ] unsafe (could be the section_end)
                        //            ^^ code_curr

                        wasm_u32 global_index;
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [global_index_next, global_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                  ::fast_io::mnp::leb128_get(global_index))};
                        if(global_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_global_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(global_index_err);
                        }

                        // global.set global_index ...
                        // [     safe            ] unsafe (could be the section_end)
                        //            ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(global_index_next);

                        // global.set global_index ...
                        // [      safe           ] unsafe (could be the section_end)
                        //                         ^^ code_curr

                        if(global_index >= all_global_count) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_global_index.global_index = global_index;
                            err.err_selectable.illegal_global_index.all_global_count = all_global_count;
                            err.err_code = code_validation_error_code::illegal_global_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        curr_operand_stack_value_type curr_global_type{};
                        bool curr_global_mutable{};
                        if(global_index < imported_global_count)
                        {
                            auto const& imported_global_rec{curr_module.imported_global_vec_storage.index_unchecked(static_cast<::std::size_t>(global_index))};
                            auto const imported_global_ptr{imported_global_rec.import_type_ptr};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            if(imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                            auto const& imported_global{imported_global_ptr->imports.storage.global};
                            curr_global_type = imported_global.type;
                            curr_global_mutable = imported_global.is_mutable;
                        }
                        else
                        {
                            auto const local_global_index{static_cast<::std::size_t>(global_index - imported_global_count)};
                            auto const& local_global_rec{curr_module.local_defined_global_vec_storage.index_unchecked(local_global_index)};
                            auto const local_global_ptr{local_global_rec.global_type_ptr};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            if(local_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                            curr_global_type = local_global_ptr->type;
                            curr_global_mutable = local_global_ptr->is_mutable;
                        }

                        if(!curr_global_mutable) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.immutable_global_set.global_index = global_index;
                            err.err_code = code_validation_error_code::immutable_global_set;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(operand_stack.empty()) [[unlikely]]
                        {
                            if(!is_polymorphic)
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.operand_stack_underflow.op_code_name = u8"global.set";
                                err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                                err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                                err.err_code = code_validation_error_code::operand_stack_underflow;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                        else
                        {
                            auto const value{operand_stack.back_unchecked()};
                            operand_stack.pop_back_unchecked();
                            if(value.type != curr_global_type) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.global_variable_type_mismatch.global_index = global_index;
                                err.err_selectable.global_variable_type_mismatch.expected_type = curr_global_type;
                                err.err_selectable.global_variable_type_mismatch.actual_type = value.type;
                                err.err_code = code_validation_error_code::global_set_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }

                        break;
                    }
                    case wasm1_code::i32_load:
                    {
                        validate_mem_load(u8"i32.load", 2u, wasm_value_type_u::i32);
                        break;
                    }
                    case wasm1_code::i64_load:
                    {
                        validate_mem_load(u8"i64.load", 3u, wasm_value_type_u::i64);
                        break;
                    }
                    case wasm1_code::f32_load:
                    {
                        validate_mem_load(u8"f32.load", 2u, wasm_value_type_u::f32);
                        break;
                    }
                    case wasm1_code::f64_load:
                    {
                        validate_mem_load(u8"f64.load", 3u, wasm_value_type_u::f64);
                        break;
                    }
                    case wasm1_code::i32_load8_s:
                    {
                        validate_mem_load(u8"i32.load8_s", 0u, wasm_value_type_u::i32);
                        break;
                    }
                    case wasm1_code::i32_load8_u:
                    {
                        validate_mem_load(u8"i32.load8_u", 0u, wasm_value_type_u::i32);
                        break;
                    }
                    case wasm1_code::i32_load16_s:
                    {
                        validate_mem_load(u8"i32.load16_s", 1u, wasm_value_type_u::i32);
                        break;
                    }
                    case wasm1_code::i32_load16_u:
                    {
                        validate_mem_load(u8"i32.load16_u", 1u, wasm_value_type_u::i32);
                        break;
                    }
                    case wasm1_code::i64_load8_s:
                    {
                        validate_mem_load(u8"i64.load8_s", 0u, wasm_value_type_u::i64);
                        break;
                    }
                    case wasm1_code::i64_load8_u:
                    {
                        validate_mem_load(u8"i64.load8_u", 0u, wasm_value_type_u::i64);
                        break;
                    }
                    case wasm1_code::i64_load16_s:
                    {
                        validate_mem_load(u8"i64.load16_s", 1u, wasm_value_type_u::i64);
                        break;
                    }
                    case wasm1_code::i64_load16_u:
                    {
                        validate_mem_load(u8"i64.load16_u", 1u, wasm_value_type_u::i64);
                        break;
                    }
                    case wasm1_code::i64_load32_s:
                    {
                        validate_mem_load(u8"i64.load32_s", 2u, wasm_value_type_u::i64);
                        break;
                    }
                    case wasm1_code::i64_load32_u:
                    {
                        validate_mem_load(u8"i64.load32_u", 2u, wasm_value_type_u::i64);
                        break;
                    }
                    case wasm1_code::i32_store:
                    {
                        validate_mem_store(u8"i32.store", 2u, wasm_value_type_u::i32);
                        break;
                    }
                    case wasm1_code::i64_store:
                    {
                        validate_mem_store(u8"i64.store", 3u, wasm_value_type_u::i64);
                        break;
                    }
                    case wasm1_code::f32_store:
                    {
                        validate_mem_store(u8"f32.store", 2u, wasm_value_type_u::f32);
                        break;
                    }
                    case wasm1_code::f64_store:
                    {
                        validate_mem_store(u8"f64.store", 3u, wasm_value_type_u::f64);
                        break;
                    }
                    case wasm1_code::i32_store8:
                    {
                        validate_mem_store(u8"i32.store8", 0u, wasm_value_type_u::i32);
                        break;
                    }
                    case wasm1_code::i32_store16:
                    {
                        validate_mem_store(u8"i32.store16", 1u, wasm_value_type_u::i32);
                        break;
                    }
                    case wasm1_code::i64_store8:
                    {
                        validate_mem_store(u8"i64.store8", 0u, wasm_value_type_u::i64);
                        break;
                    }
                    case wasm1_code::i64_store16:
                    {
                        validate_mem_store(u8"i64.store16", 1u, wasm_value_type_u::i64);
                        break;
                    }
                    case wasm1_code::i64_store32:
                    {
                        validate_mem_store(u8"i64.store32", 2u, wasm_value_type_u::i64);
                        break;
                    }
                    case wasm1_code::memory_size:
                    {
                        // memory.size memidx ...
                        // [ safe    ] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // memory.size memidx ...
                        // [ safe    ] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // memory.size memidx ...
                        // [ safe    ] unsafe (could be the section_end)
                        //             ^^ code_curr

                        wasm_u32 memidx;
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [mem_next, mem_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(memidx))};
                        if(mem_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_memory_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(mem_err);
                        }

                        // memory.size memidx ...
                        // [ safe           ] unsafe (could be the section_end)
                        //             ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(mem_next);

                        // memory.size memidx ...
                        // [ safe           ] unsafe (could be the section_end)
                        //                    ^^ code_curr

                        if(memidx != 0u) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_memory_index.memory_index = memidx;
                            err.err_selectable.illegal_memory_index.all_memory_count = all_memory_count;
                            err.err_code = code_validation_error_code::illegal_memory_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(all_memory_count == 0u) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.no_memory.op_code_name = u8"memory.size";
                            err.err_selectable.no_memory.align = 0u;
                            err.err_selectable.no_memory.offset = 0u;
                            err.err_code = code_validation_error_code::no_memory;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        operand_stack.push_back({wasm_value_type_u::i32});
                        break;
                    }
                    case wasm1_code::memory_grow:
                    {
                        // memory.grow memidx ...
                        // [ safe    ] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // memory.grow memidx ...
                        // [ safe    ] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // memory.grow memidx ...
                        // [ safe    ] unsafe (could be the section_end)
                        //             ^^ code_curr

                        wasm_u32 memidx;
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [mem_next, mem_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(memidx))};
                        if(mem_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = code_validation_error_code::invalid_memory_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(mem_err);
                        }

                        // memory.grow memidx ...
                        // [        safe    ] unsafe (could be the section_end)
                        //             ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(mem_next);

                        // memory.grow memidx ...
                        // [        safe    ] unsafe (could be the section_end)
                        //                    ^^ code_curr

                        if(memidx != 0u) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_memory_index.memory_index = memidx;
                            err.err_selectable.illegal_memory_index.all_memory_count = all_memory_count;
                            err.err_code = code_validation_error_code::illegal_memory_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(all_memory_count == 0u) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.no_memory.op_code_name = u8"memory.grow";
                            err.err_selectable.no_memory.align = 0u;
                            err.err_selectable.no_memory.offset = 0u;
                            err.err_code = code_validation_error_code::no_memory;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(!is_polymorphic)
                        {
                            if(operand_stack.empty()) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.operand_stack_underflow.op_code_name = u8"memory.grow";
                                err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                                err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                                err.err_code = code_validation_error_code::operand_stack_underflow;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }

                            auto const delta{operand_stack.back_unchecked()};
                            operand_stack.pop_back_unchecked();

                            if(delta.type != wasm_value_type_u::i32) [[unlikely]]
                            {
                                err.err_curr = op_begin;
                                err.err_selectable.memory_grow_delta_type_not_i32.delta_type = delta.type;
                                err.err_code = code_validation_error_code::memory_grow_delta_type_not_i32;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                        else
                        {
                            if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                        }

                        operand_stack.push_back({wasm_value_type_u::i32});
                        break;
                    }
                    case wasm1_code::i32_const:
                    {
                        // i32.const i32 ...
                        // [ safe  ] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // i32.const i32 ...
                        // [ safe  ] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // i32.const i32 ...
                        // [ safe  ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        wasm_i32 imm;
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(imm))};
                        if(imm_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.invalid_const_immediate.op_code_name = u8"i32.const";
                            err.err_code = code_validation_error_code::invalid_const_immediate;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(imm_err);
                        }

                        // i32.const i32 ...
                        // [    safe   ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(imm_next);

                        // i32.const i32 ...
                        // [    safe   ] unsafe (could be the section_end)
                        //               ^^ code_curr

                        operand_stack.push_back({wasm_value_type_u::i32});
                        break;
                    }
                    case wasm1_code::i64_const:
                    {
                        // i64.const i64 ...
                        // [ safe  ] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // i64.const i64 ...
                        // [ safe  ] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // i64.const i64 ...
                        // [ safe  ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        wasm_i64 imm;
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(imm))};
                        if(imm_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.invalid_const_immediate.op_code_name = u8"i64.const";
                            err.err_code = code_validation_error_code::invalid_const_immediate;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(imm_err);
                        }

                        // i64.const i64 ...
                        // [     safe  ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        code_curr = reinterpret_cast<::std::byte const*>(imm_next);

                        // i64.const i64 ...
                        // [     safe  ] unsafe (could be the section_end)
                        //               ^^ code_curr

                        operand_stack.push_back({wasm_value_type_u::i64});
                        break;
                    }
                    case wasm1_code::f32_const:
                    {
                        // f32.const f32 ...
                        // [ safe  ] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // f32.const f32 ...
                        // [ safe  ] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // f32.const f32 ...
                        // [ safe  ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        if(static_cast<::std::size_t>(code_end - code_curr) < sizeof(wasm_f32)) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.invalid_const_immediate.op_code_name = u8"f32.const";
                            err.err_code = code_validation_error_code::invalid_const_immediate;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
                        }

                        // f32.const f32 ...
                        // [ safe      ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        wasm_f32 imm;
                        ::std::memcpy(::std::addressof(imm), code_curr, sizeof(imm));

                        code_curr += sizeof(imm);

                        // f32.const f32 ...
                        // [ safe      ] unsafe (could be the section_end)
                        //               ^^ code_curr

                        operand_stack.push_back({wasm_value_type_u::f32});
                        break;
                    }
                    case wasm1_code::f64_const:
                    {
                        // f64.const f64 ...
                        // [ safe  ] unsafe (could be the section_end)
                        // ^^ code_curr

                        auto const op_begin{code_curr};

                        // f64.const f64 ...
                        // [ safe  ] unsafe (could be the section_end)
                        // ^^ op_begin

                        ++code_curr;

                        // f64.const f64 ...
                        // [ safe  ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        if(static_cast<::std::size_t>(code_end - code_curr) < sizeof(wasm_f64)) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.invalid_const_immediate.op_code_name = u8"f64.const";
                            err.err_code = code_validation_error_code::invalid_const_immediate;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
                        }

                        // f64.const f64 ...
                        // [     safe  ] unsafe (could be the section_end)
                        //           ^^ code_curr

                        wasm_f64 imm;
                        ::std::memcpy(::std::addressof(imm), code_curr, sizeof(imm));

                        code_curr += sizeof(imm);

                        // f64.const f64 ...
                        // [     safe  ] unsafe (could be the section_end)
                        //               ^^ code_curr

                        operand_stack.push_back({wasm_value_type_u::f64});
                        break;
                    }
                    case wasm1_code::i32_eqz:
                    {
                        validate_numeric_unary(u8"i32.eqz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_eq:
                    {
                        validate_numeric_binary(u8"i32.eq", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_ne:
                    {
                        validate_numeric_binary(u8"i32.ne", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_lt_s:
                    {
                        validate_numeric_binary(u8"i32.lt_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_lt_u:
                    {
                        validate_numeric_binary(u8"i32.lt_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_gt_s:
                    {
                        validate_numeric_binary(u8"i32.gt_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_gt_u:
                    {
                        validate_numeric_binary(u8"i32.gt_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_le_s:
                    {
                        validate_numeric_binary(u8"i32.le_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_le_u:
                    {
                        validate_numeric_binary(u8"i32.le_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_ge_s:
                    {
                        validate_numeric_binary(u8"i32.ge_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_ge_u:
                    {
                        validate_numeric_binary(u8"i32.ge_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_eqz:
                    {
                        validate_numeric_unary(u8"i64.eqz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_eq:
                    {
                        validate_numeric_binary(u8"i64.eq", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_ne:
                    {
                        validate_numeric_binary(u8"i64.ne", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_lt_s:
                    {
                        validate_numeric_binary(u8"i64.lt_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_lt_u:
                    {
                        validate_numeric_binary(u8"i64.lt_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_gt_s:
                    {
                        validate_numeric_binary(u8"i64.gt_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_gt_u:
                    {
                        validate_numeric_binary(u8"i64.gt_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_le_s:
                    {
                        validate_numeric_binary(u8"i64.le_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_le_u:
                    {
                        validate_numeric_binary(u8"i64.le_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_ge_s:
                    {
                        validate_numeric_binary(u8"i64.ge_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_ge_u:
                    {
                        validate_numeric_binary(u8"i64.ge_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::f32_eq:
                    {
                        validate_numeric_binary(u8"f32.eq", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::f32_ne:
                    {
                        validate_numeric_binary(u8"f32.ne", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::f32_lt:
                    {
                        validate_numeric_binary(u8"f32.lt", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::f32_gt:
                    {
                        validate_numeric_binary(u8"f32.gt", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::f32_le:
                    {
                        validate_numeric_binary(u8"f32.le", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::f32_ge:
                    {
                        validate_numeric_binary(u8"f32.ge", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::f64_eq:
                    {
                        validate_numeric_binary(u8"f64.eq", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::f64_ne:
                    {
                        validate_numeric_binary(u8"f64.ne", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::f64_lt:
                    {
                        validate_numeric_binary(u8"f64.lt", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::f64_gt:
                    {
                        validate_numeric_binary(u8"f64.gt", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::f64_le:
                    {
                        validate_numeric_binary(u8"f64.le", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::f64_ge:
                    {
                        validate_numeric_binary(u8"f64.ge", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_clz:
                    {
                        validate_numeric_unary(u8"i32.clz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_ctz:
                    {
                        validate_numeric_unary(u8"i32.ctz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_popcnt:
                    {
                        validate_numeric_unary(u8"i32.popcnt", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_add:
                    {
                        validate_numeric_binary(u8"i32.add", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_sub:
                    {
                        validate_numeric_binary(u8"i32.sub", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_mul:
                    {
                        validate_numeric_binary(u8"i32.mul", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_div_s:
                    {
                        validate_numeric_binary(u8"i32.div_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_div_u:
                    {
                        validate_numeric_binary(u8"i32.div_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_rem_s:
                    {
                        validate_numeric_binary(u8"i32.rem_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_rem_u:
                    {
                        validate_numeric_binary(u8"i32.rem_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_and:
                    {
                        validate_numeric_binary(u8"i32.and", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_or:
                    {
                        validate_numeric_binary(u8"i32.or", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_xor:
                    {
                        validate_numeric_binary(u8"i32.xor", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_shl:
                    {
                        validate_numeric_binary(u8"i32.shl", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_shr_s:
                    {
                        validate_numeric_binary(u8"i32.shr_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_shr_u:
                    {
                        validate_numeric_binary(u8"i32.shr_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_rotl:
                    {
                        validate_numeric_binary(u8"i32.rotl", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_rotr:
                    {
                        validate_numeric_binary(u8"i32.rotr", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_clz:
                    {
                        validate_numeric_unary(u8"i64.clz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_ctz:
                    {
                        validate_numeric_unary(u8"i64.ctz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_popcnt:
                    {
                        validate_numeric_unary(u8"i64.popcnt", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_add:
                    {
                        validate_numeric_binary(u8"i64.add", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_sub:
                    {
                        validate_numeric_binary(u8"i64.sub", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_mul:
                    {
                        validate_numeric_binary(u8"i64.mul", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_div_s:
                    {
                        validate_numeric_binary(u8"i64.div_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_div_u:
                    {
                        validate_numeric_binary(u8"i64.div_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_rem_s:
                    {
                        validate_numeric_binary(u8"i64.rem_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_rem_u:
                    {
                        validate_numeric_binary(u8"i64.rem_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_and:
                    {
                        validate_numeric_binary(u8"i64.and", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_or:
                    {
                        validate_numeric_binary(u8"i64.or", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_xor:
                    {
                        validate_numeric_binary(u8"i64.xor", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_shl:
                    {
                        validate_numeric_binary(u8"i64.shl", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_shr_s:
                    {
                        validate_numeric_binary(u8"i64.shr_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_shr_u:
                    {
                        validate_numeric_binary(u8"i64.shr_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_rotl:
                    {
                        validate_numeric_binary(u8"i64.rotl", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_rotr:
                    {
                        validate_numeric_binary(u8"i64.rotr", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::f32_abs:
                    {
                        validate_numeric_unary(u8"f32.abs", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_neg:
                    {
                        validate_numeric_unary(u8"f32.neg", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_ceil:
                    {
                        validate_numeric_unary(u8"f32.ceil", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_floor:
                    {
                        validate_numeric_unary(u8"f32.floor", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_trunc:
                    {
                        validate_numeric_unary(u8"f32.trunc", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_nearest:
                    {
                        validate_numeric_unary(u8"f32.nearest", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_sqrt:
                    {
                        validate_numeric_unary(u8"f32.sqrt", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_add:
                    {
                        validate_numeric_binary(u8"f32.add", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_sub:
                    {
                        validate_numeric_binary(u8"f32.sub", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_mul:
                    {
                        validate_numeric_binary(u8"f32.mul", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_div:
                    {
                        validate_numeric_binary(u8"f32.div", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_min:
                    {
                        validate_numeric_binary(u8"f32.min", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_max:
                    {
                        validate_numeric_binary(u8"f32.max", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_copysign:
                    {
                        validate_numeric_binary(u8"f32.copysign", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f64_abs:
                    {
                        validate_numeric_unary(u8"f64.abs", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_neg:
                    {
                        validate_numeric_unary(u8"f64.neg", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_ceil:
                    {
                        validate_numeric_unary(u8"f64.ceil", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_floor:
                    {
                        validate_numeric_unary(u8"f64.floor", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_trunc:
                    {
                        validate_numeric_unary(u8"f64.trunc", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_nearest:
                    {
                        validate_numeric_unary(u8"f64.nearest", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_sqrt:
                    {
                        validate_numeric_unary(u8"f64.sqrt", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_add:
                    {
                        validate_numeric_binary(u8"f64.add", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_sub:
                    {
                        validate_numeric_binary(u8"f64.sub", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_mul:
                    {
                        validate_numeric_binary(u8"f64.mul", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_div:
                    {
                        validate_numeric_binary(u8"f64.div", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_min:
                    {
                        validate_numeric_binary(u8"f64.min", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_max:
                    {
                        validate_numeric_binary(u8"f64.max", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_copysign:
                    {
                        validate_numeric_binary(u8"f64.copysign", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::i32_wrap_i64:
                    {
                        validate_numeric_unary(u8"i32.wrap_i64", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_trunc_f32_s:
                    {
                        validate_numeric_unary(u8"i32.trunc_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_trunc_f32_u:
                    {
                        validate_numeric_unary(u8"i32.trunc_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_trunc_f64_s:
                    {
                        validate_numeric_unary(u8"i32.trunc_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i32_trunc_f64_u:
                    {
                        validate_numeric_unary(u8"i32.trunc_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_extend_i32_s:
                    {
                        validate_numeric_unary(u8"i64.extend_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_extend_i32_u:
                    {
                        validate_numeric_unary(u8"i64.extend_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_trunc_f32_s:
                    {
                        validate_numeric_unary(u8"i64.trunc_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_trunc_f32_u:
                    {
                        validate_numeric_unary(u8"i64.trunc_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_trunc_f64_s:
                    {
                        validate_numeric_unary(u8"i64.trunc_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::i64_trunc_f64_u:
                    {
                        validate_numeric_unary(u8"i64.trunc_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::f32_convert_i32_s:
                    {
                        validate_numeric_unary(u8"f32.convert_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_convert_i32_u:
                    {
                        validate_numeric_unary(u8"f32.convert_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_convert_i64_s:
                    {
                        validate_numeric_unary(u8"f32.convert_i64_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_convert_i64_u:
                    {
                        validate_numeric_unary(u8"f32.convert_i64_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f32_demote_f64:
                    {
                        validate_numeric_unary(u8"f32.demote_f64", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f64_convert_i32_s:
                    {
                        validate_numeric_unary(u8"f64.convert_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_convert_i32_u:
                    {
                        validate_numeric_unary(u8"f64.convert_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_convert_i64_s:
                    {
                        validate_numeric_unary(u8"f64.convert_i64_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_convert_i64_u:
                    {
                        validate_numeric_unary(u8"f64.convert_i64_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::f64_promote_f32:
                    {
                        validate_numeric_unary(u8"f64.promote_f32", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f64);
                        break;
                    }
                    case wasm1_code::i32_reinterpret_f32:
                    {
                        validate_numeric_unary(u8"i32.reinterpret_f32", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
                        break;
                    }
                    case wasm1_code::i64_reinterpret_f64:
                    {
                        validate_numeric_unary(u8"i64.reinterpret_f64", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
                        break;
                    }
                    case wasm1_code::f32_reinterpret_i32:
                    {
                        validate_numeric_unary(u8"f32.reinterpret_i32", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);
                        break;
                    }
                    case wasm1_code::f64_reinterpret_i64:
                    {
                        validate_numeric_unary(u8"f64.reinterpret_i64", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);
                        break;
                    }
                    [[unlikely]] default:
                    {
                        err.err_curr = code_curr;
                        err.err_selectable.u8 = static_cast<::std::uint_least8_t>(curr_opbase);
                        err.err_code = code_validation_error_code::illegal_opbase;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                }

                if(!is_polymorphic) { runtime_operand_stack_max = ::std::max(runtime_operand_stack_max, operand_stack.size()); }
            }
        }

        return storage;
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
