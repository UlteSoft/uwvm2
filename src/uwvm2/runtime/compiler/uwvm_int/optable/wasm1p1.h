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
# include <uwvm2/runtime/compiler/shared/wasm1p1_simd.h>
# include "define.h"
# include "convert.h"
# include "storage.h"
# include "memory.h"
# include "stack.h"
# include "register_ring.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) && defined(UWVM_RUNTIME_LLVM_JIT)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::lib
{
    // Tiered execution installs this hook when compact LLVM call_indirect views are materialized.  Keeping the optional
    // notification as an inline hook preserves the header-only uwvm-int test/embedding boundary: interpreter-only or
    // standalone translator binaries do not acquire a hard link dependency on the full LLVM runtime object.
    using llvm_jit_refresh_call_indirect_table_views_hook_t = void (*)(
        ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t*,
        ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_mutation_kind,
        ::std::size_t,
        ::std::size_t) noexcept;
    inline llvm_jit_refresh_call_indirect_table_views_hook_t llvm_jit_refresh_call_indirect_table_views_hook{};
}
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
        using runtime_table_mutation_kind = ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_mutation_kind;
        using runtime_data_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t;
        using runtime_element_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_element_storage_t;
        using runtime_module_storage_t = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;

        UWVM_ALWAYS_INLINE inline constexpr void refresh_llvm_call_indirect_table_views_after_funcref_write(
            runtime_table_storage_t* table,
            ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_mutation_kind kind,
            ::std::size_t begin,
            ::std::size_t count) noexcept
        {
# if defined(UWVM_RUNTIME_LLVM_JIT)
            if(auto const hook{::uwvm2::runtime::lib::llvm_jit_refresh_call_indirect_table_views_hook}; hook != nullptr)
            {
                hook(table, kind, begin, count);
            }
# else
            static_cast<void>(table);
            static_cast<void>(kind);
            static_cast<void>(begin);
            static_cast<void>(count);
# endif
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool runtime_table_is_funcref(runtime_table_storage_t const& table) noexcept
        {
            using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
            return table.table_type_ptr != nullptr && table.table_type_ptr->reftype == reference_type::funcref;
        }

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
            static_cast<void>(module);
            runtime_table_elem_storage_t out{};
            switch(ref.ref.kind)
            {
                case ::uwvm2::object::global::wasm_ref_kind::wasm_null:
                {
                    return out;
                }
                case ::uwvm2::object::global::wasm_ref_kind::wasm_func:
                {
                    // Bare function indices are initializer-only staging values. A runtime value must carry its defining storage
                    // pointer; otherwise an imported global could be reinterpreted in the consumer's function index space.
                    ::fast_io::fast_terminate();
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

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_externref
            externref_from_table_elem(runtime_table_elem_storage_t const& elem) noexcept
        {
            wasm_externref out{};
            if(elem.type != runtime_table_elem_type::extern_ref) [[unlikely]]
            {
                // A zero-initialized slot is also a valid null reference. This keeps manually constructed
                // runtime tables compatible while initialized externref tables use the explicit tag.
                if(elem.storage.imported_ptr == nullptr)
                {
                    out.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_null;
                    return out;
                }
                ::fast_io::fast_terminate();
            }

            out.ref.storage.ptr = elem.storage.extern_ptr;
            out.ref.kind = elem.storage.extern_ptr == nullptr ? ::uwvm2::object::global::wasm_ref_kind::wasm_null
                                                              : ::uwvm2::object::global::wasm_ref_kind::wasm_extern;
            return out;
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr runtime_table_elem_storage_t table_elem_from_externref(wasm_externref const& ref) noexcept
        {
            runtime_table_elem_storage_t out{};
            out.type = runtime_table_elem_type::extern_ref;
            switch(ref.ref.kind)
            {
                case ::uwvm2::object::global::wasm_ref_kind::wasm_null:
                {
                    out.storage.extern_ptr = nullptr;
                    return out;
                }
                case ::uwvm2::object::global::wasm_ref_kind::wasm_extern:
                {
                    out.storage.extern_ptr = ref.ref.storage.ptr;
                    return out;
                }
                [[unlikely]] default: ::fast_io::fast_terminate();
            }
        }

        UWVM_ALWAYS_INLINE inline constexpr bool range_oob(::std::size_t begin, ::std::size_t len, ::std::size_t bound) noexcept
        { return begin > bound || len > bound - begin; }

        UWVM_ALWAYS_INLINE inline constexpr void check_table_range(::std::size_t begin, ::std::size_t len, ::std::size_t bound) noexcept
        {
            if(range_oob(begin, len, bound)) [[unlikely]] { table_oob_terminate(); }
        }

        UWVM_ALWAYS_INLINE inline constexpr void copy_funcref_element_segment(runtime_table_storage_t& table,
                                                                               runtime_element_storage_t const& element_record,
                                                                               runtime_module_storage_t const* module,
                                                                               ::std::size_t dst,
                                                                               ::std::size_t src,
                                                                               ::std::size_t len) noexcept
        {
            auto const& element{element_record.element};
            auto const funcidx_begin{element.funcidx_begin};
            auto const funcidx_end{element.funcidx_end};
            auto const funcref_begin{element.funcref_begin};
            auto const funcref_end{element.funcref_end};
            if((funcidx_begin == nullptr) != (funcidx_end == nullptr) ||
               (funcref_begin == nullptr) != (funcref_end == nullptr) ||
               (funcidx_begin != nullptr && funcref_begin != nullptr)) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }

            auto const source_size{funcref_begin == nullptr ? (funcidx_begin == nullptr ? 0uz : static_cast<::std::size_t>(funcidx_end - funcidx_begin))
                                                             : static_cast<::std::size_t>(funcref_end - funcref_begin)};
            check_table_range(src, len, source_size);
            check_table_range(dst, len, table.elems.size());

            if(funcref_begin != nullptr)
            {
                for(::std::size_t i{}; i != len; ++i) { table.elems.index_unchecked(dst + i) = funcref_begin[src + i]; }
                return;
            }

            if(module == nullptr && len != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
            for(::std::size_t i{}; i != len; ++i)
            {
                auto const funcidx{funcidx_begin[src + i]};
                table.elems.index_unchecked(dst + i) = funcidx == (::std::numeric_limits<wasm_u32>::max)()
                                                           ? runtime_table_elem_storage_t{}
                                                           : resolve_table_elem_from_func_index(module, funcidx);
            }
        }

        // These SIMD operations move scalar bits; a native floating return can
        // quiet an sNaN on x87/68881 even when no arithmetic was requested.
        template <typename T>
        using scalar_move_type = ::std::conditional_t<::std::same_as<T, wasm_f32>, wasm_i32,
                                 ::std::conditional_t<::std::same_as<T, wasm_f64>, wasm_i64, T>>;

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
            else if constexpr(::std::same_as<ValueT, wasm_i64>)
            {
                auto const bits{::uwvm2::runtime::compiler::uwvm_int::optable::details::to_u64_bits(v)};
                narrow_unsigned_t const low{static_cast<narrow_unsigned_t>(bits)};
                NarrowSignedT const narrowed{::std::bit_cast<NarrowSignedT>(low)};
                return static_cast<wasm_i64>(static_cast<::std::int_least64_t>(narrowed));
            }
            else
            {
                static_assert(sizeof(ValueT) == 0, "unhandled sign-extend value type");
            }
        }

        template <typename WasmOutT, typename FloatT>
        UWVM_ALWAYS_INLINE inline constexpr WasmOutT trunc_sat_signed(FloatT x) noexcept
        {
            static_assert(::std::same_as<WasmOutT, wasm_i32> || ::std::same_as<WasmOutT, wasm_i64>);
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
            static_assert(::std::same_as<WasmOutT, wasm_i32> || ::std::same_as<WasmOutT, wasm_i64>);
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
            else if constexpr(::std::same_as<WasmOutT, wasm_i64>)
            {
                return ::uwvm2::runtime::compiler::uwvm_int::optable::details::from_u64_bits<WasmOutT>(static_cast<::std::uint_least64_t>(out));
            }
            else
            {
                static_assert(sizeof(WasmOutT) == 0, "unhandled trunc-sat output type");
            }
        }

        template <typename WasmOutT, bool Signed, typename FloatT>
        UWVM_ALWAYS_INLINE inline constexpr WasmOutT trunc_sat(FloatT x) noexcept
        {
            if constexpr(Signed) { return trunc_sat_signed<WasmOutT>(x); }
            else if constexpr(!Signed) { return trunc_sat_unsigned<WasmOutT>(x); }
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

    namespace wasm1p1_simd_details = ::uwvm2::runtime::compiler::shared::wasm1p1_simd_details;

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

        static_assert(Op == wasm1p1_simd_details::v128_splatop::f32x4);
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(type...)};
        auto const out{wasm1p1_simd_details::eval_v128_splat_i32<wasm1p1_simd_details::v128_splatop::i32x4>(v)};
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
        static_assert(Op == wasm1p1_simd_details::v128_splatop::f32x4);
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_v128_splat_i32<wasm1p1_simd_details::v128_splatop::i32x4>(v)};
        wasm1p1_simd_details::push_v128_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Lane, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_f32x4_extract_lane(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        static_assert(Lane < 4uz);
        auto const out{wasm1p1_simd_details::eval_extract_lane_i32<wasm1p1_simd_details::simd_code::i32x4_extract_lane>(v, Lane)};
        wasm1p1_simd_details::push_i32_to_memory_stack(out, type...[1u]);

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
        static_assert(Lane < 4uz);
        auto const out{wasm1p1_simd_details::eval_extract_lane_i32<wasm1p1_simd_details::simd_code::i32x4_extract_lane>(v, Lane)};
        wasm1p1_simd_details::push_i32_to_memory_stack(out, typeref...[1u]);
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
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_details::scalar_move_type<ScalarT>>(type...)};
        wasm1p1_simd_details::wasm_v128 out;  // no init
        if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i32>) { out = wasm1p1_simd_details::eval_full_splat_i32<Op>(v); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i64>) { out = wasm1p1_simd_details::eval_full_splat_i64<Op>(v); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f32>) { out = wasm1p1_simd_details::eval_full_splat_i32<wasm1p1_simd_details::simd_code::i32x4_splat>(v); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f64>) { out = wasm1p1_simd_details::eval_full_splat_i64<wasm1p1_simd_details::simd_code::i64x2_splat>(v); }
        else
        {
            static_assert(sizeof(ScalarT) == 0, "unhandled SIMD splat scalar type");
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
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_details::scalar_move_type<ScalarT>>(typeref...)};
        wasm1p1_simd_details::wasm_v128 out;  // no init
        if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i32>) { out = wasm1p1_simd_details::eval_full_splat_i32<Op>(v); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i64>) { out = wasm1p1_simd_details::eval_full_splat_i64<Op>(v); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f32>) { out = wasm1p1_simd_details::eval_full_splat_i32<wasm1p1_simd_details::simd_code::i32x4_splat>(v); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f64>) { out = wasm1p1_simd_details::eval_full_splat_i64<wasm1p1_simd_details::simd_code::i64x2_splat>(v); }
        else
        {
            static_assert(sizeof(ScalarT) == 0, "unhandled SIMD splat scalar type");
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
        wasm1p1_details::scalar_move_type<ScalarT> out;  // Preserve lane bits on legacy FP ABIs.
        if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i32>) { out = wasm1p1_simd_details::eval_extract_lane_i32<Op>(v, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i64>) { out = wasm1p1_simd_details::eval_extract_lane_i64<Op>(v, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f32>) { out = wasm1p1_simd_details::eval_extract_lane_i32<wasm1p1_simd_details::simd_code::i32x4_extract_lane>(v, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f64>) { out = wasm1p1_simd_details::eval_extract_lane_i64<wasm1p1_simd_details::simd_code::i64x2_extract_lane>(v, lane); }
        else
        {
            static_assert(sizeof(ScalarT) == 0, "unhandled SIMD extract-lane scalar type");
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
        wasm1p1_details::scalar_move_type<ScalarT> out;  // Preserve lane bits on legacy FP ABIs.
        if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i32>) { out = wasm1p1_simd_details::eval_extract_lane_i32<Op>(v, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i64>) { out = wasm1p1_simd_details::eval_extract_lane_i64<Op>(v, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f32>) { out = wasm1p1_simd_details::eval_extract_lane_i32<wasm1p1_simd_details::simd_code::i32x4_extract_lane>(v, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f64>) { out = wasm1p1_simd_details::eval_extract_lane_i64<wasm1p1_simd_details::simd_code::i64x2_extract_lane>(v, lane); }
        else
        {
            static_assert(sizeof(ScalarT) == 0, "unhandled SIMD extract-lane scalar type");
        }
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, typename ScalarT, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_replace_lane(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const lane{wasm1p1_details::read_imm<wasm1p1_simd_details::u8>(type...[0])};
        auto const x{get_curr_val_from_operand_stack_cache<wasm1p1_details::scalar_move_type<ScalarT>>(type...)};
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        wasm1p1_simd_details::wasm_v128 out;  // no init
        if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i32>) { out = wasm1p1_simd_details::eval_replace_lane_i32<Op>(v, x, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i64>) { out = wasm1p1_simd_details::eval_replace_lane_i64<Op>(v, x, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f32>) { out = wasm1p1_simd_details::eval_replace_lane_i32<wasm1p1_simd_details::simd_code::i32x4_replace_lane>(v, x, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f64>) { out = wasm1p1_simd_details::eval_replace_lane_i64<wasm1p1_simd_details::simd_code::i64x2_replace_lane>(v, x, lane); }
        else
        {
            static_assert(sizeof(ScalarT) == 0, "unhandled SIMD replace-lane scalar type");
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
        auto const x{get_curr_val_from_operand_stack_cache<wasm1p1_details::scalar_move_type<ScalarT>>(typeref...)};
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        wasm1p1_simd_details::wasm_v128 out;  // no init
        if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i32>) { out = wasm1p1_simd_details::eval_replace_lane_i32<Op>(v, x, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_i64>) { out = wasm1p1_simd_details::eval_replace_lane_i64<Op>(v, x, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f32>) { out = wasm1p1_simd_details::eval_replace_lane_i32<wasm1p1_simd_details::simd_code::i32x4_replace_lane>(v, x, lane); }
        else if constexpr(::std::same_as<ScalarT, wasm1p1_simd_details::wasm_f64>) { out = wasm1p1_simd_details::eval_replace_lane_i64<wasm1p1_simd_details::simd_code::i64x2_replace_lane>(v, x, lane); }
        else
        {
            static_assert(sizeof(ScalarT) == 0, "unhandled SIMD replace-lane scalar type");
        }
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_shuffle(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        wasm1p1_simd_details::shuffle_controls controls;  // no init
        ::std::memcpy(::std::addressof(controls), type...[0], sizeof(controls));
        type...[0] += sizeof(controls);
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const out{wasm1p1_simd_details::eval_shuffle(lhs, rhs, controls)};
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
        wasm1p1_simd_details::shuffle_controls controls;  // no init
        ::std::memcpy(::std::addressof(controls), typeref...[0], sizeof(controls));
        typeref...[0] += sizeof(controls);
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_shuffle(lhs, rhs, controls)};
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
        wasm1p1_details::refresh_llvm_call_indirect_table_views_after_funcref_write(
            table, wasm1p1_details::runtime_table_mutation_kind::set, index, 1uz);

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
        wasm1p1_details::refresh_llvm_call_indirect_table_views_after_funcref_write(
            table, wasm1p1_details::runtime_table_mutation_kind::set, index, 1uz);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_get_externref(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const index{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }

        auto const out{wasm1p1_details::externref_from_table_elem(table->elems.index_unchecked(index))};
        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        type...[1u] += sizeof(out);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_get_externref(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const index{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }

        auto const out{wasm1p1_details::externref_from_table_elem(table->elems.index_unchecked(index))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_set_externref(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_externref>(type...)};
        auto const index{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }
        table->elems.index_unchecked(index) = wasm1p1_details::table_elem_from_externref(value);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_set_externref(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_externref>(typeref...)};
        auto const index{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }
        table->elems.index_unchecked(index) = wasm1p1_details::table_elem_from_externref(value);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_data_drop(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const data{wasm1p1_details::read_imm<wasm1p1_details::runtime_data_storage_t*>(type...[0])};
        if(data == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        ::uwvm2::uwvm::runtime::storage::drop_wasm_data_segment_payload(data->data);

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
        ::uwvm2::uwvm::runtime::storage::drop_wasm_data_segment_payload(data->data);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_elem_drop(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const element{wasm1p1_details::read_imm<wasm1p1_details::runtime_element_storage_t*>(type...[0])};
        if(element == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        ::uwvm2::uwvm::runtime::storage::drop_wasm_element_segment_payload(element->element);

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
        ::uwvm2::uwvm::runtime::storage::drop_wasm_element_segment_payload(element->element);
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

        wasm1p1_details::copy_funcref_element_segment(*table, *element, module, dst, src, len);
        if(len != 0uz)
        {
            wasm1p1_details::refresh_llvm_call_indirect_table_views_after_funcref_write(
                table, wasm1p1_details::runtime_table_mutation_kind::init, dst, len);
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

        wasm1p1_details::copy_funcref_element_segment(*table, *element, module, dst, src, len);
        if(len != 0uz)
        {
            wasm1p1_details::refresh_llvm_call_indirect_table_views_after_funcref_write(
                table, wasm1p1_details::runtime_table_mutation_kind::init, dst, len);
        }
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_init_externref(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        auto const element{wasm1p1_details::read_imm<wasm1p1_details::runtime_element_storage_t*>(type...[0])};
        if(table == nullptr || element == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const src{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const dst{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};

        auto const begin{element->element.externref_begin};
        auto const end{element->element.externref_end};
        if((begin == nullptr) != (end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const elem_len{begin == nullptr ? 0uz : static_cast<::std::size_t>(end - begin)};
        wasm1p1_details::check_table_range(src, len, elem_len);
        wasm1p1_details::check_table_range(dst, len, table->elems.size());

        for(::std::size_t i{}; i != len; ++i)
        {
            auto& slot{table->elems.index_unchecked(dst + i)};
            slot.storage.extern_ptr = begin[src + i];
            slot.type = wasm1p1_details::runtime_table_elem_type::extern_ref;
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_init_externref(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        auto const element{wasm1p1_details::read_imm<wasm1p1_details::runtime_element_storage_t*>(typeref...[0])};
        if(table == nullptr || element == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const src{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const dst{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};

        auto const begin{element->element.externref_begin};
        auto const end{element->element.externref_end};
        if((begin == nullptr) != (end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const elem_len{begin == nullptr ? 0uz : static_cast<::std::size_t>(end - begin)};
        wasm1p1_details::check_table_range(src, len, elem_len);
        wasm1p1_details::check_table_range(dst, len, table->elems.size());

        for(::std::size_t i{}; i != len; ++i)
        {
            auto& slot{table->elems.index_unchecked(dst + i)};
            slot.storage.extern_ptr = begin[src + i];
            slot.type = wasm1p1_details::runtime_table_elem_type::extern_ref;
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
            if(wasm1p1_details::runtime_table_is_funcref(*dst_table))
            {
                wasm1p1_details::refresh_llvm_call_indirect_table_views_after_funcref_write(
                    dst_table, wasm1p1_details::runtime_table_mutation_kind::copy, dst, len);
            }
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
            if(wasm1p1_details::runtime_table_is_funcref(*dst_table))
            {
                wasm1p1_details::refresh_llvm_call_indirect_table_views_after_funcref_write(
                    dst_table, wasm1p1_details::runtime_table_mutation_kind::copy, dst, len);
            }
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
            if(delta != 0uz)
            {
                wasm1p1_details::refresh_llvm_call_indirect_table_views_after_funcref_write(
                    table, wasm1p1_details::runtime_table_mutation_kind::grow, old_size, delta);
            }
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
            if(delta != 0uz)
            {
                wasm1p1_details::refresh_llvm_call_indirect_table_views_after_funcref_write(
                    table, wasm1p1_details::runtime_table_mutation_kind::grow, old_size, delta);
            }
        }

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_grow_externref(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const delta{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_externref>(type...)};
        auto const old_size{table->elems.size()};
        wasm1p1_details::wasm_i32 out{wasm1p1_details::u32_to_i32((::std::numeric_limits<::std::uint_least32_t>::max)())};

        auto const& limits{table->table_type_ptr->limits};
        auto const max_size{static_cast<::std::size_t>(limits.max)};
        if(old_size <= max_size && delta <= max_size - old_size)
        {
            auto const new_size{old_size + delta};
            auto const elem{wasm1p1_details::table_elem_from_externref(value)};
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
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_grow_externref(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const delta{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_externref>(typeref...)};
        auto const old_size{table->elems.size()};
        wasm1p1_details::wasm_i32 out{wasm1p1_details::u32_to_i32((::std::numeric_limits<::std::uint_least32_t>::max)())};

        auto const& limits{table->table_type_ptr->limits};
        auto const max_size{static_cast<::std::size_t>(limits.max)};
        if(old_size <= max_size && delta <= max_size - old_size)
        {
            auto const new_size{old_size + delta};
            auto const elem{wasm1p1_details::table_elem_from_externref(value)};
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
        if(len != 0uz)
        {
            wasm1p1_details::refresh_llvm_call_indirect_table_views_after_funcref_write(
                table, wasm1p1_details::runtime_table_mutation_kind::fill, index, len);
        }

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
        if(len != 0uz)
        {
            wasm1p1_details::refresh_llvm_call_indirect_table_views_after_funcref_write(
                table, wasm1p1_details::runtime_table_mutation_kind::fill, index, len);
        }
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_fill_externref(Type... type) UWVM_THROWS
    {
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_externref>(type...)};
        auto const index{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        wasm1p1_details::check_table_range(index, len, table->elems.size());

        auto const elem{wasm1p1_details::table_elem_from_externref(value)};
        for(::std::size_t i{}; i != len; ++i) { table->elems.index_unchecked(index + i) = elem; }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_fill_externref(TypeRef & ... typeref) UWVM_THROWS
    {
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_externref>(typeref...)};
        auto const index{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        wasm1p1_details::check_table_range(index, len, table->elems.size());

        auto const elem{wasm1p1_details::table_elem_from_externref(value)};
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
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_get_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_get_externref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_get_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_get_externref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_get_externref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_get_externref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_set_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_set_externref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_set_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_set_externref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_set_externref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_set_externref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

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
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_init_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_init_externref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_init_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_init_externref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_init_externref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_init_externref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

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
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_grow_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_grow_externref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_grow_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_grow_externref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_grow_externref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_grow_externref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

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

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_fill_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_fill_externref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_fill_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_fill_externref<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_fill_externref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_fill_externref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate
}
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
