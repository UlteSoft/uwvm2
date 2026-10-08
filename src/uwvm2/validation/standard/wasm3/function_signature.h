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
# include <initializer_list>
# include <utility>
# include <uwvm2/parser/wasm/standard/wasm3/type/function_signature.h>
# include "recursive_type_binary.h"
# include "value_immediate.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    enum class function_signature_error : unsigned
    { ok, binary, simd_disabled, reference_types_disabled, function_references_disabled, gc_disabled,
      exceptions_disabled, multi_value_disabled, rich_type_not_integrated, unknown_function_type };
    struct function_signature_result
    {
        function_signature_error error{};
        recursive_type_binary_error binary_error{};
        ::std::size_t error_offset{};
        bool in_results{};
        ::std::uint_least32_t offending_type_index{};
    };
    struct function_signature_policy
    {
        bool simd{true}, reference_types{true}, function_references{true}, exceptions{true}, gc{true}, multi_value{true};
        // A Core 3 shorthand function type may refer to its own index or an earlier function type.
        // The parser sets this only after proving every preceding type is a function composite.
        bool function_type_context{};
        ::std::size_t current_function_type_index{};
    };

    // Core 3 binary/types (valtype, resulttype). Input begins immediately after the checked 0x60 prefix.
    // The rich representation is retained even where the existing execution ABI has an equivalent carrier.
    // Function-family typed/non-null references retain their rich type separately from the funcref
    // ABI projection. Exception references retain their rich heap and use the exnref ABI carrier.
    // GC aggregates remain fail closed in this function-only shorthand decoder.
    template<typename Carrier>
    [[nodiscard]] inline constexpr function_signature_result scan_core3_function_signature(
        ::std::byte const*& cursor, ::std::byte const* end, function_signature_policy policy,
        ::uwvm2::parser::wasm::standard::wasm3::type::owned_function_signature<Carrier>& output) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        using e = function_signature_error;
        // [cursor ... end) is the caller-proven section allocation; an empty range may have null endpoints.
        // [safe         ] constructing this bounded view reads no bytes and advances no caller pointer.
        recursive_binary_details::reader input{{cursor, cursor == end ? 0uz : static_cast<::std::size_t>(end - cursor)}};
        t::owned_function_signature<Carrier> temporary{};
        function_signature_result result{};
        auto read_value = [&](t::core_value_type& value) constexpr noexcept
        {
            auto const offset{input.position};
            if(!input.value(value)) { return false; }
            // [offset ... decoded value] next ... end
            // [safe                   ] successful value() consumed at least one byte from offset.
            // ^^ offset: this peek reads the original encoding without advancing any cursor.
            auto const prefix{::std::to_integer<unsigned>(input.input[offset])};
            unsigned carrier{};
            switch(value.kind)
            {
                case t::value_kind::i32: carrier = 0x7f; break;
                case t::value_kind::i64: carrier = 0x7e; break;
                case t::value_kind::f32: carrier = 0x7d; break;
                case t::value_kind::f64: carrier = 0x7c; break;
                case t::value_kind::v128:
                    if(!policy.simd) { result.error = e::simd_disabled; }
                    carrier = 0x7b; break;
                case t::value_kind::reference:
                    if(!policy.reference_types) { result.error = e::reference_types_disabled; }
                    else if(value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::func) ||
                            value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::nofunc))
                    {
                        if(value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::nofunc))
                        { if(!policy.gc) { result.error = e::gc_disabled; } }
                        else { temporary.requires_function_references |= prefix == 0x64u; }
                        carrier = 0x70;
                    }
                    else if(value.heap.is_defined() && policy.function_type_context &&
                            static_cast<::std::uint_least64_t>(value.heap.code) <= policy.current_function_type_index)
                    {
                        temporary.requires_function_references = true;
                        carrier = 0x70;
                    }
                    else if(value.heap.is_defined() && policy.function_type_context)
                    {
                        result.error = e::unknown_function_type;
                        result.offending_type_index = static_cast<::std::uint_least32_t>(value.heap.code);
                    }
                    else if(value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::extern_) ||
                            value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::noextern))
                    {
                        if(value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::noextern))
                        { if(!policy.gc) { result.error = e::gc_disabled; } }
                        else { temporary.requires_function_references |= prefix == 0x64u; }
                        carrier = 0x6f;
                    }
                    else if(value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::exn) ||
                            value.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::noexn))
                    {
                        if(!policy.exceptions) { result.error = e::exceptions_disabled; }
                        carrier = 0x69;
                    }
                    else { result.error = e::rich_type_not_integrated; }
                    break;
            }
            if(temporary.requires_function_references && !policy.function_references)
            { result.error = e::function_references_disabled; }
            if(result.error != e::ok) { result.error_offset = offset; return false; }
            // Retain the requirement in this successful decoded-value walk; no second signature scan.
            temporary.requires_exceptions |= core3_value_requires_exceptions(value);
            temporary.requires_simd |= value.kind == t::value_kind::v128;
            temporary.requires_reference_types |= value.kind == t::value_kind::reference;
            temporary.carriers.push_back(static_cast<Carrier>(carrier));
            return true;
        };
        for(bool in_results : {false, true})
        {
            result.in_results = in_results;
            auto& values{in_results ? temporary.results : temporary.parameters};
            auto const count_offset{input.position};
            if(!input.list(values, 1uz, read_value))
            {
                if(result.error != e::ok) { return result; }
                return {e::binary, input.status.error, input.status.error_offset, in_results};
            }
            // The existing result-vector traversal proves its complete size here.
            // Multiple parameters do not imply multi-value; only multiple results do.
            if(in_results) { temporary.requires_multi_value |= values.size() > 1uz; }
            if(in_results && !policy.multi_value && values.size() > 1uz)
            { return {e::multi_value_disabled, {}, count_offset, true}; }
        }
        temporary.carriers.push_back(static_cast<Carrier>(0)); // Owned sentinel, excluded from both signature views.
        output = ::std::move(temporary);
        // [parameter vector][result vector] ... end
        // [safe                           ] every offset consumed by reader is <= its bounded span length.
        //                                  ^^ cursor: single transactional commit, possibly equal to end.
        // Successful decoding consumed at least two vector lengths; cursor is therefore non-null here.
        cursor += input.position;
        // [parameter vector][result vector] next ... end
        // [safe                             ] unsafe (could be end)
        //                                     ^^ cursor: reader bounded every consumed byte before commit.
        return {};
    }
}
