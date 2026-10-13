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
    // Compatibility spelling refers to the one storage-owned hook shared by
    // LLVM-only initialization and tiered interpreter mutations.
    using llvm_jit_refresh_call_indirect_table_views_hook_t =
        ::uwvm2::uwvm::runtime::storage::llvm_jit_table_refresh_hook_t;
    inline auto& llvm_jit_refresh_call_indirect_table_views_hook{
        ::uwvm2::uwvm::runtime::storage::llvm_jit_table_refresh_hook};
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
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool runtime_table_is_externref(runtime_table_storage_t const& table) noexcept
        {
            using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
            return table.table_type_ptr != nullptr && table.table_type_ptr->reftype == reference_type::externref;
        }
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool runtime_table_is_exnref(runtime_table_storage_t const& table) noexcept
        {
            return table.table_type_ptr != nullptr &&
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(table.table_type_ptr->reftype) == 0x69u;
        }

        template <typename T>
        UWVM_ALWAYS_INLINE inline constexpr T read_imm(::std::byte const*& ip) noexcept
        {
            T v;  // no init
            // [complete native immediate][remaining bytecode] ... bytecode_end
            // [safe                    ] caller's translator emitted this entire fixed-width record.
            // ^^ ip before the memcpy; copying never interprets guest-controlled pointer bits.
            ::std::memcpy(::std::addressof(v), ip, sizeof(v));
            ip += sizeof(v);
            // [consumed immediate][next field or handler] ... bytecode_end
            // [safe              ] ^^ ip advanced exactly sizeof(T), possibly to the next handler.
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
        inline void retain_table_externref(runtime_table_storage_t const* table,
                                           wasm_externref const& value) noexcept
        {
            if(::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(table, value.ref) !=
               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
            { ::fast_io::fast_terminate(); }
        }
        inline void retain_table_extern_payload(runtime_table_storage_t const* table,
                                                void* payload) noexcept
        {
            if(::uwvm2::uwvm::runtime::storage::retain_runtime_table_extern_payload(table, payload) !=
               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
            { ::fast_io::fast_terminate(); }
        }

        template <bool ExnRef>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_externref table_reference_from_elem(runtime_table_elem_storage_t const& elem) noexcept
        {
            if constexpr(!ExnRef) { return externref_from_table_elem(elem); }
            else
            {
                wasm_externref out{};
                if(elem.type != runtime_table_elem_type::exn_ref) [[unlikely]]
                {
                    if(elem.storage.extern_ptr != nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    out.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_null;
                    return out;
                }
                out.ref.storage.ptr = elem.storage.extern_ptr;
                out.ref.kind = elem.storage.extern_ptr == nullptr ? ::uwvm2::object::global::wasm_ref_kind::wasm_null :
                                                                    ::uwvm2::object::global::wasm_ref_kind::wasm_exn;
                return out;
            }
        }

        template <bool ExnRef>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr runtime_table_elem_storage_t table_elem_from_reference(wasm_externref const& ref) noexcept
        {
            if constexpr(!ExnRef) { return table_elem_from_externref(ref); }
            else
            {
                runtime_table_elem_storage_t out{};
                out.type = runtime_table_elem_type::exn_ref;
                if(ref.ref.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null) { return out; }
                if(ref.ref.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_exn) [[unlikely]] { ::fast_io::fast_terminate(); }
                out.storage.extern_ptr = ref.ref.storage.ptr;
                return out;
            }
        }

        template <bool ExnRef>
        inline void retain_table_reference(runtime_table_storage_t const* table, wasm_externref const& ref) noexcept
        {
            if constexpr(!ExnRef) { retain_table_externref(table, ref); }
            else if(::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(table, ref.ref) !=
                    ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
            { ::fast_io::fast_terminate(); }
        }

        template <bool ExnRef>
        inline void retain_table_payload(runtime_table_storage_t const* table, void* payload) noexcept
        {
            if constexpr(!ExnRef) { retain_table_extern_payload(table, payload); }
            else if(::uwvm2::uwvm::runtime::storage::retain_runtime_table_exn_payload(table, payload) !=
                    ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
            { ::fast_io::fast_terminate(); }
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
            // One acquired payload snapshot supplies every bound and source
            // pointer; elem.drop changes only visibility, never these pointers.
            auto const element{::uwvm2::uwvm::runtime::storage::load_wasm_element_segment_payload(element_record.element)};
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

        // Splat/extract/replace-lane are transport, not numerical conversions.
        // Use same-width integer helpers end to end, including the scalar stack
        // boundary: wrapping a Float-returning lane helper in bit_cast is too late
        // if GCC -O0 or an x87/68881 ABI has already quieted the signaling bit.
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
                // operand frame: operand_base ... [live bytes][typed result bytes] | frame_end
                //                [validated post-op stack depth           ] unsafe past frame_end
                // ^^ type...[1u] advances by sizeof(out); operand_stack_byte_max reserves this result.
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
            // operand frame: operand_base ... [live bytes][typed result bytes] | frame_end
            //                [validated post-op stack depth           ] unsafe past frame_end
            // ^^ type...[1u] advances by sizeof(out); operand_stack_byte_max reserves this result.
            type...[1u] += sizeof(out);
        }

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        ValueT const v{get_curr_val_from_operand_stack_cache<ValueT>(typeref...)};
        ValueT const out{wasm1p1_details::sign_extend_low_bits<ValueT, NarrowSignedT>(v)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        // result push: operand_base ... [current stack][result bytes] | frame_end
        // [safe compiled post-op byte depth                   ] unsafe (past frame_end)
        // ^^ typeref...[1u] advances by sizeof(out); the frame reserves operand_stack_byte_max from validation.
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
                // operand frame: operand_base ... [live bytes][typed result bytes] | frame_end
                //                [validated post-op stack depth           ] unsafe past frame_end
                // ^^ type...[1u] advances by sizeof(out); operand_stack_byte_max reserves this result.
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            FloatT const v{get_curr_val_from_operand_stack_cache<FloatT>(type...)};
            WasmOutT const out{wasm1p1_details::trunc_sat<WasmOutT, Signed>(v)};
            wasm1p1_details::push_out_value<CompileOption, WasmOutT, curr_out_stack_top>(out, type...);
        }

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        FloatT const v{get_curr_val_from_operand_stack_cache<FloatT>(typeref...)};
        WasmOutT const out{wasm1p1_details::trunc_sat<WasmOutT, Signed>(v)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        // result push: operand_base ... [current stack][result bytes] | frame_end
        // [safe compiled post-op byte depth                   ] unsafe (past frame_end)
        // ^^ typeref...[1u] advances by sizeof(out); the frame reserves operand_stack_byte_max from validation.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_unop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_unop(TypeRef & ... typeref) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_binop(TypeRef & ... typeref) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_testop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_testop(TypeRef & ... typeref) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_splatop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_i32x4_splat(TypeRef & ... typeref) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::v128_splatop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_f32x4_splat(TypeRef & ... typeref) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Lane, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_f32x4_extract_lane(TypeRef & ... typeref) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        static_assert(Lane < 4uz);
        auto const out{wasm1p1_simd_details::eval_extract_lane_i32<wasm1p1_simd_details::simd_code::i32x4_extract_lane>(v, Lane)};
        wasm1p1_simd_details::push_i32_to_memory_stack(out, typeref...[1u]);
    }

    template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_load(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        // Compiled stream: [handler][memory*][offset][lane?][next handler]
        // [safe: the emitter writes every immediate before publishing this function] | unsafe (stream end)
        //                            ^^ IP after the handler-sized advance below
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(type...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                   ^^ IP after read_imm<memory*>
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(type...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                           ^^ IP after read_imm<u32>
        // The memory-aware translator has proved this module-owned pointer live.
        // Only the generic compatibility entry accepts an unclassified immediate.
        if constexpr(BoundsCheckFn == ::uwvm2::runtime::compiler::uwvm_int::optable::details::bounds_check_generic)
        { if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); } }

        // Validated stack: [... address] (loads) or [... address, value] (stores).
        // Popping the address stays within the interpreter's checked stack allocation.
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(type...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(memory);
        BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, sizeof(wasm1p1_simd_details::wasm_v128));

        wasm1p1_simd_details::wasm_v128 out;  // no init
        ::std::memcpy(::std::addressof(out),
                      ::uwvm2::runtime::compiler::uwvm_int::optable::details::ptr_add_u64(memory.memory_begin, eff65.offset),
                      sizeof(out));
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(memory);
        // The compiler includes this result in operand_stack_byte_max.
        // [...][16-byte result][remaining allocated stack] | unsafe (allocation end)
        //                      ^^ SP after the result push
        wasm1p1_simd_details::push_v128_to_memory_stack(out, type...[1u]);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_load(TypeRef & ... typeref) UWVM_THROWS
    {
        // Compiled stream: [handler][memory*][offset][lane?][next handler]
        // [safe: the emitter writes every immediate before publishing this function] | unsafe (stream end)
        //                            ^^ IP after the handler-sized advance below
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(typeref...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                   ^^ IP after read_imm<memory*>
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(typeref...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                           ^^ IP after read_imm<u32>
        // The memory-aware translator has proved this module-owned pointer live.
        // Only the generic compatibility entry accepts an unclassified immediate.
        if constexpr(BoundsCheckFn == ::uwvm2::runtime::compiler::uwvm_int::optable::details::bounds_check_generic)
        { if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); } }

        // Validated stack: [... address] (loads) or [... address, value] (stores).
        // Popping the address stays within the interpreter's checked stack allocation.
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(typeref...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        [[maybe_unused]] auto lock{::uwvm2::runtime::compiler::uwvm_int::optable::details::lock_memory(memory)};
        BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, sizeof(wasm1p1_simd_details::wasm_v128));

        wasm1p1_simd_details::wasm_v128 out;  // no init
        ::std::memcpy(::std::addressof(out),
                      ::uwvm2::runtime::compiler::uwvm_int::optable::details::ptr_add_u64(memory.memory_begin, eff65.offset),
                      sizeof(out));
        // The compiler includes this result in operand_stack_byte_max.
        // [...][16-byte result][remaining allocated stack] | unsafe (allocation end)
        //                      ^^ SP after the result push
        wasm1p1_simd_details::push_v128_to_memory_stack(out, typeref...[1u]);
    }

    template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_store(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        // Compiled stream: [handler][memory*][offset][lane?][next handler]
        // [safe: the emitter writes every immediate before publishing this function] | unsafe (stream end)
        //                            ^^ IP after the handler-sized advance below
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(type...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                   ^^ IP after read_imm<memory*>
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(type...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                           ^^ IP after read_imm<u32>
        // The memory-aware translator has proved this module-owned pointer live.
        // Only the generic compatibility entry accepts an unclassified immediate.
        if constexpr(BoundsCheckFn == ::uwvm2::runtime::compiler::uwvm_int::optable::details::bounds_check_generic)
        { if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); } }

        // [... address][v128 value] | unsafe (stack top)
        //             ^^ SP after consuming the validated 16-byte value
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        // Validated stack: [... address] (loads) or [... address, value] (stores).
        // Popping the address stays within the interpreter's checked stack allocation.
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(type...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(memory);
        BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, sizeof(value));
        ::std::memcpy(::uwvm2::runtime::compiler::uwvm_int::optable::details::prepare_memory_store_pointer_with_policy<BoundsCheckFn, 16uz>(memory, eff65.offset),
                      ::std::addressof(value),
                      sizeof(value));
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(memory);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_v128_store(TypeRef & ... typeref) UWVM_THROWS
    {
        // Compiled stream: [handler][memory*][offset][lane?][next handler]
        // [safe: the emitter writes every immediate before publishing this function] | unsafe (stream end)
        //                            ^^ IP after the handler-sized advance below
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(typeref...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                   ^^ IP after read_imm<memory*>
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(typeref...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                           ^^ IP after read_imm<u32>
        // The memory-aware translator has proved this module-owned pointer live.
        // Only the generic compatibility entry accepts an unclassified immediate.
        if constexpr(BoundsCheckFn == ::uwvm2::runtime::compiler::uwvm_int::optable::details::bounds_check_generic)
        { if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); } }

        // [... address][v128 value] | unsafe (stack top)
        //             ^^ SP after consuming the validated 16-byte value
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        // Validated stack: [... address] (loads) or [... address, value] (stores).
        // Popping the address stays within the interpreter's checked stack allocation.
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(typeref...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        [[maybe_unused]] auto lock{::uwvm2::runtime::compiler::uwvm_int::optable::details::lock_memory(memory)};
        BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, sizeof(value));
        ::std::memcpy(::uwvm2::runtime::compiler::uwvm_int::optable::details::prepare_memory_store_pointer_with_policy<BoundsCheckFn, 16uz>(memory, eff65.offset),
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_unop(TypeRef & ... typeref) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_binop(TypeRef & ... typeref) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_bitselect(TypeRef & ... typeref) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const mask{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_bitselect(lhs, rhs, mask)};
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_ternop(Type... type) UWVM_THROWS
    {
        // Validated v128 operands are in memory after the compiler's cache flush.
        // [safe lower operands][safe mask:16] sp; helper decrements sp by 16 before reading.
        auto const mask{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        // [safe lower operands] sp (the popped mask no longer belongs to the stack).
        // Validated v128 operands are in memory after the compiler's cache flush.
        // [safe lower operands][safe rhs:16] sp; helper decrements sp by 16 before reading.
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        // [safe lower operands] sp (the popped rhs no longer belongs to the stack).
        // Validated v128 operands are in memory after the compiler's cache flush.
        // [safe lower operands][safe lhs:16] sp; helper decrements sp by 16 before reading.
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        // [safe lower operands] sp (the popped lhs no longer belongs to the stack).
        auto const out{wasm1p1_simd_details::eval_full_ternop<Op>(lhs, rhs, mask)};
        // [safe lower operands] sp [safe free:48]; the three pops reserved 48 bytes.
        wasm1p1_simd_details::push_to_memory_stack(out, type...[1u]);
        // [safe lower operands][safe result:16] sp [safe free:32].

        // opfunc next ...; validated bytecode contains the next opfunc slot.
        // [safe] [safe next] ...
        // ^^ ip before dispatch; after += sizeof(opfunc), ip addresses next.
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        // [safe consumed opfunc] [safe next opfunc] ...
        //                        ^^ ip; the function terminator supplies a dispatch target.
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_ternop(TypeRef & ... typeref) UWVM_THROWS
    {
        // opfunc next ...; validated bytecode contains the next opfunc slot.
        // [safe] [safe next] ...
        // ^^ ip before dispatch; after += sizeof(opfunc), ip addresses next.
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        // [safe consumed opfunc] [safe next opfunc] ...
        //                        ^^ ip; the function terminator supplies a dispatch target.
        // Validated v128 operands are in memory after the compiler's cache flush.
        // [safe lower operands][safe mask:16] sp; helper decrements sp by 16 before reading.
        auto const mask{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        // [safe lower operands] sp (the popped mask no longer belongs to the stack).
        // Validated v128 operands are in memory after the compiler's cache flush.
        // [safe lower operands][safe rhs:16] sp; helper decrements sp by 16 before reading.
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        // [safe lower operands] sp (the popped rhs no longer belongs to the stack).
        // Validated v128 operands are in memory after the compiler's cache flush.
        // [safe lower operands][safe lhs:16] sp; helper decrements sp by 16 before reading.
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        // [safe lower operands] sp (the popped lhs no longer belongs to the stack).
        auto const out{wasm1p1_simd_details::eval_full_ternop<Op>(lhs, rhs, mask)};
        // [safe lower operands] sp [safe free:48]; the three pops reserved 48 bytes.
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
        // [safe lower operands][safe result:16] sp [safe free:32].
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_testop(Type... type) UWVM_THROWS
    {
        auto const v{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        auto const out{wasm1p1_simd_details::eval_full_test<Op>(v)};
        wasm1p1_simd_details::push_to_memory_stack(out, type...[1u]);

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_testop(TypeRef & ... typeref) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_shift(TypeRef & ... typeref) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, typename ScalarT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_splat(TypeRef & ... typeref) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        wasm1p1_simd_details::shuffle_controls controls;  // no init
        ::std::memcpy(::std::addressof(controls), type...[0], sizeof(controls));
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ type...[0] advances by sizeof(controls); translation emitted the matching typed slot.
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        wasm1p1_simd_details::shuffle_controls controls;  // no init
        ::std::memcpy(::std::addressof(controls), typeref...[0], sizeof(controls));
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ typeref...[0] advances by sizeof(controls); translation emitted the matching typed slot.
        typeref...[0] += sizeof(controls);
        auto const rhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const lhs{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        auto const out{wasm1p1_simd_details::eval_shuffle(lhs, rhs, controls)};
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_mem_load(Type... type) UWVM_THROWS
    {
        // Compiled stream: [handler][memory*][offset][lane?][next handler]
        // [safe: the emitter writes every immediate before publishing this function] | unsafe (stream end)
        //                            ^^ IP after the handler-sized advance below
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(type...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                   ^^ IP after read_imm<memory*>
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(type...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                           ^^ IP after read_imm<u32>
        wasm1p1_simd_details::u8 lane{};
        if constexpr(Op == wasm1p1_simd_details::simd_code::v128_load8_lane || Op == wasm1p1_simd_details::simd_code::v128_load16_lane ||
                     Op == wasm1p1_simd_details::simd_code::v128_load32_lane || Op == wasm1p1_simd_details::simd_code::v128_load64_lane)
        {
            // [lane][next handler] ... [safe] unsafe (stream end)
            //       ^^ IP after the validated one-byte lane read
            lane = wasm1p1_details::read_imm<wasm1p1_simd_details::u8>(type...[0]);
        }
        // The memory-aware translator has proved this module-owned pointer live.
        // Only the generic compatibility entry accepts an unclassified immediate.
        if constexpr(BoundsCheckFn == ::uwvm2::runtime::compiler::uwvm_int::optable::details::bounds_check_generic)
        { if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); } }

        wasm1p1_simd_details::wasm_v128 old{};  // init
        if constexpr(Op == wasm1p1_simd_details::simd_code::v128_load8_lane || Op == wasm1p1_simd_details::simd_code::v128_load16_lane ||
                     Op == wasm1p1_simd_details::simd_code::v128_load32_lane || Op == wasm1p1_simd_details::simd_code::v128_load64_lane)
        {
            // [... address][old v128] | unsafe (stack top)
            //             ^^ SP after consuming the validated lane source
            old = get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...);
        }
        // Validated stack: [... address] (loads) or [... address, value] (stores).
        // Popping the address stays within the interpreter's checked stack allocation.
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(type...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(memory);
        BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, wasm1p1_simd_details::simd_memory_access_size<Op>());
        auto const out{
            wasm1p1_simd_details::eval_memory_load<Op>(::uwvm2::runtime::compiler::uwvm_int::optable::details::ptr_add_u64(memory.memory_begin, eff65.offset),
                                                       old,
                                                       lane)};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(memory);
        // The compiler includes this result in operand_stack_byte_max.
        // [...][16-byte result][remaining allocated stack] | unsafe (allocation end)
        //                      ^^ SP after the result push
        wasm1p1_simd_details::push_to_memory_stack(out, type...[1u]);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_mem_load(TypeRef & ... typeref) UWVM_THROWS
    {
        // Compiled stream: [handler][memory*][offset][lane?][next handler]
        // [safe: the emitter writes every immediate before publishing this function] | unsafe (stream end)
        //                            ^^ IP after the handler-sized advance below
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(typeref...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                   ^^ IP after read_imm<memory*>
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(typeref...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                           ^^ IP after read_imm<u32>
        wasm1p1_simd_details::u8 lane{};
        if constexpr(Op == wasm1p1_simd_details::simd_code::v128_load8_lane || Op == wasm1p1_simd_details::simd_code::v128_load16_lane ||
                     Op == wasm1p1_simd_details::simd_code::v128_load32_lane || Op == wasm1p1_simd_details::simd_code::v128_load64_lane)
        {
            // [lane][next handler] ... [safe] unsafe (stream end)
            //       ^^ IP after the validated one-byte lane read
            lane = wasm1p1_details::read_imm<wasm1p1_simd_details::u8>(typeref...[0]);
        }
        // The memory-aware translator has proved this module-owned pointer live.
        // Only the generic compatibility entry accepts an unclassified immediate.
        if constexpr(BoundsCheckFn == ::uwvm2::runtime::compiler::uwvm_int::optable::details::bounds_check_generic)
        { if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); } }

        wasm1p1_simd_details::wasm_v128 old{};  // init
        if constexpr(Op == wasm1p1_simd_details::simd_code::v128_load8_lane || Op == wasm1p1_simd_details::simd_code::v128_load16_lane ||
                     Op == wasm1p1_simd_details::simd_code::v128_load32_lane || Op == wasm1p1_simd_details::simd_code::v128_load64_lane)
        {
            // [... address][old v128] | unsafe (stack top)
            //             ^^ SP after consuming the validated lane source
            old = get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...);
        }
        // Validated stack: [... address] (loads) or [... address, value] (stores).
        // Popping the address stays within the interpreter's checked stack allocation.
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(typeref...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        [[maybe_unused]] auto lock{::uwvm2::runtime::compiler::uwvm_int::optable::details::lock_memory(memory)};
        BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, wasm1p1_simd_details::simd_memory_access_size<Op>());
        auto const out{
            wasm1p1_simd_details::eval_memory_load<Op>(::uwvm2::runtime::compiler::uwvm_int::optable::details::ptr_add_u64(memory.memory_begin, eff65.offset),
                                                       old,
                                                       lane)};
        // The compiler includes this result in operand_stack_byte_max.
        // [...][16-byte result][remaining allocated stack] | unsafe (allocation end)
        //                      ^^ SP after the result push
        wasm1p1_simd_details::push_to_memory_stack(out, typeref...[1u]);
    }

    template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_mem_store(Type... type) UWVM_THROWS
    {
        // Compiled stream: [handler][memory*][offset][lane?][next handler]
        // [safe: the emitter writes every immediate before publishing this function] | unsafe (stream end)
        //                            ^^ IP after the handler-sized advance below
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(type...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                   ^^ IP after read_imm<memory*>
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(type...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                           ^^ IP after read_imm<u32>
        wasm1p1_simd_details::u8 lane{};
        if constexpr(Op != wasm1p1_simd_details::simd_code::v128_store)
        {
            // [lane][next handler] ... [safe] unsafe (stream end)
            //       ^^ IP after the validated one-byte lane read
            lane = wasm1p1_details::read_imm<wasm1p1_simd_details::u8>(type...[0]);
        }
        // The memory-aware translator has proved this module-owned pointer live.
        // Only the generic compatibility entry accepts an unclassified immediate.
        if constexpr(BoundsCheckFn == ::uwvm2::runtime::compiler::uwvm_int::optable::details::bounds_check_generic)
        { if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); } }

        // [... address][v128 value] | unsafe (stack top)
        //             ^^ SP after consuming the validated 16-byte value
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(type...)};
        // Validated stack: [... address] (loads) or [... address, value] (stores).
        // Popping the address stays within the interpreter's checked stack allocation.
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(type...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(memory);
        BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, wasm1p1_simd_details::simd_memory_access_size<Op>());
        wasm1p1_simd_details::eval_memory_store<Op>(::uwvm2::runtime::compiler::uwvm_int::optable::details::prepare_memory_store_pointer_with_policy<BoundsCheckFn, wasm1p1_simd_details::simd_memory_access_size<Op>()>(memory, eff65.offset),
                                                    value,
                                                    lane);
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(memory);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_simd_full_mem_store(TypeRef & ... typeref) UWVM_THROWS
    {
        // Compiled stream: [handler][memory*][offset][lane?][next handler]
        // [safe: the emitter writes every immediate before publishing this function] | unsafe (stream end)
        //                            ^^ IP after the handler-sized advance below
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_simd_details::native_memory_t*>(typeref...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                   ^^ IP after read_imm<memory*>
        auto const offset{wasm1p1_details::read_imm<wasm1p1_simd_details::wasm_u32>(typeref...[0])};
        // [handler][memory*][offset][lane?][next handler] ...
        // [safe                                                   ] unsafe
        //                           ^^ IP after read_imm<u32>
        wasm1p1_simd_details::u8 lane{};
        if constexpr(Op != wasm1p1_simd_details::simd_code::v128_store)
        {
            // [lane][next handler] ... [safe] unsafe (stream end)
            //       ^^ IP after the validated one-byte lane read
            lane = wasm1p1_details::read_imm<wasm1p1_simd_details::u8>(typeref...[0]);
        }
        // The memory-aware translator has proved this module-owned pointer live.
        // Only the generic compatibility entry accepts an unclassified immediate.
        if constexpr(BoundsCheckFn == ::uwvm2::runtime::compiler::uwvm_int::optable::details::bounds_check_generic)
        { if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); } }

        // [... address][v128 value] | unsafe (stack top)
        //             ^^ SP after consuming the validated 16-byte value
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_v128>(typeref...)};
        // Validated stack: [... address] (loads) or [... address, value] (stores).
        // Popping the address stays within the interpreter's checked stack allocation.
        auto const addr{get_curr_val_from_operand_stack_cache<wasm1p1_simd_details::wasm_i32>(typeref...)};
        auto const eff65{::uwvm2::runtime::compiler::uwvm_int::optable::details::wasm32_effective_offset(addr, offset)};

        auto& memory{*memory_p};
        [[maybe_unused]] auto lock{::uwvm2::runtime::compiler::uwvm_int::optable::details::lock_memory(memory)};
        BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, wasm1p1_simd_details::simd_memory_access_size<Op>());
        wasm1p1_simd_details::eval_memory_store<Op>(::uwvm2::runtime::compiler::uwvm_int::optable::details::prepare_memory_store_pointer_with_policy<BoundsCheckFn, wasm1p1_simd_details::simd_memory_access_size<Op>()>(memory, eff65.offset),
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        wasm_v128 imm;  // no init
        ::std::memcpy(::std::addressof(imm), type...[0], sizeof(imm));
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ type...[0] advances by sizeof(imm); translation emitted the matching typed slot.
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
            // operand frame: operand_base ... [live bytes][typed result bytes] | frame_end
            //                [validated post-op stack depth           ] unsafe past frame_end
            // ^^ type...[1u] advances by sizeof(imm); operand_stack_byte_max reserves this result.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_v128 imm;  // no init
        ::std::memcpy(::std::addressof(imm), typeref...[0], sizeof(imm));
        // bytecode: [consumed prefix][current typed immediate][following slots] | stream_end
        //                              [complete emitted slot] safe to its end
        // ^^ typeref...[0] advances by sizeof(imm); translation emitted the matching typed slot.
        typeref...[0] += sizeof(imm);

        ::std::memcpy(typeref...[1u], ::std::addressof(imm), sizeof(imm));
        // result push: operand_base ... [current stack][result bytes] | frame_end
        // [safe compiled post-op byte depth                   ] unsafe (past frame_end)
        // ^^ typeref...[1u] advances by sizeof(imm); the frame reserves operand_stack_byte_max from validation.
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

        // [live operand bytes][reserved RefT slot] ... operand allocation end
        // [safe                                 ] validator included this push in the maximum frame size.
        //                      ^^ type...[1u]: the entire slot is writable before advancing the stack cursor.
        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        type...[1u] += sizeof(out);
        // [live operand bytes][RefT] next free slot ... allocation end
        // [safe                   ] ^^ type...[1u], possibly one-past the allocated stack.

        // [current handler pointer][next handler pointer ... bytecode end)
        // [safe                   ] compiler emitted a successor for this nonterminating opcode.
        // Advancing skips exactly the current handler; the successor pointer is fully readable.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        // [current handler pointer][next handler pointer ... bytecode end)
        // [safe                                       ]
        //                           ^^ type...[0]
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_ref_null(TypeRef & ... typeref) UWVM_THROWS
    {
        // [current handler pointer] next bytecode ... end
        // [safe                   ] compiler proved the current handler's complete encoding.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        // [current handler pointer] next bytecode ... end
        // [safe                   ] ^^ typeref...[0], possibly bytecode end; dispatcher checks termination.
        RefT out{};
        out.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_null;
        // [live operand bytes][reserved RefT slot] ... operand allocation end
        // [safe                                 ] validated maximum frame size covers this complete push.
        //                      ^^ typeref...[1u]
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        // result push: operand_base ... [current stack][result bytes] | frame_end
        // [safe compiled post-op byte depth                   ] unsafe (past frame_end)
        // ^^ typeref...[1u] advances by sizeof(out); the frame reserves operand_stack_byte_max from validation.
        typeref...[1u] += sizeof(out);
        // [live operand bytes][RefT] next free slot ... allocation end
        // [safe                   ] ^^ typeref...[1u], possibly one-past the allocated stack.
    }

    namespace wasm1p1_details
    {
        UWVM_GNU_COLD [[noreturn]] inline constexpr void null_reference_terminate() noexcept
        {
            if(trap_null_reference_func != nullptr) { trap_null_reference_func(); }
            ::fast_io::fast_terminate();
        }
        template<typename RefT>
        UWVM_ALWAYS_INLINE inline constexpr void require_non_null_reference(::std::byte const* stack_end) noexcept
        {
            // [older operand values ...][RefT] stack_end
            // [safe                         ] one-past live operands
            //                                 ^^ stack_end: validator proved a reference above this frame's base.
            // Subtraction stays within the allocated operand stack. No pop/push or payload copy is needed.
            auto const reference_begin{stack_end - sizeof(RefT)};
            using kind_type = ::uwvm2::object::global::wasm_ref_kind;
            kind_type kind;
            static_assert(::std::is_standard_layout_v<RefT>);
            static_assert(offsetof(RefT, ref) == 0uz);
            using ref_type = decltype(RefT{}.ref);
            static_assert(offsetof(ref_type, kind) + sizeof(kind_type) <= sizeof(RefT));
            // [RefT payload/padding ...][kind] ... stack_end
            // [safe                                          ]
            //                           ^^ tag_address: offsetof and static_assert bound this field inside RefT.
            auto const tag_address{reference_begin + offsetof(ref_type, kind)};
            ::std::memcpy(::std::addressof(kind), tag_address, sizeof(kind));
            if(kind == kind_type::wasm_null) [[unlikely]] { null_reference_terminate(); }
        }
    }

    namespace wasm1p1_details
    {
        template<typename RefT, bool NonNull, typename Handler>
        inline constexpr void branch_on_reference(::std::byte const*& ip, ::std::byte*& stack_end) noexcept
        {
            static_assert(::std::is_standard_layout_v<RefT> && offsetof(RefT, ref) == 0uz);
            using ref_type = decltype(RefT{}.ref);
            using kind_type = ::uwvm2::object::global::wasm_ref_kind;
            static_assert(offsetof(ref_type, kind) + sizeof(kind_type) <= sizeof(RefT));
            // [older values ...][RefT] stack_end
            // [safe                 ] one-past live operands
            //                    ^^ tag_address lies inside the validator-proven top RefT slot.
            auto const tag_address{stack_end - sizeof(RefT) + offsetof(ref_type, kind)};
            kind_type kind;
            ::std::memcpy(::std::addressof(kind), tag_address, sizeof(kind));
            bool const null_value{kind == kind_type::wasm_null};
            if(null_value)
            {
                // [older values ...][RefT] stack_end
                // [safe                 ] one-past live operands
                //                    ^^ stack_end moves to the checked slot's start; only null values are discarded.
                stack_end -= sizeof(RefT);
            }
            // [handler][target pointer][next handler] ... bytecode_end
            // [safe                                 ] translator-owned, fixed-up bytecode
            // ^^ ip; both pointer slots and the following handler were emitted together.
            auto const target_slot{ip + sizeof(Handler)};
            if(null_value != NonNull)
            {
                ::std::byte const* target;
                ::std::memcpy(::std::addressof(target), target_slot, sizeof(target));
                // [target handler] ... bytecode_end
                // [safe          ] target was patched to a live label in this compiled function.
                // [compiler-emitted bytecode slots][successor] | stream end
                // [complete typed slots                    ] | no guest Wasm read
                // ^^ ip: the emitted target slot was fixed up to a live handler; this branch copies that address.
                ip = target;
            }
            else
            {
                // [handler][target pointer][next handler] ... bytecode_end
                // [safe                                 ]
                //                          ^^ ip advances over the proven target slot.
                ip = target_slot + sizeof(::std::byte const*);
            }
        }
    }
    template<uwvm_interpreter_translate_option_t CompileOption, typename RefT, bool NonNull, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_on_reference(Type... type) UWVM_THROWS
    {
        using handler = uwvm_interpreter_opfunc_t<Type...>;
        wasm1p1_details::branch_on_reference<RefT, NonNull, handler>(type...[0u], type...[1u]);
        handler next;
        // ip names a translator-proven handler slot on either branch edge; memcpy permits unaligned bytecode.
        ::std::memcpy(::std::addressof(next), type...[0u], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }
    template<uwvm_interpreter_translate_option_t CompileOption, typename RefT, bool NonNull, uwvm_int_stack_top_type... Type>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_on_reference(Type&... type) UWVM_THROWS
    {
        using handler = uwvm_interpreter_opfunc_byref_t<Type...>;
        wasm1p1_details::branch_on_reference<RefT, NonNull, handler>(type...[0u], type...[1u]);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_ref_as_non_null(Type... type) UWVM_THROWS
    {
        wasm1p1_details::require_non_null_reference<RefT>(type...[1u]);
        // [this handler pointer][next handler pointer] ... bytecode_end
        // [safe                                           ] translator-owned threaded bytecode
        // ^^ type...[0]: dispatch proves this slot; the translator always emits a following handler.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        // [this handler pointer][next handler pointer] ... bytecode_end
        // [safe                                           ]
        //                       ^^ type...[0]: next pointer-sized slot is available.
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }
    template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_ref_as_non_null(TypeRef&... typeref) UWVM_THROWS
    {
        wasm1p1_details::require_non_null_reference<RefT>(typeref...[1u]);
        // [this handler pointer][next handler pointer] ... bytecode_end
        // [safe                                           ] translator emitted both slots.
        // ^^ typeref...[0]; advance to the next slot, without changing the operand stack pointer.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        // [this handler pointer][next handler pointer] ... bytecode_end
        // [safe                                           ]
        //                       ^^ typeref...[0]
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        RefT const ref{get_curr_val_from_operand_stack_cache<RefT>(typeref...)};
        wasm_i32 const out{ref.ref.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null ? wasm_i32{1} : wasm_i32{}};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        // result push: operand_base ... [current stack][result bytes] | frame_end
        // [safe compiled post-op byte depth                   ] unsafe (past frame_end)
        // ^^ typeref...[1u] advances by sizeof(out); the frame reserves operand_stack_byte_max from validation.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(type...[0])};
        auto const func_index{wasm1p1_details::read_imm<wasm_u32>(type...[0])};
        wasm_funcref const out{wasm1p1_details::funcref_from_table_elem(wasm1p1_details::resolve_table_elem_from_func_index(module, func_index))};

        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        // operand frame: operand_base ... [live bytes][typed result bytes] | frame_end
        //                [validated post-op stack depth           ] unsafe past frame_end
        // ^^ type...[1u] advances by sizeof(out); operand_stack_byte_max reserves this result.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(typeref...[0])};
        auto const func_index{wasm1p1_details::read_imm<wasm_u32>(typeref...[0])};
        wasm_funcref const out{wasm1p1_details::funcref_from_table_elem(wasm1p1_details::resolve_table_elem_from_func_index(module, func_index))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        // result push: operand_base ... [current stack][result bytes] | frame_end
        // [safe compiled post-op byte depth                   ] unsafe (past frame_end)
        // ^^ typeref...[1u] advances by sizeof(out); the frame reserves operand_stack_byte_max from validation.
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_get_funcref(Type... type) UWVM_THROWS
    {
        using wasm_funcref = wasm1p1_details::wasm_funcref;

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const index{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }

        wasm_funcref const out{wasm1p1_details::funcref_from_table_elem(table->elems.index_unchecked(index))};
        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        // operand frame: operand_base ... [live bytes][typed result bytes] | frame_end
        //                [validated post-op stack depth           ] unsafe past frame_end
        // ^^ type...[1u] advances by sizeof(out); operand_stack_byte_max reserves this result.
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

        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const index{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }

        wasm_funcref const out{wasm1p1_details::funcref_from_table_elem(table->elems.index_unchecked(index))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        // result push: operand_base ... [current stack][result bytes] | frame_end
        // [safe compiled post-op byte depth                   ] unsafe (past frame_end)
        // ^^ typeref...[1u] advances by sizeof(out); the frame reserves operand_stack_byte_max from validation.
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_set_funcref(Type... type) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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

    template <bool ExnRef, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_get_externref(Type... type) UWVM_THROWS
    {
        // [handler pointer][validated table/element immediates][next handler] ... bytecode_end
        // [safe           ][safe                             ] compiler-owned record.
        // ^^ IP before; advance over the complete handler pointer only.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        // [consumed handler][first complete pointer immediate] ... bytecode_end
        // [safe           ] ^^ IP, now at a compiler-emitted native pointer.
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        [[maybe_unused]] wasm1p1_details::runtime_module_storage_t const* caller_module{};
        if constexpr(ExnRef)
        {
            // [caller-module immediate][next handler] ... bytecode_end
            // [safe                  ] read_imm advances only through compiler-owned bytecode.
            caller_module = wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(type...[0]);
            if(caller_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        }
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const index{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }

        auto const out{wasm1p1_details::table_reference_from_elem<ExnRef>(table->elems.index_unchecked(index))};
        if constexpr(ExnRef)
        {
            // The provider table may later be cleared or unloaded. The caller owns a
            // checked exn-token root before this complete 16-byte value enters its stack.
            if(out.ref.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_exn &&
               ::uwvm2::uwvm::runtime::storage::uwvm2_gc_retain_reference(
                   caller_module->gc_store.get(), ::std::addressof(out.ref)) !=
                   ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
            { ::fast_io::fast_terminate(); }
        }
        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        // [live operands][complete 16-byte reference] <- SP; slot proved by the translator.
        type...[1u] += sizeof(out);
        // [live operands reference] <- SP, possibly at the reserved one-past boundary.

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <bool ExnRef, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_get_externref(TypeRef & ... typeref) UWVM_THROWS
    {
        // [handler pointer][validated table/element immediates][next handler] ... bytecode_end
        // [safe           ][safe                             ] compiler-owned record.
        // ^^ IP before; advance over the complete handler pointer only.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        // [consumed handler][first complete pointer immediate] ... bytecode_end
        // [safe           ] ^^ IP, now at a compiler-emitted native pointer.
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        [[maybe_unused]] wasm1p1_details::runtime_module_storage_t const* caller_module{};
        if constexpr(ExnRef)
        {
            // [caller-module immediate][next handler] ... bytecode_end
            // [safe                  ] read_imm advances only through compiler-owned bytecode.
            caller_module = wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(typeref...[0]);
            if(caller_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        }
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const index{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }

        auto const out{wasm1p1_details::table_reference_from_elem<ExnRef>(table->elems.index_unchecked(index))};
        if constexpr(ExnRef)
        {
            // The provider table may later be cleared or unloaded. The caller owns a
            // checked exn-token root before this complete 16-byte value enters its stack.
            if(out.ref.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_exn &&
               ::uwvm2::uwvm::runtime::storage::uwvm2_gc_retain_reference(
                   caller_module->gc_store.get(), ::std::addressof(out.ref)) !=
                   ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
            { ::fast_io::fast_terminate(); }
        }
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        // [live operands][complete 16-byte reference] <- SP; slot proved by the translator.
        // result push: operand_base ... [current stack][result bytes] | frame_end
        // [safe compiled post-op byte depth                   ] unsafe (past frame_end)
        // ^^ typeref...[1u] advances by sizeof(out); the frame reserves operand_stack_byte_max from validation.
        typeref...[1u] += sizeof(out);
        // [live operands reference] <- SP, possibly at the reserved one-past boundary.
    }

    template <bool ExnRef, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_set_externref(Type... type) UWVM_THROWS
    {
        // [handler pointer][validated table/element immediates][next handler] ... bytecode_end
        // [safe           ][safe                             ] compiler-owned record.
        // ^^ IP before; advance over the complete handler pointer only.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        // [consumed handler][first complete pointer immediate] ... bytecode_end
        // [safe           ] ^^ IP, now at a compiler-emitted native pointer.
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_externref>(type...)};
        auto const index{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }
        wasm1p1_details::retain_table_reference<ExnRef>(table, value);
        table->elems.index_unchecked(index) = wasm1p1_details::table_elem_from_reference<ExnRef>(value);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <bool ExnRef, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_set_externref(TypeRef & ... typeref) UWVM_THROWS
    {
        // [handler pointer][validated table/element immediates][next handler] ... bytecode_end
        // [safe           ][safe                             ] compiler-owned record.
        // ^^ IP before; advance over the complete handler pointer only.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        // [consumed handler][first complete pointer immediate] ... bytecode_end
        // [safe           ] ^^ IP, now at a compiler-emitted native pointer.
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_externref>(typeref...)};
        auto const index{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        if(index >= table->elems.size()) [[unlikely]] { wasm1p1_details::table_oob_terminate(); }
        wasm1p1_details::retain_table_reference<ExnRef>(table, value);
        table->elems.index_unchecked(index) = wasm1p1_details::table_elem_from_reference<ExnRef>(value);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_data_drop(Type... type) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const data{wasm1p1_details::read_imm<wasm1p1_details::runtime_data_storage_t*>(typeref...[0])};
        if(data == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        ::uwvm2::uwvm::runtime::storage::drop_wasm_data_segment_payload(data->data);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_elem_drop(Type... type) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const element{wasm1p1_details::read_imm<wasm1p1_details::runtime_element_storage_t*>(typeref...[0])};
        if(element == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        ::uwvm2::uwvm::runtime::storage::drop_wasm_element_segment_payload(element->element);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory_init(Type... type) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_details::native_memory_t*>(type...[0])};
        auto const data{wasm1p1_details::read_imm<wasm1p1_details::runtime_data_storage_t*>(type...[0])};
        if(memory_p == nullptr || data == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const src{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};

        auto const payload{::uwvm2::uwvm::runtime::storage::load_wasm_data_segment_payload(data->data)};
        // [module-owned immutable payload] end, or an empty dropped snapshot.
        // ^^ data_begin; data.drop cannot modify either borrowed pointer.
        auto const data_begin{payload.byte_begin};
        auto const data_end{payload.byte_end};
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_details::native_memory_t*>(typeref...[0])};
        auto const data{wasm1p1_details::read_imm<wasm1p1_details::runtime_data_storage_t*>(typeref...[0])};
        if(memory_p == nullptr || data == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const src{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};

        auto const payload{::uwvm2::uwvm::runtime::storage::load_wasm_data_segment_payload(data->data)};
        // [module-owned immutable payload] end, or an empty dropped snapshot.
        // ^^ data_begin; data.drop cannot modify either borrowed pointer.
        auto const data_begin{payload.byte_begin};
        auto const data_end{payload.byte_end};
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
        // [opfunc] [destination pointer] [source pointer] ...: trusted compiler-owned bytecode.
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_details::native_memory_t*>(type...[0])};
        // [opfunc destination pointer] source pointer ...
        // [safe                      ] safe: the compiler emits both pointer immediates.
        auto const source_memory_p{wasm1p1_details::read_imm<wasm1p1_details::native_memory_t*>(type...[0])};
        // [opfunc destination pointer source pointer] next opfunc: bytecode cursor is at the next record.
        if(source_memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const src{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};

        auto& memory{*memory_p};
        auto& source_memory{*source_memory_p};
        // Independent memories may be copied in opposite directions by different threads. A total
        // pointer order avoids lock inversion; aliased imports take the shared object's lock once.
        bool const source_first{reinterpret_cast<::std::uintptr_t>(source_memory_p) < reinterpret_cast<::std::uintptr_t>(memory_p)};
        auto& first{source_first ? source_memory : memory};
        auto& second{source_first ? memory : source_memory};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(first);
        if(memory_p != source_memory_p) { ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(second); }
        auto const memory_length{::uwvm2::runtime::compiler::uwvm_int::optable::details::load_memory_length_for_oob_unlocked(memory)};
        auto const source_length{::uwvm2::runtime::compiler::uwvm_int::optable::details::load_memory_length_for_oob_unlocked(source_memory)};
        if(wasm1p1_details::range_oob(src, len, source_length) || wasm1p1_details::range_oob(dst, len, memory_length)) [[unlikely]]
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::details::memory_oob_terminate(
                0uz, 0uz, {static_cast<::std::uint_least64_t>(dst), false}, memory_length, len);
        }
        // Both complete ranges were checked before any mutation. Zero-length endpoints may be one-past-end;
        // do not form/dereference their pointers. memmove preserves overlapping aliased-memory semantics.
        if(len != 0uz) { ::std::memmove(memory.memory_begin + dst, source_memory.memory_begin + src, len); }
        if(memory_p != source_memory_p) { ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(second); }
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(first);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory_copy(TypeRef & ... typeref) UWVM_THROWS
    {
        // [opfunc] [destination pointer] [source pointer] ...: trusted compiler-owned bytecode.
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const memory_p{wasm1p1_details::read_imm<wasm1p1_details::native_memory_t*>(typeref...[0])};
        // [opfunc destination pointer] source pointer ...
        // [safe                      ] safe: the compiler emits both pointer immediates.
        auto const source_memory_p{wasm1p1_details::read_imm<wasm1p1_details::native_memory_t*>(typeref...[0])};
        // [opfunc destination pointer source pointer] next opfunc: bytecode cursor is at the next record.
        if(source_memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const src{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const dst{static_cast<::std::size_t>(wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};

        auto& memory{*memory_p};
        auto& source_memory{*source_memory_p};
        // Independent memories may be copied in opposite directions by different threads. A total
        // pointer order avoids lock inversion; aliased imports take the shared object's lock once.
        bool const source_first{reinterpret_cast<::std::uintptr_t>(source_memory_p) < reinterpret_cast<::std::uintptr_t>(memory_p)};
        auto& first{source_first ? source_memory : memory};
        auto& second{source_first ? memory : source_memory};
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(first);
        if(memory_p != source_memory_p) { ::uwvm2::runtime::compiler::uwvm_int::optable::details::enter_memory_operation_memory_lock(second); }
        auto const memory_length{::uwvm2::runtime::compiler::uwvm_int::optable::details::load_memory_length_for_oob_unlocked(memory)};
        auto const source_length{::uwvm2::runtime::compiler::uwvm_int::optable::details::load_memory_length_for_oob_unlocked(source_memory)};
        if(wasm1p1_details::range_oob(src, len, source_length) || wasm1p1_details::range_oob(dst, len, memory_length)) [[unlikely]]
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::details::memory_oob_terminate(
                0uz, 0uz, {static_cast<::std::uint_least64_t>(dst), false}, memory_length, len);
        }
        // Both complete ranges were checked before any mutation. Zero-length endpoints may be one-past-end;
        // do not form/dereference their pointers. memmove preserves overlapping aliased-memory semantics.
        if(len != 0uz) { ::std::memmove(memory.memory_begin + dst, source_memory.memory_begin + src, len); }
        if(memory_p != source_memory_p) { ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(second); }
        ::uwvm2::runtime::compiler::uwvm_int::optable::details::exit_memory_operation_memory_lock(first);

    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_memory_fill(Type... type) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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

    template <bool ExnRef, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_init_externref(Type... type) UWVM_THROWS
    {
        // [handler pointer][validated table/element immediates][next handler] ... bytecode_end
        // [safe           ][safe                             ] compiler-owned record.
        // ^^ IP before; advance over the complete handler pointer only.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        // [consumed handler][first complete pointer immediate] ... bytecode_end
        // [safe           ] ^^ IP, now at a compiler-emitted native pointer.
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        auto const element{wasm1p1_details::read_imm<wasm1p1_details::runtime_element_storage_t*>(type...[0])};
        if(table == nullptr || element == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const src{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const dst{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};

        // [live element record] retained by this entry's execution lease.
        // The acquired view is either wholly empty or the immutable source pair.
        auto const payload{::uwvm2::uwvm::runtime::storage::load_wasm_element_segment_payload(element->element)};
        auto const begin{payload.externref_begin};
        auto const end{payload.externref_end};
        if((begin == nullptr) != (end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const elem_len{begin == nullptr ? 0uz : static_cast<::std::size_t>(end - begin)};
        wasm1p1_details::check_table_range(src, len, elem_len);
        wasm1p1_details::check_table_range(dst, len, table->elems.size());

        for(::std::size_t i{}; i != len; ++i)
        {
            auto& slot{table->elems.index_unchecked(dst + i)};
            wasm1p1_details::retain_table_payload<ExnRef>(table, begin[src + i]);
            slot.storage.extern_ptr = begin[src + i];
            slot.type = (ExnRef ? wasm1p1_details::runtime_table_elem_type::exn_ref : wasm1p1_details::runtime_table_elem_type::extern_ref);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <bool ExnRef, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_init_externref(TypeRef & ... typeref) UWVM_THROWS
    {
        // [handler pointer][validated table/element immediates][next handler] ... bytecode_end
        // [safe           ][safe                             ] compiler-owned record.
        // ^^ IP before; advance over the complete handler pointer only.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        // [consumed handler][first complete pointer immediate] ... bytecode_end
        // [safe           ] ^^ IP, now at a compiler-emitted native pointer.
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        auto const element{wasm1p1_details::read_imm<wasm1p1_details::runtime_element_storage_t*>(typeref...[0])};
        if(table == nullptr || element == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const src{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const dst{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};

        // [live element record] retained by this entry's execution lease.
        // The acquired view is either wholly empty or the immutable source pair.
        auto const payload{::uwvm2::uwvm::runtime::storage::load_wasm_element_segment_payload(element->element)};
        auto const begin{payload.externref_begin};
        auto const end{payload.externref_end};
        if((begin == nullptr) != (end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const elem_len{begin == nullptr ? 0uz : static_cast<::std::size_t>(end - begin)};
        wasm1p1_details::check_table_range(src, len, elem_len);
        wasm1p1_details::check_table_range(dst, len, table->elems.size());

        for(::std::size_t i{}; i != len; ++i)
        {
            auto& slot{table->elems.index_unchecked(dst + i)};
            wasm1p1_details::retain_table_payload<ExnRef>(table, begin[src + i]);
            slot.storage.extern_ptr = begin[src + i];
            slot.type = (ExnRef ? wasm1p1_details::runtime_table_elem_type::exn_ref : wasm1p1_details::runtime_table_elem_type::extern_ref);
        }
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_copy_funcref(Type... type) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
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
            if(wasm1p1_details::runtime_table_is_externref(*dst_table))
            {
                for(::std::size_t i{}; i != len; ++i)
                {
                    // [source range] was bounded above; only VM-owned table
                    // slots are read before the destination becomes visible.
                    auto const value{wasm1p1_details::externref_from_table_elem(
                        src_table->elems.index_unchecked(src + i))};
                    wasm1p1_details::retain_table_externref(dst_table, value);
                }
            }
            else if(wasm1p1_details::runtime_table_is_exnref(*dst_table))
            {
                for(::std::size_t i{}; i != len; ++i)
                {
                    // [source slots ... src+i] end: range checked before entry; retain before memmove.
                    auto const value{wasm1p1_details::table_reference_from_elem<true>(
                        src_table->elems.index_unchecked(src + i))};
                    wasm1p1_details::retain_table_reference<true>(dst_table, value);
                }
            }
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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
            if(wasm1p1_details::runtime_table_is_externref(*dst_table))
            {
                for(::std::size_t i{}; i != len; ++i)
                {
                    // [source range] was bounded above; no guest pointer is dereferenced.
                    auto const value{wasm1p1_details::externref_from_table_elem(
                        src_table->elems.index_unchecked(src + i))};
                    wasm1p1_details::retain_table_externref(dst_table, value);
                }
            }
            else if(wasm1p1_details::runtime_table_is_exnref(*dst_table))
            {
                for(::std::size_t i{}; i != len; ++i)
                {
                    // [source slots ... src+i] end: range checked before entry; retain before memmove.
                    auto const value{wasm1p1_details::table_reference_from_elem<true>(
                        src_table->elems.index_unchecked(src + i))};
                    wasm1p1_details::retain_table_reference<true>(dst_table, value);
                }
            }
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
        // [handler pointer][validated table/module immediates][next handler] ... bytecode_end
        // [safe           ] compiler-owned bytecode guarantees the complete encoded operation.
        // Advance IP from this live handler slot to its first immediate.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        // [current pointer immediate][remaining immediates or next handler] ... bytecode_end
        // [safe                     ] read_imm copies one native pointer and advances IP by its size.
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        // [current pointer immediate][remaining immediates or next handler] ... bytecode_end
        // [safe                     ] read_imm copies one native pointer and advances IP by its size.
        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(type...[0])};
        if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        // [live reference operand][live i32 delta] <- SP (or validated register-ring entries)
        // Both pops consume compiler-validated operands; spilled pops retreat SP only within its live allocation.
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
            if(::uwvm2::uwvm::runtime::storage::try_grow_table_elements(*table, new_size, elem))
            {
                out = wasm1p1_details::u32_to_i32(static_cast<::std::uint_least32_t>(old_size));
                if(delta != 0uz)
                {
                    wasm1p1_details::refresh_llvm_call_indirect_table_views_after_funcref_write(
                        table, wasm1p1_details::runtime_table_mutation_kind::grow, old_size, delta);
                }
            }
        }

        // [remaining operands][vacated operand space] <- SP
        // [safe              ][at least sizeof(i32) writable bytes] from the two operands just consumed.
        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        type...[1u] += sizeof(out);
        // [remaining operands][live i32 result] <- SP; may be one-past the live operand region.

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        // IP now names the complete next handler pointer retained in compiler-owned bytecode.
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_grow_funcref(TypeRef & ... typeref) UWVM_THROWS
    {
        // [handler pointer][validated table/module immediates][next handler] ... bytecode_end
        // [safe           ] compiler-owned bytecode guarantees the complete encoded operation.
        // Advance IP from this live handler slot to its first immediate.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        // [current pointer immediate][remaining immediates or next handler] ... bytecode_end
        // [safe                     ] read_imm copies one native pointer and advances IP by its size.
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        // [current pointer immediate][remaining immediates or next handler] ... bytecode_end
        // [safe                     ] read_imm copies one native pointer and advances IP by its size.
        auto const module{wasm1p1_details::read_imm<wasm1p1_details::runtime_module_storage_t const*>(typeref...[0])};
        if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        // [live reference operand][live i32 delta] <- SP (or validated register-ring entries)
        // Both pops consume compiler-validated operands; spilled pops retreat SP only within its live allocation.
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
            if(::uwvm2::uwvm::runtime::storage::try_grow_table_elements(*table, new_size, elem))
            {
                out = wasm1p1_details::u32_to_i32(static_cast<::std::uint_least32_t>(old_size));
                if(delta != 0uz)
                {
                    wasm1p1_details::refresh_llvm_call_indirect_table_views_after_funcref_write(
                        table, wasm1p1_details::runtime_table_mutation_kind::grow, old_size, delta);
                }
            }
        }

        // [remaining operands][vacated operand space] <- SP
        // [safe              ][at least sizeof(i32) writable bytes] from the two operands just consumed.
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        // result push: operand_base ... [current stack][result bytes] | frame_end
        // [safe compiled post-op byte depth                   ] unsafe (past frame_end)
        // ^^ typeref...[1u] advances by sizeof(out); the frame reserves operand_stack_byte_max from validation.
        typeref...[1u] += sizeof(out);
        // [remaining operands][live i32 result] <- SP; may be one-past the live operand region.
    }

    template <bool ExnRef, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_grow_externref(Type... type) UWVM_THROWS
    {
        // [handler pointer][validated table/module immediates][next handler] ... bytecode_end
        // [safe           ] compiler-owned bytecode guarantees the complete encoded operation.
        // Advance IP from this live handler slot to its first immediate.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        // [current pointer immediate][remaining immediates or next handler] ... bytecode_end
        // [safe                     ] read_imm copies one native pointer and advances IP by its size.
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        // [live reference operand][live i32 delta] <- SP (or validated register-ring entries)
        // Both pops consume compiler-validated operands; spilled pops retreat SP only within its live allocation.
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
            auto const elem{wasm1p1_details::table_elem_from_reference<ExnRef>(value)};
            if(delta != 0uz) { wasm1p1_details::retain_table_reference<ExnRef>(table, value); }
            if(::uwvm2::uwvm::runtime::storage::try_grow_table_elements(*table, new_size, elem))
            {
                out = wasm1p1_details::u32_to_i32(static_cast<::std::uint_least32_t>(old_size));
            }
        }

        // [remaining operands][vacated operand space] <- SP
        // [safe              ][at least sizeof(i32) writable bytes] from the two operands just consumed.
        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        type...[1u] += sizeof(out);
        // [remaining operands][live i32 result] <- SP; may be one-past the live operand region.
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        // IP now names the complete next handler pointer retained in compiler-owned bytecode.
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <bool ExnRef, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_grow_externref(TypeRef & ... typeref) UWVM_THROWS
    {
        // [handler pointer][validated table/module immediates][next handler] ... bytecode_end
        // [safe           ] compiler-owned bytecode guarantees the complete encoded operation.
        // Advance IP from this live handler slot to its first immediate.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        // [current pointer immediate][remaining immediates or next handler] ... bytecode_end
        // [safe                     ] read_imm copies one native pointer and advances IP by its size.
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        // [live reference operand][live i32 delta] <- SP (or validated register-ring entries)
        // Both pops consume compiler-validated operands; spilled pops retreat SP only within its live allocation.
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
            auto const elem{wasm1p1_details::table_elem_from_reference<ExnRef>(value)};
            if(delta != 0uz) { wasm1p1_details::retain_table_reference<ExnRef>(table, value); }
            if(::uwvm2::uwvm::runtime::storage::try_grow_table_elements(*table, new_size, elem))
            {
                out = wasm1p1_details::u32_to_i32(static_cast<::std::uint_least32_t>(old_size));
            }
        }

        // [remaining operands][vacated operand space] <- SP
        // [safe              ][at least sizeof(i32) writable bytes] from the two operands just consumed.
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        // result push: operand_base ... [current stack][result bytes] | frame_end
        // [safe compiled post-op byte depth                   ] unsafe (past frame_end)
        // ^^ typeref...[1u] advances by sizeof(out); the frame reserves operand_stack_byte_max from validation.
        typeref...[1u] += sizeof(out);
        // [remaining operands][live i32 result] <- SP; may be one-past the live operand region.
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_size(Type... type) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const out{wasm1p1_details::u32_to_i32(static_cast<::std::uint_least32_t>(table->elems.size()))};
        ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
        // operand frame: operand_base ... [live bytes][typed result bytes] | frame_end
        //                [validated post-op stack depth           ] unsafe past frame_end
        // ^^ type...[1u] advances by sizeof(out); operand_stack_byte_max reserves this result.
        type...[1u] += sizeof(out);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_size(TypeRef & ... typeref) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const out{wasm1p1_details::u32_to_i32(static_cast<::std::uint_least32_t>(table->elems.size()))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        // result push: operand_base ... [current stack][result bytes] | frame_end
        // [safe compiled post-op byte depth                   ] unsafe (past frame_end)
        // ^^ typeref...[1u] advances by sizeof(out); the frame reserves operand_stack_byte_max from validation.
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_fill_funcref(Type... type) UWVM_THROWS
    {
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ type...[0] advances by sizeof(uwvm_interpreter_opfunc_t<Type...>); the emitter wrote this complete opfunc slot.
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
        // bytecode: [current opfunc pointer][emitted operands / successor] | stream_end
        //           [complete pointer slot ] safe to its end; unsafe past stream_end
        // ^^ typeref...[0] advances by sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>); the emitter wrote this complete opfunc slot.
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

    template <bool ExnRef, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_fill_externref(Type... type) UWVM_THROWS
    {
        // [handler pointer][validated table/element immediates][next handler] ... bytecode_end
        // [safe           ][safe                             ] compiler-owned record.
        // ^^ IP before; advance over the complete handler pointer only.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        // [consumed handler][first complete pointer immediate] ... bytecode_end
        // [safe           ] ^^ IP, now at a compiler-emitted native pointer.
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(type...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_externref>(type...)};
        auto const index{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(type...)))};
        wasm1p1_details::check_table_range(index, len, table->elems.size());

        auto const elem{wasm1p1_details::table_elem_from_reference<ExnRef>(value)};
        if(len != 0uz) { wasm1p1_details::retain_table_reference<ExnRef>(table, value); }
        for(::std::size_t i{}; i != len; ++i) { table->elems.index_unchecked(index + i) = elem; }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <bool ExnRef, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_table_fill_externref(TypeRef & ... typeref) UWVM_THROWS
    {
        // [handler pointer][validated table/element immediates][next handler] ... bytecode_end
        // [safe           ][safe                             ] compiler-owned record.
        // ^^ IP before; advance over the complete handler pointer only.
        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        // [consumed handler][first complete pointer immediate] ... bytecode_end
        // [safe           ] ^^ IP, now at a compiler-emitted native pointer.
        auto const table{wasm1p1_details::read_imm<wasm1p1_details::runtime_table_storage_t*>(typeref...[0])};
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const len{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        auto const value{get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_externref>(typeref...)};
        auto const index{static_cast<::std::size_t>(
            wasm1p1_details::i32_to_u32(get_curr_val_from_operand_stack_cache<wasm1p1_details::wasm_i32>(typeref...)))};
        wasm1p1_details::check_table_range(index, len, table->elems.size());

        auto const elem{wasm1p1_details::table_elem_from_reference<ExnRef>(value)};
        if(len != 0uz) { wasm1p1_details::retain_table_reference<ExnRef>(table, value); }
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

        // Select the immutable protection policy once while translating, exactly
        // as scalar memory handlers do. Full mmap SIMD loads then contain no
        // length/status lookup or bounds branch; software/pinned backends retain
        // their complete checks. Wasm memarg.align is never treated as a proof.
        template <bool WritesMemory, typename Select>
        inline constexpr auto select_simd_memory_fptr(wasm1p1_simd_details::native_memory_t const& memory, Select select) noexcept
        {
            namespace mem = ::uwvm2::runtime::compiler::uwvm_int::optable::details;
# if defined(UWVM_SUPPORT_MMAP)
            switch(details::select_mmap_variant(memory))
            {
                case details::mmap_variant::full: return select.template operator()<mem::bounds_check_mmap_full>();
                case details::mmap_variant::full_standard_page:
                    if constexpr(WritesMemory) { return select.template operator()<mem::bounds_check_mmap_full_standard_page>(); }
                    else { return select.template operator()<mem::bounds_check_mmap_full>(); }
                case details::mmap_variant::path: return select.template operator()<mem::bounds_check_mmap_path>();
                case details::mmap_variant::judge: return select.template operator()<mem::bounds_check_mmap_judge>();
            }
            ::fast_io::fast_terminate();
# else
            static_cast<void>(memory);
            return select.template operator()<mem::bounds_check_allocator>();
# endif
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_v128_load_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_v128_load<::uwvm2::runtime::compiler::uwvm_int::optable::details::bounds_check_generic, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_v128_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_v128_load_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_v128_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const&,
            wasm1p1_simd_details::native_memory_t const& memory,
            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        {
            return select_simd_memory_fptr<false>(memory, []<auto BoundsCheckFn>() constexpr noexcept
            { return uwvmint_simd_v128_load<BoundsCheckFn, CompileOption, TypeInTuple...>; });
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_v128_store_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_v128_store<::uwvm2::runtime::compiler::uwvm_int::optable::details::bounds_check_generic, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_v128_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_v128_store_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_v128_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const&,
            wasm1p1_simd_details::native_memory_t const& memory,
            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        {
            return select_simd_memory_fptr<true>(memory, []<auto BoundsCheckFn>() constexpr noexcept
            { return uwvmint_simd_v128_store<BoundsCheckFn, CompileOption, TypeInTuple...>; });
        }

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
        inline constexpr auto get_uwvmint_simd_full_ternop_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_full_ternop<CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_ternop_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_ternop_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

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
        { return uwvmint_simd_full_mem_load<::uwvm2::runtime::compiler::uwvm_int::optable::details::bounds_check_generic, CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_mem_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_mem_load_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_mem_load_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const&,
            wasm1p1_simd_details::native_memory_t const& memory,
            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        {
            return select_simd_memory_fptr<false>(memory, []<auto BoundsCheckFn>() constexpr noexcept
            { return uwvmint_simd_full_mem_load<BoundsCheckFn, CompileOption, Op, TypeInTuple...>; });
        }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... Type>
        inline constexpr auto get_uwvmint_simd_full_mem_store_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_simd_full_mem_store<::uwvm2::runtime::compiler::uwvm_int::optable::details::bounds_check_generic, CompileOption, Op, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_mem_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_simd_full_mem_store_fptr<CompileOption, Op, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, wasm1p1_simd_details::simd_code Op, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_simd_full_mem_store_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const&,
            wasm1p1_simd_details::native_memory_t const& memory,
            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        {
            return select_simd_memory_fptr<true>(memory, []<auto BoundsCheckFn>() constexpr noexcept
            { return uwvmint_simd_full_mem_store<BoundsCheckFn, CompileOption, Op, TypeInTuple...>; });
        }

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

        template<uwvm_interpreter_translate_option_t CompileOption, typename RefT, bool NonNull, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_on_reference_typed_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_on_reference<CompileOption, RefT, NonNull, Type...>; }
        template<uwvm_interpreter_translate_option_t CompileOption, typename RefT, bool NonNull, typename... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_on_reference_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& position,
            ::uwvm2::utils::container::tuple<Type...>) noexcept
        { return get_uwvmint_br_on_reference_typed_fptr<CompileOption, RefT, NonNull, Type...>(position); }
        template<uwvm_interpreter_translate_option_t CompileOption, typename RefT, bool NonNull, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_on_reference_typed_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_on_reference<CompileOption, RefT, NonNull, Type...>; }
        template<uwvm_interpreter_translate_option_t CompileOption, typename RefT, bool NonNull, typename... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_on_reference_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& position,
            ::uwvm2::utils::container::tuple<Type...>) noexcept
        { return get_uwvmint_br_on_reference_typed_fptr<CompileOption, RefT, NonNull, Type...>(position); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_ref_as_non_null_typed_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_ref_as_non_null<CompileOption, RefT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_ref_as_non_null_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_ref_as_non_null_typed_fptr<CompileOption, RefT, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_ref_as_non_null_typed_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_ref_as_non_null<CompileOption, RefT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RefT, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_ref_as_non_null_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_ref_as_non_null_typed_fptr<CompileOption, RefT, TypeInTuple...>(curr_stacktop); }

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
        { return uwvmint_table_get_externref<false, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_get_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_get_externref<false, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_get_externref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_get_externref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_get_exnref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_get_externref<true, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_get_exnref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_get_externref<true, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_get_exnref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_get_exnref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_set_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_set_externref<false, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_set_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_set_externref<false, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_set_externref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_set_externref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_set_exnref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_set_externref<true, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_set_exnref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_set_externref<true, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_set_exnref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_set_exnref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

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
        { return uwvmint_table_init_externref<false, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_init_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_init_externref<false, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_init_externref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_init_externref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_init_exnref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_init_externref<true, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_init_exnref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_init_externref<true, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_init_exnref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_init_exnref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

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
        { return uwvmint_table_grow_externref<false, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_grow_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_grow_externref<false, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_grow_externref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_grow_externref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_grow_exnref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_grow_externref<true, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_grow_exnref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_grow_externref<true, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_grow_exnref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_grow_exnref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

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
        { return uwvmint_table_fill_externref<false, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_fill_externref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_fill_externref<false, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_fill_externref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_fill_externref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_table_fill_exnref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_fill_externref<true, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_table_fill_exnref_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_table_fill_externref<true, CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
        inline constexpr auto get_uwvmint_table_fill_exnref_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_table_fill_exnref_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate
}
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
