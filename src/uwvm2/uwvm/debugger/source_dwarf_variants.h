/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_types.h"
# include <bit>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_dwarf
{
    enum class variant_query_error { none, unavailable, malformed, ambiguous, limit_exceeded, allocation_failure };
    [[nodiscard]] inline constexpr bool variant_integer_width(::std::uint8_t width) noexcept
    { return width == 1u || width == 2u || width == 4u || width == 8u; }
    [[nodiscard]] inline constexpr ::std::uint64_t variant_integer_mask(::std::uint8_t width) noexcept
    { return !variant_integer_width(width) ? 0u : width == 8u ? ~::std::uint64_t{} : (::std::uint64_t{1u} << (width * 8u)) - 1u; }
    [[nodiscard]] inline constexpr ::std::uint64_t canonical_variant_bits(::std::uint64_t bits, ::std::uint8_t width, bool signed_value) noexcept
    {
        if(!variant_integer_width(width)) { return 0u; }
        auto const mask{variant_integer_mask(width)}; bits &= mask;
        if(signed_value && width != 8u && (bits & (::std::uint64_t{1u} << (width * 8u - 1u))) != 0u) { bits |= ~mask; }
        return bits;
    }
    [[nodiscard]] inline constexpr bool variant_interval_ordered(variant_selector_record const& range, bool signed_value) noexcept
    { return signed_value ? ::std::bit_cast<::std::int64_t>(range.low) <= ::std::bit_cast<::std::int64_t>(range.high) : range.low <= range.high; }
    // DWARF5 5.7.10: DW_DSC_label/range operands use SLEB128 for a signed
    // discriminant, ULEB128 otherwise. LLVM DwarfUnit::addDiscriminant emits
    // these exact shapes. This parses metadata only; it never evaluates DWARF.
    [[nodiscard]] inline variant_query_error decode_variant_selectors(::std::span<::std::byte const> bytes,
        ::std::uint8_t width, bool signed_value, ::std::vector<variant_selector_record>& out,
        ::std::size_t max_selectors = 65536u, ::std::size_t max_expression_bytes = 256u) noexcept
    {
        out.clear();
        if(!variant_integer_width(width) || bytes.empty()) { return variant_query_error::malformed; }
        if(bytes.size() > max_expression_bytes) { return variant_query_error::limit_exceeded; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            details::reader input{bytes}; ::std::vector<variant_selector_record> pending{};
            auto const endpoint{[&](::std::uint64_t& bits) noexcept
            {
                if(signed_value)
                {
                    ::std::int64_t value{}; if(!input.leb(value)) { return false; }
                    bits = static_cast<::std::uint64_t>(value);
                    return canonical_variant_bits(bits, width, true) == bits;
                }
                if(!input.leb(bits)) { return false; }
                return (bits & ~variant_integer_mask(width)) == 0u;
            }};
            while(input.cursor != bytes.size())
            {
                if(pending.size() >= max_selectors) { return variant_query_error::limit_exceeded; }
                ::std::uint8_t opcode{}; variant_selector_record range{};
                if(!input.byte(opcode) || (opcode != 0u /* DW_DSC_label */ && opcode != 1u /* DW_DSC_range */) || !endpoint(range.low))
                { return variant_query_error::malformed; }
                if(opcode == 0u) { range.high = range.low; }
                else if(!endpoint(range.high)) { return variant_query_error::malformed; }
                if(!variant_interval_ordered(range, signed_value)) { return variant_query_error::malformed; }
                pending.push_back(range);
            }
            out = ::std::move(pending); return variant_query_error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out.clear(); return variant_query_error::allocation_failure; }
#endif
    }
    // Only a FULLY KNOWN, copied integer discriminant can select a variant.
    // Unknown niche encodings/unsupported selector records never become case 0.
    // Payload bytes and memory ownership are outside this pure scalar query.
    [[nodiscard]] inline variant_query_error select_variant(variant_part_record const& part,
        ::std::uint64_t bits, bool fully_known, ::std::size_t& selected, ::std::size_t max_edges = 65536u) noexcept
    {
        selected = no_record;
        if(part.variants.empty()) { return variant_query_error::unavailable; }
        if(part.variants.size() > max_edges) { return variant_query_error::limit_exceeded; }
        if(!part.has_discriminant)
        {
            if(part.variants.size() == 1u && part.variants.front().selector_kind == variant_selector_kind::default_case)
            { selected = 0u; return variant_query_error::none; }
            return variant_query_error::unavailable;
        }
        if(!part.discriminant_supported || !variant_integer_width(part.discriminant_bytes) || !fully_known)
        { return variant_query_error::unavailable; }
        bits = canonical_variant_bits(bits, part.discriminant_bytes, part.discriminant_signed);
        ::std::size_t fallback{no_record}, matched{no_record}, edges{part.variants.size()};
        for(::std::size_t i{}; i != part.variants.size(); ++i)
        {
            // [immutable bounded variants ... i ... end]
            // [safe                                  ] i < size; scalar index only.
            auto const& variant{part.variants[i]};
            if(!budget::charge(variant.selectors.size(), max_edges, edges)) { return variant_query_error::limit_exceeded; }
            if(variant.selector_kind == variant_selector_kind::unavailable) { return variant_query_error::unavailable; }
            if(variant.selector_kind == variant_selector_kind::default_case)
            {
                if(!variant.selectors.empty()) { return variant_query_error::malformed; }
                if(fallback != no_record) { return variant_query_error::ambiguous; }
                fallback = i; continue;
            }
            if(variant.selector_kind != variant_selector_kind::intervals || variant.selectors.empty()) { return variant_query_error::malformed; }
            bool matches{};
            for(auto const& range : variant.selectors)
            {
                if(canonical_variant_bits(range.low, part.discriminant_bytes, part.discriminant_signed) != range.low ||
                   canonical_variant_bits(range.high, part.discriminant_bytes, part.discriminant_signed) != range.high ||
                   !variant_interval_ordered(range, part.discriminant_signed)) { return variant_query_error::malformed; }
                bool const contains{part.discriminant_signed ?
                    (::std::bit_cast<::std::int64_t>(range.low) <= ::std::bit_cast<::std::int64_t>(bits) &&
                     ::std::bit_cast<::std::int64_t>(bits) <= ::std::bit_cast<::std::int64_t>(range.high)) :
                    (range.low <= bits && bits <= range.high)};
                if(contains) { if(matches) { return variant_query_error::ambiguous; } matches = true; }
            }
            if(matches) { if(matched != no_record) { return variant_query_error::ambiguous; } matched = i; }
        }
        if(matched == no_record) { matched = fallback; }
        if(matched == no_record) { return variant_query_error::unavailable; }
        selected = matched; return variant_query_error::none;
    }
}
