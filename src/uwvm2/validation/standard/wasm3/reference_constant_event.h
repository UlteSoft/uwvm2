/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <type_traits>
# include <uwvm2/parser/wasm/standard/wasm3/type/recursive_type.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Owned DATA for THIS successful ref.null typed transition. It is not a
    // source seal, standalone validation, or execution permission. The native
    // caller already proved the heap, carrier, index and feature policy with
    // the sole bounded heap decoder in its authoritative instruction walk.
    struct core3_ref_null_event
    {
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type result_type{};
        unsigned carrier{};
        ::std::size_t exact_function_type_index{(::std::numeric_limits<::std::size_t>::max)()};
    };
    // Owned DATA from a successful ref.func transition. The caller already
    // checked C.funcs and C.refs, decoded the sole bounded u32 immediate and
    // proved the exact importer/local declaration by owned type-record identity.
    // No pointer, bytecode span, source seal or execution permission is retained.
    struct core3_ref_func_event
    {
        ::std::uint_least32_t function_index{};
        ::std::size_t exact_function_type_index{};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type result_type{};
    };
    static_assert(::std::is_trivially_copyable_v<core3_ref_func_event>);
    [[nodiscard]] inline constexpr core3_ref_func_event make_core3_ref_func_event(
        ::std::uint_least32_t validated_function_index, ::std::size_t validated_declared_type_index) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        // Exact non-null function heap, copied after the authoritative checks.
        // This does not resolve an imported alias or repeat a type lookup.
        return {validated_function_index, validated_declared_type_index,
            {t::value_kind::reference, {static_cast<::std::int_least64_t>(validated_declared_type_index)}, false}};
    }
    static_assert(::std::is_trivially_copyable_v<core3_ref_null_event>);
    [[nodiscard]] inline constexpr core3_ref_null_event make_core3_ref_null_event(
        ::uwvm2::parser::wasm::standard::wasm3::type::heap_type decoded_heap,
        unsigned validated_carrier, bool validated_exact_function_heap) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        // No raw input, pointer arithmetic, context lookup or feature check
        // is repeated. Preserve bottom heaps; their ABI carrier may be shared.
        core3_ref_null_event result{{t::value_kind::reference, decoded_heap, true}, validated_carrier};
        if(validated_carrier == 0x70u)
        {
            if(validated_exact_function_heap)
            { result.exact_function_type_index = static_cast<::std::size_t>(decoded_heap.code); }
            else if(decoded_heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::nofunc))
            { result.exact_function_type_index = (::std::numeric_limits<::std::size_t>::max)() - 1uz; }
        }
        return result;
    }
}
