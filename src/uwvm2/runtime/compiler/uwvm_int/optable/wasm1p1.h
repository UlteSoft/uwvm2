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
# include <bit>
# include <concepts>
# include <cmath>
# include <limits>
# include <memory>
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
# include <uwvm2/object/impl.h>
# include <uwvm2/uwvm/runtime/storage/wasm_module.h>
# include "define.h"
# include "convert.h"
# include "storage.h"
# include "memory.h"
# include "register_ring.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
# if !(__cpp_pack_indexing >= 202311L)
#  error "UWVM requires at least C++26 standard compiler. See https://en.cppreference.com/w/cpp/feature_test#cpp_pack_indexing"
# endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    namespace wasm1p1_details
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using wasm_v128 = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;
        using wasm_funcref = ::uwvm2::object::global::wasm_funcref_t;
        using wasm_externref = ::uwvm2::object::global::wasm_externref_t;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;
        using runtime_table_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t;
        using runtime_table_elem_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t;
        using runtime_table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
        using runtime_data_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t;
        using runtime_element_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_element_storage_t;
        using runtime_module_storage_t = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;

        template <typename T>
        UWVM_ALWAYS_INLINE inline constexpr T read_imm(::std::byte const*& ip) noexcept
        {
            T v;  // no init
            ::std::memcpy(::std::addressof(v), ip, sizeof(v));
            ip += sizeof(v);
            return v;
        }

        UWVM_GNU_COLD [[noreturn]] inline constexpr void table_oob_terminate() noexcept
        {
            if(::uwvm2::runtime::compiler::uwvm_int::optable::trap_table_out_of_bounds_func == nullptr) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                ::fast_io::fast_terminate();
            }

            ::uwvm2::runtime::compiler::uwvm_int::optable::trap_table_out_of_bounds_func();
            ::fast_io::fast_terminate();
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::uint_least32_t i32_to_u32(wasm_i32 v) noexcept
        { return ::uwvm2::runtime::compiler::uwvm_int::optable::details::to_u32_bits(v); }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_i32 u32_to_i32(::std::uint_least32_t v) noexcept
        { return ::uwvm2::runtime::compiler::uwvm_int::optable::details::from_u32_bits<wasm_i32>(v); }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr runtime_table_elem_storage_t
            resolve_table_elem_from_func_index(runtime_module_storage_t const* module, wasm_u32 func_index) noexcept
        {
            if(module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const idx{static_cast<::std::size_t>(func_index)};
            auto const imported_count{module->imported_function_vec_storage.size()};
            auto const local_count{module->local_defined_function_vec_storage.size()};
            if(idx >= imported_count + local_count) [[unlikely]] { ::fast_io::fast_terminate(); }

            runtime_table_elem_storage_t out{};
            if(idx < imported_count)
            {
                out.storage.imported_ptr = ::std::addressof(module->imported_function_vec_storage.index_unchecked(idx));
                out.type = runtime_table_elem_type::func_ref_imported;
            }
            else
            {
                out.storage.defined_ptr = ::std::addressof(module->local_defined_function_vec_storage.index_unchecked(idx - imported_count));
                out.type = runtime_table_elem_type::func_ref_defined;
            }
            return out;
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_funcref funcref_from_table_elem(runtime_table_elem_storage_t const& elem) noexcept
        {
            wasm_funcref out{};
            switch(elem.type)
            {
                case runtime_table_elem_type::func_ref_imported:
                {
                    auto const ptr{elem.storage.imported_ptr};
                    if(ptr == nullptr)
                    {
                        out.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_null;
                        return out;
                    }
                    out.ref.storage.ptr = const_cast<void*>(static_cast<void const*>(ptr));
                    out.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_func_imported;
                    return out;
                }
                case runtime_table_elem_type::func_ref_defined:
                {
                    auto const ptr{elem.storage.defined_ptr};
                    if(ptr == nullptr)
                    {
                        out.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_null;
                        return out;
                    }
                    out.ref.storage.ptr = const_cast<void*>(static_cast<void const*>(ptr));
                    out.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_func_defined;
                    return out;
                }
                [[unlikely]] default:
                {
                    ::fast_io::fast_terminate();
                }
            }
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr runtime_table_elem_storage_t table_elem_from_funcref(runtime_module_storage_t const* module,
                                                                                                               wasm_funcref const& ref) noexcept
        {
            runtime_table_elem_storage_t out{};
            switch(ref.ref.kind)
            {
                case ::uwvm2::object::global::wasm_ref_kind::wasm_null:
                {
                    return out;
                }
                case ::uwvm2::object::global::wasm_ref_kind::wasm_func:
                {
                    return resolve_table_elem_from_func_index(module, ref.ref.storage.func_idx);
                }
                case ::uwvm2::object::global::wasm_ref_kind::wasm_func_imported:
                {
                    out.storage.imported_ptr = static_cast<::uwvm2::uwvm::runtime::storage::imported_function_storage_t const*>(ref.ref.storage.ptr);
                    if(out.storage.imported_ptr == nullptr) { return {}; }
                    out.type = runtime_table_elem_type::func_ref_imported;
                    return out;
                }
                case ::uwvm2::object::global::wasm_ref_kind::wasm_func_defined:
                {
                    out.storage.defined_ptr = static_cast<::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const*>(ref.ref.storage.ptr);
                    if(out.storage.defined_ptr == nullptr) { return {}; }
                    out.type = runtime_table_elem_type::func_ref_defined;
                    return out;
                }
                [[unlikely]] default:
                {
                    ::fast_io::fast_terminate();
                }
            }
        }

        UWVM_ALWAYS_INLINE inline constexpr bool range_oob(::std::size_t begin, ::std::size_t len, ::std::size_t bound) noexcept
        { return begin > bound || len > bound - begin; }

        UWVM_ALWAYS_INLINE inline constexpr void check_table_range(::std::size_t begin, ::std::size_t len, ::std::size_t bound) noexcept
        {
            if(range_oob(begin, len, bound)) [[unlikely]] { table_oob_terminate(); }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval bool stacktop_enabled_for() noexcept
        {
            if constexpr(::std::same_as<OperandT, wasm_i32>) { return CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_i64>) { return CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f32>) { return CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f64>) { return CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_v128>) { return CompileOption.v128_stack_top_begin_pos != CompileOption.v128_stack_top_end_pos; }
            else
            {
                return false;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval ::std::size_t stacktop_begin_pos() noexcept
        {
            if constexpr(::std::same_as<OperandT, wasm_i32>) { return CompileOption.i32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_i64>) { return CompileOption.i64_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f32>) { return CompileOption.f32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f64>) { return CompileOption.f64_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_v128>) { return CompileOption.v128_stack_top_begin_pos; }
            else
            {
                return SIZE_MAX;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval ::std::size_t stacktop_end_pos() noexcept
        {
            if constexpr(::std::same_as<OperandT, wasm_i32>) { return CompileOption.i32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_i64>) { return CompileOption.i64_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f32>) { return CompileOption.f32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f64>) { return CompileOption.f64_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_v128>) { return CompileOption.v128_stack_top_end_pos; }
            else
            {
                return SIZE_MAX;
            }
        }

        template <typename ValueT, typename NarrowSignedT>
        UWVM_ALWAYS_INLINE inline constexpr ValueT sign_extend_low_bits(ValueT v) noexcept
        {
            using narrow_unsigned_t = ::std::make_unsigned_t<NarrowSignedT>;

            if constexpr(::std::same_as<ValueT, wasm_i32>)
            {
                auto const bits{::uwvm2::runtime::compiler::uwvm_int::optable::details::to_u32_bits(v)};
                narrow_unsigned_t const low{static_cast<narrow_unsigned_t>(bits)};
                NarrowSignedT const narrowed{::std::bit_cast<NarrowSignedT>(low)};
                return static_cast<wasm_i32>(static_cast<::std::int_least32_t>(narrowed));
            }
            else
            {
                static_assert(::std::same_as<ValueT, wasm_i64>);
                auto const bits{::uwvm2::runtime::compiler::uwvm_int::optable::details::to_u64_bits(v)};
                narrow_unsigned_t const low{static_cast<narrow_unsigned_t>(bits)};
                NarrowSignedT const narrowed{::std::bit_cast<NarrowSignedT>(low)};
                return static_cast<wasm_i64>(static_cast<::std::int_least64_t>(narrowed));
            }
        }

        template <typename WasmOutT, typename FloatT>
        UWVM_ALWAYS_INLINE inline constexpr WasmOutT trunc_sat_signed(FloatT x) noexcept
        {
            using int_out_t = ::std::conditional_t<::std::same_as<WasmOutT, wasm_i32>, ::std::int_least32_t, ::std::int_least64_t>;

            if(x != x) [[unlikely]] { return WasmOutT{}; }

            constexpr FloatT min_v{static_cast<FloatT>(::std::numeric_limits<int_out_t>::min())};
            constexpr FloatT max_plus_one{static_cast<FloatT>(static_cast<long double>(::std::numeric_limits<int_out_t>::max()) + 1.0L)};

            if(x <= min_v) [[unlikely]] { return static_cast<WasmOutT>((::std::numeric_limits<int_out_t>::min)()); }
            if(x >= max_plus_one) [[unlikely]] { return static_cast<WasmOutT>((::std::numeric_limits<int_out_t>::max)()); }
            return static_cast<WasmOutT>(static_cast<int_out_t>(x));
        }

        template <typename WasmOutT, typename FloatT>
        UWVM_ALWAYS_INLINE inline constexpr WasmOutT trunc_sat_unsigned(FloatT x) noexcept
        {
            using uint_out_t = ::std::conditional_t<::std::same_as<WasmOutT, wasm_i32>, ::std::uint_least32_t, ::std::uint_least64_t>;

            if(x != x || x <= static_cast<FloatT>(0)) [[unlikely]] { return WasmOutT{}; }

            constexpr FloatT max_plus_one{static_cast<FloatT>(static_cast<long double>((::std::numeric_limits<uint_out_t>::max)()) + 1.0L)};
            uint_out_t out{};
            if(x >= max_plus_one) [[unlikely]] { out = (::std::numeric_limits<uint_out_t>::max)(); }
            else
            {
                out = static_cast<uint_out_t>(x);
            }

            if constexpr(::std::same_as<WasmOutT, wasm_i32>)
            {
                return ::uwvm2::runtime::compiler::uwvm_int::optable::details::from_u32_bits<WasmOutT>(static_cast<::std::uint_least32_t>(out));
            }
            else
            {
                return ::uwvm2::runtime::compiler::uwvm_int::optable::details::from_u64_bits<WasmOutT>(static_cast<::std::uint_least64_t>(out));
            }
        }

        template <typename WasmOutT, bool Signed, typename FloatT>
        UWVM_ALWAYS_INLINE inline constexpr WasmOutT trunc_sat(FloatT x) noexcept
        {
            if constexpr(Signed) { return trunc_sat_signed<WasmOutT>(x); }
            else
            {
                return trunc_sat_unsigned<WasmOutT>(x);
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename OutT, ::std::size_t curr_out_stack_top, uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr void push_out_value(OutT const& out, Type&... type) noexcept
        {
            if constexpr(stacktop_enabled_for<CompileOption, OutT>())
            {
                constexpr ::std::size_t out_begin{stacktop_begin_pos<CompileOption, OutT>()};
                constexpr ::std::size_t out_end{stacktop_end_pos<CompileOption, OutT>()};
                static_assert(out_begin <= curr_out_stack_top && curr_out_stack_top < out_end);
                constexpr ::std::size_t new_pos{details::ring_prev_pos(curr_out_stack_top, out_begin, out_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, OutT, new_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        template <uwvm_int_stack_top_type... Type>
        UWVM_ALWAYS_INLINE inline constexpr void tail_next(::std::byte const*& opcurr, Type... type) UWVM_THROWS
        {
            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), opcurr, sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }
    }  // namespace wasm1p1_details

    template <uwvm_interpreter_translate_option_t CompileOption,
              typename ValueT,
              typename NarrowSignedT,
              ::std::size_t curr_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_sign_extend_typed(Type... type) UWVM_THROWS
    {
        static_assert(::std::same_as<ValueT, wasm1p1_details::wasm_i32> || ::std::same_as<ValueT, wasm1p1_details::wasm_i64>);
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(wasm1p1_details::stacktop_enabled_for<CompileOption, ValueT>())
        {
            constexpr ::std::size_t range_begin{wasm1p1_details::stacktop_begin_pos<CompileOption, ValueT>()};
            constexpr ::std::size_t range_end{wasm1p1_details::stacktop_end_pos<CompileOption, ValueT>()};
            static_assert(range_begin <= curr_stack_top && curr_stack_top < range_end);

            ValueT const v{get_curr_val_from_operand_stack_top<CompileOption, ValueT, curr_stack_top>(type...)};
            ValueT const out{wasm1p1_details::sign_extend_low_bits<ValueT, NarrowSignedT>(v)};
            details::set_curr_val_to_stacktop_cache<CompileOption, ValueT, curr_stack_top>(out, type...);
        }
        else
        {
            ValueT const v{get_curr_val_from_operand_stack_cache<ValueT>(type...)};
            ValueT const out{wasm1p1_details::sign_extend_low_bits<ValueT, NarrowSignedT>(v)};
            ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
            type...[1u] += sizeof(out);
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, typename ValueT, typename NarrowSignedT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_sign_extend_typed(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        ValueT const v{get_curr_val_from_operand_stack_cache<ValueT>(typeref...)};
        ValueT const out{wasm1p1_details::sign_extend_low_bits<ValueT, NarrowSignedT>(v)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption,
              typename FloatT,
              typename WasmOutT,
              bool Signed,
              ::std::size_t curr_in_stack_top,
              ::std::size_t curr_out_stack_top = curr_in_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_trunc_sat_typed(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(wasm1p1_details::stacktop_enabled_for<CompileOption, FloatT>())
        {
            constexpr ::std::size_t in_begin{wasm1p1_details::stacktop_begin_pos<CompileOption, FloatT>()};
            constexpr ::std::size_t in_end{wasm1p1_details::stacktop_end_pos<CompileOption, FloatT>()};
            static_assert(in_begin <= curr_in_stack_top && curr_in_stack_top < in_end);

            FloatT const v{get_curr_val_from_operand_stack_top<CompileOption, FloatT, curr_in_stack_top>(type...)};
            WasmOutT const out{wasm1p1_details::trunc_sat<WasmOutT, Signed>(v)};

            if constexpr(wasm1p1_details::stacktop_enabled_for<CompileOption, WasmOutT>())
            {
                constexpr ::std::size_t out_begin{wasm1p1_details::stacktop_begin_pos<CompileOption, WasmOutT>()};
                constexpr ::std::size_t out_end{wasm1p1_details::stacktop_end_pos<CompileOption, WasmOutT>()};
                if constexpr(in_begin == out_begin && in_end == out_end)
                {
                    static_assert(curr_in_stack_top == curr_out_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, WasmOutT, curr_in_stack_top>(out, type...);
                }
                else
                {
                    static_assert(out_begin <= curr_out_stack_top && curr_out_stack_top < out_end);
                    constexpr ::std::size_t new_out_pos{details::ring_prev_pos(curr_out_stack_top, out_begin, out_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, WasmOutT, new_out_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            FloatT const v{get_curr_val_from_operand_stack_cache<FloatT>(type...)};
            WasmOutT const out{wasm1p1_details::trunc_sat<WasmOutT, Signed>(v)};
            wasm1p1_details::push_out_value<CompileOption, WasmOutT, curr_out_stack_top>(out, type...);
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, typename FloatT, typename WasmOutT, bool Signed, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_trunc_sat_typed(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        FloatT const v{get_curr_val_from_operand_stack_cache<FloatT>(typeref...)};
        WasmOutT const out{wasm1p1_details::trunc_sat<WasmOutT, Signed>(v)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    namespace wasm1p1_simd_details
    {
        using wasm_i32 = wasm1p1_details::wasm_i32;
        using wasm_f32 = wasm1p1_details::wasm_f32;
        using wasm_u32 = wasm1p1_details::wasm_u32;
        using wasm_v128 = wasm1p1_details::wasm_v128;
        using native_memory_t = wasm1p1_details::native_memory_t;

        static_assert(sizeof(wasm_i32) == 4uz);
        static_assert(sizeof(wasm_u32) == 4uz);
        static_assert(sizeof(wasm_f32) == 4uz);
        static_assert(sizeof(wasm_v128) == 16uz);

        template <typename LaneT, ::std::size_t N>
        struct lane_array
        {
            LaneT lane[N];
        };

        enum class v128_unop
        {
            not_,
            f32x4_convert_i32x4_s,
            f32x4_convert_i32x4_u
        };

        enum class v128_binop
        {
            and_,
            andnot,
            or_,
            xor_,
            i32x4_add,
            i32x4_sub,
            i32x4_mul,
            i32x4_eq,
            f32x4_add,
            f32x4_sub,
            f32x4_mul,
            f32x4_eq
        };

        enum class v128_testop
        {
            any_true,
            i32x4_all_true
        };

        enum class v128_splatop
        {
            i32x4,
            f32x4
        };

        template <typename UIntT, ::std::size_t N>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr lane_array<UIntT, N> load_uint_lanes(wasm_v128 v) noexcept
        {
            static_assert(sizeof(UIntT) * N == sizeof(wasm_v128));
            lane_array<UIntT, N> out{};              // init
            ::std::byte bytes[sizeof(wasm_v128)]{};  // init
            ::std::memcpy(bytes, ::std::addressof(v), sizeof(bytes));
            for(::std::size_t i{}; i != N; ++i)
            {
                UIntT lane{};  // init
                ::std::memcpy(::std::addressof(lane), bytes + i * sizeof(UIntT), sizeof(UIntT));
                out.lane[i] = ::fast_io::little_endian(lane);
            }
            return out;
        }

        template <typename UIntT, ::std::size_t N>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 store_uint_lanes(lane_array<UIntT, N> const& lanes) noexcept
        {
            static_assert(sizeof(UIntT) * N == sizeof(wasm_v128));
            ::std::byte bytes[sizeof(wasm_v128)]{};  // init
            for(::std::size_t i{}; i != N; ++i)
            {
                auto lane{::fast_io::little_endian(lanes.lane[i])};
                ::std::memcpy(bytes + i * sizeof(UIntT), ::std::addressof(lane), sizeof(UIntT));
            }

            wasm_v128 out;  // no init
            ::std::memcpy(::std::addressof(out), bytes, sizeof(out));
            return out;
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr lane_array<wasm_f32, 4uz> load_f32x4_lanes(wasm_v128 v) noexcept
        {
            auto const bits{load_uint_lanes<wasm_u32, 4uz>(v)};
            lane_array<wasm_f32, 4uz> out{};  // init
            for(::std::size_t i{}; i != 4uz; ++i) { out.lane[i] = ::std::bit_cast<wasm_f32>(bits.lane[i]); }
            return out;
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 store_f32x4_lanes(lane_array<wasm_f32, 4uz> const& lanes) noexcept
        {
            lane_array<wasm_u32, 4uz> bits{};  // init
            for(::std::size_t i{}; i != 4uz; ++i) { bits.lane[i] = ::std::bit_cast<wasm_u32>(lanes.lane[i]); }
            return store_uint_lanes<wasm_u32, 4uz>(bits);
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 v128_bitwise_not(wasm_v128 v) noexcept
        {
            auto bytes{load_uint_lanes<::std::uint_least8_t, 16uz>(v)};
            for(::std::size_t i{}; i != 16uz; ++i) { bytes.lane[i] = static_cast<::std::uint_least8_t>(~bytes.lane[i]); }
            return store_uint_lanes<::std::uint_least8_t, 16uz>(bytes);
        }

        template <v128_binop Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_v128_binop(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
            if constexpr(Op == v128_binop::and_ || Op == v128_binop::andnot || Op == v128_binop::or_ || Op == v128_binop::xor_)
            {
                auto l{load_uint_lanes<::std::uint_least8_t, 16uz>(lhs)};
                auto const r{load_uint_lanes<::std::uint_least8_t, 16uz>(rhs)};
                for(::std::size_t i{}; i != 16uz; ++i)
                {
                    if constexpr(Op == v128_binop::and_) { l.lane[i] = static_cast<::std::uint_least8_t>(l.lane[i] & r.lane[i]); }
                    else if constexpr(Op == v128_binop::andnot) { l.lane[i] = static_cast<::std::uint_least8_t>(l.lane[i] & ~r.lane[i]); }
                    else if constexpr(Op == v128_binop::or_) { l.lane[i] = static_cast<::std::uint_least8_t>(l.lane[i] | r.lane[i]); }
                    else
                    {
                        l.lane[i] = static_cast<::std::uint_least8_t>(l.lane[i] ^ r.lane[i]);
                    }
                }
                return store_uint_lanes<::std::uint_least8_t, 16uz>(l);
            }
            else if constexpr(Op == v128_binop::i32x4_add || Op == v128_binop::i32x4_sub || Op == v128_binop::i32x4_mul || Op == v128_binop::i32x4_eq)
            {
                auto l{load_uint_lanes<wasm_u32, 4uz>(lhs)};
                auto const r{load_uint_lanes<wasm_u32, 4uz>(rhs)};
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if constexpr(Op == v128_binop::i32x4_add) { l.lane[i] = static_cast<wasm_u32>(l.lane[i] + r.lane[i]); }
                    else if constexpr(Op == v128_binop::i32x4_sub) { l.lane[i] = static_cast<wasm_u32>(l.lane[i] - r.lane[i]); }
                    else if constexpr(Op == v128_binop::i32x4_mul) { l.lane[i] = static_cast<wasm_u32>(l.lane[i] * r.lane[i]); }
                    else
                    {
                        l.lane[i] = l.lane[i] == r.lane[i] ? static_cast<wasm_u32>(0xFFFFFFFFu) : wasm_u32{};
                    }
                }
                return store_uint_lanes<wasm_u32, 4uz>(l);
            }
            else
            {
                auto l{load_f32x4_lanes(lhs)};
                auto const r{load_f32x4_lanes(rhs)};
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if constexpr(Op == v128_binop::f32x4_add) { l.lane[i] = static_cast<wasm_f32>(l.lane[i] + r.lane[i]); }
                    else if constexpr(Op == v128_binop::f32x4_sub) { l.lane[i] = static_cast<wasm_f32>(l.lane[i] - r.lane[i]); }
                    else if constexpr(Op == v128_binop::f32x4_mul) { l.lane[i] = static_cast<wasm_f32>(l.lane[i] * r.lane[i]); }
                    else
                    {
                        lane_array<wasm_u32, 4uz> out{};  // init
                        for(::std::size_t j{}; j != 4uz; ++j) { out.lane[j] = l.lane[j] == r.lane[j] ? static_cast<wasm_u32>(0xFFFFFFFFu) : wasm_u32{}; }
                        return store_uint_lanes<wasm_u32, 4uz>(out);
                    }
                }
                return store_f32x4_lanes(l);
            }
        }

        template <v128_unop Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_v128_unop(wasm_v128 v) noexcept
        {
            if constexpr(Op == v128_unop::not_) { return v128_bitwise_not(v); }
            else
            {
                auto const lanes{load_uint_lanes<wasm_u32, 4uz>(v)};
                lane_array<wasm_f32, 4uz> out{};  // init
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if constexpr(Op == v128_unop::f32x4_convert_i32x4_s) { out.lane[i] = static_cast<wasm_f32>(::std::bit_cast<wasm_i32>(lanes.lane[i])); }
                    else
                    {
                        out.lane[i] = static_cast<wasm_f32>(lanes.lane[i]);
                    }
                }
                return store_f32x4_lanes(out);
            }
        }

        template <v128_testop Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_i32 eval_v128_testop(wasm_v128 v) noexcept
        {
            if constexpr(Op == v128_testop::any_true)
            {
                auto const bytes{load_uint_lanes<::std::uint_least8_t, 16uz>(v)};
                for(::std::size_t i{}; i != 16uz; ++i)
                {
                    if(bytes.lane[i] != 0u) { return wasm_i32{1}; }
                }
                return wasm_i32{};
            }
            else
            {
                auto const lanes{load_uint_lanes<wasm_u32, 4uz>(v)};
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if(lanes.lane[i] == 0u) { return wasm_i32{}; }
                }
                return wasm_i32{1};
            }
        }

        template <v128_splatop Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_v128_splat_i32(wasm_i32 v) noexcept
        {
            lane_array<wasm_u32, 4uz> lanes{};  // init
            auto const bits{::std::bit_cast<wasm_u32>(v)};
            for(::std::size_t i{}; i != 4uz; ++i) { lanes.lane[i] = bits; }
            return store_uint_lanes<wasm_u32, 4uz>(lanes);
        }

        template <v128_splatop Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_v128_splat_f32(wasm_f32 v) noexcept
        {
            lane_array<wasm_f32, 4uz> lanes{};  // init
            for(::std::size_t i{}; i != 4uz; ++i) { lanes.lane[i] = v; }
            return store_f32x4_lanes(lanes);
        }

        template <::std::size_t Lane>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_f32 eval_f32x4_extract_lane(wasm_v128 v) noexcept
        {
            static_assert(Lane < 4uz);
            return load_f32x4_lanes(v).lane[Lane];
        }

        UWVM_ALWAYS_INLINE inline constexpr void push_v128_to_memory_stack(wasm_v128 const& out, ::std::byte*& stack_top) noexcept
        {
            ::std::memcpy(stack_top, ::std::addressof(out), sizeof(out));
            stack_top += sizeof(out);
        }

        UWVM_ALWAYS_INLINE inline constexpr void push_i32_to_memory_stack(wasm_i32 const& out, ::std::byte*& stack_top) noexcept
        {
            ::std::memcpy(stack_top, ::std::addressof(out), sizeof(out));
            stack_top += sizeof(out);
        }

        UWVM_ALWAYS_INLINE inline constexpr void push_f32_to_memory_stack(wasm_f32 const& out, ::std::byte*& stack_top) noexcept
        {
            ::std::memcpy(stack_top, ::std::addressof(out), sizeof(out));
            stack_top += sizeof(out);
        }

        template <typename T>
        UWVM_ALWAYS_INLINE inline constexpr void push_to_memory_stack(T const& out, ::std::byte*& stack_top) noexcept
        {
            ::std::memcpy(stack_top, ::std::addressof(out), sizeof(out));
            stack_top += sizeof(out);
        }

        using wasm_i64 = wasm1p1_details::wasm_i64;
        using wasm_f64 = wasm1p1_details::wasm_f64;
        using simd_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_simd;

        using u8 = ::std::uint8_t;
        using u16 = ::std::uint16_t;
        using u32 = ::std::uint32_t;
        using u64 = ::std::uint64_t;
        using s8 = ::std::int8_t;
        using s16 = ::std::int16_t;
        using s32 = ::std::int32_t;
        using s64 = ::std::int64_t;

        static_assert(sizeof(wasm_i64) == 8uz);
        static_assert(sizeof(wasm_f64) == 8uz);
        static_assert(sizeof(u8) == 1uz);
        static_assert(sizeof(u16) == 2uz);
        static_assert(sizeof(u32) == 4uz);
        static_assert(sizeof(u64) == 8uz);

        template <typename U>
        struct signed_lane;

        template <>
        struct signed_lane<u8>
        {
            using type = s8;
        };

        template <>
        struct signed_lane<u16>
        {
            using type = s16;
        };

        template <>
        struct signed_lane<u32>
        {
            using type = s32;
        };

        template <>
        struct signed_lane<u64>
        {
            using type = s64;
        };

        template <typename U>
        using signed_lane_t = typename signed_lane<U>::type;

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr signed_lane_t<U> as_signed_lane(U v) noexcept
        { return ::std::bit_cast<signed_lane_t<U>>(v); }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U from_signed_lane(signed_lane_t<U> v) noexcept
        { return ::std::bit_cast<U>(v); }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U lane_true() noexcept
        { return static_cast<U>((::std::numeric_limits<U>::max)()); }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U lane_bool(bool v) noexcept
        { return v ? lane_true<U>() : U{}; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr lane_array<wasm_f64, 2uz> load_f64x2_lanes(wasm_v128 v) noexcept
        {
            auto const bits{load_uint_lanes<u64, 2uz>(v)};
            lane_array<wasm_f64, 2uz> out{};  // init
            for(::std::size_t i{}; i != 2uz; ++i) { out.lane[i] = ::std::bit_cast<wasm_f64>(bits.lane[i]); }
            return out;
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 store_f64x2_lanes(lane_array<wasm_f64, 2uz> const& lanes) noexcept
        {
            lane_array<u64, 2uz> bits{};  // init
            for(::std::size_t i{}; i != 2uz; ++i) { bits.lane[i] = ::std::bit_cast<u64>(lanes.lane[i]); }
            return store_uint_lanes<u64, 2uz>(bits);
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U abs_signed_bits(U v) noexcept
        {
            auto const s{as_signed_lane<U>(v)};
            return s < 0 ? static_cast<U>(U{} - v) : v;
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U neg_bits(U v) noexcept
        { return static_cast<U>(U{} - v); }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U min_signed_bits(U a, U b) noexcept
        { return as_signed_lane<U>(a) < as_signed_lane<U>(b) ? a : b; }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U max_signed_bits(U a, U b) noexcept
        { return as_signed_lane<U>(a) > as_signed_lane<U>(b) ? a : b; }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U sat_add_unsigned(U a, U b) noexcept
        {
            U const out{static_cast<U>(a + b)};
            return out < a ? lane_true<U>() : out;
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U sat_sub_unsigned(U a, U b) noexcept
        { return a < b ? U{} : static_cast<U>(a - b); }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U sat_add_signed(U a, U b) noexcept
        {
            using S = signed_lane_t<U>;
            using Wide = ::std::conditional_t<(sizeof(U) <= 2uz), ::std::int_least32_t, ::std::int_least64_t>;
            auto const sum{static_cast<Wide>(as_signed_lane<U>(a)) + static_cast<Wide>(as_signed_lane<U>(b))};
            auto const lo{static_cast<Wide>((::std::numeric_limits<S>::min)())};
            auto const hi{static_cast<Wide>((::std::numeric_limits<S>::max)())};
            auto const clamped{sum < lo ? lo : (sum > hi ? hi : sum)};
            return from_signed_lane<U>(static_cast<S>(clamped));
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U sat_sub_signed(U a, U b) noexcept
        {
            using S = signed_lane_t<U>;
            using Wide = ::std::conditional_t<(sizeof(U) <= 2uz), ::std::int_least32_t, ::std::int_least64_t>;
            auto const diff{static_cast<Wide>(as_signed_lane<U>(a)) - static_cast<Wide>(as_signed_lane<U>(b))};
            auto const lo{static_cast<Wide>((::std::numeric_limits<S>::min)())};
            auto const hi{static_cast<Wide>((::std::numeric_limits<S>::max)())};
            auto const clamped{diff < lo ? lo : (diff > hi ? hi : diff)};
            return from_signed_lane<U>(static_cast<S>(clamped));
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr u16 q15mulr_sat_s(u16 a, u16 b) noexcept
        {
            auto const lhs{static_cast<::std::int_least32_t>(as_signed_lane<u16>(a))};
            auto const rhs{static_cast<::std::int_least32_t>(as_signed_lane<u16>(b))};
            auto out{(lhs * rhs + 0x4000) >> 15};
            if(out > static_cast<::std::int_least32_t>((::std::numeric_limits<s16>::max)()))
            {
                out = static_cast<::std::int_least32_t>((::std::numeric_limits<s16>::max)());
            }
            if(out < static_cast<::std::int_least32_t>((::std::numeric_limits<s16>::min)()))
            {
                out = static_cast<::std::int_least32_t>((::std::numeric_limits<s16>::min)());
            }
            return from_signed_lane<u16>(static_cast<s16>(out));
        }

        template <typename OutU, typename InU>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr OutU narrow_signed_lane(InU v) noexcept
        {
            using OutS = signed_lane_t<OutU>;
            using Wide = signed_lane_t<InU>;
            auto const x{static_cast<Wide>(as_signed_lane<InU>(v))};
            auto const lo{static_cast<Wide>((::std::numeric_limits<OutS>::min)())};
            auto const hi{static_cast<Wide>((::std::numeric_limits<OutS>::max)())};
            auto const clamped{x < lo ? lo : (x > hi ? hi : x)};
            return from_signed_lane<OutU>(static_cast<OutS>(clamped));
        }

        template <typename OutU, typename InU>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr OutU narrow_unsigned_lane(InU v) noexcept
        {
            using InS = signed_lane_t<InU>;
            auto const x{static_cast<InS>(as_signed_lane<InU>(v))};
            if(x <= 0) { return OutU{}; }
            auto const hi{static_cast<::std::uint_least64_t>((::std::numeric_limits<OutU>::max)())};
            auto const ux{static_cast<::std::uint_least64_t>(x)};
            return static_cast<OutU>(ux > hi ? hi : ux);
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U shl_lane(U v, wasm_i32 count) noexcept
        {
            constexpr unsigned bits{static_cast<unsigned>(sizeof(U) * 8uz)};
            auto const sh{static_cast<unsigned>(::std::bit_cast<u32>(count)) & (bits - 1u)};
            return static_cast<U>(v << sh);
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U shr_u_lane(U v, wasm_i32 count) noexcept
        {
            constexpr unsigned bits{static_cast<unsigned>(sizeof(U) * 8uz)};
            auto const sh{static_cast<unsigned>(::std::bit_cast<u32>(count)) & (bits - 1u)};
            return static_cast<U>(v >> sh);
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U shr_s_lane(U v, wasm_i32 count) noexcept
        {
            constexpr unsigned bits{static_cast<unsigned>(sizeof(U) * 8uz)};
            auto const sh{static_cast<unsigned>(::std::bit_cast<u32>(count)) & (bits - 1u)};
            if(sh == 0u) { return v; }
            U const sign{static_cast<U>(U{1} << (bits - 1u))};
            if((v & sign) == 0u) { return static_cast<U>(v >> sh); }
            return static_cast<U>((v >> sh) | static_cast<U>(lane_true<U>() << (bits - sh)));
        }

        template <typename FloatT>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline FloatT wasm_float_min(FloatT a, FloatT b) noexcept
        {
            if(::std::isnan(a) || ::std::isnan(b)) { return (::std::numeric_limits<FloatT>::quiet_NaN)(); }
            if(a == b)
            {
                if(::std::signbit(a)) { return a; }
                return b;
            }
            return a < b ? a : b;
        }

        template <typename FloatT>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline FloatT wasm_float_max(FloatT a, FloatT b) noexcept
        {
            if(::std::isnan(a) || ::std::isnan(b)) { return (::std::numeric_limits<FloatT>::quiet_NaN)(); }
            if(a == b)
            {
                if(::std::signbit(a)) { return b; }
                return a;
            }
            return a > b ? a : b;
        }

        template <typename FloatT>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline FloatT wasm_float_pmin(FloatT a, FloatT b) noexcept
        { return b < a ? b : a; }

        template <typename FloatT>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline FloatT wasm_float_pmax(FloatT a, FloatT b) noexcept
        { return a < b ? b : a; }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_splat_i32(wasm_i32 v) noexcept
        {
            auto const bits{::std::bit_cast<u32>(v)};
            if constexpr(Op == simd_code::i8x16_splat)
            {
                lane_array<u8, 16uz> lanes{};  // init
                for(auto& lane: lanes.lane) { lane = static_cast<u8>(bits); }
                return store_uint_lanes<u8, 16uz>(lanes);
            }
            else if constexpr(Op == simd_code::i16x8_splat)
            {
                lane_array<u16, 8uz> lanes{};  // init
                for(auto& lane: lanes.lane) { lane = static_cast<u16>(bits); }
                return store_uint_lanes<u16, 8uz>(lanes);
            }
            else
            {
                static_assert(Op == simd_code::i32x4_splat);
                lane_array<u32, 4uz> lanes{};  // init
                for(auto& lane: lanes.lane) { lane = bits; }
                return store_uint_lanes<u32, 4uz>(lanes);
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_splat_i64(wasm_i64 v) noexcept
        {
            static_assert(Op == simd_code::i64x2_splat);
            auto const bits{::std::bit_cast<u64>(v)};
            lane_array<u64, 2uz> lanes{};  // init
            for(auto& lane: lanes.lane) { lane = bits; }
            return store_uint_lanes<u64, 2uz>(lanes);
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_splat_f32(wasm_f32 v) noexcept
        {
            static_assert(Op == simd_code::f32x4_splat);
            lane_array<wasm_f32, 4uz> lanes{};  // init
            for(auto& lane: lanes.lane) { lane = v; }
            return store_f32x4_lanes(lanes);
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_splat_f64(wasm_f64 v) noexcept
        {
            static_assert(Op == simd_code::f64x2_splat);
            lane_array<wasm_f64, 2uz> lanes{};  // init
            for(auto& lane: lanes.lane) { lane = v; }
            return store_f64x2_lanes(lanes);
        }

        template <typename U, ::std::size_t N, bool Signed>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_i32 extract_int_lane(wasm_v128 v, u8 lane) noexcept
        {
            auto const lanes{load_uint_lanes<U, N>(v)};
            auto const raw{lanes.lane[lane]};
            if constexpr(Signed) { return static_cast<wasm_i32>(static_cast<::std::int_least32_t>(as_signed_lane<U>(raw))); }
            else
            {
                return ::std::bit_cast<wasm_i32>(static_cast<u32>(raw));
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_i32 eval_extract_lane_i32(wasm_v128 v, u8 lane) noexcept
        {
            if constexpr(Op == simd_code::i8x16_extract_lane_s) { return extract_int_lane<u8, 16uz, true>(v, lane); }
            else if constexpr(Op == simd_code::i8x16_extract_lane_u) { return extract_int_lane<u8, 16uz, false>(v, lane); }
            else if constexpr(Op == simd_code::i16x8_extract_lane_s) { return extract_int_lane<u16, 8uz, true>(v, lane); }
            else if constexpr(Op == simd_code::i16x8_extract_lane_u) { return extract_int_lane<u16, 8uz, false>(v, lane); }
            else
            {
                static_assert(Op == simd_code::i32x4_extract_lane);
                auto const lanes{load_uint_lanes<u32, 4uz>(v)};
                return ::std::bit_cast<wasm_i32>(lanes.lane[lane]);
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_i64 eval_extract_lane_i64(wasm_v128 v, u8 lane) noexcept
        {
            static_assert(Op == simd_code::i64x2_extract_lane);
            auto const lanes{load_uint_lanes<u64, 2uz>(v)};
            return ::std::bit_cast<wasm_i64>(lanes.lane[lane]);
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_f32 eval_extract_lane_f32(wasm_v128 v, u8 lane) noexcept
        {
            static_assert(Op == simd_code::f32x4_extract_lane);
            return load_f32x4_lanes(v).lane[lane];
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_f64 eval_extract_lane_f64(wasm_v128 v, u8 lane) noexcept
        {
            static_assert(Op == simd_code::f64x2_extract_lane);
            return load_f64x2_lanes(v).lane[lane];
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_replace_lane_i32(wasm_v128 v, wasm_i32 x, u8 lane) noexcept
        {
            auto const bits{::std::bit_cast<u32>(x)};
            if constexpr(Op == simd_code::i8x16_replace_lane)
            {
                auto lanes{load_uint_lanes<u8, 16uz>(v)};
                lanes.lane[lane] = static_cast<u8>(bits);
                return store_uint_lanes<u8, 16uz>(lanes);
            }
            else if constexpr(Op == simd_code::i16x8_replace_lane)
            {
                auto lanes{load_uint_lanes<u16, 8uz>(v)};
                lanes.lane[lane] = static_cast<u16>(bits);
                return store_uint_lanes<u16, 8uz>(lanes);
            }
            else
            {
                static_assert(Op == simd_code::i32x4_replace_lane);
                auto lanes{load_uint_lanes<u32, 4uz>(v)};
                lanes.lane[lane] = bits;
                return store_uint_lanes<u32, 4uz>(lanes);
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_replace_lane_i64(wasm_v128 v, wasm_i64 x, u8 lane) noexcept
        {
            static_assert(Op == simd_code::i64x2_replace_lane);
            auto lanes{load_uint_lanes<u64, 2uz>(v)};
            lanes.lane[lane] = ::std::bit_cast<u64>(x);
            return store_uint_lanes<u64, 2uz>(lanes);
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_replace_lane_f32(wasm_v128 v, wasm_f32 x, u8 lane) noexcept
        {
            static_assert(Op == simd_code::f32x4_replace_lane);
            auto lanes{load_f32x4_lanes(v)};
            lanes.lane[lane] = x;
            return store_f32x4_lanes(lanes);
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_replace_lane_f64(wasm_v128 v, wasm_f64 x, u8 lane) noexcept
        {
            static_assert(Op == simd_code::f64x2_replace_lane);
            auto lanes{load_f64x2_lanes(v)};
            lanes.lane[lane] = x;
            return store_f64x2_lanes(lanes);
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_shuffle(wasm_v128 lhs, wasm_v128 rhs, u8 const lanes_imm[16]) noexcept
        {
            auto const l{load_uint_lanes<u8, 16uz>(lhs)};
            auto const r{load_uint_lanes<u8, 16uz>(rhs)};
            lane_array<u8, 16uz> out{};  // init
            for(::std::size_t i{}; i != 16uz; ++i)
            {
                auto const lane{lanes_imm[i]};
                out.lane[i] = lane < 16u ? l.lane[lane] : r.lane[lane - 16u];
            }
            return store_uint_lanes<u8, 16uz>(out);
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_swizzle(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
            auto const values{load_uint_lanes<u8, 16uz>(lhs)};
            auto const indices{load_uint_lanes<u8, 16uz>(rhs)};
            lane_array<u8, 16uz> out{};  // init
            for(::std::size_t i{}; i != 16uz; ++i)
            {
                auto const lane{indices.lane[i]};
                out.lane[i] = lane < 16u ? values.lane[lane] : u8{};
            }
            return store_uint_lanes<u8, 16uz>(out);
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_bitselect(wasm_v128 lhs, wasm_v128 rhs, wasm_v128 mask) noexcept
        {
            auto const l{load_uint_lanes<u8, 16uz>(lhs)};
            auto const r{load_uint_lanes<u8, 16uz>(rhs)};
            auto const m{load_uint_lanes<u8, 16uz>(mask)};
            lane_array<u8, 16uz> out{};  // init
            for(::std::size_t i{}; i != 16uz; ++i) { out.lane[i] = static_cast<u8>((l.lane[i] & m.lane[i]) | (r.lane[i] & ~m.lane[i])); }
            return store_uint_lanes<u8, 16uz>(out);
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_unop(wasm_v128 v) noexcept
        {
            if constexpr(Op == simd_code::v128_not) { return v128_bitwise_not(v); }
            else if constexpr(Op == simd_code::i8x16_abs || Op == simd_code::i8x16_neg || Op == simd_code::i8x16_popcnt)
            {
                auto lanes{load_uint_lanes<u8, 16uz>(v)};
                for(auto& lane: lanes.lane)
                {
                    if constexpr(Op == simd_code::i8x16_abs) { lane = abs_signed_bits(lane); }
                    else if constexpr(Op == simd_code::i8x16_neg) { lane = neg_bits(lane); }
                    else
                    {
                        lane = static_cast<u8>(::std::popcount(static_cast<unsigned>(lane)));
                    }
                }
                return store_uint_lanes<u8, 16uz>(lanes);
            }
            else if constexpr(Op == simd_code::i16x8_abs || Op == simd_code::i16x8_neg || Op == simd_code::i16x8_extend_low_i8x16_s ||
                              Op == simd_code::i16x8_extend_high_i8x16_s || Op == simd_code::i16x8_extend_low_i8x16_u ||
                              Op == simd_code::i16x8_extend_high_i8x16_u || Op == simd_code::i16x8_extadd_pairwise_i8x16_s ||
                              Op == simd_code::i16x8_extadd_pairwise_i8x16_u)
            {
                if constexpr(Op == simd_code::i16x8_abs || Op == simd_code::i16x8_neg)
                {
                    auto lanes{load_uint_lanes<u16, 8uz>(v)};
                    for(auto& lane: lanes.lane)
                    {
                        if constexpr(Op == simd_code::i16x8_abs) { lane = abs_signed_bits(lane); }
                        else
                        {
                            lane = neg_bits(lane);
                        }
                    }
                    return store_uint_lanes<u16, 8uz>(lanes);
                }
                else
                {
                    auto const in{load_uint_lanes<u8, 16uz>(v)};
                    lane_array<u16, 8uz> out{};  // init
                    if constexpr(Op == simd_code::i16x8_extadd_pairwise_i8x16_s || Op == simd_code::i16x8_extadd_pairwise_i8x16_u)
                    {
                        for(::std::size_t i{}; i != 8uz; ++i)
                        {
                            if constexpr(Op == simd_code::i16x8_extadd_pairwise_i8x16_s)
                            {
                                auto const a{static_cast<::std::int_least16_t>(as_signed_lane<u8>(in.lane[i * 2uz]))};
                                auto const b{static_cast<::std::int_least16_t>(as_signed_lane<u8>(in.lane[i * 2uz + 1uz]))};
                                out.lane[i] = from_signed_lane<u16>(static_cast<s16>(a + b));
                            }
                            else
                            {
                                out.lane[i] = static_cast<u16>(static_cast<u16>(in.lane[i * 2uz]) + static_cast<u16>(in.lane[i * 2uz + 1uz]));
                            }
                        }
                    }
                    else
                    {
                        constexpr ::std::size_t begin{(Op == simd_code::i16x8_extend_high_i8x16_s || Op == simd_code::i16x8_extend_high_i8x16_u) ? 8uz : 0uz};
                        for(::std::size_t i{}; i != 8uz; ++i)
                        {
                            auto const lane{in.lane[begin + i]};
                            if constexpr(Op == simd_code::i16x8_extend_low_i8x16_s || Op == simd_code::i16x8_extend_high_i8x16_s)
                            {
                                out.lane[i] = from_signed_lane<u16>(static_cast<s16>(as_signed_lane<u8>(lane)));
                            }
                            else
                            {
                                out.lane[i] = static_cast<u16>(lane);
                            }
                        }
                    }
                    return store_uint_lanes<u16, 8uz>(out);
                }
            }
            else if constexpr(Op == simd_code::i32x4_abs || Op == simd_code::i32x4_neg || Op == simd_code::i32x4_extend_low_i16x8_s ||
                              Op == simd_code::i32x4_extend_high_i16x8_s || Op == simd_code::i32x4_extend_low_i16x8_u ||
                              Op == simd_code::i32x4_extend_high_i16x8_u || Op == simd_code::i32x4_extadd_pairwise_i16x8_s ||
                              Op == simd_code::i32x4_extadd_pairwise_i16x8_u || Op == simd_code::i32x4_trunc_sat_f32x4_s ||
                              Op == simd_code::i32x4_trunc_sat_f32x4_u || Op == simd_code::i32x4_trunc_sat_f64x2_s_zero ||
                              Op == simd_code::i32x4_trunc_sat_f64x2_u_zero)
            {
                if constexpr(Op == simd_code::i32x4_abs || Op == simd_code::i32x4_neg)
                {
                    auto lanes{load_uint_lanes<u32, 4uz>(v)};
                    for(auto& lane: lanes.lane)
                    {
                        if constexpr(Op == simd_code::i32x4_abs) { lane = abs_signed_bits(lane); }
                        else
                        {
                            lane = neg_bits(lane);
                        }
                    }
                    return store_uint_lanes<u32, 4uz>(lanes);
                }
                else if constexpr(Op == simd_code::i32x4_trunc_sat_f32x4_s || Op == simd_code::i32x4_trunc_sat_f32x4_u)
                {
                    auto const in{load_f32x4_lanes(v)};
                    lane_array<u32, 4uz> out{};  // init
                    for(::std::size_t i{}; i != 4uz; ++i)
                    {
                        auto const x{wasm1p1_details::trunc_sat<wasm_i32, Op == simd_code::i32x4_trunc_sat_f32x4_s>(in.lane[i])};
                        out.lane[i] = ::std::bit_cast<u32>(x);
                    }
                    return store_uint_lanes<u32, 4uz>(out);
                }
                else if constexpr(Op == simd_code::i32x4_trunc_sat_f64x2_s_zero || Op == simd_code::i32x4_trunc_sat_f64x2_u_zero)
                {
                    auto const in{load_f64x2_lanes(v)};
                    lane_array<u32, 4uz> out{};  // init
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        auto const x{wasm1p1_details::trunc_sat<wasm_i32, Op == simd_code::i32x4_trunc_sat_f64x2_s_zero>(in.lane[i])};
                        out.lane[i] = ::std::bit_cast<u32>(x);
                    }
                    return store_uint_lanes<u32, 4uz>(out);
                }
                else
                {
                    auto const in{load_uint_lanes<u16, 8uz>(v)};
                    lane_array<u32, 4uz> out{};  // init
                    if constexpr(Op == simd_code::i32x4_extadd_pairwise_i16x8_s || Op == simd_code::i32x4_extadd_pairwise_i16x8_u)
                    {
                        for(::std::size_t i{}; i != 4uz; ++i)
                        {
                            if constexpr(Op == simd_code::i32x4_extadd_pairwise_i16x8_s)
                            {
                                auto const a{static_cast<::std::int_least32_t>(as_signed_lane<u16>(in.lane[i * 2uz]))};
                                auto const b{static_cast<::std::int_least32_t>(as_signed_lane<u16>(in.lane[i * 2uz + 1uz]))};
                                out.lane[i] = from_signed_lane<u32>(static_cast<s32>(a + b));
                            }
                            else
                            {
                                out.lane[i] = static_cast<u32>(static_cast<u32>(in.lane[i * 2uz]) + static_cast<u32>(in.lane[i * 2uz + 1uz]));
                            }
                        }
                    }
                    else
                    {
                        constexpr ::std::size_t begin{(Op == simd_code::i32x4_extend_high_i16x8_s || Op == simd_code::i32x4_extend_high_i16x8_u) ? 4uz : 0uz};
                        for(::std::size_t i{}; i != 4uz; ++i)
                        {
                            auto const lane{in.lane[begin + i]};
                            if constexpr(Op == simd_code::i32x4_extend_low_i16x8_s || Op == simd_code::i32x4_extend_high_i16x8_s)
                            {
                                out.lane[i] = from_signed_lane<u32>(static_cast<s32>(as_signed_lane<u16>(lane)));
                            }
                            else
                            {
                                out.lane[i] = static_cast<u32>(lane);
                            }
                        }
                    }
                    return store_uint_lanes<u32, 4uz>(out);
                }
            }
            else if constexpr(Op == simd_code::i64x2_abs || Op == simd_code::i64x2_neg || Op == simd_code::i64x2_extend_low_i32x4_s ||
                              Op == simd_code::i64x2_extend_high_i32x4_s || Op == simd_code::i64x2_extend_low_i32x4_u ||
                              Op == simd_code::i64x2_extend_high_i32x4_u)
            {
                if constexpr(Op == simd_code::i64x2_abs || Op == simd_code::i64x2_neg)
                {
                    auto lanes{load_uint_lanes<u64, 2uz>(v)};
                    for(auto& lane: lanes.lane)
                    {
                        if constexpr(Op == simd_code::i64x2_abs) { lane = abs_signed_bits(lane); }
                        else
                        {
                            lane = neg_bits(lane);
                        }
                    }
                    return store_uint_lanes<u64, 2uz>(lanes);
                }
                else
                {
                    auto const in{load_uint_lanes<u32, 4uz>(v)};
                    lane_array<u64, 2uz> out{};  // init
                    constexpr ::std::size_t begin{(Op == simd_code::i64x2_extend_high_i32x4_s || Op == simd_code::i64x2_extend_high_i32x4_u) ? 2uz : 0uz};
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        auto const lane{in.lane[begin + i]};
                        if constexpr(Op == simd_code::i64x2_extend_low_i32x4_s || Op == simd_code::i64x2_extend_high_i32x4_s)
                        {
                            out.lane[i] = from_signed_lane<u64>(static_cast<s64>(as_signed_lane<u32>(lane)));
                        }
                        else
                        {
                            out.lane[i] = static_cast<u64>(lane);
                        }
                    }
                    return store_uint_lanes<u64, 2uz>(out);
                }
            }
            else if constexpr(Op == simd_code::f32x4_abs || Op == simd_code::f32x4_neg || Op == simd_code::f32x4_sqrt || Op == simd_code::f32x4_ceil ||
                              Op == simd_code::f32x4_floor || Op == simd_code::f32x4_trunc || Op == simd_code::f32x4_nearest ||
                              Op == simd_code::f32x4_convert_i32x4_s || Op == simd_code::f32x4_convert_i32x4_u || Op == simd_code::f32x4_demote_f64x2_zero)
            {
                lane_array<wasm_f32, 4uz> out{};  // init
                if constexpr(Op == simd_code::f32x4_convert_i32x4_s || Op == simd_code::f32x4_convert_i32x4_u)
                {
                    auto const in{load_uint_lanes<u32, 4uz>(v)};
                    for(::std::size_t i{}; i != 4uz; ++i)
                    {
                        if constexpr(Op == simd_code::f32x4_convert_i32x4_s) { out.lane[i] = static_cast<wasm_f32>(as_signed_lane<u32>(in.lane[i])); }
                        else
                        {
                            out.lane[i] = static_cast<wasm_f32>(in.lane[i]);
                        }
                    }
                }
                else if constexpr(Op == simd_code::f32x4_demote_f64x2_zero)
                {
                    auto const in{load_f64x2_lanes(v)};
                    out.lane[0] = static_cast<wasm_f32>(in.lane[0]);
                    out.lane[1] = static_cast<wasm_f32>(in.lane[1]);
                }
                else
                {
                    auto const in{load_f32x4_lanes(v)};
                    for(::std::size_t i{}; i != 4uz; ++i)
                    {
                        if constexpr(Op == simd_code::f32x4_abs) { out.lane[i] = ::std::fabs(in.lane[i]); }
                        else if constexpr(Op == simd_code::f32x4_neg) { out.lane[i] = -in.lane[i]; }
                        else if constexpr(Op == simd_code::f32x4_sqrt) { out.lane[i] = ::std::sqrt(in.lane[i]); }
                        else if constexpr(Op == simd_code::f32x4_ceil) { out.lane[i] = ::std::ceil(in.lane[i]); }
                        else if constexpr(Op == simd_code::f32x4_floor) { out.lane[i] = ::std::floor(in.lane[i]); }
                        else if constexpr(Op == simd_code::f32x4_trunc) { out.lane[i] = ::std::trunc(in.lane[i]); }
                        else
                        {
                            out.lane[i] = ::std::nearbyint(in.lane[i]);
                        }
                    }
                }
                return store_f32x4_lanes(out);
            }
            else
            {
                static_assert(Op == simd_code::f64x2_abs || Op == simd_code::f64x2_neg || Op == simd_code::f64x2_sqrt || Op == simd_code::f64x2_ceil ||
                              Op == simd_code::f64x2_floor || Op == simd_code::f64x2_trunc || Op == simd_code::f64x2_nearest ||
                              Op == simd_code::f64x2_promote_low_f32x4 || Op == simd_code::f64x2_convert_low_i32x4_s ||
                              Op == simd_code::f64x2_convert_low_i32x4_u);
                lane_array<wasm_f64, 2uz> out{};  // init
                if constexpr(Op == simd_code::f64x2_promote_low_f32x4)
                {
                    auto const in{load_f32x4_lanes(v)};
                    out.lane[0] = static_cast<wasm_f64>(in.lane[0]);
                    out.lane[1] = static_cast<wasm_f64>(in.lane[1]);
                }
                else if constexpr(Op == simd_code::f64x2_convert_low_i32x4_s || Op == simd_code::f64x2_convert_low_i32x4_u)
                {
                    auto const in{load_uint_lanes<u32, 4uz>(v)};
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        if constexpr(Op == simd_code::f64x2_convert_low_i32x4_s) { out.lane[i] = static_cast<wasm_f64>(as_signed_lane<u32>(in.lane[i])); }
                        else
                        {
                            out.lane[i] = static_cast<wasm_f64>(in.lane[i]);
                        }
                    }
                }
                else
                {
                    auto const in{load_f64x2_lanes(v)};
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        if constexpr(Op == simd_code::f64x2_abs) { out.lane[i] = ::std::fabs(in.lane[i]); }
                        else if constexpr(Op == simd_code::f64x2_neg) { out.lane[i] = -in.lane[i]; }
                        else if constexpr(Op == simd_code::f64x2_sqrt) { out.lane[i] = ::std::sqrt(in.lane[i]); }
                        else if constexpr(Op == simd_code::f64x2_ceil) { out.lane[i] = ::std::ceil(in.lane[i]); }
                        else if constexpr(Op == simd_code::f64x2_floor) { out.lane[i] = ::std::floor(in.lane[i]); }
                        else if constexpr(Op == simd_code::f64x2_trunc) { out.lane[i] = ::std::trunc(in.lane[i]); }
                        else
                        {
                            out.lane[i] = ::std::nearbyint(in.lane[i]);
                        }
                    }
                }
                return store_f64x2_lanes(out);
            }
        }

        template <typename U, ::std::size_t N, simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_int_compare(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
            auto const l{load_uint_lanes<U, N>(lhs)};
            auto const r{load_uint_lanes<U, N>(rhs)};
            lane_array<U, N> out{};  // init
            for(::std::size_t i{}; i != N; ++i)
            {
                bool b{};
                if constexpr(Op == simd_code::i8x16_eq || Op == simd_code::i16x8_eq || Op == simd_code::i32x4_eq || Op == simd_code::i64x2_eq)
                {
                    b = l.lane[i] == r.lane[i];
                }
                else if constexpr(Op == simd_code::i8x16_ne || Op == simd_code::i16x8_ne || Op == simd_code::i32x4_ne || Op == simd_code::i64x2_ne)
                {
                    b = l.lane[i] != r.lane[i];
                }
                else if constexpr(Op == simd_code::i8x16_lt_s || Op == simd_code::i16x8_lt_s || Op == simd_code::i32x4_lt_s || Op == simd_code::i64x2_lt_s)
                {
                    b = as_signed_lane<U>(l.lane[i]) < as_signed_lane<U>(r.lane[i]);
                }
                else if constexpr(Op == simd_code::i8x16_lt_u || Op == simd_code::i16x8_lt_u || Op == simd_code::i32x4_lt_u) { b = l.lane[i] < r.lane[i]; }
                else if constexpr(Op == simd_code::i8x16_gt_s || Op == simd_code::i16x8_gt_s || Op == simd_code::i32x4_gt_s || Op == simd_code::i64x2_gt_s)
                {
                    b = as_signed_lane<U>(l.lane[i]) > as_signed_lane<U>(r.lane[i]);
                }
                else if constexpr(Op == simd_code::i8x16_gt_u || Op == simd_code::i16x8_gt_u || Op == simd_code::i32x4_gt_u) { b = l.lane[i] > r.lane[i]; }
                else if constexpr(Op == simd_code::i8x16_le_s || Op == simd_code::i16x8_le_s || Op == simd_code::i32x4_le_s || Op == simd_code::i64x2_le_s)
                {
                    b = as_signed_lane<U>(l.lane[i]) <= as_signed_lane<U>(r.lane[i]);
                }
                else if constexpr(Op == simd_code::i8x16_le_u || Op == simd_code::i16x8_le_u || Op == simd_code::i32x4_le_u) { b = l.lane[i] <= r.lane[i]; }
                else if constexpr(Op == simd_code::i8x16_ge_s || Op == simd_code::i16x8_ge_s || Op == simd_code::i32x4_ge_s || Op == simd_code::i64x2_ge_s)
                {
                    b = as_signed_lane<U>(l.lane[i]) >= as_signed_lane<U>(r.lane[i]);
                }
                else
                {
                    static_assert(Op == simd_code::i8x16_ge_u || Op == simd_code::i16x8_ge_u || Op == simd_code::i32x4_ge_u);
                    b = l.lane[i] >= r.lane[i];
                }
                out.lane[i] = lane_bool<U>(b);
            }
            return store_uint_lanes<U, N>(out);
        }

        template <typename FloatT, typename MaskU, ::std::size_t N, simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_float_compare(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
            auto const l{[](wasm_v128 x)
                         {
                             if constexpr(::std::same_as<FloatT, wasm_f32>) { return load_f32x4_lanes(x); }
                             else
                             {
                                 return load_f64x2_lanes(x);
                             }
                         }(lhs)};
            auto const r{[](wasm_v128 x)
                         {
                             if constexpr(::std::same_as<FloatT, wasm_f32>) { return load_f32x4_lanes(x); }
                             else
                             {
                                 return load_f64x2_lanes(x);
                             }
                         }(rhs)};
            lane_array<MaskU, N> out{};  // init
            for(::std::size_t i{}; i != N; ++i)
            {
                bool b{};
                if constexpr(Op == simd_code::f32x4_eq || Op == simd_code::f64x2_eq) { b = l.lane[i] == r.lane[i]; }
                else if constexpr(Op == simd_code::f32x4_ne || Op == simd_code::f64x2_ne) { b = l.lane[i] != r.lane[i]; }
                else if constexpr(Op == simd_code::f32x4_lt || Op == simd_code::f64x2_lt) { b = l.lane[i] < r.lane[i]; }
                else if constexpr(Op == simd_code::f32x4_gt || Op == simd_code::f64x2_gt) { b = l.lane[i] > r.lane[i]; }
                else if constexpr(Op == simd_code::f32x4_le || Op == simd_code::f64x2_le) { b = l.lane[i] <= r.lane[i]; }
                else
                {
                    static_assert(Op == simd_code::f32x4_ge || Op == simd_code::f64x2_ge);
                    b = l.lane[i] >= r.lane[i];
                }
                out.lane[i] = lane_bool<MaskU>(b);
            }
            return store_uint_lanes<MaskU, N>(out);
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_binop(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
            if constexpr(Op == simd_code::i8x16_swizzle) { return eval_swizzle(lhs, rhs); }
            else if constexpr(Op == simd_code::v128_and || Op == simd_code::v128_andnot || Op == simd_code::v128_or || Op == simd_code::v128_xor)
            {
                if constexpr(Op == simd_code::v128_and) { return eval_v128_binop<v128_binop::and_>(lhs, rhs); }
                else if constexpr(Op == simd_code::v128_andnot) { return eval_v128_binop<v128_binop::andnot>(lhs, rhs); }
                else if constexpr(Op == simd_code::v128_or) { return eval_v128_binop<v128_binop::or_>(lhs, rhs); }
                else
                {
                    return eval_v128_binop<v128_binop::xor_>(lhs, rhs);
                }
            }
            else if constexpr(Op >= simd_code::i8x16_eq && Op <= simd_code::i8x16_ge_u) { return eval_int_compare<u8, 16uz, Op>(lhs, rhs); }
            else if constexpr(Op >= simd_code::i16x8_eq && Op <= simd_code::i16x8_ge_u) { return eval_int_compare<u16, 8uz, Op>(lhs, rhs); }
            else if constexpr(Op >= simd_code::i32x4_eq && Op <= simd_code::i32x4_ge_u) { return eval_int_compare<u32, 4uz, Op>(lhs, rhs); }
            else if constexpr(Op >= simd_code::i64x2_eq && Op <= simd_code::i64x2_ge_s) { return eval_int_compare<u64, 2uz, Op>(lhs, rhs); }
            else if constexpr(Op >= simd_code::f32x4_eq && Op <= simd_code::f32x4_ge) { return eval_float_compare<wasm_f32, u32, 4uz, Op>(lhs, rhs); }
            else if constexpr(Op >= simd_code::f64x2_eq && Op <= simd_code::f64x2_ge) { return eval_float_compare<wasm_f64, u64, 2uz, Op>(lhs, rhs); }
            else if constexpr(Op == simd_code::i8x16_narrow_i16x8_s || Op == simd_code::i8x16_narrow_i16x8_u)
            {
                auto const l{load_uint_lanes<u16, 8uz>(lhs)};
                auto const r{load_uint_lanes<u16, 8uz>(rhs)};
                lane_array<u8, 16uz> out{};  // init
                for(::std::size_t i{}; i != 8uz; ++i)
                {
                    if constexpr(Op == simd_code::i8x16_narrow_i16x8_s)
                    {
                        out.lane[i] = narrow_signed_lane<u8>(l.lane[i]);
                        out.lane[i + 8uz] = narrow_signed_lane<u8>(r.lane[i]);
                    }
                    else
                    {
                        out.lane[i] = narrow_unsigned_lane<u8>(l.lane[i]);
                        out.lane[i + 8uz] = narrow_unsigned_lane<u8>(r.lane[i]);
                    }
                }
                return store_uint_lanes<u8, 16uz>(out);
            }
            else if constexpr(Op == simd_code::i16x8_narrow_i32x4_s || Op == simd_code::i16x8_narrow_i32x4_u)
            {
                auto const l{load_uint_lanes<u32, 4uz>(lhs)};
                auto const r{load_uint_lanes<u32, 4uz>(rhs)};
                lane_array<u16, 8uz> out{};  // init
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if constexpr(Op == simd_code::i16x8_narrow_i32x4_s)
                    {
                        out.lane[i] = narrow_signed_lane<u16>(l.lane[i]);
                        out.lane[i + 4uz] = narrow_signed_lane<u16>(r.lane[i]);
                    }
                    else
                    {
                        out.lane[i] = narrow_unsigned_lane<u16>(l.lane[i]);
                        out.lane[i + 4uz] = narrow_unsigned_lane<u16>(r.lane[i]);
                    }
                }
                return store_uint_lanes<u16, 8uz>(out);
            }
            else if constexpr(Op == simd_code::i8x16_add || Op == simd_code::i8x16_add_sat_s || Op == simd_code::i8x16_add_sat_u ||
                              Op == simd_code::i8x16_sub || Op == simd_code::i8x16_sub_sat_s || Op == simd_code::i8x16_sub_sat_u ||
                              Op == simd_code::i8x16_min_s || Op == simd_code::i8x16_min_u || Op == simd_code::i8x16_max_s || Op == simd_code::i8x16_max_u ||
                              Op == simd_code::i8x16_avgr_u)
            {
                auto l{load_uint_lanes<u8, 16uz>(lhs)};
                auto const r{load_uint_lanes<u8, 16uz>(rhs)};
                for(::std::size_t i{}; i != 16uz; ++i)
                {
                    if constexpr(Op == simd_code::i8x16_add) { l.lane[i] = static_cast<u8>(l.lane[i] + r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_sub) { l.lane[i] = static_cast<u8>(l.lane[i] - r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_add_sat_s) { l.lane[i] = sat_add_signed(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_add_sat_u) { l.lane[i] = sat_add_unsigned(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_sub_sat_s) { l.lane[i] = sat_sub_signed(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_sub_sat_u) { l.lane[i] = sat_sub_unsigned(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_min_s) { l.lane[i] = min_signed_bits(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_min_u) { l.lane[i] = l.lane[i] < r.lane[i] ? l.lane[i] : r.lane[i]; }
                    else if constexpr(Op == simd_code::i8x16_max_s) { l.lane[i] = max_signed_bits(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_max_u) { l.lane[i] = l.lane[i] > r.lane[i] ? l.lane[i] : r.lane[i]; }
                    else
                    {
                        l.lane[i] = static_cast<u8>((static_cast<unsigned>(l.lane[i]) + static_cast<unsigned>(r.lane[i]) + 1u) >> 1u);
                    }
                }
                return store_uint_lanes<u8, 16uz>(l);
            }
            else if constexpr(Op == simd_code::i16x8_q15mulr_sat_s || Op == simd_code::i16x8_add || Op == simd_code::i16x8_add_sat_s ||
                              Op == simd_code::i16x8_add_sat_u || Op == simd_code::i16x8_sub || Op == simd_code::i16x8_sub_sat_s ||
                              Op == simd_code::i16x8_sub_sat_u || Op == simd_code::i16x8_mul || Op == simd_code::i16x8_min_s || Op == simd_code::i16x8_min_u ||
                              Op == simd_code::i16x8_max_s || Op == simd_code::i16x8_max_u || Op == simd_code::i16x8_avgr_u)
            {
                auto l{load_uint_lanes<u16, 8uz>(lhs)};
                auto const r{load_uint_lanes<u16, 8uz>(rhs)};
                for(::std::size_t i{}; i != 8uz; ++i)
                {
                    if constexpr(Op == simd_code::i16x8_add) { l.lane[i] = static_cast<u16>(l.lane[i] + r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_sub) { l.lane[i] = static_cast<u16>(l.lane[i] - r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_mul) { l.lane[i] = static_cast<u16>(l.lane[i] * r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_q15mulr_sat_s) { l.lane[i] = q15mulr_sat_s(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_add_sat_s) { l.lane[i] = sat_add_signed(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_add_sat_u) { l.lane[i] = sat_add_unsigned(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_sub_sat_s) { l.lane[i] = sat_sub_signed(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_sub_sat_u) { l.lane[i] = sat_sub_unsigned(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_min_s) { l.lane[i] = min_signed_bits(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_min_u) { l.lane[i] = l.lane[i] < r.lane[i] ? l.lane[i] : r.lane[i]; }
                    else if constexpr(Op == simd_code::i16x8_max_s) { l.lane[i] = max_signed_bits(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_max_u) { l.lane[i] = l.lane[i] > r.lane[i] ? l.lane[i] : r.lane[i]; }
                    else
                    {
                        l.lane[i] = static_cast<u16>((static_cast<u32>(l.lane[i]) + static_cast<u32>(r.lane[i]) + 1u) >> 1u);
                    }
                }
                return store_uint_lanes<u16, 8uz>(l);
            }
            else if constexpr(Op == simd_code::i16x8_extmul_low_i8x16_s || Op == simd_code::i16x8_extmul_high_i8x16_s ||
                              Op == simd_code::i16x8_extmul_low_i8x16_u || Op == simd_code::i16x8_extmul_high_i8x16_u)
            {
                auto const l{load_uint_lanes<u8, 16uz>(lhs)};
                auto const r{load_uint_lanes<u8, 16uz>(rhs)};
                lane_array<u16, 8uz> out{};  // init
                constexpr ::std::size_t begin{(Op == simd_code::i16x8_extmul_high_i8x16_s || Op == simd_code::i16x8_extmul_high_i8x16_u) ? 8uz : 0uz};
                for(::std::size_t i{}; i != 8uz; ++i)
                {
                    if constexpr(Op == simd_code::i16x8_extmul_low_i8x16_s || Op == simd_code::i16x8_extmul_high_i8x16_s)
                    {
                        out.lane[i] = from_signed_lane<u16>(static_cast<s16>(static_cast<::std::int_least16_t>(as_signed_lane<u8>(l.lane[begin + i])) *
                                                                             static_cast<::std::int_least16_t>(as_signed_lane<u8>(r.lane[begin + i]))));
                    }
                    else
                    {
                        out.lane[i] = static_cast<u16>(static_cast<u16>(l.lane[begin + i]) * static_cast<u16>(r.lane[begin + i]));
                    }
                }
                return store_uint_lanes<u16, 8uz>(out);
            }
            else if constexpr(Op == simd_code::i32x4_add || Op == simd_code::i32x4_sub || Op == simd_code::i32x4_mul || Op == simd_code::i32x4_min_s ||
                              Op == simd_code::i32x4_min_u || Op == simd_code::i32x4_max_s || Op == simd_code::i32x4_max_u)
            {
                auto l{load_uint_lanes<u32, 4uz>(lhs)};
                auto const r{load_uint_lanes<u32, 4uz>(rhs)};
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if constexpr(Op == simd_code::i32x4_add) { l.lane[i] = static_cast<u32>(l.lane[i] + r.lane[i]); }
                    else if constexpr(Op == simd_code::i32x4_sub) { l.lane[i] = static_cast<u32>(l.lane[i] - r.lane[i]); }
                    else if constexpr(Op == simd_code::i32x4_mul) { l.lane[i] = static_cast<u32>(l.lane[i] * r.lane[i]); }
                    else if constexpr(Op == simd_code::i32x4_min_s) { l.lane[i] = min_signed_bits(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i32x4_min_u) { l.lane[i] = l.lane[i] < r.lane[i] ? l.lane[i] : r.lane[i]; }
                    else if constexpr(Op == simd_code::i32x4_max_s) { l.lane[i] = max_signed_bits(l.lane[i], r.lane[i]); }
                    else
                    {
                        l.lane[i] = l.lane[i] > r.lane[i] ? l.lane[i] : r.lane[i];
                    }
                }
                return store_uint_lanes<u32, 4uz>(l);
            }
            else if constexpr(Op == simd_code::i32x4_dot_i16x8_s || Op == simd_code::i32x4_extmul_low_i16x8_s || Op == simd_code::i32x4_extmul_high_i16x8_s ||
                              Op == simd_code::i32x4_extmul_low_i16x8_u || Op == simd_code::i32x4_extmul_high_i16x8_u)
            {
                auto const l{load_uint_lanes<u16, 8uz>(lhs)};
                auto const r{load_uint_lanes<u16, 8uz>(rhs)};
                lane_array<u32, 4uz> out{};  // init
                if constexpr(Op == simd_code::i32x4_dot_i16x8_s)
                {
                    for(::std::size_t i{}; i != 4uz; ++i)
                    {
                        auto const a0{static_cast<::std::int_least32_t>(as_signed_lane<u16>(l.lane[i * 2uz]))};
                        auto const a1{static_cast<::std::int_least32_t>(as_signed_lane<u16>(l.lane[i * 2uz + 1uz]))};
                        auto const b0{static_cast<::std::int_least32_t>(as_signed_lane<u16>(r.lane[i * 2uz]))};
                        auto const b1{static_cast<::std::int_least32_t>(as_signed_lane<u16>(r.lane[i * 2uz + 1uz]))};
                        out.lane[i] = from_signed_lane<u32>(static_cast<s32>(a0 * b0 + a1 * b1));
                    }
                }
                else
                {
                    constexpr ::std::size_t begin{(Op == simd_code::i32x4_extmul_high_i16x8_s || Op == simd_code::i32x4_extmul_high_i16x8_u) ? 4uz : 0uz};
                    for(::std::size_t i{}; i != 4uz; ++i)
                    {
                        if constexpr(Op == simd_code::i32x4_extmul_low_i16x8_s || Op == simd_code::i32x4_extmul_high_i16x8_s)
                        {
                            out.lane[i] = from_signed_lane<u32>(static_cast<s32>(static_cast<::std::int_least32_t>(as_signed_lane<u16>(l.lane[begin + i])) *
                                                                                 static_cast<::std::int_least32_t>(as_signed_lane<u16>(r.lane[begin + i]))));
                        }
                        else
                        {
                            out.lane[i] = static_cast<u32>(static_cast<u32>(l.lane[begin + i]) * static_cast<u32>(r.lane[begin + i]));
                        }
                    }
                }
                return store_uint_lanes<u32, 4uz>(out);
            }
            else if constexpr(Op == simd_code::i64x2_add || Op == simd_code::i64x2_sub || Op == simd_code::i64x2_mul ||
                              Op == simd_code::i64x2_extmul_low_i32x4_s || Op == simd_code::i64x2_extmul_high_i32x4_s ||
                              Op == simd_code::i64x2_extmul_low_i32x4_u || Op == simd_code::i64x2_extmul_high_i32x4_u)
            {
                if constexpr(Op == simd_code::i64x2_add || Op == simd_code::i64x2_sub || Op == simd_code::i64x2_mul)
                {
                    auto l{load_uint_lanes<u64, 2uz>(lhs)};
                    auto const r{load_uint_lanes<u64, 2uz>(rhs)};
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        if constexpr(Op == simd_code::i64x2_add) { l.lane[i] = static_cast<u64>(l.lane[i] + r.lane[i]); }
                        else if constexpr(Op == simd_code::i64x2_sub) { l.lane[i] = static_cast<u64>(l.lane[i] - r.lane[i]); }
                        else
                        {
                            l.lane[i] = static_cast<u64>(l.lane[i] * r.lane[i]);
                        }
                    }
                    return store_uint_lanes<u64, 2uz>(l);
                }
                else
                {
                    auto const l{load_uint_lanes<u32, 4uz>(lhs)};
                    auto const r{load_uint_lanes<u32, 4uz>(rhs)};
                    lane_array<u64, 2uz> out{};  // init
                    constexpr ::std::size_t begin{(Op == simd_code::i64x2_extmul_high_i32x4_s || Op == simd_code::i64x2_extmul_high_i32x4_u) ? 2uz : 0uz};
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        if constexpr(Op == simd_code::i64x2_extmul_low_i32x4_s || Op == simd_code::i64x2_extmul_high_i32x4_s)
                        {
                            out.lane[i] = from_signed_lane<u64>(static_cast<s64>(static_cast<::std::int_least64_t>(as_signed_lane<u32>(l.lane[begin + i])) *
                                                                                 static_cast<::std::int_least64_t>(as_signed_lane<u32>(r.lane[begin + i]))));
                        }
                        else
                        {
                            out.lane[i] = static_cast<u64>(static_cast<u64>(l.lane[begin + i]) * static_cast<u64>(r.lane[begin + i]));
                        }
                    }
                    return store_uint_lanes<u64, 2uz>(out);
                }
            }
            else if constexpr(Op == simd_code::f32x4_add || Op == simd_code::f32x4_sub || Op == simd_code::f32x4_mul || Op == simd_code::f32x4_div ||
                              Op == simd_code::f32x4_min || Op == simd_code::f32x4_max || Op == simd_code::f32x4_pmin || Op == simd_code::f32x4_pmax)
            {
                auto l{load_f32x4_lanes(lhs)};
                auto const r{load_f32x4_lanes(rhs)};
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if constexpr(Op == simd_code::f32x4_add) { l.lane[i] = static_cast<wasm_f32>(l.lane[i] + r.lane[i]); }
                    else if constexpr(Op == simd_code::f32x4_sub) { l.lane[i] = static_cast<wasm_f32>(l.lane[i] - r.lane[i]); }
                    else if constexpr(Op == simd_code::f32x4_mul) { l.lane[i] = static_cast<wasm_f32>(l.lane[i] * r.lane[i]); }
                    else if constexpr(Op == simd_code::f32x4_div) { l.lane[i] = static_cast<wasm_f32>(l.lane[i] / r.lane[i]); }
                    else if constexpr(Op == simd_code::f32x4_min) { l.lane[i] = wasm_float_min(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f32x4_max) { l.lane[i] = wasm_float_max(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f32x4_pmin) { l.lane[i] = wasm_float_pmin(l.lane[i], r.lane[i]); }
                    else
                    {
                        l.lane[i] = wasm_float_pmax(l.lane[i], r.lane[i]);
                    }
                }
                return store_f32x4_lanes(l);
            }
            else
            {
                static_assert(Op == simd_code::f64x2_add || Op == simd_code::f64x2_sub || Op == simd_code::f64x2_mul || Op == simd_code::f64x2_div ||
                              Op == simd_code::f64x2_min || Op == simd_code::f64x2_max || Op == simd_code::f64x2_pmin || Op == simd_code::f64x2_pmax);
                auto l{load_f64x2_lanes(lhs)};
                auto const r{load_f64x2_lanes(rhs)};
                for(::std::size_t i{}; i != 2uz; ++i)
                {
                    if constexpr(Op == simd_code::f64x2_add) { l.lane[i] = static_cast<wasm_f64>(l.lane[i] + r.lane[i]); }
                    else if constexpr(Op == simd_code::f64x2_sub) { l.lane[i] = static_cast<wasm_f64>(l.lane[i] - r.lane[i]); }
                    else if constexpr(Op == simd_code::f64x2_mul) { l.lane[i] = static_cast<wasm_f64>(l.lane[i] * r.lane[i]); }
                    else if constexpr(Op == simd_code::f64x2_div) { l.lane[i] = static_cast<wasm_f64>(l.lane[i] / r.lane[i]); }
                    else if constexpr(Op == simd_code::f64x2_min) { l.lane[i] = wasm_float_min(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f64x2_max) { l.lane[i] = wasm_float_max(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f64x2_pmin) { l.lane[i] = wasm_float_pmin(l.lane[i], r.lane[i]); }
                    else
                    {
                        l.lane[i] = wasm_float_pmax(l.lane[i], r.lane[i]);
                    }
                }
                return store_f64x2_lanes(l);
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_shift(wasm_v128 lhs, wasm_i32 rhs) noexcept
        {
            if constexpr(Op == simd_code::i8x16_shl || Op == simd_code::i8x16_shr_s || Op == simd_code::i8x16_shr_u)
            {
                auto lanes{load_uint_lanes<u8, 16uz>(lhs)};
                for(auto& lane: lanes.lane)
                {
                    if constexpr(Op == simd_code::i8x16_shl) { lane = shl_lane(lane, rhs); }
                    else if constexpr(Op == simd_code::i8x16_shr_s) { lane = shr_s_lane(lane, rhs); }
                    else
                    {
                        lane = shr_u_lane(lane, rhs);
                    }
                }
                return store_uint_lanes<u8, 16uz>(lanes);
            }
            else if constexpr(Op == simd_code::i16x8_shl || Op == simd_code::i16x8_shr_s || Op == simd_code::i16x8_shr_u)
            {
                auto lanes{load_uint_lanes<u16, 8uz>(lhs)};
                for(auto& lane: lanes.lane)
                {
                    if constexpr(Op == simd_code::i16x8_shl) { lane = shl_lane(lane, rhs); }
                    else if constexpr(Op == simd_code::i16x8_shr_s) { lane = shr_s_lane(lane, rhs); }
                    else
                    {
                        lane = shr_u_lane(lane, rhs);
                    }
                }
                return store_uint_lanes<u16, 8uz>(lanes);
            }
            else if constexpr(Op == simd_code::i32x4_shl || Op == simd_code::i32x4_shr_s || Op == simd_code::i32x4_shr_u)
            {
                auto lanes{load_uint_lanes<u32, 4uz>(lhs)};
                for(auto& lane: lanes.lane)
                {
                    if constexpr(Op == simd_code::i32x4_shl) { lane = shl_lane(lane, rhs); }
                    else if constexpr(Op == simd_code::i32x4_shr_s) { lane = shr_s_lane(lane, rhs); }
                    else
                    {
                        lane = shr_u_lane(lane, rhs);
                    }
                }
                return store_uint_lanes<u32, 4uz>(lanes);
            }
            else
            {
                static_assert(Op == simd_code::i64x2_shl || Op == simd_code::i64x2_shr_s || Op == simd_code::i64x2_shr_u);
                auto lanes{load_uint_lanes<u64, 2uz>(lhs)};
                for(auto& lane: lanes.lane)
                {
                    if constexpr(Op == simd_code::i64x2_shl) { lane = shl_lane(lane, rhs); }
                    else if constexpr(Op == simd_code::i64x2_shr_s) { lane = shr_s_lane(lane, rhs); }
                    else
                    {
                        lane = shr_u_lane(lane, rhs);
                    }
                }
                return store_uint_lanes<u64, 2uz>(lanes);
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_i32 eval_full_test(wasm_v128 v) noexcept
        {
            if constexpr(Op == simd_code::v128_any_true) { return eval_v128_testop<v128_testop::any_true>(v); }
            else if constexpr(Op == simd_code::i8x16_all_true)
            {
                auto const lanes{load_uint_lanes<u8, 16uz>(v)};
                for(auto lane: lanes.lane)
                {
                    if(lane == 0u) { return wasm_i32{}; }
                }
                return wasm_i32{1};
            }
            else if constexpr(Op == simd_code::i16x8_all_true)
            {
                auto const lanes{load_uint_lanes<u16, 8uz>(v)};
                for(auto lane: lanes.lane)
                {
                    if(lane == 0u) { return wasm_i32{}; }
                }
                return wasm_i32{1};
            }
            else if constexpr(Op == simd_code::i32x4_all_true) { return eval_v128_testop<v128_testop::i32x4_all_true>(v); }
            else if constexpr(Op == simd_code::i64x2_all_true)
            {
                auto const lanes{load_uint_lanes<u64, 2uz>(v)};
                for(auto lane: lanes.lane)
                {
                    if(lane == 0u) { return wasm_i32{}; }
                }
                return wasm_i32{1};
            }
            else if constexpr(Op == simd_code::i8x16_bitmask)
            {
                auto const lanes{load_uint_lanes<u8, 16uz>(v)};
                u32 out{};
                for(::std::size_t i{}; i != 16uz; ++i) { out |= static_cast<u32>((lanes.lane[i] >> 7u) & 1u) << i; }
                return ::std::bit_cast<wasm_i32>(out);
            }
            else if constexpr(Op == simd_code::i16x8_bitmask)
            {
                auto const lanes{load_uint_lanes<u16, 8uz>(v)};
                u32 out{};
                for(::std::size_t i{}; i != 8uz; ++i) { out |= static_cast<u32>((lanes.lane[i] >> 15u) & 1u) << i; }
                return ::std::bit_cast<wasm_i32>(out);
            }
            else if constexpr(Op == simd_code::i32x4_bitmask)
            {
                auto const lanes{load_uint_lanes<u32, 4uz>(v)};
                u32 out{};
                for(::std::size_t i{}; i != 4uz; ++i) { out |= static_cast<u32>((lanes.lane[i] >> 31u) & 1u) << i; }
                return ::std::bit_cast<wasm_i32>(out);
            }
            else
            {
                static_assert(Op == simd_code::i64x2_bitmask);
                auto const lanes{load_uint_lanes<u64, 2uz>(v)};
                u32 out{};
                for(::std::size_t i{}; i != 2uz; ++i) { out |= static_cast<u32>((lanes.lane[i] >> 63u) & 1u) << i; }
                return ::std::bit_cast<wasm_i32>(out);
            }
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline U read_memory_lane(::std::byte const* p) noexcept
        {
            U v;  // no init
            ::std::memcpy(::std::addressof(v), p, sizeof(v));
            return ::fast_io::little_endian(v);
        }

        template <typename U>
        UWVM_ALWAYS_INLINE inline void write_memory_lane(::std::byte* p, U v) noexcept
        {
            auto le{::fast_io::little_endian(v)};
            ::std::memcpy(p, ::std::addressof(le), sizeof(le));
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::size_t simd_memory_access_size() noexcept
        {
            if constexpr(Op == simd_code::v128_load || Op == simd_code::v128_store) { return 16uz; }
            else if constexpr(Op == simd_code::v128_load8x8_s || Op == simd_code::v128_load8x8_u || Op == simd_code::v128_load8_splat ||
                              Op == simd_code::v128_load8_lane || Op == simd_code::v128_store8_lane)
            {
                return 1uz;
            }
            else if constexpr(Op == simd_code::v128_load16x4_s || Op == simd_code::v128_load16x4_u || Op == simd_code::v128_load16_splat ||
                              Op == simd_code::v128_load16_lane || Op == simd_code::v128_store16_lane)
            {
                return 2uz;
            }
            else if constexpr(Op == simd_code::v128_load32x2_s || Op == simd_code::v128_load32x2_u || Op == simd_code::v128_load32_splat ||
                              Op == simd_code::v128_load32_zero || Op == simd_code::v128_load32_lane || Op == simd_code::v128_store32_lane)
            {
                return 4uz;
            }
            else
            {
                static_assert(Op == simd_code::v128_load64_splat || Op == simd_code::v128_load64_zero || Op == simd_code::v128_load64_lane ||
                              Op == simd_code::v128_store64_lane);
                return 8uz;
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline wasm_v128 eval_memory_load(::std::byte const* p, wasm_v128 old, u8 lane) noexcept
        {
            if constexpr(Op == simd_code::v128_load)
            {
                wasm_v128 out;  // no init
                ::std::memcpy(::std::addressof(out), p, sizeof(out));
                return out;
            }
            else if constexpr(Op == simd_code::v128_load8x8_s || Op == simd_code::v128_load8x8_u)
            {
                lane_array<u16, 8uz> out{};  // init
                for(::std::size_t i{}; i != 8uz; ++i)
                {
                    auto const x{read_memory_lane<u8>(p + i)};
                    if constexpr(Op == simd_code::v128_load8x8_s) { out.lane[i] = from_signed_lane<u16>(static_cast<s16>(as_signed_lane<u8>(x))); }
                    else
                    {
                        out.lane[i] = x;
                    }
                }
                return store_uint_lanes<u16, 8uz>(out);
            }
            else if constexpr(Op == simd_code::v128_load16x4_s || Op == simd_code::v128_load16x4_u)
            {
                lane_array<u32, 4uz> out{};  // init
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    auto const x{read_memory_lane<u16>(p + i * 2uz)};
                    if constexpr(Op == simd_code::v128_load16x4_s) { out.lane[i] = from_signed_lane<u32>(static_cast<s32>(as_signed_lane<u16>(x))); }
                    else
                    {
                        out.lane[i] = x;
                    }
                }
                return store_uint_lanes<u32, 4uz>(out);
            }
            else if constexpr(Op == simd_code::v128_load32x2_s || Op == simd_code::v128_load32x2_u)
            {
                lane_array<u64, 2uz> out{};  // init
                for(::std::size_t i{}; i != 2uz; ++i)
                {
                    auto const x{read_memory_lane<u32>(p + i * 4uz)};
                    if constexpr(Op == simd_code::v128_load32x2_s) { out.lane[i] = from_signed_lane<u64>(static_cast<s64>(as_signed_lane<u32>(x))); }
                    else
                    {
                        out.lane[i] = x;
                    }
                }
                return store_uint_lanes<u64, 2uz>(out);
            }
            else if constexpr(Op == simd_code::v128_load8_splat)
            {
                lane_array<u8, 16uz> out{};  // init
                auto const x{read_memory_lane<u8>(p)};
                for(auto& v: out.lane) { v = x; }
                return store_uint_lanes<u8, 16uz>(out);
            }
            else if constexpr(Op == simd_code::v128_load16_splat)
            {
                lane_array<u16, 8uz> out{};  // init
                auto const x{read_memory_lane<u16>(p)};
                for(auto& v: out.lane) { v = x; }
                return store_uint_lanes<u16, 8uz>(out);
            }
            else if constexpr(Op == simd_code::v128_load32_splat)
            {
                lane_array<u32, 4uz> out{};  // init
                auto const x{read_memory_lane<u32>(p)};
                for(auto& v: out.lane) { v = x; }
                return store_uint_lanes<u32, 4uz>(out);
            }
            else if constexpr(Op == simd_code::v128_load64_splat)
            {
                lane_array<u64, 2uz> out{};  // init
                auto const x{read_memory_lane<u64>(p)};
                for(auto& v: out.lane) { v = x; }
                return store_uint_lanes<u64, 2uz>(out);
            }
            else if constexpr(Op == simd_code::v128_load32_zero)
            {
                lane_array<u32, 4uz> out{};  // init
                out.lane[0] = read_memory_lane<u32>(p);
                return store_uint_lanes<u32, 4uz>(out);
            }
            else if constexpr(Op == simd_code::v128_load64_zero)
            {
                lane_array<u64, 2uz> out{};  // init
                out.lane[0] = read_memory_lane<u64>(p);
                return store_uint_lanes<u64, 2uz>(out);
            }
            else if constexpr(Op == simd_code::v128_load8_lane)
            {
                auto out{load_uint_lanes<u8, 16uz>(old)};
                out.lane[lane] = read_memory_lane<u8>(p);
                return store_uint_lanes<u8, 16uz>(out);
            }
            else if constexpr(Op == simd_code::v128_load16_lane)
            {
                auto out{load_uint_lanes<u16, 8uz>(old)};
                out.lane[lane] = read_memory_lane<u16>(p);
                return store_uint_lanes<u16, 8uz>(out);
            }
            else if constexpr(Op == simd_code::v128_load32_lane)
            {
                auto out{load_uint_lanes<u32, 4uz>(old)};
                out.lane[lane] = read_memory_lane<u32>(p);
                return store_uint_lanes<u32, 4uz>(out);
            }
            else
            {
                static_assert(Op == simd_code::v128_load64_lane);
                auto out{load_uint_lanes<u64, 2uz>(old)};
                out.lane[lane] = read_memory_lane<u64>(p);
                return store_uint_lanes<u64, 2uz>(out);
            }
        }

        template <simd_code Op>
        UWVM_ALWAYS_INLINE inline void eval_memory_store(::std::byte* p, wasm_v128 value, u8 lane) noexcept
        {
            if constexpr(Op == simd_code::v128_store) { ::std::memcpy(p, ::std::addressof(value), sizeof(value)); }
            else if constexpr(Op == simd_code::v128_store8_lane)
            {
                auto const lanes{load_uint_lanes<u8, 16uz>(value)};
                write_memory_lane(p, lanes.lane[lane]);
            }
            else if constexpr(Op == simd_code::v128_store16_lane)
            {
                auto const lanes{load_uint_lanes<u16, 8uz>(value)};
                write_memory_lane(p, lanes.lane[lane]);
            }
            else if constexpr(Op == simd_code::v128_store32_lane)
            {
                auto const lanes{load_uint_lanes<u32, 4uz>(value)};
                write_memory_lane(p, lanes.lane[lane]);
            }
            else
            {
                static_assert(Op == simd_code::v128_store64_lane);
                auto const lanes{load_uint_lanes<u64, 2uz>(value)};
                write_memory_lane(p, lanes.lane[lane]);
            }
        }
    }  // namespace wasm1p1_simd_details

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_unop Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_unop(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const out{wasm1p1_simd_details::eval_v128_unop<Op>(v)};
        wasm1p1_simd_details::push_v128_to_memory_stack(out, type...[1u]);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_unop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_unop(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_v128_unop<Op>(v)};
        wasm1p1_simd_details::push_v128_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_binop Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_binop(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const out{wasm1p1_simd_details::eval_v128_binop<Op>(lhs, rhs)};
        wasm1p1_simd_details::push_v128_to_memory_stack(out, type...[1u]);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_binop(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_v128_binop<Op>(lhs, rhs)};
        wasm1p1_simd_details::push_v128_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_testop Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_testop(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const out{wasm1p1_simd_details::eval_v128_testop<Op>(v)};
        wasm1p1_simd_details::push_i32_to_memory_stack(out, type...[1u]);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_testop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_testop(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_v128_testop<Op>(v)};
        wasm1p1_simd_details::push_i32_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_splatop Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_i32x4_splat(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(type...)};
        auto const out{wasm1p1_simd_details::eval_v128_splat_i32<Op>(v)};
        wasm1p1_simd_details::push_v128_to_memory_stack(out, type...[1u]);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_splatop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_i32x4_splat(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_v128_splat_i32<Op>(v)};
        wasm1p1_simd_details::push_v128_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_splatop Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_f32x4_splat(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_f32>(type...)};
        auto const out{wasm1p1_simd_details::eval_v128_splat_f32<Op>(v)};
        wasm1p1_simd_details::push_v128_to_memory_stack(out, type...[1u]);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_splatop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_f32x4_splat(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_f32>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_v128_splat_f32<Op>(v)};
        wasm1p1_simd_details::push_v128_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Lane, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_f32x4_extract_lane(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const out{wasm1p1_simd_details::eval_f32x4_extract_lane<Lane>(v)};
        wasm1p1_simd_details::push_f32_to_memory_stack(out, type...[1u]);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Lane, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_f32x4_extract_lane(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_f32x4_extract_lane<Lane>(v)};
        wasm1p1_simd_details::push_f32_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_load(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(type...[0])};
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(type...[0])};
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(type...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(memory);
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::check_memory_bounds_unlocked(memory,
                                                                                             0uz,
                                                                                             static_cast<::std::uint_least64_t>(offset),
                                                                                             eff65,
                                                                                             sizeof(wasm1p1_simd_details::wasm_v128));

        wasm1p1_simd_details::wasm_v128 out;  // no init
        ::std::memcpy(::std::addressof(out),
                      ::uwvm2::runtime::compiler::uwvm_int::optable::details::ptr_add_u64(memory.memory_begin, eff65.offset),
                      sizeof(out));
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(memory);
        wasm1p1_simd_details::push_v128_to_memory_stack(out, type...[1u]);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_load(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(typeref...[0])};
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(typeref...[0])};
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(typeref...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        [[maybe_unused]] auto lock{::uwvm2::runtime::compiler::uwvm_int::optable::details::lock_memory(memory)};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::check_memory_bounds_unlocked(memory,
                                                                                             0uz,
                                                                                             static_cast<::std::uint_least64_t>(offset),
                                                                                             eff65,
                                                                                             sizeof(wasm1p1_simd_details::wasm_v128));

        wasm1p1_simd_details::wasm_v128 out;  // no init
        ::std::memcpy(::std::addressof(out),
                      ::uwvm2::runtime::compiler::uwvm_int::optable::details::ptr_add_u64(memory.memory_begin, eff65.offset),
                      sizeof(out));
        wasm1p1_simd_details::push_v128_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_store(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(type...[0])};
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(type...[0])};
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(type...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(memory);
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::check_memory_bounds_unlocked(memory,
                                                                                             0uz,
                                                                                             static_cast<::std::uint_least64_t>(offset),
                                                                                             eff65,
                                                                                             sizeof(value));
        ::std::memcpy(::uwvm2::runtime::compiler::uwvm_int::optable::details::ptr_add_u64(memory.memory_begin, eff65.offset),
                      ::std::addressof(value),
                      sizeof(value));
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(memory);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_store(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(typeref...[0])};
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(typeref...[0])};
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(typeref...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        [[maybe_unused]] auto lock{::uwvm2::runtime::compiler::uwvm_int::optable::details::lock_memory(memory)};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::check_memory_bounds_unlocked(memory,
                                                                                             0uz,
                                                                                             static_cast<::std::uint_least64_t>(offset),
                                                                                             eff65,
                                                                                             sizeof(value));
        ::std::memcpy(::uwvm2::runtime::compiler::uwvm_int::optable::details::ptr_add_u64(memory.memory_begin, eff65.offset),
                      ::std::addressof(value),
                      sizeof(value));
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_unop(Type... type) UWVM_THROWS
    {
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const out{wasm1p1_simd_details::eval_full_unop<Op>(v)};
        wasm1p1_simd_details::push_to_memory_stack(out, type...[1u]);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_unop(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_full_unop<Op>(v)};
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_binop(Type... type) UWVM_THROWS
    {
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const out{wasm1p1_simd_details::eval_full_binop<Op>(lhs, rhs)};
        wasm1p1_simd_details::push_to_memory_stack(out, type...[1u]);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_binop(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_full_binop<Op>(lhs, rhs)};
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_bitselect(Type... type) UWVM_THROWS
    {
        auto const mask{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const out{wasm1p1_simd_details::eval_bitselect(lhs, rhs, mask)};
        wasm1p1_simd_details::push_to_memory_stack(out, type...[1u]);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_bitselect(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const mask{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_bitselect(lhs, rhs, mask)};
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_testop(Type... type) UWVM_THROWS
    {
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const out{wasm1p1_simd_details::eval_full_test<Op>(v)};
        wasm1p1_simd_details::push_to_memory_stack(out, type...[1u]);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_testop(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_full_test<Op>(v)};
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_shift(Type... type) UWVM_THROWS
    {
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(type...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const out{wasm1p1_simd_details::eval_full_shift<Op>(lhs, rhs)};
        wasm1p1_simd_details::push_to_memory_stack(out, type...[1u]);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_shift(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(typeref...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_full_shift<Op>(lhs, rhs)};
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, typename ScalarT, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_splat(Type... type) UWVM_THROWS
    {
        auto const v{get_curr_val_from_operand_stack_cache<ScalarT>(type...)};
        wasm1p1_simd_details::wasm_v128 out;  // no init
        if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i32>) { out = wasm1p1_simd_details::eval_full_splat_i32<Op>(v); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i64>) { out = wasm1p1_simd_details::eval_full_splat_i64<Op>(v); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f32>) { out = wasm1p1_simd_details::eval_full_splat_f32<Op>(v); }
        else
        {
            out = wasm1p1_simd_details::eval_full_splat_f64<Op>(v);
        }
        wasm1p1_simd_details::push_to_memory_stack(out, type...[1u]);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, typename ScalarT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_splat(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const v{get_curr_val_from_operand_stack_cache<ScalarT>(typeref...)};
        wasm1p1_simd_details::wasm_v128 out;  // no init
        if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i32>) { out = wasm1p1_simd_details::eval_full_splat_i32<Op>(v); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i64>) { out = wasm1p1_simd_details::eval_full_splat_i64<Op>(v); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f32>) { out = wasm1p1_simd_details::eval_full_splat_f32<Op>(v); }
        else
        {
            out = wasm1p1_simd_details::eval_full_splat_f64<Op>(v);
        }
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, typename ScalarT, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_extract_lane(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const lane{wasm1p1_details::read_imm<wasm1p1_simd_details::u8>(type...[0])};
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        ScalarT out;  // no init
        if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i32>) { out = wasm1p1_simd_details::eval_extract_lane_i32<Op>(v, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i64>) { out = wasm1p1_simd_details::eval_extract_lane_i64<Op>(v, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f32>) { out = wasm1p1_simd_details::eval_extract_lane_f32<Op>(v, lane); }
        else
        {
            out = wasm1p1_simd_details::eval_extract_lane_f64<Op>(v, lane);
        }
        wasm1p1_simd_details::push_to_memory_stack(out, type...[1u]);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, typename ScalarT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_extract_lane(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const lane{wasm1p1_details::read_imm<wasm1p1_simd_details::u8>(typeref...[0])};
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        ScalarT out;  // no init
        if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i32>) { out = wasm1p1_simd_details::eval_extract_lane_i32<Op>(v, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i64>) { out = wasm1p1_simd_details::eval_extract_lane_i64<Op>(v, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f32>) { out = wasm1p1_simd_details::eval_extract_lane_f32<Op>(v, lane); }
        else
        {
            out = wasm1p1_simd_details::eval_extract_lane_f64<Op>(v, lane);
        }
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, typename ScalarT, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_replace_lane(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const lane{wasm1p1_details::read_imm<wasm1p1_simd_details::u8>(type...[0])};
        auto const x{get_curr_val_from_operand_stack_cache<ScalarT>(type...)};
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        wasm1p1_simd_details::wasm_v128 out;  // no init
        if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i32>) { out = wasm1p1_simd_details::eval_replace_lane_i32<Op>(v, x, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i64>) { out = wasm1p1_simd_details::eval_replace_lane_i64<Op>(v, x, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f32>) { out = wasm1p1_simd_details::eval_replace_lane_f32<Op>(v, x, lane); }
        else
        {
            out = wasm1p1_simd_details::eval_replace_lane_f64<Op>(v, x, lane);
        }
        wasm1p1_simd_details::push_to_memory_stack(out, type...[1u]);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, typename ScalarT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_replace_lane(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const lane{wasm1p1_details::read_imm<wasm1p1_simd_details::u8>(typeref...[0])};
        auto const x{get_curr_val_from_operand_stack_cache<ScalarT>(typeref...)};
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        wasm1p1_simd_details::wasm_v128 out;  // no init
        if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i32>) { out = wasm1p1_simd_details::eval_replace_lane_i32<Op>(v, x, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i64>) { out = wasm1p1_simd_details::eval_replace_lane_i64<Op>(v, x, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f32>) { out = wasm1p1_simd_details::eval_replace_lane_f32<Op>(v, x, lane); }
        else
        {
            out = wasm1p1_simd_details::eval_replace_lane_f64<Op>(v, x, lane);
        }
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_shuffle(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        wasm1p1_simd_details::u8 lanes[16]{};  // init
        ::std::memcpy(lanes, type...[0], sizeof(lanes));
        type...[0] += sizeof(lanes);
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const out{wasm1p1_simd_details::eval_shuffle(lhs, rhs, lanes)};
        wasm1p1_simd_details::push_to_memory_stack(out, type...[1u]);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_shuffle(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        wasm1p1_simd_details::u8 lanes[16]{};  // init
        ::std::memcpy(lanes, typeref...[0], sizeof(lanes));
        typeref...[0] += sizeof(lanes);
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_shuffle(lhs, rhs, lanes)};
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_mem_load(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(type...[0])};
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(type...[0])};
        wasm1p1_simd_details::u8 lane{};
        if constexpr(Op == wasm1p1_simd_details::simd_code::v128_load8_lane || Op == wasm1p1_simd_details::simd_code::v128_load16_lane ||
                     Op == wasm1p1_simd_details::simd_code::v128_load32_lane || Op == wasm1p1_simd_details::simd_code::v128_load64_lane)
        {
            lane = wasm1p1_details::read_imm<wasm1p1_simd_details::u8>(type...[0]);
        }
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        wasm1p1_simd_details::wasm_v128 old{};  // init
        if constexpr(Op == wasm1p1_simd_details::simd_code::v128_load8_lane || Op == wasm1p1_simd_details::simd_code::v128_load16_lane ||
                     Op == wasm1p1_simd_details::simd_code::v128_load32_lane || Op == wasm1p1_simd_details::simd_code::v128_load64_lane)
        {
            old = get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...);
        }
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(type...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(memory);
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::check_memory_bounds_unlocked(memory,
                                                                                             0uz,
                                                                                             static_cast<::std::uint_least64_t>(offset),
                                                                                             eff65,
                                                                                             wasm1p1_simd_details::simd_memory_access_size<Op>());
        auto const out{
            wasm1p1_simd_details::eval_memory_load<Op>(::uwvm2::runtime::compiler::uwvm_int::optable::details::ptr_add_u64(memory.memory_begin, eff65.offset),
                                                       old,
                                                       lane)};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(memory);
        wasm1p1_simd_details::push_to_memory_stack(out, type...[1u]);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_mem_load(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(typeref...[0])};
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(typeref...[0])};
        wasm1p1_simd_details::u8 lane{};
        if constexpr(Op == wasm1p1_simd_details::simd_code::v128_load8_lane || Op == wasm1p1_simd_details::simd_code::v128_load16_lane ||
                     Op == wasm1p1_simd_details::simd_code::v128_load32_lane || Op == wasm1p1_simd_details::simd_code::v128_load64_lane)
        {
            lane = wasm1p1_details::read_imm<wasm1p1_simd_details::u8>(typeref...[0]);
        }
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        wasm1p1_simd_details::wasm_v128 old{};  // init
        if constexpr(Op == wasm1p1_simd_details::simd_code::v128_load8_lane || Op == wasm1p1_simd_details::simd_code::v128_load16_lane ||
                     Op == wasm1p1_simd_details::simd_code::v128_load32_lane || Op == wasm1p1_simd_details::simd_code::v128_load64_lane)
        {
            old = get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...);
        }
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(typeref...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        [[maybe_unused]] auto lock{::uwvm2::runtime::compiler::uwvm_int::optable::details::lock_memory(memory)};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::check_memory_bounds_unlocked(memory,
                                                                                             0uz,
                                                                                             static_cast<::std::uint_least64_t>(offset),
                                                                                             eff65,
                                                                                             wasm1p1_simd_details::simd_memory_access_size<Op>());
        auto const out{
            wasm1p1_simd_details::eval_memory_load<Op>(::uwvm2::runtime::compiler::uwvm_int::optable::details::ptr_add_u64(memory.memory_begin, eff65.offset),
                                                       old,
                                                       lane)};
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_mem_store(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(type...[0])};
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(type...[0])};
        wasm1p1_simd_details::u8 lane{};
        if constexpr(Op != wasm1p1_simd_details::simd_code::v128_store) { lane = wasm1p1_details::read_imm<wasm1p1_simd_details::u8>(type...[0]); }
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(type...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(memory);
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::check_memory_bounds_unlocked(memory,
                                                                                             0uz,
                                                                                             static_cast<::std::uint_least64_t>(offset),
                                                                                             eff65,
                                                                                             wasm1p1_simd_details::simd_memory_access_size<Op>());
        wasm1p1_simd_details::eval_memory_store<Op>(::uwvm2::runtime::compiler::uwvm_int::optable::details::ptr_add_u64(memory.memory_begin, eff65.offset),
                                                    value,
                                                    lane);
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(memory);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_mem_store(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(typeref...[0])};
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(typeref...[0])};
        wasm1p1_simd_details::u8 lane{};
        if constexpr(Op != wasm1p1_simd_details::simd_code::v128_store) { lane = wasm1p1_details::read_imm<wasm1p1_simd_details::u8>(typeref...[0]); }
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(typeref...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        [[maybe_unused]] auto lock{::uwvm2::runtime::compiler::uwvm_int::optable::details::lock_memory(memory)};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::check_memory_bounds_unlocked(memory,
                                                                                             0uz,
                                                                                             static_cast<::std::uint_least64_t>(offset),
                                                                                             eff65,
                                                                                             wasm1p1_simd_details::simd_memory_access_size<Op>());
        wasm1p1_simd_details::eval_memory_store<Op>(::uwvm2::runtime::compiler::uwvm_int::optable::details::ptr_add_u64(memory.memory_begin, eff65.offset),
                                                    value,
                                                    lane);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_v128_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_v128_const(Type... type) UWVM_THROWS
    {
        using wasm_v128 = wasm1p1_details::wasm_v128;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        wasm_v128 imm;  // no init
        ::std::memcpy(::std::addressof(imm), type...[0], sizeof(imm));
        type...[0] += sizeof(imm);

        if constexpr(wasm1p1_details::stacktop_enabled_for<CompileOption, wasm_v128>())
        {
            constexpr ::std::size_t begin_pos{CompileOption.v128_stack_top_begin_pos};
            constexpr ::std::size_t end_pos{CompileOption.v128_stack_top_end_pos};
            static_assert(begin_pos <= curr_v128_stack_top && curr_v128_stack_top < end_pos);
            constexpr ::std::size_t new_pos{details::ring_prev_pos(curr_v128_stack_top, begin_pos, end_pos)};
            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_v128, new_pos>(imm, type...);
        }
        else
        {
            ::std::memcpy(type...[1u], ::std::addressof(imm), sizeof(imm));
            type...[1u] += sizeof(imm);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_v128_const(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_v128 = wasm1p1_details::wasm_v128;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_v128 imm;  // no init
        ::std::memcpy(::std::addressof(imm), typeref...[0], sizeof(imm));
        typeref...[0] += sizeof(imm);

        ::std::memcpy(typeref...[1u], ::std::addressof(imm), sizeof(imm));
        typeref...[1u] += sizeof(imm);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_ref_null(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        RefT out{};  // null ref: zero storage plus wasm_null kind
        out.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_null;

        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        type...[1u] += sizeof(out);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_ref_null(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        RefT out{};
        out.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_null;
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, typename RefT, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_ref_is_null(Type... type) UWVM_THROWS
    {
        using wasm_i32 = wasm1p1_details::wasm_i32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        RefT const ref{get_curr_val_from_operand_stack_cache<RefT>(type...)};
        wasm_i32 const out{ref.ref.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null ? wasm_i32{1} : wasm_i32{}};
        wasm1p1_details::push_out_value<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_ref_is_null(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = wasm1p1_details::wasm_i32;

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        RefT const ref{get_curr_val_from_operand_stack_cache<RefT>(typeref...)};
        wasm_i32 const out{ref.ref.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null ? wasm_i32{1} : wasm_i32{}};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_ref_func(Type... type) UWVM_THROWS
    {
        using wasm_u32 = wasm1p1_details::wasm_u32;
        using wasm_funcref = wasm1p1_details::wasm_funcref;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(type...[0])};
        auto const func_index{wasm1p1_details::read_imm<wasm_u32>(type...[0])};
        wasm_funcref const out{wasm1p1_details::funcref_from_table_elem(wasm1p1_details::resolve_table_elem_from_func_index(module, func_index))};

        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        type...[1u] += sizeof(out);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_ref_func(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_u32 = wasm1p1_details::wasm_u32;
        using wasm_funcref = wasm1p1_details::wasm_funcref;

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(typeref...[0])};
        auto const func_index{wasm1p1_details::read_imm<wasm_u32>(typeref...[0])};
        wasm_funcref const out{wasm1p1_details::funcref_from_table_elem(wasm1p1_details::resolve_table_elem_from_func_index(module, func_index))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_get_funcref(Type... type) UWVM_THROWS
    {
        using wasm_funcref = wasm1p1_details::wasm_funcref;

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const index{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }

        wasm_funcref const out{wasm1p1_details::funcref_from_table_elem(table->elems.index_unchecked(index))};
        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        type...[1u] += sizeof(out);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_get_funcref(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_funcref = wasm1p1_details::wasm_funcref;

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const index{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }

        wasm_funcref const out{wasm1p1_details::funcref_from_table_elem(table->elems.index_unchecked(index))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_set_funcref(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(type...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_funcref>(type...)};
        auto const index{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }

        table->elems.index_unchecked(index) = wasm1p1_details::table_elem_from_funcref(module, value);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_set_funcref(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(typeref...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_funcref>(typeref...)};
        auto const index{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }

        table->elems.index_unchecked(index) = wasm1p1_details::table_elem_from_funcref(module, value);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_data_drop(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const data{wasm1p1_details::read_imm<wasm1p1_details::runtime_data_storage_t*>(type...[0])};
        if(data == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        data->data.dropped = true;
        data->data.byte_begin = nullptr;
        data->data.byte_end = nullptr;

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_data_drop(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const data{wasm1p1_details::read_imm<wasm1p1_details::runtime_data_storage_t*>(typeref...[0])};
        if(data == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        data->data.dropped = true;
        data->data.byte_begin = nullptr;
        data->data.byte_end = nullptr;
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_elem_drop(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const element{wasm1p1_details::read_imm<wasm1p1_details::runtime_element_storage_t*>(type...[0])};
        if(element == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        element->element.dropped = true;
        element->element.funcidx_begin = nullptr;
        element->element.funcidx_end = nullptr;

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_elem_drop(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const element{wasm1p1_details::read_imm<wasm1p1_details::runtime_element_storage_t*>(typeref...[0])};
        if(element == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        element->element.dropped = true;
        element->element.funcidx_begin = nullptr;
        element->element.funcidx_end = nullptr;
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory_init(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_details::native_memory_t*>(type...[0])};
        auto const data{wasm1p1_details::read_imm<wasm1p1_details::runtime_data_storage_t*>(type...[0])};
        if(memory_p == nullptr || data == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const src{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};

        auto const data_begin{data->data.byte_begin};
        auto const data_end{data->data.byte_end};
        if((data_begin == nullptr) != (data_end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const data_len{data_begin == nullptr ? 0uz : static_cast<::std::size_t>(data_end - data_begin)};

        if(wasm1p1_details::range_oob(src, len, data_len)) [[unlikely]]
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::details::memory_oob_terminate(0uz,
                                                                                         0uz,
                                                                                         {static_cast<::std::uint_least64_t>(src), false},
                                                                                         data_len,
                                                                                         len);
        }

        auto& memory{*memory_p};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(memory);
        auto const memory_length{::uwvm2::runtime::compiler::uwvm_int::optable::details::load_memory_length_for_oob_unlocked(memory)};
        if(wasm1p1_details::range_oob(dst, len, memory_length)) [[unlikely]]
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::details::memory_oob_terminate(0uz,
                                                                                         0uz,
                                                                                         {static_cast<::std::uint_least64_t>(dst), false},
                                                                                         memory_length,
                                                                                         len);
        }
        if(len != 0uz) { ::std::memcpy(memory.memory_begin + dst, data_begin + src, len); }
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(memory);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory_init(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_details::native_memory_t*>(typeref...[0])};
        auto const data{wasm1p1_details::read_imm<wasm1p1_details::runtime_data_storage_t*>(typeref...[0])};
        if(memory_p == nullptr || data == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const src{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};

        auto const data_begin{data->data.byte_begin};
        auto const data_end{data->data.byte_end};
        if((data_begin == nullptr) != (data_end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const data_len{data_begin == nullptr ? 0uz : static_cast<::std::size_t>(data_end - data_begin)};
        if(wasm1p1_details::range_oob(src, len, data_len)) [[unlikely]]
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::details::memory_oob_terminate(0uz,
                                                                                         0uz,
                                                                                         {static_cast<::std::uint_least64_t>(src), false},
                                                                                         data_len,
                                                                                         len);
        }

        auto& memory{*memory_p};
        [[maybe_unused]] auto lock{::uwvm2::runtime::compiler::uwvm_int::optable::details::lock_memory(memory)};
        auto const memory_length{::uwvm2::runtime::compiler::uwvm_int::optable::details::load_memory_length_for_oob_unlocked(memory)};
        if(wasm1p1_details::range_oob(dst, len, memory_length)) [[unlikely]]
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::details::memory_oob_terminate(0uz,
                                                                                         0uz,
                                                                                         {static_cast<::std::uint_least64_t>(dst), false},
                                                                                         memory_length,
                                                                                         len);
        }
        if(len != 0uz) { ::std::memcpy(memory.memory_begin + dst, data_begin + src, len); }
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory_copy(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_details::native_memory_t*>(type...[0])};
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const src{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};

        auto& memory{*memory_p};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(memory);
        auto const memory_length{::uwvm2::runtime::compiler::uwvm_int::optable::details::load_memory_length_for_oob_unlocked(memory)};
        if(wasm1p1_details::range_oob(src, len, memory_length) || wasm1p1_details::range_oob(dst, len, memory_length)) [[unlikely]]
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::details::memory_oob_terminate(0uz,
                                                                                         0uz,
                                                                                         {static_cast<::std::uint_least64_t>(dst), false},
                                                                                         memory_length,
                                                                                         len);
        }
        if(len != 0uz) { ::std::memmove(memory.memory_begin + dst, memory.memory_begin + src, len); }
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(memory);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory_copy(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_details::native_memory_t*>(typeref...[0])};
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const src{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};

        auto& memory{*memory_p};
        [[maybe_unused]] auto lock{::uwvm2::runtime::compiler::uwvm_int::optable::details::lock_memory(memory)};
        auto const memory_length{::uwvm2::runtime::compiler::uwvm_int::optable::details::load_memory_length_for_oob_unlocked(memory)};
        if(wasm1p1_details::range_oob(src, len, memory_length) || wasm1p1_details::range_oob(dst, len, memory_length)) [[unlikely]]
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::details::memory_oob_terminate(0uz,
                                                                                         0uz,
                                                                                         {static_cast<::std::uint_least64_t>(dst), false},
                                                                                         memory_length,
                                                                                         len);
        }
        if(len != 0uz) { ::std::memmove(memory.memory_begin + dst, memory.memory_begin + src, len); }
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory_fill(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_details::native_memory_t*>(type...[0])};
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const value{static_cast<unsigned char>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};

        auto& memory{*memory_p};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(memory);
        auto const memory_length{::uwvm2::runtime::compiler::uwvm_int::optable::details::load_memory_length_for_oob_unlocked(memory)};
        if(wasm1p1_details::range_oob(dst, len, memory_length)) [[unlikely]]
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::details::memory_oob_terminate(0uz,
                                                                                         0uz,
                                                                                         {static_cast<::std::uint_least64_t>(dst), false},
                                                                                         memory_length,
                                                                                         len);
        }
        if(len != 0uz) { ::std::memset(memory.memory_begin + dst, value, len); }
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(memory);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory_fill(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_details::native_memory_t*>(typeref...[0])};
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const value{static_cast<unsigned char>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};

        auto& memory{*memory_p};
        [[maybe_unused]] auto lock{::uwvm2::runtime::compiler::uwvm_int::optable::details::lock_memory(memory)};
        auto const memory_length{::uwvm2::runtime::compiler::uwvm_int::optable::details::load_memory_length_for_oob_unlocked(memory)};
        if(wasm1p1_details::range_oob(dst, len, memory_length)) [[unlikely]]
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::details::memory_oob_terminate(0uz,
                                                                                         0uz,
                                                                                         {static_cast<::std::uint_least64_t>(dst), false},
                                                                                         memory_length,
                                                                                         len);
        }
        if(len != 0uz) { ::std::memset(memory.memory_begin + dst, value, len); }
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_init_funcref(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        auto const element{wasm1p1_details::read_imm<wasm1p1_details::runtime_element_storage_t*>(type...[0])};
        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(type...[0])};
        if(table == nullptr || element == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const src{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};

        auto const begin{element->element.funcidx_begin};
        auto const end{element->element.funcidx_end};
        if((begin == nullptr) != (end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const elem_len{begin == nullptr ? 0uz : static_cast<::std::size_t>(end - begin)};
        wasm1p1_details::check_table_range(src, len, elem_len);
        wasm1p1_details::check_table_range(dst, len, table->elems.size());

        for(::std::size_t i{}; i != len; ++i)
        {
            auto const funcidx{begin[src + i]};
            table->elems.index_unchecked(dst + i) = funcidx == (::std::numeric_limits<wasm1p1_details::wasm_u32>::max)()
                                                        ? wasm1p1_details::runtime_table_elem_storage_t{}
                                                        : wasm1p1_details::resolve_table_elem_from_func_index(module, funcidx);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_init_funcref(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        auto const element{wasm1p1_details::read_imm<wasm1p1_details::runtime_element_storage_t*>(typeref...[0])};
        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(typeref...[0])};
        if(table == nullptr || element == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const src{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};

        auto const begin{element->element.funcidx_begin};
        auto const end{element->element.funcidx_end};
        if((begin == nullptr) != (end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const elem_len{begin == nullptr ? 0uz : static_cast<::std::size_t>(end - begin)};
        wasm1p1_details::check_table_range(src, len, elem_len);
        wasm1p1_details::check_table_range(dst, len, table->elems.size());

        for(::std::size_t i{}; i != len; ++i)
        {
            auto const funcidx{begin[src + i]};
            table->elems.index_unchecked(dst + i) = funcidx == (::std::numeric_limits<wasm1p1_details::wasm_u32>::max)()
                                                        ? wasm1p1_details::runtime_table_elem_storage_t{}
                                                        : wasm1p1_details::resolve_table_elem_from_func_index(module, funcidx);
        }
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_copy_funcref(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const dst_table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        auto const src_table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        if(dst_table == nullptr || src_table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const src{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        wasm1p1_details::check_table_range(src, len, src_table->elems.size());
        wasm1p1_details::check_table_range(dst, len, dst_table->elems.size());
        if(len != 0uz)
        {
            ::std::memmove(dst_table->elems.data() + dst, src_table->elems.data() + src, len * sizeof(wasm1p1_details::runtime_table_elem_storage_t));
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_copy_funcref(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const dst_table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        auto const src_table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        if(dst_table == nullptr || src_table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const src{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        wasm1p1_details::check_table_range(src, len, src_table->elems.size());
        wasm1p1_details::check_table_range(dst, len, dst_table->elems.size());
        if(len != 0uz)
        {
            ::std::memmove(dst_table->elems.data() + dst, src_table->elems.data() + src, len * sizeof(wasm1p1_details::runtime_table_elem_storage_t));
        }
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_grow_funcref(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(type...[0])};
        if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const delta{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_funcref>(type...)};
        auto const old_size{table->elems.size()};
        wasm1p1_details::wasm_i32 out{wasm1p1_details::u32_to_i32((::std::numeric_limits<::std::uint_least32_t>::max)())};

        auto const& limits{table->table_type_ptr->limits};
        auto const max_size{static_cast<::std::size_t>(limits.max)};
        if(old_size <= max_size && delta <= max_size - old_size)
        {
            auto const new_size{old_size + delta};
            auto const elem{wasm1p1_details::table_elem_from_funcref(module, value)};
            table->elems.resize(new_size);
            for(::std::size_t i{old_size}; i != new_size; ++i) { table->elems.index_unchecked(i) = elem; }
            out = wasm1p1_details::u32_to_i32(static_cast<::std::uint_least32_t>(old_size));
        }

        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        type...[1u] += sizeof(out);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_grow_funcref(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(typeref...[0])};
        if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const delta{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_funcref>(typeref...)};
        auto const old_size{table->elems.size()};
        wasm1p1_details::wasm_i32 out{wasm1p1_details::u32_to_i32((::std::numeric_limits<::std::uint_least32_t>::max)())};

        auto const& limits{table->table_type_ptr->limits};
        auto const max_size{static_cast<::std::size_t>(limits.max)};
        if(old_size <= max_size && delta <= max_size - old_size)
        {
            auto const new_size{old_size + delta};
            auto const elem{wasm1p1_details::table_elem_from_funcref(module, value)};
            table->elems.resize(new_size);
            for(::std::size_t i{old_size}; i != new_size; ++i) { table->elems.index_unchecked(i) = elem; }
            out = wasm1p1_details::u32_to_i32(static_cast<::std::uint_least32_t>(old_size));
        }

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_size(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const out{wasm1p1_details::u32_to_i32(static_cast<::std::uint_least32_t>(table->elems.size()))};
        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        type...[1u] += sizeof(out);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_size(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const out{wasm1p1_details::u32_to_i32(static_cast<::std::uint_least32_t>(table->elems.size()))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_fill_funcref(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(type...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_funcref>(type...)};
        auto const index{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        wasm1p1_details::check_table_range(index, len, table->elems.size());

        auto const elem{wasm1p1_details::table_elem_from_funcref(module, value)};
        for(::std::size_t i{}; i != len; ++i) { table->elems.index_unchecked(index + i) = elem; }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_fill_funcref(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(typeref...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_funcref>(typeref...)};
        auto const index{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        wasm1p1_details::check_table_range(index, len, table->elems.size());

        auto const elem{wasm1p1_details::table_elem_from_funcref(module, value)};
        for(::std::size_t i{}; i != len; ++i) { table->elems.index_unchecked(index + i) = elem; }
    }

    namespace translate
    {
        namespace details
        {
            struct i32_extend8_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_sign_extend_typed<Opt, wasm1p1_details::wasm_i32, ::std::int_least8_t, Pos, Type...>; }
            };

            struct i32_extend16_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_sign_extend_typed<Opt, wasm1p1_details::wasm_i32, ::std::int_least16_t, Pos, Type...>; }
            };

            struct i64_extend8_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_sign_extend_typed<Opt, wasm1p1_details::wasm_i64, ::std::int_least8_t, Pos, Type...>; }
            };

            struct i64_extend16_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_sign_extend_typed<Opt, wasm1p1_details::wasm_i64, ::std::int_least16_t, Pos, Type...>; }
            };

            struct i64_extend32_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_sign_extend_typed<Opt, wasm1p1_details::wasm_i64, ::std::int_least32_t, Pos, Type...>; }
            };

            template <typename FloatT, typename WasmOutT, bool Signed>
            struct trunc_sat_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_trunc_sat_typed<Opt, FloatT, WasmOutT, Signed, Pos, Pos, Type...>; }
            };

            template <typename FloatT, typename WasmOutT, bool Signed>
            struct trunc_sat_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t OutPos, ::std::size_t InPos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_trunc_sat_typed<Opt, FloatT, WasmOutT, Signed, InPos, OutPos, Type...>; }
            };

            template <typename FloatT, typename WasmOutT, bool Signed>
            struct trunc_sat_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t OutPos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_trunc_sat_typed<Opt, FloatT, WasmOutT, Signed, 0uz, OutPos, Type...>; }
            };

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Curr, ::std::size_t End, uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...>
                get_uwvmint_v128_const_fptr_impl(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
            {
                static_assert(Curr < End);
                if(curr_stacktop.v128_stack_top_curr_pos == Curr) { return uwvmint_v128_const<CompileOption, Curr, Type...>; }
                else
                {
                    if constexpr(Curr + 1uz < End) { return get_uwvmint_v128_const_fptr_impl<CompileOption, Curr + 1uz, End, Type...>(curr_stacktop); }
                    else
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            template <typename RefT>
            struct ref_is_null_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_ref_is_null<Opt, I32Pos, RefT, Type...>; }
            };
        }  // namespace details

        template <uwvm_interpreter_translate_option_t CompileOption,
                  typename ValueT,
                  typename NarrowSignedT,
                  typename OpWrapper,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_sign_extend_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::select_unary_convert_fptr<CompileOption,
                                                      ValueT,
                                                      ValueT,
                                                      wasm1p1_details::stacktop_begin_pos<CompileOption, ValueT>(),
                                                      wasm1p1_details::stacktop_end_pos<CompileOption, ValueT>(),
                                                      wasm1p1_details::stacktop_begin_pos<CompileOption, ValueT>(),
                                                      wasm1p1_details::stacktop_end_pos<CompileOption, ValueT>(),
                                                      OpWrapper,
                                                      OpWrapper,
                                                      OpWrapper,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_extend8_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return get_uwvmint_sign_extend_fptr<CompileOption, wasm1p1_details::wasm_i32, ::std::int_least8_t, details::i32_extend8_s_op, Type...>(
                curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_extend8_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_extend8_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_extend16_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return get_uwvmint_sign_extend_fptr<CompileOption, wasm1p1_details::wasm_i32, ::std::int_least16_t, details::i32_extend16_s_op, Type...>(
                curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_extend16_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_extend16_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_extend8_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return get_uwvmint_sign_extend_fptr<CompileOption, wasm1p1_details::wasm_i64, ::std::int_least8_t, details::i64_extend8_s_op, Type...>(
                curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_extend8_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_extend8_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_extend16_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return get_uwvmint_sign_extend_fptr<CompileOption, wasm1p1_details::wasm_i64, ::std::int_least16_t, details::i64_extend16_s_op, Type...>(
                curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_extend16_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_extend16_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_extend32_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return get_uwvmint_sign_extend_fptr<CompileOption, wasm1p1_details::wasm_i64, ::std::int_least32_t, details::i64_extend32_s_op, Type...>(
                curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_extend32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_extend32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

# define UWVM_WASM1P1_TRUNC_SAT_FPTR(NAME, FLOAT_T, OUT_T, SIGNED_VALUE)                                                                                       \
     template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>                                                             \
         requires (CompileOption.is_tail_call)                                                                                                                 \
     inline constexpr uwvm_interpreter_opfunc_t<Type...> NAME##_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept                        \
     {                                                                                                                                                         \
         return details::select_unary_convert_fptr<CompileOption,                                                                                              \
                                                   FLOAT_T,                                                                                                    \
                                                   OUT_T,                                                                                                      \
                                                   wasm1p1_details::stacktop_begin_pos<CompileOption, FLOAT_T>(),                                              \
                                                   wasm1p1_details::stacktop_end_pos<CompileOption, FLOAT_T>(),                                                \
                                                   wasm1p1_details::stacktop_begin_pos<CompileOption, OUT_T>(),                                                \
                                                   wasm1p1_details::stacktop_end_pos<CompileOption, OUT_T>(),                                                  \
                                                   details::trunc_sat_op<FLOAT_T, OUT_T, SIGNED_VALUE>,                                                        \
                                                   details::trunc_sat_op_2d<FLOAT_T, OUT_T, SIGNED_VALUE>,                                                     \
                                                   details::trunc_sat_op_out_only<FLOAT_T, OUT_T, SIGNED_VALUE>,                                               \
                                                   Type...>(curr_stacktop);                                                                                    \
     }                                                                                                                                                         \
     template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>                                                      \
         requires (CompileOption.is_tail_call)                                                                                                                 \
     inline constexpr auto NAME##_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,                                                    \
                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept                                            \
     { return NAME##_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }                                                                                     \
     template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>                                                             \
         requires (!CompileOption.is_tail_call)                                                                                                                \
     inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> NAME##_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept                                \
     { return uwvmint_trunc_sat_typed<CompileOption, FLOAT_T, OUT_T, SIGNED_VALUE, Type...>; }                                                                 \
     template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>                                                      \
         requires (!CompileOption.is_tail_call)                                                                                                                \
     inline constexpr auto NAME##_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,                                                    \
                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept                                            \
     { return NAME##_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        UWVM_WASM1P1_TRUNC_SAT_FPTR(get_uwvmint_i32_trunc_sat_f32_s, wasm1p1_details::wasm_f32, wasm1p1_details::wasm_i32, true)
        UWVM_WASM1P1_TRUNC_SAT_FPTR(get_uwvmint_i32_trunc_sat_f32_u, wasm1p1_details::wasm_f32, wasm1p1_details::wasm_i32, false)
        UWVM_WASM1P1_TRUNC_SAT_FPTR(get_uwvmint_i32_trunc_sat_f64_s, wasm1p1_details::wasm_f64, wasm1p1_details::wasm_i32, true)
        UWVM_WASM1P1_TRUNC_SAT_FPTR(get_uwvmint_i32_trunc_sat_f64_u, wasm1p1_details::wasm_f64, wasm1p1_details::wasm_i32, false)
        UWVM_WASM1P1_TRUNC_SAT_FPTR(get_uwvmint_i64_trunc_sat_f32_s, wasm1p1_details::wasm_f32, wasm1p1_details::wasm_i64, true)
        UWVM_WASM1P1_TRUNC_SAT_FPTR(get_uwvmint_i64_trunc_sat_f32_u, wasm1p1_details::wasm_f32, wasm1p1_details::wasm_i64, false)
        UWVM_WASM1P1_TRUNC_SAT_FPTR(get_uwvmint_i64_trunc_sat_f64_s, wasm1p1_details::wasm_f64, wasm1p1_details::wasm_i64, true)
        UWVM_WASM1P1_TRUNC_SAT_FPTR(get_uwvmint_i64_trunc_sat_f64_u, wasm1p1_details::wasm_f64, wasm1p1_details::wasm_i64, false)

# undef UWVM_WASM1P1_TRUNC_SAT_FPTR

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_extend8_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_sign_extend_typed<CompileOption, wasm1p1_details::wasm_i32, ::std::int_least8_t, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_extend8_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_extend8_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_extend16_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_sign_extend_typed<CompileOption, wasm1p1_details::wasm_i32, ::std::int_least16_t, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_extend16_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_extend16_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_extend8_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_sign_extend_typed<CompileOption, wasm1p1_details::wasm_i64, ::std::int_least8_t, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_extend8_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_extend8_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_extend16_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_sign_extend_typed<CompileOption, wasm1p1_details::wasm_i64, ::std::int_least16_t, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_extend16_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_extend16_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_extend32_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_sign_extend_typed<CompileOption, wasm1p1_details::wasm_i64, ::std::int_least32_t, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_extend32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_extend32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_v128_const_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.v128_stack_top_begin_pos != CompileOption.v128_stack_top_end_pos)
            {
                return details::
                    get_uwvmint_v128_const_fptr_impl<CompileOption, CompileOption.v128_stack_top_begin_pos, CompileOption.v128_stack_top_end_pos, Type...>(
                        curr_stacktop);
            }
            else
            {
                return uwvmint_v128_const<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_v128_const_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_v128_const_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_v128_const_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_v128_const<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_v128_const_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_v128_const_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_unop Op, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_v128_unop_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_v128_unop<CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_unop Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_v128_unop_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_v128_unop_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_binop Op, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_v128_binop_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_v128_binop<CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_binop Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_v128_binop_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_v128_binop_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_testop Op, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_v128_testop_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_v128_testop<CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_testop Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_v128_testop_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_v128_testop_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_splatop Op, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_i32x4_splat_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_i32x4_splat<CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_splatop Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_i32x4_splat_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_i32x4_splat_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_splatop Op, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_f32x4_splat_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_f32x4_splat<CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_splatop Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_f32x4_splat_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_f32x4_splat_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Lane, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_f32x4_extract_lane_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_f32x4_extract_lane<CompileOption, Lane, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Lane, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_f32x4_extract_lane_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_f32x4_extract_lane_fptr<CompileOption, Lane, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_v128_load_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_v128_load<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_v128_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_v128_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_v128_store_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_v128_store<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_v128_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_v128_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_full_unop_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_full_unop<CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_unop_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_unop_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_full_binop_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_full_binop<CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_binop_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_binop_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_full_bitselect_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_full_bitselect<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_bitselect_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_bitselect_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_full_testop_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_full_testop<CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_testop_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_testop_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_full_shift_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_full_shift<CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_shift_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_shift_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, typename ScalarT, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_full_splat_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_full_splat<CompileOption, Op, ScalarT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  wasm1p1_simd_details::simd_code Op,
                  typename ScalarT,
                  uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_splat_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_splat_fptr<CompileOption, Op, ScalarT, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, typename ScalarT, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_full_extract_lane_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_full_extract_lane<CompileOption, Op, ScalarT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  wasm1p1_simd_details::simd_code Op,
                  typename ScalarT,
                  uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_extract_lane_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_extract_lane_fptr<CompileOption, Op, ScalarT, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, typename ScalarT, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_full_replace_lane_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_full_replace_lane<CompileOption, Op, ScalarT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  wasm1p1_simd_details::simd_code Op,
                  typename ScalarT,
                  uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_replace_lane_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_replace_lane_fptr<CompileOption, Op, ScalarT, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_full_shuffle_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_full_shuffle<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_shuffle_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_shuffle_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_full_mem_load_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_full_mem_load<CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_mem_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_mem_load_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_full_mem_store_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_full_mem_store<CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_mem_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_mem_store_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_ref_null_typed_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_ref_null<CompileOption, RefT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_ref_null_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_ref_null_typed_fptr<CompileOption, RefT, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_ref_null_typed_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_ref_null<CompileOption, RefT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_ref_null_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_ref_null_typed_fptr<CompileOption, RefT, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_ref_is_null_typed_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return ::uwvm2::runtime::compiler::uwvm_int::optable::translate::details::select_stacktop_fptr_by_currpos_impl_stack<
                    CompileOption,
                    CompileOption.i32_stack_top_begin_pos,
                    CompileOption.i32_stack_top_end_pos,
                    details::ref_is_null_op<RefT>,
                    Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_ref_is_null<CompileOption, 0uz, RefT, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_ref_is_null_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_ref_is_null_typed_fptr<CompileOption, RefT, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_ref_is_null_typed_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_ref_is_null<CompileOption, RefT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_ref_is_null_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_ref_is_null_typed_fptr<CompileOption, RefT, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_ref_func_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_ref_func<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_ref_func_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_ref_func_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_ref_func_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_ref_func<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_ref_func_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_ref_func_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_get_funcref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_get_funcref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_get_funcref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_get_funcref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_get_funcref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_get_funcref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_set_funcref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_set_funcref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_set_funcref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_set_funcref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_set_funcref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_set_funcref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_data_drop_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_data_drop<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_data_drop_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_data_drop<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_data_drop_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_data_drop_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_elem_drop_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_elem_drop<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_elem_drop_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_elem_drop<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_elem_drop_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_elem_drop_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_memory_init_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_memory_init<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_memory_init_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_memory_init<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_memory_init_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_memory_init_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_memory_copy_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_memory_copy<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_memory_copy_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_memory_copy<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_memory_copy_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_memory_copy_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_memory_fill_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_memory_fill<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_memory_fill_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_memory_fill<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_memory_fill_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_memory_fill_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_init_funcref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_init_funcref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_init_funcref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_init_funcref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_init_funcref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_init_funcref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_copy_funcref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_copy_funcref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_copy_funcref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_copy_funcref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_copy_funcref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_copy_funcref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_grow_funcref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_grow_funcref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_grow_funcref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_grow_funcref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_grow_funcref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_grow_funcref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_size_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_size<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_size_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_size<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_size_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_size_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_fill_funcref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_fill_funcref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_fill_funcref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_fill_funcref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_fill_funcref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_fill_funcref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate
}
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
