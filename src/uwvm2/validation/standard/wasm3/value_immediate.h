/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include "recursive_type_binary.h"
# include "recursive_type_validation.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    enum class value_carrier_error : unsigned
    { ok, binary, function_references_disabled, rich_type_not_integrated, unknown_function_type,
      gc_disabled, exceptions_disabled, unknown_type };
    struct value_carrier_result
    {
        value_carrier_error error{};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type type{};
        ::std::size_t error_offset{};
        unsigned carrier{};
        bool requires_function_references{};
    };
    // Core 3 syntax/types: exn has no concrete subtypes. The exception hierarchy
    // is exactly the two abstract heaps exn/noexn, independently of nullability,
    // GC and function-reference encoding. Consume an already decoded value only.
    [[nodiscard]] inline constexpr bool core3_value_requires_exceptions(
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type value) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        return value.kind == t::value_kind::reference &&
            (value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::exn) ||
             value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::noexn));
    }

    // Classify a successfully decoded declaration without scanning its bytes again.
    // This is a feature requirement, not a type-validation substitute. A nonnull
    // empty context denotes the caller's actual legacy 0x60 function-only model;
    // missing context/count or an unknown kind never grants a GC-disabled policy.
    [[nodiscard]] inline constexpr bool core3_value_requires_gc(
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type value,
        recursive_type_context const* context, ::std::size_t known_type_count) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        switch(value.kind)
        {
            case t::value_kind::i32: case t::value_kind::i64:
            case t::value_kind::f32: case t::value_kind::f64: case t::value_kind::v128: return false;
            case t::value_kind::reference: break;
            default: return true;
        }
        if(value.heap.is_defined())
        {
            auto const index{static_cast<::std::uint_least64_t>(value.heap.code)};
            if(context == nullptr || index >= known_type_count) { return true; }
            if(context->records.empty()) { return false; } // Proven legacy function-only context.
            if(!context->contains(index)) { return true; }
            // [records[0] ... records[index] ... records.end())
            // [safe                                           ] contains(index) proves this exact kind borrow.
            return context->records.index_unchecked(static_cast<::std::size_t>(index)).kind !=
                t::composite_kind::function;
        }
        using h = t::abstract_heap_type;
        switch(static_cast<h>(value.heap.code))
        {
            case h::func: case h::extern_: case h::exn: case h::noexn: return false;
            case h::any: case h::eq: case h::i31: case h::struct_: case h::array:
            case h::none: case h::nofunc: case h::noextern: return true;
            default: return true;
        }
    }

    // Core 3 binary/valtype. Function-family references use the existing funcref execution ABI,
    // while `type` retains the complete nullable/heap declaration for static validation. A
    // defined heap is admitted only when the caller proves that index names a function type.
    // GC/exn heaps have no corresponding execution carrier and must never be erased into funcref.
    // The caller applies its independent SIMD/reference-types policies to the returned carrier.
    [[nodiscard]] inline constexpr value_carrier_result scan_core3_value_carrier(
        ::std::byte const*& cursor, ::std::byte const* end, bool function_references,
        ::std::size_t known_function_type_count = 0uz, bool gc_enabled = false) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        using e = value_carrier_error;
        // [cursor ... end) is the caller-proven body/section allocation. Equal endpoints may be null.
        // [safe         ] no read or pointer movement while borrowing the bounded span.
        recursive_binary_details::reader input{{cursor, cursor == end ? 0uz : static_cast<::std::size_t>(end - cursor)}};
        t::core_value_type value{};
        if(!input.value(value)) { return {e::binary, value, input.status.error_offset}; }
        // [value prefix][optional heap] ... end
        // [safe                       ] value() consumed at least one byte, so input[0] exists.
        auto const prefix{::std::to_integer<unsigned>(input.input[0])};
        // Nullable func/extern are the pre-existing reference types. Non-nullable
        // func/extern and indexed function heaps need typed function references;
        // nofunc/noextern are GC heap bottoms even in their non-nullable spelling.
        if(value.kind == t::value_kind::reference &&
           (value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::nofunc) ||
            value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::noextern)) &&
           !gc_enabled)
        { return {e::gc_disabled, value, input.position}; }
        bool const needs_function_references{value.kind == t::value_kind::reference &&
            (value.heap.is_defined() ||
             (prefix == 0x64u &&
              (value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::func) ||
               value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::extern_))))};
        if(needs_function_references && !function_references)
        { return {e::function_references_disabled, value, input.position}; }
        unsigned carrier{};
        switch(value.kind)
        {
            case t::value_kind::i32: carrier = 0x7f; break;
            case t::value_kind::i64: carrier = 0x7e; break;
            case t::value_kind::f32: carrier = 0x7d; break;
            case t::value_kind::f64: carrier = 0x7c; break;
            case t::value_kind::v128: carrier = 0x7b; break;
            case t::value_kind::reference:
                if(value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::func) ||
                   value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::nofunc)) { carrier = 0x70; }
                else if(value.heap.is_defined())
                {
                    if(static_cast<::std::uint_least64_t>(value.heap.code) >= known_function_type_count)
                    { return {e::unknown_function_type, value, input.position}; }
                    carrier = 0x70;
                }
                else if(value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::extern_) ||
                        value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::noextern)) { carrier = 0x6f; }
                else { return {e::rich_type_not_integrated, value}; }
                break;
        }
        // [complete checked valtype] next ... end
        // [safe                    ] unsafe (could be end)
        //                            ^^ cursor: commit a bounded, nonzero consumption only on success.
        cursor += input.position;
        // [complete checked valtype] next ... end
        // [safe                    ] unsafe (could be end)
        //                            ^^ cursor: reader proved input.position <= its original span length.
        return {e::ok, value, 0, carrier, needs_function_references};
    }

    // Context-aware Core 3 valtype projection for mixed function/aggregate type sections.
    // Rich identity remains in `type`; the one-byte carrier is only a VM stack-layout class.
    // An indexed heap is classified by the validated flat type table, never by its index alone.
    // The legacy four-argument scanner above remains the function-only compatibility API.
    [[nodiscard]] inline constexpr value_carrier_result scan_core3_value_carrier(
        ::std::byte const*& cursor, ::std::byte const* end, bool function_references,
        ::std::size_t known_type_count, recursive_type_context const& context,
        bool gc_enabled, bool exceptions_enabled) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        using e = value_carrier_error;
        // [cursor ... end) is one caller-proven body/section allocation. Equal null endpoints
        // are allowed; reader bounds every local byte access before the final commit.
        // [safe         ] unsafe (end is one-past)
        // ^^ cursor: borrowed only while the decoder works on its own offset.
        recursive_binary_details::reader input{{cursor, cursor == end ? 0uz : static_cast<::std::size_t>(end - cursor)}};
        t::core_value_type value{};
        if(!input.value(value)) { return {e::binary, value, input.status.error_offset}; }
        unsigned carrier{};
        bool needs_function_references{};
        switch(value.kind)
        {
            case t::value_kind::i32: carrier = 0x7fu; break;
            case t::value_kind::i64: carrier = 0x7eu; break;
            case t::value_kind::f32: carrier = 0x7du; break;
            case t::value_kind::f64: carrier = 0x7cu; break;
            case t::value_kind::v128: carrier = 0x7bu; break;
            case t::value_kind::reference:
            {
                using h = t::abstract_heap_type;
                if(value.heap.is_defined())
                {
                    auto const index{static_cast<::std::uint_least64_t>(value.heap.code)};
                    if(index >= known_type_count ||
                       (!context.records.empty() && !context.contains(index)))
                    { return {e::unknown_type, value, input.position}; }
                    // An empty context is the older function-only type-section representation.
                    // Otherwise contains() proves the indexed mixed-kind record before access.
                    needs_function_references = context.records.empty() ||
                        context.records.index_unchecked(static_cast<::std::size_t>(index)).kind ==
                            t::composite_kind::function;
                    if(!needs_function_references && !gc_enabled) { return {e::gc_disabled, value, input.position}; }
                    carrier = 0x70u;
                }
                else
                {
                    switch(static_cast<h>(value.heap.code))
                    {
                        case h::func: case h::nofunc:
                            if(value.heap.code == static_cast<::std::int_least64_t>(h::nofunc) && !gc_enabled)
                            { return {e::gc_disabled, value, input.position}; }
                            needs_function_references = value.heap.code == static_cast<::std::int_least64_t>(h::func) &&
                                value.source_prefix == 0x64u;
                            carrier = 0x70u; break;
                        case h::extern_: case h::noextern:
                            if(value.heap.code == static_cast<::std::int_least64_t>(h::noextern) && !gc_enabled)
                            { return {e::gc_disabled, value, input.position}; }
                            needs_function_references = value.heap.code == static_cast<::std::int_least64_t>(h::extern_) &&
                                value.source_prefix == 0x64u;
                            carrier = 0x6fu; break;
                        case h::exn: case h::noexn:
                            if(!exceptions_enabled) { return {e::exceptions_disabled, value, input.position}; }
                            carrier = 0x69u; break;
                        case h::any: case h::eq: case h::i31: case h::struct_: case h::array: case h::none:
                            if(!gc_enabled) { return {e::gc_disabled, value, input.position}; }
                            carrier = 0x70u; break;
                        default: return {e::rich_type_not_integrated, value, input.position};
                    }
                }
                if(needs_function_references && !function_references)
                { return {e::function_references_disabled, value, input.position}; }
                break;
            }
        }
        // [complete checked valtype] next ... end; input.position <= original span length.
        // [safe                    ] unsafe (possibly end)
        //                            ^^ cursor: commit only after every heap and feature check.
        // A successful valtype consumes at least one byte, so addition never uses a null pointer.
        cursor += input.position;
        // [complete checked valtype] next ... end
        // [safe                    ] unsafe (possibly end)
        //                            ^^ cursor: one-past is allowed, and no dereference occurs here.
        return {e::ok, value, 0uz, carrier, needs_function_references};
    }
}
