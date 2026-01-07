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

        // memory
        auto const& memsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 2uz);
        auto const& imported_memories{importsec.importdesc.index_unchecked(2u)};
        auto const imported_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_memories.size())};
        auto const local_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(memsec.memories.size())};
        // all_memory_count never overflow and never exceed the max of u32 (validated by parser limits)
        auto const all_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_memory_count + local_memory_count)};

        // stack
        using curr_operand_stack_value_type = operand_stack_value_type<Fs...>;
        using curr_operand_stack_type = operand_stack_type<Fs...>;
        curr_operand_stack_type operand_stack{};
        bool is_polymorphic{};

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
                    /// @todo
                    break;
                }
                case wasm1_code::loop:
                {
                    /// @todo
                    break;
                }
                case wasm1_code::if_:
                {
                    /// @todo
                    break;
                }
                case wasm1_code::else_:
                {
                    /// @todo
                    break;
                }
                case wasm1_code::end:
                {
                    /// @todo
                    break;
                }
                case wasm1_code::br:
                {
                    /// @todo
                    break;
                }
                case wasm1_code::br_if:
                {
                    /// @todo
                    break;
                }
                case wasm1_code::br_table:
                {
                    /// @todo
                    break;
                }
                case wasm1_code::return_:
                {
                    /// @todo
                    break;
                }
                case wasm1_code::call:
                {
                    /// @todo
                    break;
                }
                case wasm1_code::call_indirect:
                {
                    /// @todo
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
                    break;
                }
                case wasm1_code::i32_load8_u:
                {
                    break;
                }
                case wasm1_code::i32_load16_s:
                {
                    break;
                }
                case wasm1_code::i32_load16_u:
                {
                    break;
                }
                case wasm1_code::i64_load8_s:
                {
                    break;
                }
                case wasm1_code::i64_load8_u:
                {
                    break;
                }
                case wasm1_code::i64_load16_s:
                {
                    break;
                }
                case wasm1_code::i64_load16_u:
                {
                    break;
                }
                case wasm1_code::i64_load32_s:
                {
                    break;
                }
                case wasm1_code::i64_load32_u:
                {
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
