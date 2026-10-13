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
    enum class function_heap_immediate_error : unsigned
    { ok, binary, function_references_disabled, gc_disabled, unknown_type, unsupported_heap };
    struct function_heap_immediate_result
    {
        function_heap_immediate_error error{};
        ::uwvm2::parser::wasm::standard::wasm3::type::heap_type heap{};
        ::std::size_t error_offset{};
        unsigned carrier{};
    };
    // Core 3 binary/types + valid/ref.null. This adapter's indexed context contains FUNCTION TYPES
    // ONLY, matching the currently integrated module type parser. A future mixed GC type table must
    // pass its type-kind/subtyping context; bounds alone cannot classify an index as callable.
    // Exception abstract heaps are a separate opt-in validation token; runtime compilers leave it off.
    // The full signed heap code is returned separately from the compatible nullable carrier.
    [[nodiscard]] inline constexpr function_heap_immediate_result scan_function_ref_null_heap(
        ::std::byte const*& cursor, ::std::byte const* end, bool function_references,
        ::std::size_t function_type_count, bool exception_references = false, bool gc_enabled = false) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        using e = function_heap_immediate_error;
        // [cursor ... end) is a caller-proven instruction/section allocation. Equal endpoints may be null.
        // [safe         ] borrow the span without reading or moving either endpoint.
        recursive_binary_details::reader input{{cursor, cursor == end ? 0uz : static_cast<::std::size_t>(end - cursor)}};
        t::heap_type heap{};
        if(!input.heap(heap)) { return {e::binary, heap, input.status.error_offset}; }
        unsigned carrier{};
        if(heap.is_defined())
        {
            if(!function_references) { return {e::function_references_disabled, heap}; }
            if(static_cast<::std::uint_least64_t>(heap.code) >= function_type_count) { return {e::unknown_type, heap}; }
            carrier = 0x70;
        }
        else if(heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::func)) { carrier = 0x70; }
        else if(heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::extern_)) { carrier = 0x6f; }
        else if(heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::nofunc) ||
                heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::noextern))
        {
            if(!gc_enabled) { return {e::gc_disabled, heap, input.position}; }
            carrier = heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::nofunc) ? 0x70 : 0x6f;
        }
        else if(exception_references &&
                (heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::exn) ||
                 heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::noexn)))
        {
            // Core 3 ref.null exn/noexn both have a null exn-hierarchy value. This single-byte
            // carrier is only validation metadata: it is never a runtime funcref/externref alias.
            carrier = 0x69u;
        }
        else { return {e::unsupported_heap, heap}; }
        // [complete single-byte abstract heap OR signed-33 index] ... end
        // [safe                                                ] unsafe (could be end)
        //                                                        ^^ cursor: commit only a fully checked immediate.
        // Successful heap decoding consumed at least one byte, so cursor cannot be null here.
        cursor += input.position;
        // [complete checked heap immediate] next ... end
        // [safe                           ] unsafe (could be end)
        //                                   ^^ cursor: the bounded reader permits the one-past value.
        return {e::ok, heap, 0, carrier};
    }

    enum class core3_ref_null_error : unsigned
    { ok, binary, unknown_type, unsupported_heap, gc_disabled, function_references_disabled, exceptions_disabled };
    struct core3_ref_null_result
    {
        core3_ref_null_error error{};
        ::uwvm2::parser::wasm::standard::wasm3::type::heap_type heap{};
        ::std::size_t error_offset{};
        unsigned carrier{};
    };
    // Full Core 3 heap decoder for ref.null. The validated context classifies every indexed
    // type as function, struct, or array; an index alone is never proof of a function reference.
    // Failure leaves cursor unchanged. The result carrier is validation/ABI metadata, not a
    // guest pointer or object allocation.
    [[nodiscard]] inline constexpr core3_ref_null_result scan_core3_ref_null_heap(
        ::std::byte const*& cursor, ::std::byte const* end,
        recursive_type_context const& context, bool gc_enabled,
        bool function_references_enabled, bool exceptions_enabled) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        using e = core3_ref_null_error;
        // [cursor ... end) is one caller-proven instruction allocation; equal null endpoints
        // are permitted. Reader bounds every byte access before local cursor movement.
        // [safe         ] unsafe (end is one-past)
        // ^^ cursor: borrowed only, never changed until a complete validated immediate.
        recursive_binary_details::reader input{{cursor, cursor == end ? 0uz : static_cast<::std::size_t>(end - cursor)}};
        t::heap_type heap{};
        if(!input.heap(heap)) { return {e::binary, heap, input.status.error_offset}; }
        unsigned carrier{};
        if(heap.is_defined())
        {
            if(!context.contains(static_cast<::std::uint_least64_t>(heap.code)))
            { return {e::unknown_type, heap, input.position}; }
            // contains() proves the type index is inside the validated context vector.
            auto const kind{context.records.index_unchecked(static_cast<::std::size_t>(heap.code)).kind};
            if(kind == t::composite_kind::function)
            {
                if(!function_references_enabled) { return {e::function_references_disabled, heap, input.position}; }
            }
            else if(!gc_enabled) { return {e::gc_disabled, heap, input.position}; }
            carrier = 0x70u;
        }
        else
        {
            using h = t::abstract_heap_type;
            switch(static_cast<h>(heap.code))
            {
                case h::func: carrier = 0x70u; break;
                case h::extern_: carrier = 0x6fu; break;
                case h::nofunc: case h::noextern:
                    if(!gc_enabled) { return {e::gc_disabled, heap, input.position}; }
                    carrier = heap.code == static_cast<::std::int_least64_t>(h::nofunc) ? 0x70u : 0x6fu;
                    break;
                case h::exn: case h::noexn:
                    if(!exceptions_enabled) { return {e::exceptions_disabled, heap, input.position}; }
                    carrier = 0x69u; break;
                case h::any: case h::eq: case h::i31: case h::struct_: case h::array: case h::none:
                    if(!gc_enabled) { return {e::gc_disabled, heap, input.position}; }
                    carrier = 0x70u; break;
                default: return {e::unsupported_heap, heap, input.position};
            }
        }
        // [complete signed-33 heap immediate] next ... end; input.position <= original length.
        // [safe                             ] unsafe (possibly end)
        //                                  ^^ cursor: commit only after all feature/type checks.
        // A successful heap consumes at least one byte; addition cannot use a null pointer.
        cursor += input.position;
        // [complete signed-33 heap immediate] next ... end
        // [safe                             ] unsafe (possibly end)
        //                                  ^^ cursor: no dereference follows this transactional commit.
        return {e::ok, heap, 0uz, carrier};
    }
}
