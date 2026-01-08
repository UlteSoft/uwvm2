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

#include "uwvm2/compiler/validation/error/error.h"
#include "uwvm2/parser/wasm/standard/wasm1/opcode/mvp.h"
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
# include <uwvm2/compiler/validation/error/impl.h>
# include <uwvm2/compiler/validation/concepts/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::compiler::validation::standard::wasm1
{
    using wasm1_code = ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic;

    using wasm1_code_version_type = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using operand_stack_value_type = ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct operand_stack_storage_t
    {
        operand_stack_value_type<Fs...> type{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using operand_stack_type = ::uwvm2::utils::container::vector<operand_stack_storage_t<Fs...>>;

    template <typename T>
    struct fast_io_native_typed_global_allocator_guard
    {
        using allocator = ::fast_io::native_typed_global_allocator<T>;
        T* ptr{};

        inline constexpr fast_io_native_typed_global_allocator_guard() noexcept = default;

        inline constexpr fast_io_native_typed_global_allocator_guard(T* o_ptr) noexcept : ptr{o_ptr} {}

        inline constexpr fast_io_native_typed_global_allocator_guard(fast_io_native_typed_global_allocator_guard const&) = delete;
        inline constexpr fast_io_native_typed_global_allocator_guard& operator= (fast_io_native_typed_global_allocator_guard const&) = delete;

        inline constexpr fast_io_native_typed_global_allocator_guard(fast_io_native_typed_global_allocator_guard&& other) noexcept :
            ptr{::std::exchange(other.ptr, nullptr)}
        {
        }

        inline constexpr fast_io_native_typed_global_allocator_guard& operator= (fast_io_native_typed_global_allocator_guard&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            // The deallocator performs internal null pointer checks.
            allocator::deallocate(this->ptr);
            this->ptr = ::std::exchange(other.ptr, nullptr);

            return *this;
        }

        inline constexpr ~fast_io_native_typed_global_allocator_guard()
        {
            // The deallocator performs internal null pointer checks.
            allocator::deallocate(this->ptr);
        }
    };

    template <typename T>
    struct fast_io_native_typed_thread_local_allocator_guard
    {
        using allocator = ::fast_io::native_typed_thread_local_allocator<T>;
        T* ptr{};

        inline constexpr fast_io_native_typed_thread_local_allocator_guard() noexcept = default;

        inline constexpr fast_io_native_typed_thread_local_allocator_guard(T* o_ptr) noexcept : ptr{o_ptr} {}

        inline constexpr fast_io_native_typed_thread_local_allocator_guard(fast_io_native_typed_thread_local_allocator_guard const&) = delete;
        inline constexpr fast_io_native_typed_thread_local_allocator_guard& operator= (fast_io_native_typed_thread_local_allocator_guard const&) = delete;

        inline constexpr fast_io_native_typed_thread_local_allocator_guard(fast_io_native_typed_thread_local_allocator_guard&& other) noexcept :
            ptr{::std::exchange(other.ptr, nullptr)}
        {
        }

        inline constexpr fast_io_native_typed_thread_local_allocator_guard& operator= (fast_io_native_typed_thread_local_allocator_guard&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            // The deallocator performs internal null pointer checks.
            allocator::deallocate(this->ptr);
            this->ptr = ::std::exchange(other.ptr, nullptr);

            return *this;
        }

        inline constexpr ~fast_io_native_typed_thread_local_allocator_guard()
        {
            // The deallocator performs internal null pointer checks.
            allocator::deallocate(this->ptr);
        }
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using block_result_type = ::uwvm2::parser::wasm::standard::wasm1::features::final_result_type<Fs...>;

    enum class block_type : unsigned
    {
        function,
        block,
        loop,
        if_,
        else_
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct block_t
    {
        block_result_type<Fs...> result{};
        ::std::size_t operand_stack_base{};
        block_type type{};
        bool polymorphic_base{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline void validate_code(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version,
                              ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
                              ::std::size_t const function_index,
                              ::std::byte const* code_begin,
                              ::std::byte const* code_end,
                              ::uwvm2::compiler::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        // check
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 0uz);
        auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};
        if(function_index < import_func_count) [[unlikely]]
        {
            err.err_curr = code_begin;
            err.err_selectable.not_local_function.function_index = function_index;
            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::not_local_function;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        auto const local_func_idx{function_index - import_func_count};

        auto const& funcsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>(
                module_storage.sections)};
        auto const local_func_count{funcsec.funcs.size()};
        if(local_func_idx >= local_func_count) [[unlikely]]
        {
            err.err_curr = code_begin;
            err.err_selectable.invalid_function_index.function_index = function_index;
            // this add will never overflow, because it has been validated in parsing.
            err.err_selectable.invalid_function_index.all_function_size = import_func_count + local_func_count;
            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_function_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(module_storage.sections)};

        auto const& curr_func_type{typesec.types.index_unchecked(funcsec.funcs.index_unchecked(local_func_idx))};
        auto const func_parameter_begin{curr_func_type.parameter.begin};
        auto const func_parameter_end{curr_func_type.parameter.end};
        auto const func_parameter_count_uz{static_cast<::std::size_t>(func_parameter_end - func_parameter_begin)};
        auto const func_parameter_count_u32{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_parameter_count_uz)};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(func_parameter_count_u32 != func_parameter_count_uz) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(module_storage.sections)};

        auto const& curr_code{codesec.codes.index_unchecked(local_func_idx)};
        auto const& curr_code_locals{curr_code.locals};

        // all local count = parameter + local defined local count
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_local_count{func_parameter_count_u32};
        for(auto const& local_part: curr_code_locals)
        {
            // all_local_count never overflow and never exceed the max of size_t
            all_local_count += local_part.count;
        }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if constexpr(::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max() > ::std::numeric_limits<::std::size_t>::max())
        {
            if(all_local_count > ::std::numeric_limits<::std::size_t>::max()) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
        }
#endif

        auto const& globalsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 3uz);
        auto const& imported_globals{importsec.importdesc.index_unchecked(3u)};
        auto const imported_global_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_globals.size())};
        auto const local_global_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(globalsec.local_globals.size())};
        // all_global_count never overflow and never exceed the max of u32 (validated by parser limits)
        auto const all_global_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_global_count + local_global_count)};

        // table
        auto const& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 1uz);
        auto const& imported_tables{importsec.importdesc.index_unchecked(1u)};
        auto const imported_table_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_tables.size())};
        auto const local_table_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(tablesec.tables.size())};
        // all_table_count never overflow and never exceed the max of u32 (validated by parser limits)
        auto const all_table_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_table_count + local_table_count)};

        // memory
        auto const& memsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 2uz);
        auto const& imported_memories{importsec.importdesc.index_unchecked(2u)};
        auto const imported_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_memories.size())};
        auto const local_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(memsec.memories.size())};
        // all_memory_count never overflow and never exceed the max of u32 (validated by parser limits)
        auto const all_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_memory_count + local_memory_count)};

        // control-flow stack
        using curr_block_type = block_t<Fs...>;
        ::uwvm2::utils::container::vector<curr_block_type> control_flow_stack{};

        // operand stack
        using curr_operand_stack_value_type = operand_stack_value_type<Fs...>;
        using curr_operand_stack_type = operand_stack_type<Fs...>;
        curr_operand_stack_type operand_stack{};
        bool is_polymorphic{};

        // block type
        using value_type_enum = curr_operand_stack_value_type;
        static constexpr value_type_enum i32_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32)};
        static constexpr value_type_enum i64_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64)};
        static constexpr value_type_enum f32_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32)};
        static constexpr value_type_enum f64_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64)};

        // function block (label/result type is the function result)
        control_flow_stack.push_back({.result = curr_func_type.result, .type = block_type::function, .operand_stack_base = 0uz, .polymorphic_base = false});

        // start parse the code
        auto code_curr{code_begin};

        // [before_section ... ] | opbase opextent
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ code_curr

        // a WebAssembly function with type '() -> ()' (often written as returning “nil”) can have no meaningful code, but it still must have a valid
        // instruction sequence—at minimum an end.

        for(;;)
        {
            if(code_curr == code_end) [[unlikely]]
            {
                // [... ] | (end)
                // [safe] | unsafe (could be the section_end)
                //          ^^ code_curr

                // Validation completes when the end is reached, so this condition can never be met. If it were met, it would indicate a missing end.
                err.err_curr = code_curr;
                err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::missing_end;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // opbase ...
            // [safe] unsafe (could be the section_end)
            // ^^ code_curr

            // switch the code
            wasm1_code curr_opbase;  // no initialize necessary
            ::std::memcpy(::std::addressof(curr_opbase), code_curr, sizeof(wasm1_code));

            switch(curr_opbase)
            {
                case wasm1_code::unreachable:
                {
                    // `unreachable` makes the operand stack "polymorphic" (per Wasm validation rules):
                    // after an unreachable point, the following instructions are type-checked under the
                    // assumption that any required operands can be popped (and any results pushed),
                    // because this code path will not execute at runtime; this suppresses false
                    // operand-stack underflow/type errors until the control-flow merges/ends.

                    // unreachable ...
                    // [   safe  ] unsafe (could be the section_end)
                    // ^^ code_curr

                    ++code_curr;

                    // unreachable ...
                    // [   safe  ] unsafe (could be the section_end)
                    //             ^^ code_curr

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
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::missing_block_type;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
                    }

                    // block blocktype ...
                    // [     safe    ] unsafe (could be the section_end)
                    //       ^^ op_begin

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte blocktype_byte;  // No initialization necessary
                    ::std::memcpy(::std::addressof(blocktype_byte), code_curr, sizeof(blocktype_byte));

                    ++code_curr;

                    // block blocktype ...
                    // [     safe    ] unsafe (could be the section_end)
                    //                 ^^ op_begin

                    // MVP blocktype: 0x40 (empty) or a single value type (i32/i64/f32/f64)
                    block_result_type<Fs...> block_result{};

                    switch(blocktype_byte)
                    {
                        case 0x40u:
                        {
                            // empty result
                            block_result = {};
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                            ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32):
                        {
                            block_result.begin = i32_result_arr;
                            block_result.end = i32_result_arr + 1u;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                            ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64):
                        {
                            block_result.begin = i64_result_arr;
                            block_result.end = i64_result_arr + 1u;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                            ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32):
                        {
                            block_result.begin = f32_result_arr;
                            block_result.end = f32_result_arr + 1u;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                            ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64):
                        {
                            block_result.begin = f64_result_arr;
                            block_result.end = f64_result_arr + 1u;
                            break;
                        }
                        [[unlikely]] default:
                        {
                            // Unknown blocktype encoding; treat as invalid code.
                            err.err_curr = op_begin;
                            err.err_selectable.u8 = blocktype_byte;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_block_type;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    control_flow_stack.push_back(
                        {.result = block_result, .operand_stack_base = operand_stack.size(), .type = block_type::block, .polymorphic_base = is_polymorphic});

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
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::missing_block_type;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
                    }

                    // loop blocktype ...
                    // [    safe    ] unsafe (could be the section_end)
                    //      ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte blocktype_byte;  // No initialization necessary
                    ::std::memcpy(::std::addressof(blocktype_byte), code_curr, sizeof(blocktype_byte));

                    ++code_curr;

                    // loop blocktype ...
                    // [    safe    ] unsafe (could be the section_end)
                    //                ^^ code_curr

                    // MVP blocktype: 0x40 (empty) or a single value type (i32/i64/f32/f64)
                    block_result_type<Fs...> block_result{};

                    switch(blocktype_byte)
                    {
                        case 0x40u:
                        {
                            // empty result
                            block_result = {};
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                            ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32):
                        {
                            block_result.begin = i32_result_arr;
                            block_result.end = i32_result_arr + 1u;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                            ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64):
                        {
                            block_result.begin = i64_result_arr;
                            block_result.end = i64_result_arr + 1u;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                            ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32):
                        {
                            block_result.begin = f32_result_arr;
                            block_result.end = f32_result_arr + 1u;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                            ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64):
                        {
                            block_result.begin = f64_result_arr;
                            block_result.end = f64_result_arr + 1u;
                            break;
                        }
                        [[unlikely]] default:
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.u8 = blocktype_byte;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_block_type;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    control_flow_stack.push_back(
                        {.result = block_result, .type = block_type::loop, .operand_stack_base = operand_stack.size(), .polymorphic_base = is_polymorphic});

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
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::missing_block_type;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
                    }

                    // if blocktype ...
                    // [   safe   ] unsafe (could be the section_end)
                    //    ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte blocktype_byte;  // No initialization necessary
                    ::std::memcpy(::std::addressof(blocktype_byte), code_curr, sizeof(blocktype_byte));

                    ++code_curr;

                    // if blocktype ...
                    // [   safe   ] unsafe (could be the section_end)
                    //              ^^ code_curr

                    // MVP blocktype: 0x40 (empty) or a single value type (i32/i64/f32/f64)
                    block_result_type<Fs...> block_result{};

                    switch(blocktype_byte)
                    {
                        case 0x40u:
                        {
                            block_result = {};
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                            ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32):
                        {
                            block_result.begin = i32_result_arr;
                            block_result.end = i32_result_arr + 1u;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                            ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64):
                        {
                            block_result.begin = i64_result_arr;
                            block_result.end = i64_result_arr + 1u;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                            ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32):
                        {
                            block_result.begin = f32_result_arr;
                            block_result.end = f32_result_arr + 1u;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                            ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64):
                        {
                            block_result.begin = f64_result_arr;
                            block_result.end = f64_result_arr + 1u;
                            break;
                        }
                        [[unlikely]] default:
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.u8 = blocktype_byte;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_block_type;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    // Stack effect: (i32 cond) -> () before entering the then branch.
                    if(!is_polymorphic && operand_stack.empty()) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.operand_stack_underflow.op_code_name = u8"if";
                        err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                        err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(!operand_stack.empty())
                    {
                        auto const cond{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(cond.type != curr_operand_stack_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.if_cond_type_not_i32.cond_type = cond.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::if_cond_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    control_flow_stack.push_back(
                        {.result = block_result, .operand_stack_base = operand_stack.size(), .type = block_type::if_, .polymorphic_base = is_polymorphic});

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
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_else;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto& if_frame{control_flow_stack.back_unchecked()};

                    // Validate the then-branch result types/count before switching to else.
                    // In polymorphic mode (e.g. then branch was unreachable), result checking is suppressed.
                    if(!is_polymorphic)
                    {
                        auto const expected_count{static_cast<::std::size_t>(if_frame.result.end - if_frame.result.begin)};
                        auto const actual_count{operand_stack.size() - if_frame.operand_stack_base};

                        bool mismatch{expected_count != actual_count};

                        ::uwvm2::parser::wasm::standard::wasm1::type::value_type expected_type{};
                        ::uwvm2::parser::wasm::standard::wasm1::type::value_type actual_type{};

                        bool const expected_single{expected_count == 1uz};
                        bool const actual_single{actual_count == 1uz};

                        if(expected_single) { expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(*if_frame.result.begin); }
                        if(actual_single)
                        {
                            actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(operand_stack.back_unchecked().type);
                        }

                        if(!mismatch && expected_single && actual_single && expected_type != actual_type) { mismatch = true; }

                        if(mismatch) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.if_then_result_mismatch.expected_count = expected_count;
                            err.err_selectable.if_then_result_mismatch.actual_count = actual_count;
                            err.err_selectable.if_then_result_mismatch.expected_type = expected_type;
                            err.err_selectable.if_then_result_mismatch.actual_type = actual_type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::if_then_result_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    // Start else branch with the operand stack at if-entry height.
                    while(operand_stack.size() > if_frame.operand_stack_base) { operand_stack.pop_back_unchecked(); }
                    is_polymorphic = if_frame.polymorphic_base;

                    // Mark that else has been consumed.
                    if_frame.type = block_type::else_;

                    break;
                }
                case wasm1_code::end:
                {
                    /// @todo
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

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 label_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [label_next, label_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(label_index))};
                    if(label_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_label_index;
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
                        err.err_selectable.illegal_label_index.all_label_count =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_label_count_uz);
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_label_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const& target_frame{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - label_index_uz)};

                    // Label arity = label_types count. In MVP, we only support empty or single-value blocktypes.
                    // For loops, label_types are the loop's blocktype (same as `result` here).
                    auto const target_arity{static_cast<::std::size_t>(target_frame.result.end - target_frame.result.begin)};

                    if(!is_polymorphic && operand_stack.size() < target_arity) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.operand_stack_underflow.op_code_name = u8"br";
                        err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                        err.err_selectable.operand_stack_underflow.stack_size_required = target_arity;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // type-check the branch arguments if present
                    if(!is_polymorphic && target_arity != 0uz && operand_stack.size() >= target_arity)
                    {
                        auto const expected_type{*target_frame.result.begin};
                        auto const actual_type{operand_stack.back_unchecked().type};
                        if(actual_type != expected_type) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.br_value_type_mismatch.op_code_name = u8"br";
                            err.err_selectable.br_value_type_mismatch.expected_type =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                            err.err_selectable.br_value_type_mismatch.actual_type =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::br_value_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    // Consume branch arguments (if present) and make stack polymorphic (unreachable).
                    if(target_arity != 0uz)
                    {
                        auto n{target_arity};
                        while(!operand_stack.empty() && n-- != 0uz) { operand_stack.pop_back_unchecked(); }
                    }
                    // Avoid leaking concrete stack values into the polymorphic region (prevents false type errors after an unconditional branch).
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

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 label_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [label_next, label_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(label_index))};
                    if(label_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_label_index;
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
                        err.err_selectable.illegal_label_index.all_label_count =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_label_count_uz);
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_label_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const& target_frame{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - label_index_uz)};

                    // Label arity = label_types count (MVP: 0 or 1). For loops, label_types are the loop's blocktype.
                    auto const target_arity{static_cast<::std::size_t>(target_frame.result.end - target_frame.result.begin)};

                    // Need (labelargs..., i32 cond)
                    if(!is_polymorphic && operand_stack.size() < target_arity + 1uz) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.operand_stack_underflow.op_code_name = u8"br_if";
                        err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                        err.err_selectable.operand_stack_underflow.stack_size_required = target_arity + 1uz;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // cond (must be i32 if present)
                    if(!operand_stack.empty())
                    {
                        auto const cond{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();
                        if(!is_polymorphic && cond.type != curr_operand_stack_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.br_cond_type_not_i32.op_code_name = u8"br_if";
                            err.err_selectable.br_cond_type_not_i32.cond_type =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(cond.type);
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::br_cond_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    // type-check label arguments if present (they remain on stack for the fallthrough path)
                    if(!is_polymorphic && target_arity != 0uz && operand_stack.size() >= target_arity)
                    {
                        auto const expected_type{*target_frame.result.begin};
                        auto const actual_type{operand_stack.back_unchecked().type};
                        if(actual_type != expected_type) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.br_value_type_mismatch.op_code_name = u8"br_if";
                            err.err_selectable.br_value_type_mismatch.expected_type =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                            err.err_selectable.br_value_type_mismatch.actual_type =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::br_value_type_mismatch;
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

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 target_count;  // No initialization necessary
                    auto const [cnt_next, cnt_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(target_count))};
                    if(cnt_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_label_index;
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
                    auto const validate_label{[&](::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 li) constexpr UWVM_THROWS
                                              {
                                                  if(static_cast<::std::size_t>(li) >= all_label_count_uz) [[unlikely]]
                                                  {
                                                      err.err_curr = op_begin;
                                                      err.err_selectable.illegal_label_index.label_index = li;
                                                      err.err_selectable.illegal_label_index.all_label_count =
                                                          static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_label_count_uz);
                                                      err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_label_index;
                                                      ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                                  }
                                              }};

                    struct get_sig_result_t
                    {
                        ::std::size_t arity{};
                        curr_operand_stack_value_type type{};
                    };

                    auto const get_sig{[&](::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 li) constexpr noexcept
                                       {
                                           auto const& frame{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - static_cast<::std::size_t>(li))};

                                           ::std::size_t const arity{static_cast<::std::size_t>(frame.result.end - frame.result.begin)};
                                           curr_operand_stack_value_type type{};
                                           if(arity != 0uz) { type = frame.result.begin[0]; }

                                           return get_sig_result_t{arity, type};
                                       }};

                    bool have_expected_sig{};
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 expected_label{};
                    ::std::size_t expected_arity{};
                    curr_operand_stack_value_type expected_type{};

                    for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 i{}; i != target_count; ++i)
                    {
                        // ...    | curr_target ...
                        // [safe] | unsafe (could be the section_end)
                        //          ^^ code_curr

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 li;  // No initialization necessary
                        auto const [li_next, li_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                              ::fast_io::mnp::leb128_get(li))};
                        if(li_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_label_index;
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
                            err.err_selectable.br_table_target_type_mismatch.expected_arity =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(expected_arity);
                            err.err_selectable.br_table_target_type_mismatch.actual_arity =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(arity);
                            err.err_selectable.br_table_target_type_mismatch.expected_type =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                            err.err_selectable.br_table_target_type_mismatch.actual_type =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(type);
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::br_table_target_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    // ... last_target | default_label ...
                    // [   safe      ]   unsafe (could be the section_end)
                    //                   ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 default_label;  // No initialization necessary
                    auto const [def_next, def_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(default_label))};
                    if(def_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_label_index;
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
                        err.err_selectable.br_table_target_type_mismatch.expected_arity =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(expected_arity);
                        err.err_selectable.br_table_target_type_mismatch.actual_arity =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(default_arity);
                        err.err_selectable.br_table_target_type_mismatch.expected_type =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                        err.err_selectable.br_table_target_type_mismatch.actual_type =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(default_type);
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::br_table_target_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (labelargs..., i32 index) -> unreachable
                    if(!is_polymorphic && operand_stack.size() < expected_arity + 1uz) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.operand_stack_underflow.op_code_name = u8"br_table";
                        err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                        err.err_selectable.operand_stack_underflow.stack_size_required = expected_arity + 1uz;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(!operand_stack.empty())
                    {
                        auto const idx{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();
                        if(!is_polymorphic && idx.type != curr_operand_stack_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.br_cond_type_not_i32.op_code_name = u8"br_table";
                            err.err_selectable.br_cond_type_not_i32.cond_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(idx.type);
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::br_cond_type_not_i32;
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
                            err.err_selectable.br_value_type_mismatch.expected_type =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                            err.err_selectable.br_value_type_mismatch.actual_type =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::br_value_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    // Consume label args if present and make stack polymorphic.
                    if(expected_arity != 0uz)
                    {
                        auto n{expected_arity};
                        while(!operand_stack.empty() && n-- != 0uz) { operand_stack.pop_back_unchecked(); }
                    }
                    // Avoid leaking concrete stack values into the polymorphic region (prevents false type errors after br_table).
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

                    // `return` exits the function immediately. It is equivalent to an unconditional branch to the
                    // implicit outer function label (the bottom frame in control_flow_stack).
                    auto const& func_frame{control_flow_stack.index_unchecked(0u)};

                    ::std::size_t const return_arity{static_cast<::std::size_t>(func_frame.result.end - func_frame.result.begin)};

                    if(!is_polymorphic && operand_stack.size() < return_arity) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.operand_stack_underflow.op_code_name = u8"return";
                        err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                        err.err_selectable.operand_stack_underflow.stack_size_required = return_arity;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const operator_stack_size{operand_stack.size()};

                    // Type-check the return values if present. For multi-value, values are validated from the top of the stack.
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
                                err.err_selectable.br_value_type_mismatch.expected_type =
                                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                                err.err_selectable.br_value_type_mismatch.actual_type =
                                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                                err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                    }

                    // Consume return values (if present) and make stack polymorphic (unreachable).
                    if(return_arity != 0uz)
                    {
                        auto n{return_arity};
                        while(!operand_stack.empty() && n-- != 0uz) { operand_stack.pop_back_unchecked(); }
                    }

                    // Avoid leaking concrete stack values into the polymorphic region (prevents false type errors after return).
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

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [func_next, func_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                              ::fast_io::mnp::leb128_get(func_index))};
                    if(func_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_function_index_encoding;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(func_err);
                    }

                    // call func_index ...
                    // [      safe   ] unsafe (could be the section_end)
                    //      ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(func_next);

                    // call func_index ...
                    // [      safe   ] unsafe (could be the section_end)
                    //                ^^ code_curr

                    // Validate function index range (imports + locals)
                    auto const all_function_size{import_func_count + local_func_count};
                    if(static_cast<::std::size_t>(func_index) >= all_function_size) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.invalid_function_index.function_index = static_cast<::std::size_t>(func_index);
                        err.err_selectable.invalid_function_index.all_function_size = all_function_size;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_function_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Resolve callee type
                    ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...> const* callee_type_ptr{};
                    if(static_cast<::std::size_t>(func_index) < import_func_count)
                    {
                        auto const& imported_funcs{importsec.importdesc.index_unchecked(0u)};
                        auto const imported_func_ptr{imported_funcs.index_unchecked(static_cast<::std::size_t>(func_index))};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(imported_func_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                        callee_type_ptr = imported_func_ptr->imports.storage.function;
                    }
                    else
                    {
                        auto const local_idx{static_cast<::std::size_t>(func_index) - import_func_count};
                        callee_type_ptr = typesec.types.cbegin() + funcsec.funcs.index_unchecked(local_idx);
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
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    auto const stack_size{operand_stack.size()};

                    // Type-check arguments when the stack is non-polymorphic.
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
                                err.err_selectable.br_value_type_mismatch.expected_type =
                                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                                err.err_selectable.br_value_type_mismatch.actual_type =
                                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                                err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::br_value_type_mismatch;
                                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                            }
                        }
                    }

                    // Consume parameters if present.
                    if(param_count != 0uz)
                    {
                        auto n{param_count};
                        while(!operand_stack.empty() && n-- != 0uz) { operand_stack.pop_back_unchecked(); }
                    }

                    // Push results.
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

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 type_index;  // No initialization necessary
                    auto const [type_next, type_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                              ::fast_io::mnp::leb128_get(type_index))};
                    if(type_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(type_err);
                    }

                    // call_indirect type_index table_index ...
                    // [          safe        ] unsafe (could be the section_end)
                    //               ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(type_next);

                    // call_indirect type_index table_index ...
                    // [          safe        ] unsafe (could be the section_end)
                    //                          ^^ code_curr

                    auto const all_type_count_uz{typesec.types.size()};
                    if(static_cast<::std::size_t>(type_index) >= all_type_count_uz) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_type_index.type_index = type_index;
                        err.err_selectable.illegal_type_index.all_type_count =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_type_count_uz);
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_type_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_index;  // No initialization necessary
                    auto const [table_next, table_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(table_index))};
                    if(table_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_table_index;
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
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_table_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Resolve the function signature by type index.
                    auto const& callee_type{typesec.types.index_unchecked(static_cast<::std::size_t>(type_index))};
                    auto const param_count{static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
                    auto const result_count{static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};

                    // Stack effect: (args..., i32 func_index) -> (results...)
                    if(!is_polymorphic && operand_stack.size() < param_count + 1uz) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.operand_stack_underflow.op_code_name = u8"call_indirect";
                        err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                        err.err_selectable.operand_stack_underflow.stack_size_required = param_count + 1uz;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // function index operand (must be i32 if present)
                    if(!operand_stack.empty())
                    {
                        auto const idx{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(!is_polymorphic && idx.type != curr_operand_stack_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.br_cond_type_not_i32.op_code_name = u8"call_indirect";
                            err.err_selectable.br_cond_type_not_i32.cond_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(idx.type);
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::br_cond_type_not_i32;
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
                                err.err_selectable.br_value_type_mismatch.expected_type =
                                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                                err.err_selectable.br_value_type_mismatch.actual_type =
                                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                                err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::br_value_type_mismatch;
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
                        // Polymorphic stack: underflow is allowed, so drop becomes a no-op on the concrete stack.
                        if(!is_polymorphic)
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"drop";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
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

                    // Stack effect: (v1 v2 i32) -> (v) where v is v1/v2 and v1,v2 must have the same type.
                    // In polymorphic mode, operand-stack underflow is allowed, but concrete operands (if present) are still type-checked.

                    if(!is_polymorphic && operand_stack.size() < 3uz) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.operand_stack_underflow.op_code_name = u8"select";
                        err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                        err.err_selectable.operand_stack_underflow.stack_size_required = 3uz;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // cond (must be i32 if it exists on the concrete stack)
                    bool cond_from_stack{};
                    curr_operand_stack_value_type cond_type{};
                    if(!operand_stack.empty())
                    {
                        auto const cond{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();
                        cond_from_stack = true;
                        cond_type = cond.type;
                    }

                    if(cond_from_stack && cond_type != curr_operand_stack_type::i32) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.select_cond_type_not_i32.cond_type = cond_type;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::select_cond_type_not_i32;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // v2
                    bool v2_from_stack{};
                    curr_operand_stack_value_type v2_type{};
                    if(!operand_stack.empty())
                    {
                        auto const v2{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();
                        v2_from_stack = true;
                        v2_type = v2.type;
                    }

                    // v1 (kept as result when present, matching existing implementation)
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
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::select_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // If v1 is not present on the concrete stack but v2 is, we must still produce one result of v2's type.
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

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
                    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                            ::fast_io::mnp::leb128_get(local_index))};

                    if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_local_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
                    }

                    // local.get local_index ...
                    // [     safe          ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

                    // local.get local_index ...
                    // [     safe          ] unsafe (could be the section_end)
                    //                       ^^ code_curr

                    // check the local_index is valid
                    if(local_index >= all_local_count) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_local_index.local_index = local_index;
                        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_local_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    curr_operand_stack_type curr_local_type{};

                    if(local_index < func_parameter_count_u32)
                    {
                        // function parameter
                        curr_local_type = func_parameter_begin[local_index];
                    }
                    else
                    {
                        // function defined local variable
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
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    // local.get always pushes one value of the local's type (even in polymorphic mode)
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

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
                    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                            ::fast_io::mnp::leb128_get(local_index))};

                    if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_local_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
                    }

                    // local.set local_index ...
                    // [     safe          ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

                    // local.set local_index ...
                    // [     safe          ] unsafe (could be the section_end)
                    //                       ^^ code_curr

                    // check the local_index is valid
                    if(local_index >= all_local_count) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_local_index.local_index = local_index;
                        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_local_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    curr_operand_stack_type curr_local_type{};

                    if(local_index < func_parameter_count_u32)
                    {
                        // function parameter
                        curr_local_type = func_parameter_begin[local_index];
                    }
                    else
                    {
                        // function defined local variable
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
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    if(operand_stack.empty()) [[unlikely]]
                    {
                        // Polymorphic stack: underflow is allowed, so local.set becomes a no-op on the concrete stack.
                        if(!is_polymorphic)
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"local.set";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
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
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::local_set_type_mismatch;
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

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
                    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                            ::fast_io::mnp::leb128_get(local_index))};

                    if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_local_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
                    }

                    // local.tee local_index ...
                    // [     safe          ] unsafe (could be the section_end)
                    //           ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

                    // local.tee local_index ...
                    // [     safe          ] unsafe (could be the section_end)
                    //                       ^^ code_curr

                    // check the local_index is valid
                    if(local_index >= all_local_count) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_local_index.local_index = local_index;
                        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_local_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    curr_operand_stack_type curr_local_type{};

                    if(local_index < func_parameter_count_u32)
                    {
                        // function parameter
                        curr_local_type = func_parameter_begin[local_index];
                    }
                    else
                    {
                        // function defined local variable
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
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_local_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    if(operand_stack.empty()) [[unlikely]]
                    {
                        // Polymorphic stack: underflow is allowed.
                        if(!is_polymorphic)
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"local.tee";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                        else
                        {
                            // In polymorphic mode, `local.tee` still produces a value of the local's type.
                            // pop t (dismiss), push t (here)
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
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::local_tee_type_mismatch;
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

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 global_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
                    auto const [global_index_next, global_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                              ::fast_io::mnp::leb128_get(global_index))};

                    if(global_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_global_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(global_index_err);
                    }

                    // global.get global_index ...
                    // [     safe            ] unsafe (could be the section_end)
                    //            ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(global_index_next);

                    // global.get global_index ...
                    // [      safe           ] unsafe (could be the section_end)
                    //                         ^^ code_curr

                    // check the global_index is valid
                    if(global_index >= all_global_count) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_global_index.global_index = global_index;
                        err.err_selectable.illegal_global_index.all_global_count = all_global_count;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_global_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    curr_operand_stack_value_type curr_global_type{};
                    if(global_index < imported_global_count)
                    {
                        auto const imported_global_ptr{imported_globals.index_unchecked(global_index)};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                        curr_global_type = imported_global_ptr->imports.storage.global.type;
                    }
                    else
                    {
                        auto const local_global_index{global_index - imported_global_count};
                        curr_global_type = globalsec.local_globals.index_unchecked(local_global_index).global.type;
                    }

                    // global.get always pushes one value of the global's type (even in polymorphic mode)
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

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 global_index;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
                    auto const [global_index_next, global_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                              ::fast_io::mnp::leb128_get(global_index))};

                    if(global_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_global_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(global_index_err);
                    }

                    // global.set global_index ...
                    // [     safe            ] unsafe (could be the section_end)
                    //            ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(global_index_next);

                    // global.set global_index ...
                    // [      safe           ] unsafe (could be the section_end)
                    //                         ^^ code_curr

                    // Validate global_index range (imports + local globals)
                    if(global_index >= all_global_count) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_global_index.global_index = global_index;
                        err.err_selectable.illegal_global_index.all_global_count = all_global_count;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_global_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Resolve the global's value type and mutability for global.set
                    curr_operand_stack_value_type curr_global_type{};

                    bool curr_global_mutable{};
                    if(global_index < imported_global_count)
                    {
                        auto const imported_global_ptr{imported_globals.index_unchecked(global_index)};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                        auto const& imported_global{imported_global_ptr->imports.storage.global};
                        curr_global_type = imported_global.type;
                        curr_global_mutable = imported_global.is_mutable;
                    }
                    else
                    {
                        auto const local_global_index{global_index - imported_global_count};
                        auto const& local_global{globalsec.local_globals.index_unchecked(local_global_index).global};
                        curr_global_type = local_global.type;
                        curr_global_mutable = local_global.is_mutable;
                    }

                    // global.set requires the target global to be mutable (immutable globals cannot be written)
                    if(!curr_global_mutable) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.immutable_global_set.global_index = global_index;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::immutable_global_set;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (value) -> () where value must match global's value type
                    if(operand_stack.empty()) [[unlikely]]
                    {
                        // Polymorphic stack: underflow is allowed, so global.set becomes a no-op on the concrete stack.
                        if(!is_polymorphic)
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"global.set";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
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
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::global_set_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

                    break;
                }
                case wasm1_code::i32_load:
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

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
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
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // i32.load align offset ...
                    // [        safe       ] unsafe (could be the section_end)
                    //                ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // i32.load align offset ...
                    // [        safe       ] unsafe (could be the section_end)
                    //                       ^^ code_curr

                    // MVP memory instructions implicitly target memory 0. If the module has no imported/defined memory, any load/store is invalid.
                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"i32.load";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // For i32.load the natural alignment is 4 bytes => alignment exponent must be <= 2
                    if(align > 2u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"i32.load";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 2u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (i32 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"i32.load";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"i32.load";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        // In polymorphic mode we still apply the stack effect, but we do not raise underflow/type errors.
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32});
                    break;
                }
                case wasm1_code::i64_load:
                {
                    // i64.load align offset ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i64.load align offset ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i64.load align offset ...
                    // [ safe ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // i64.load align offset ...
                    // [     safe   ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // i64.load align offset ...
                    // [     safe   ] unsafe (could be the section_end)
                    //                ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // i64.load align offset ...
                    // [      safe         ] unsafe (could be the section_end)
                    //                ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // i64.load align offset ...
                    // [      safe         ] unsafe (could be the section_end)
                    //                       ^^ code_curr

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"i64.load";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // i64.load natural alignment is 8 bytes => alignment exponent must be <= 3
                    if(align > 3u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"i64.load";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 3u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (i64 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"i64.load";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"i64.load";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64});
                    break;
                }
                case wasm1_code::f32_load:
                {
                    // f32.load align offset ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // f32.load align offset ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // f32.load align offset ...
                    // [ safe ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // f32.load align offset ...
                    // [      safe  ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // f32.load align offset ...
                    // [      safe  ] unsafe (could be the section_end)
                    //                ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // f32.load align offset ...
                    // [        safe       ] unsafe (could be the section_end)
                    //                ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // f32.load align offset ...
                    // [        safe       ] unsafe (could be the section_end)
                    //                       ^^ code_curr

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"f32.load";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // f32.load natural alignment is 4 bytes => alignment exponent must be <= 2
                    if(align > 2u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"f32.load";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 2u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (f32 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"f32.load";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"f32.load";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32});
                    break;
                }
                case wasm1_code::f64_load:
                {
                    // f64.load align offset ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // f64.load align offset ...
                    // [ safe ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // f64.load align offset ...
                    // [ safe ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // f64.load align offset ...
                    // [      safe  ] unsafe (could be the section_end)
                    //          ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // f64.load align offset ...
                    // [      safe  ] unsafe (could be the section_end)
                    //                ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // f64.load align offset ...
                    // [         safe      ] unsafe (could be the section_end)
                    //                ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // f64.load align offset ...
                    // [         safe      ] unsafe (could be the section_end)
                    //                       ^^ code_curr

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"f64.load";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // f64.load natural alignment is 8 bytes => alignment exponent must be <= 3
                    if(align > 3u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"f64.load";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 3u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (f64 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"f64.load";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"f64.load";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64});
                    break;
                }
                case wasm1_code::i32_load8_s:
                {
                    // i32_load8_s align offset ...
                    // [   safe  ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i32_load8_s align offset ...
                    // [   safe  ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i32_load8_s align offset ...
                    // [  safe   ] unsafe (could be the section_end)
                    //             ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // i32_load8_s align offset ...
                    // [     safe      ] unsafe (could be the section_end)
                    //             ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // i32_load8_s align offset ...
                    // [     safe      ] unsafe (could be the section_end)
                    //                   ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // i32_load8_s align offset ...
                    // [        safe          ] unsafe (could be the section_end)
                    //                   ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // i32_load8_s align offset ...
                    // [        safe          ] unsafe (could be the section_end)
                    //                          ^^ code_curr

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"i32.load8_s";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // i32.load8_s natural alignment is 1 byte => alignment exponent must be <= 0
                    if(align > 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"i32.load8_s";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 0u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (i32 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"i32.load8_s";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"i32.load8_s";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32});
                    break;
                }
                case wasm1_code::i32_load8_u:
                {
                    // i32_load8_u align offset ...
                    // [   safe  ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i32_load8_u align offset ...
                    // [   safe  ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i32_load8_u align offset ...
                    // [   safe  ] unsafe (could be the section_end)
                    //             ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // i32_load8_u align offset ...
                    // [      safe     ] unsafe (could be the section_end)
                    //             ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // i32_load8_u align offset ...
                    // [      safe     ] unsafe (could be the section_end)
                    //                   ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // i32_load8_u align offset ...
                    // [         safe         ] unsafe (could be the section_end)
                    //                   ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // i32_load8_u align offset ...
                    // [         safe         ] unsafe (could be the section_end)
                    //                          ^^ code_curr

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"i32.load8_u";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // i32.load8_u natural alignment is 1 byte => alignment exponent must be <= 0
                    if(align > 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"i32.load8_u";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 0u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (i32 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"i32.load8_u";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"i32.load8_u";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32});
                    break;
                }
                case wasm1_code::i32_load16_s:
                {
                    // i32_load16_s align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i32_load16_s align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i32_load16_s align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    //              ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // i32_load16_s align offset ...
                    // [      safe      ] unsafe (could be the section_end)
                    //              ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // i32_load16_s align offset ...
                    // [      safe      ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // i32_load16_s align offset ...
                    // [          safe         ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // i32_load16_s align offset ...
                    // [          safe         ] unsafe (could be the section_end)
                    //                           ^^ code_curr

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"i32.load16_s";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // i32.load16_s natural alignment is 2 bytes => alignment exponent must be <= 1
                    if(align > 1u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"i32.load16_s";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 1u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (i32 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"i32.load16_s";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"i32.load16_s";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32});
                    break;
                }
                case wasm1_code::i32_load16_u:
                {
                    // i32_load16_u align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i32_load16_u align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i32_load16_u align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    //              ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // i32_load16_u align offset ...
                    // [      safe      ] unsafe (could be the section_end)
                    //              ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // i32_load16_u align offset ...
                    // [      safe      ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // i32_load16_u align offset ...
                    // [          safe         ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // i32_load16_u align offset ...
                    // [          safe         ] unsafe (could be the section_end)
                    //                           ^^ code_curr

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"i32.load16_u";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // i32.load16_u natural alignment is 2 bytes => alignment exponent must be <= 1
                    if(align > 1u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"i32.load16_u";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 1u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (i32 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"i32.load16_u";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"i32.load16_u";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32});
                    break;
                }
                case wasm1_code::i64_load8_s:
                {
                    // i64_load8_s align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i64_load8_s align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i64_load8_s align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    //              ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"i64.load8_s";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // i64.load8_s natural alignment is 1 byte => alignment exponent must be <= 0
                    if(align > 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"i64.load8_s";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 0u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (i64 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"i64.load8_s";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"i64.load8_s";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64});
                    break;
                }
                case wasm1_code::i64_load8_u:
                {
                    // i64_load8_u align offset ...
                    // [   safe  ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i64_load8_u align offset ...
                    // [   safe  ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i64_load8_u align offset ...
                    // [   safe  ] unsafe (could be the section_end)
                    //             ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // i64_load8_u align offset ...
                    // [      safe     ] unsafe (could be the section_end)
                    //             ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // i64_load8_u align offset ...
                    // [      safe     ] unsafe (could be the section_end)
                    //                   ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // i64_load8_u align offset ...
                    // [         safe         ] unsafe (could be the section_end)
                    //                   ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // i64_load8_u align offset ...
                    // [         safe         ] unsafe (could be the section_end)
                    //                          ^^ code_curr

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"i64.load8_u";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // i64.load8_u natural alignment is 1 byte => alignment exponent must be <= 0
                    if(align > 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"i64.load8_u";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 0u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (i64 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"i64.load8_u";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"i64.load8_u";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64});
                    break;
                }
                case wasm1_code::i64_load16_s:
                {
                    // i64_load16_s align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i64_load16_s align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i64_load16_s align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    //              ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // i64_load16_s align offset ...
                    // [       safe     ] unsafe (could be the section_end)
                    //              ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // i64_load16_s align offset ...
                    // [       safe     ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // i64_load16_s align offset ...
                    // [          safe         ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // i64_load16_s align offset ...
                    // [          safe         ] unsafe (could be the section_end)
                    //                           ^^ code_curr

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"i64.load16_s";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // i64.load16_s natural alignment is 2 bytes => alignment exponent must be <= 1
                    if(align > 1u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"i64.load16_s";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 1u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (i64 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"i64.load16_s";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"i64.load16_s";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64});
                    break;
                }
                case wasm1_code::i64_load16_u:
                {
                    // i64_load16_u align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i64_load16_u align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i64_load16_u align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    //              ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // i64_load16_u align offset ...
                    // [       safe     ] unsafe (could be the section_end)
                    //              ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // i64_load16_u align offset ...
                    // [       safe     ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // i64_load16_u align offset ...
                    // [          safe         ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // i64_load16_u align offset ...
                    // [          safe         ] unsafe (could be the section_end)
                    //                           ^^ code_curr

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"i64.load16_u";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // i64.load16_u natural alignment is 2 bytes => alignment exponent must be <= 1
                    if(align > 1u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"i64.load16_u";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 1u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (i64 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"i64.load16_u";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"i64.load16_u";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64});
                    break;
                }
                case wasm1_code::i64_load32_s:
                {
                    // i64_load32_s align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i64_load32_s align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i64_load32_s align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    //             ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // i64_load32_s align offset ...
                    // [       safe     ] unsafe (could be the section_end)
                    //             ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // i64_load32_s align offset ...
                    // [       safe     ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // i64_load32_s align offset ...
                    // [          safe         ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // i64_load32_s align offset ...
                    // [          safe         ] unsafe (could be the section_end)
                    //                           ^^ code_curr

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"i64.load32_s";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // i64.load32_s natural alignment is 4 bytes => alignment exponent must be <= 2
                    if(align > 2u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"i64.load32_s";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 2u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (i64 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"i64.load32_s";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"i64.load32_s";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64});
                    break;
                }
                case wasm1_code::i64_load32_u:
                {
                    // i64_load32_u align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ code_curr

                    auto const op_begin{code_curr};

                    // i64_load32_u align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    // ^^ op_begin

                    ++code_curr;

                    // i64_load32_u align offset ...
                    // [   safe   ] unsafe (could be the section_end)
                    //              ^^ code_curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                    auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(align))};
                    if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_align;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                    }

                    // i64_load32_u align offset ...
                    // [       safe     ] unsafe (could be the section_end)
                    //              ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(align_next);

                    // i64_load32_u align offset ...
                    // [       safe     ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(offset))};
                    if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::invalid_memarg_offset;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                    }

                    // i64_load32_u align offset ...
                    // [          safe         ] unsafe (could be the section_end)
                    //                    ^^ code_curr

                    code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                    // i64_load32_u align offset ...
                    // [          safe         ] unsafe (could be the section_end)
                    //                           ^^ code_curr

                    if(all_memory_count == 0u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.no_memory.op_code_name = u8"i64.load32_u";
                        err.err_selectable.no_memory.align = align;
                        err.err_selectable.no_memory.offset = offset;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::no_memory;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // i64.load32_u natural alignment is 4 bytes => alignment exponent must be <= 2
                    if(align > 2u) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_memarg_alignment.op_code_name = u8"i64.load32_u";
                        err.err_selectable.illegal_memarg_alignment.align = align;
                        err.err_selectable.illegal_memarg_alignment.max_align = 2u;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_memarg_alignment;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Stack effect: (i32 addr) -> (i64 value)
                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"i64.load32_u";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const addr{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        if(addr.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.memarg_address_type_not_i32.op_code_name = u8"i64.load32_u";
                            err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::memarg_address_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                    else
                    {
                        if(!operand_stack.empty()) { operand_stack.pop_back_unchecked(); }
                    }

                    operand_stack.push_back({::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64});
                    break;
                }
                case wasm1_code::i32_store:
                {
                    break;
                }
                case wasm1_code::i64_store:
                {
                    break;
                }
                case wasm1_code::f32_store:
                {
                    break;
                }
                case wasm1_code::f64_store:
                {
                    break;
                }
                case wasm1_code::i32_store8:
                {
                    break;
                }
                case wasm1_code::i32_store16:
                {
                    break;
                }
                case wasm1_code::i64_store8:
                {
                    break;
                }
                case wasm1_code::i64_store16:
                {
                    break;
                }
                case wasm1_code::i64_store32:
                {
                    break;
                }
                case wasm1_code::memory_size:
                {
                    break;
                }
                case wasm1_code::memory_grow:
                {
                    break;
                }
                case wasm1_code::i32_const:
                {
                    break;
                }
                case wasm1_code::i64_const:
                {
                    break;
                }
                case wasm1_code::f32_const:
                {
                    break;
                }
                case wasm1_code::f64_const:
                {
                    break;
                }
                case wasm1_code::i32_eqz:
                {
                    break;
                }
                case wasm1_code::i32_eq:
                {
                    break;
                }
                case wasm1_code::i32_ne:
                {
                    break;
                }
                case wasm1_code::i32_lt_s:
                {
                    break;
                }
                case wasm1_code::i32_lt_u:
                {
                    break;
                }
                case wasm1_code::i32_gt_s:
                {
                    break;
                }
                case wasm1_code::i32_gt_u:
                {
                    break;
                }
                case wasm1_code::i32_le_s:
                {
                    break;
                }
                case wasm1_code::i32_le_u:
                {
                    break;
                }
                case wasm1_code::i32_ge_s:
                {
                    break;
                }
                case wasm1_code::i32_ge_u:
                {
                    break;
                }
                case wasm1_code::i64_eqz:
                {
                    break;
                }
                case wasm1_code::i64_eq:
                {
                    break;
                }
                case wasm1_code::i64_ne:
                {
                    break;
                }
                case wasm1_code::i64_lt_s:
                {
                    break;
                }
                case wasm1_code::i64_lt_u:
                {
                    break;
                }
                case wasm1_code::i64_gt_s:
                {
                    break;
                }
                case wasm1_code::i64_gt_u:
                {
                    break;
                }
                case wasm1_code::i64_le_s:
                {
                    break;
                }
                case wasm1_code::i64_le_u:
                {
                    break;
                }
                case wasm1_code::i64_ge_s:
                {
                    break;
                }
                case wasm1_code::i64_ge_u:
                {
                    break;
                }
                case wasm1_code::f32_eq:
                {
                    break;
                }
                case wasm1_code::f32_ne:
                {
                    break;
                }
                case wasm1_code::f32_lt:
                {
                    break;
                }
                case wasm1_code::f32_gt:
                {
                    break;
                }
                case wasm1_code::f32_le:
                {
                    break;
                }
                case wasm1_code::f32_ge:
                {
                    break;
                }
                case wasm1_code::f64_eq:
                {
                    break;
                }
                case wasm1_code::f64_ne:
                {
                    break;
                }
                case wasm1_code::f64_lt:
                {
                    break;
                }
                case wasm1_code::f64_gt:
                {
                    break;
                }
                case wasm1_code::f64_le:
                {
                    break;
                }
                case wasm1_code::f64_ge:
                {
                    break;
                }
                case wasm1_code::i32_clz:
                {
                    break;
                }
                case wasm1_code::i32_ctz:
                {
                    break;
                }
                case wasm1_code::i32_popcnt:
                {
                    break;
                }
                case wasm1_code::i32_add:
                {
                    break;
                }
                case wasm1_code::i32_sub:
                {
                    break;
                }
                case wasm1_code::i32_mul:
                {
                    break;
                }
                case wasm1_code::i32_div_s:
                {
                    break;
                }
                case wasm1_code::i32_div_u:
                {
                    break;
                }
                case wasm1_code::i32_rem_s:
                {
                    break;
                }
                case wasm1_code::i32_rem_u:
                {
                    break;
                }
                case wasm1_code::i32_and:
                {
                    break;
                }
                case wasm1_code::i32_or:
                {
                    break;
                }
                case wasm1_code::i32_xor:
                {
                    break;
                }
                case wasm1_code::i32_shl:
                {
                    break;
                }
                case wasm1_code::i32_shr_s:
                {
                    break;
                }
                case wasm1_code::i32_shr_u:
                {
                    break;
                }
                case wasm1_code::i32_rotl:
                {
                    break;
                }
                case wasm1_code::i32_rotr:
                {
                    break;
                }
                case wasm1_code::i64_clz:
                {
                    break;
                }
                case wasm1_code::i64_ctz:
                {
                    break;
                }
                case wasm1_code::i64_popcnt:
                {
                    break;
                }
                case wasm1_code::i64_add:
                {
                    break;
                }
                case wasm1_code::i64_sub:
                {
                    break;
                }
                case wasm1_code::i64_mul:
                {
                    break;
                }
                case wasm1_code::i64_div_s:
                {
                    break;
                }
                case wasm1_code::i64_div_u:
                {
                    break;
                }
                case wasm1_code::i64_rem_s:
                {
                    break;
                }
                case wasm1_code::i64_rem_u:
                {
                    break;
                }
                case wasm1_code::i64_and:
                {
                    break;
                }
                case wasm1_code::i64_or:
                {
                    break;
                }
                case wasm1_code::i64_xor:
                {
                    break;
                }
                case wasm1_code::i64_shl:
                {
                    break;
                }
                case wasm1_code::i64_shr_s:
                {
                    break;
                }
                case wasm1_code::i64_shr_u:
                {
                    break;
                }
                case wasm1_code::i64_rotl:
                {
                    break;
                }
                case wasm1_code::i64_rotr:
                {
                    break;
                }
                case wasm1_code::f32_abs:
                {
                    break;
                }
                case wasm1_code::f32_neg:
                {
                    break;
                }
                case wasm1_code::f32_ceil:
                {
                    break;
                }
                case wasm1_code::f32_floor:
                {
                    break;
                }
                case wasm1_code::f32_trunc:
                {
                    break;
                }
                case wasm1_code::f32_nearest:
                {
                    break;
                }
                case wasm1_code::f32_sqrt:
                {
                    break;
                }
                case wasm1_code::f32_add:
                {
                    break;
                }
                case wasm1_code::f32_sub:
                {
                    break;
                }
                case wasm1_code::f32_mul:
                {
                    break;
                }
                case wasm1_code::f32_div:
                {
                    break;
                }
                case wasm1_code::f32_min:
                {
                    break;
                }
                case wasm1_code::f32_max:
                {
                    break;
                }
                case wasm1_code::f32_copysign:
                {
                    break;
                }
                case wasm1_code::f64_abs:
                {
                    break;
                }
                case wasm1_code::f64_neg:
                {
                    break;
                }
                case wasm1_code::f64_ceil:
                {
                    break;
                }
                case wasm1_code::f64_floor:
                {
                    break;
                }
                case wasm1_code::f64_trunc:
                {
                    break;
                }
                case wasm1_code::f64_nearest:
                {
                    break;
                }
                case wasm1_code::f64_sqrt:
                {
                    break;
                }
                case wasm1_code::f64_add:
                {
                    break;
                }
                case wasm1_code::f64_sub:
                {
                    break;
                }
                case wasm1_code::f64_mul:
                {
                    break;
                }
                case wasm1_code::f64_div:
                {
                    break;
                }
                case wasm1_code::f64_min:
                {
                    break;
                }
                case wasm1_code::f64_max:
                {
                    break;
                }
                case wasm1_code::f64_copysign:
                {
                    break;
                }
                case wasm1_code::i32_wrap_i64:
                {
                    break;
                }
                case wasm1_code::i32_trunc_f32_s:
                {
                    break;
                }
                case wasm1_code::i32_trunc_f32_u:
                {
                    break;
                }
                case wasm1_code::i32_trunc_f64_s:
                {
                    break;
                }
                case wasm1_code::i32_trunc_f64_u:
                {
                    break;
                }
                case wasm1_code::i64_extend_i32_s:
                {
                    break;
                }
                case wasm1_code::i64_extend_i32_u:
                {
                    break;
                }
                case wasm1_code::i64_trunc_f32_s:
                {
                    break;
                }
                case wasm1_code::i64_trunc_f32_u:
                {
                    break;
                }
                case wasm1_code::i64_trunc_f64_s:
                {
                    break;
                }
                case wasm1_code::i64_trunc_f64_u:
                {
                    break;
                }
                case wasm1_code::f32_convert_i32_s:
                {
                    break;
                }
                case wasm1_code::f32_convert_i32_u:
                {
                    break;
                }
                case wasm1_code::f32_convert_i64_s:
                {
                    break;
                }
                case wasm1_code::f32_convert_i64_u:
                {
                    break;
                }
                case wasm1_code::f32_demote_f64:
                {
                    break;
                }
                case wasm1_code::f64_convert_i32_s:
                {
                    break;
                }
                case wasm1_code::f64_convert_i32_u:
                {
                    break;
                }
                case wasm1_code::f64_convert_i64_s:
                {
                    break;
                }
                case wasm1_code::f64_convert_i64_u:
                {
                    break;
                }
                case wasm1_code::f64_promote_f32:
                {
                    break;
                }
                case wasm1_code::i32_reinterpret_f32:
                {
                    break;
                }
                case wasm1_code::i64_reinterpret_f64:
                {
                    break;
                }
                case wasm1_code::f32_reinterpret_i32:
                {
                    break;
                }
                case wasm1_code::f64_reinterpret_i64:
                {
                    break;
                }
                [[unlikely]] default:
                {
                    err.err_curr = code_curr;
                    err.err_selectable.u8 = static_cast<::std::uint_least8_t>(curr_opbase);
                    err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_opbase;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    break;
                }
            }
        }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
