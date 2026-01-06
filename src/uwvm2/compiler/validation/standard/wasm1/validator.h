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

    using operand_stack_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;

    union operand_stack_storate_u
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 i32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 i64;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 f32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 f64;
    };

    struct operand_stack_storage_t
    {
        operand_stack_value_type type{};
    };

    using operand_stack_type = ::uwvm2::utils::container::vector<operand_stack_storage_t>;

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

        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(module_storage.sections)};

        auto const& curr_code{codesec.codes.index_unchecked(local_func_idx)};
        auto const& curr_code_locals{curr_code.locals};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_local_count{};
        for(auto const& local_part: curr_code_locals)
        {
            // all_local_count never overflow and never exceed the max of size_t
            all_local_count += local_part.count;
        }

        // stack
        operand_stack_type operand_stack{};
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

            // opbase [opextent|code] ...
            // [safe] unsafe (could be the section_end)
            // ^^ code_curr

            // switch the code
            wasm1_code curr_opbase;  // no initialize necessary
            ::std::memcpy(::std::addressof(curr_opbase), code_curr, sizeof(wasm1_code));

            switch(curr_opbase)
            {
                case wasm1_code::unreachable:
                {
                    // unreachable make operand_stack polymorphic

                    /// @todo
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

                    if(!is_polymorphic)
                    {
                        if(operand_stack.empty()) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"drop";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
                            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

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

                    if(!is_polymorphic)
                    {
                        if(operand_stack.size() < 3uz) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.operand_stack_underflow.op_code_name = u8"select";
                            err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
                            err.err_selectable.operand_stack_underflow.stack_size_required = 3uz;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::operand_stack_underflow;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        auto const cond{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        auto const v2{operand_stack.back_unchecked()};
                        operand_stack.pop_back_unchecked();

                        auto const v1{operand_stack.back_unchecked()};
                        // select need same type for v1 and v2, no necessary to pop and push back

                        if(cond.type != operand_stack_value_type::i32) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.select_cond_type_not_i32.cond_type = cond.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::select_cond_type_not_i32;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // select need same type for v1 and v2
                        if(v1.type != v2.type) [[unlikely]]
                        {
                            err.err_curr = op_begin;
                            err.err_selectable.select_type_mismatch.type_v1 = v1.type;
                            err.err_selectable.select_type_mismatch.type_v2 = v2.type;
                            err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::select_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }

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
                    auto tem_local_index{local_index};
                    ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...> curr_local_type{};

                    for(auto const& local_part: curr_code_locals)
                    {
                        if(tem_local_index < local_part.count)
                        {
                            curr_local_type = local_part.type;
                            tem_local_index = 0uz;
                            break;
                        }

                        tem_local_index -= local_part.count;
                    }

                    if(tem_local_index != 0uz) [[unlikely]]
                    {
                        err.err_curr = op_begin;
                        err.err_selectable.illegal_local_index.local_index = local_index;
                        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
                        err.err_code = ::uwvm2::compiler::validation::error::code_validation_error_code::illegal_local_index;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(!is_polymorphic) { operand_stack.push_back({curr_local_type}); }

                    break;
                }
                case wasm1_code::local_set:
                {
                    break;
                }
                case wasm1_code::local_tee:
                {
                    break;
                }
                case wasm1_code::global_get:
                {
                    break;
                }
                case wasm1_code::global_set:
                {
                    break;
                }
                case wasm1_code::i32_load:
                {
                    break;
                }
                case wasm1_code::i64_load:
                {
                    break;
                }
                case wasm1_code::f32_load:
                {
                    break;
                }
                case wasm1_code::f64_load:
                {
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
