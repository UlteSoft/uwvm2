/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_query.h"
# include "source_type_declarators.h"
# include "source_dwarf_pieces.h"
# include <array>
# include <bit>
# include <cstring>
# include <memory>
# include <utility>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_dwarf
{
    enum class numeric_kind { unavailable, boolean, signed_integer, unsigned_integer, f32_bits, f64_bits };
    enum class numeric_unavailable_reason
    {
        none, no_location, inactive_location, unsupported_type, unsupported_plan,
        local_not_captured, carrier_mismatch, implicit_width_mismatch, incomplete_composite, local_unavailable
    };
    struct numeric_variable
    {
        die_key identity{};
        ::std::string name{}, type_name{};
        bool parameter{};
        numeric_kind kind{numeric_kind::unavailable};
        numeric_unavailable_reason reason{numeric_unavailable_reason::no_location};
        ::std::uint8_t byte_count{};
        ::std::uint64_t bits{}; // Scalar bits only; never a native/guest address.
    };
    struct numeric_query_limits
    {
        inline_query_limits inline_limits{};
        ::std::size_t max_types{65536u}, max_variables{65536u}, max_locations{65536u};
        ::std::size_t max_captured_locals{256u}, max_results{1024u};
        ::std::size_t max_metadata_string_bytes{1024u * 1024u}, max_result_string_bytes{16384u};
    };
    [[nodiscard]] inline constexpr ::std::string_view numeric_reason_text(numeric_unavailable_reason reason) noexcept
    {
        switch(reason)
        {
            case numeric_unavailable_reason::none: return "none";
            case numeric_unavailable_reason::no_location: return "no concrete location";
            case numeric_unavailable_reason::inactive_location: return "no location active at this Wasm position";
            case numeric_unavailable_reason::unsupported_type: return "unsupported or unknown scalar type";
            case numeric_unavailable_reason::unsupported_plan: return "location needs unsupported evaluation or memory access";
            case numeric_unavailable_reason::local_not_captured: return "Wasm local outside the captured numeric snapshot";
            case numeric_unavailable_reason::local_unavailable: return "captured Wasm local is unavailable at this stop";
            case numeric_unavailable_reason::carrier_mismatch: return "source type does not match the actual Wasm carrier";
            case numeric_unavailable_reason::implicit_width_mismatch: return "implicit value width differs from the source scalar";
            case numeric_unavailable_reason::incomplete_composite: return "composite source value has unknown or unavailable bits";
        }
        return "invalid value metadata";
    }
    [[nodiscard]] inline constexpr ::std::uint64_t numeric_mask(::std::uint8_t width) noexcept
    {
        if(width == 8u) { return ~::std::uint64_t{}; }
        if(width != 1u && width != 2u && width != 4u) { return 0u; }
        return (::std::uint64_t{1u} << (width * 8u)) - 1u;
    }
    [[nodiscard]] inline constexpr ::std::int64_t numeric_signed_value(numeric_variable const& value) noexcept
    {
        if(value.byte_count != 1u && value.byte_count != 2u && value.byte_count != 4u && value.byte_count != 8u) { return 0; }
        auto bits{value.bits & numeric_mask(value.byte_count)};
        if(value.byte_count != 8u && (bits & (::std::uint64_t{1u} << (value.byte_count * 8u - 1u))) != 0u)
        { bits |= ~numeric_mask(value.byte_count); }
        return ::std::bit_cast<::std::int64_t>(bits);
    }
    namespace value_details
    {
        [[nodiscard]] inline numeric_kind classify(type_record const& type) noexcept
        {
            if(type.kind != type_kind::scalar ||
               (type.byte_count != 1u && type.byte_count != 2u && type.byte_count != 4u && type.byte_count != 8u))
            { return numeric_kind::unavailable; }
            // Exact DW_ATE meanings. A pointer, language-specific aggregate or
            // unknown encoding is never inferred from its name or byte width.
            switch(type.encoding)
            {
                case 0x02u: return numeric_kind::boolean;
                case 0x05u: case 0x06u: return numeric_kind::signed_integer;
                case 0x07u: case 0x08u: return numeric_kind::unsigned_integer;
                case 0x10u: // DW_ATE_UTF: one UTF-8/16/32 code unit, not a string.
                    // Preserve unsigned code-unit bits from an authenticated
                    // numeric carrier or owned guest-byte copy. A lone UTF-8
                    // continuation byte or UTF-16 surrogate stays inspectable;
                    // no string decoding, pointer following or memory read is
                    // authorized here. Eight-byte UTF metadata is unsupported.
                    return type.byte_count == 1u || type.byte_count == 2u || type.byte_count == 4u ?
                        numeric_kind::unsigned_integer : numeric_kind::unavailable;
                case 0x04u: return type.byte_count == 4u ? numeric_kind::f32_bits :
                                   type.byte_count == 8u ? numeric_kind::f64_bits : numeric_kind::unavailable;
                default: return numeric_kind::unavailable;
            }
        }
        [[nodiscard]] inline bool implicit_bits(location_plan const& plan, ::std::uint64_t& bits) noexcept
        {
            if(plan.byte_count != 1u && plan.byte_count != 2u && plan.byte_count != 4u && plan.byte_count != 8u) { return false; }
            // [owned implicit_bytes (16 bytes)] end
            // [safe                           ] checked width is 1/2/4/8.
            //  ^^ both char borrows are within the same owned array; no pointer
            //     escapes and fast_io decodes DWARF LITTLE endian independently
            //     of the host layout used by generated local snapshots.
            auto const first{reinterpret_cast<char const*>(plan.implicit_bytes.data())};
            auto const last{first + plan.byte_count};
            ::fast_io::parse_result<char const*> parsed{};
            switch(plan.byte_count)
            {
                case 1u: parsed = ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<8>(bits)); break;
                case 2u: parsed = ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<16>(bits)); break;
                case 4u: parsed = ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<32>(bits)); break;
                case 8u: parsed = ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<64>(bits)); break;
                default: return false;
            }
            return parsed.code == ::fast_io::parse_code::ok && parsed.iter == last;
        }
        inline void copy_value(location_plan const& plan, type_record const& type,
            ::std::span<copied_numeric_local const> locals, ::std::size_t total_count, numeric_variable& out) noexcept
        {
            auto const kind{classify(type)};
            if(kind == numeric_kind::unavailable) { out.reason = numeric_unavailable_reason::unsupported_type; return; }
            auto const floating{kind == numeric_kind::f32_bits || kind == numeric_kind::f64_bits};
            ::std::uint64_t bits{};
            if(plan.kind == plan_kind::wasm_local_value)
            {
                if(plan.storage != wasm_location_space::local || plan.local_index > (::std::numeric_limits<::std::uint32_t>::max)() ||
                   plan.local_index >= total_count || plan.local_index >= locals.size())
                { out.reason = numeric_unavailable_reason::local_not_captured; return; }
                auto const& local{locals[static_cast<::std::size_t>(plan.local_index)]};
                if(!local.available) { out.reason = numeric_unavailable_reason::local_unavailable; return; }
                bool const narrow{local.wasm_type == 0x7fu || local.wasm_type == 0x7du};
                bool const wide{local.wasm_type == 0x7eu || local.wasm_type == 0x7cu};
                bool const integer{local.wasm_type == 0x7fu || local.wasm_type == 0x7eu};
                if((!narrow && !wide) || (floating ?
                   ((kind == numeric_kind::f32_bits && local.wasm_type != 0x7du) ||
                    (kind == numeric_kind::f64_bits && local.wasm_type != 0x7cu)) :
                   (!integer || (narrow && type.byte_count > 4u))))
                { out.reason = numeric_unavailable_reason::carrier_mismatch; return; }
                // [complete owned 16-byte snapshot slot] end
                // [safe                                ] full carrier width is 4/8, not source width.
                //  ^^ copy the actual native carrier FIRST, then mask low source
                //     bits. Taking the first source-width bytes is wrong on BE.
                if(narrow)
                { ::std::uint32_t carrier{}; ::std::memcpy(::std::addressof(carrier), local.bytes.data(), sizeof(carrier)); bits = carrier; }
                else { ::std::memcpy(::std::addressof(bits), local.bytes.data(), sizeof(bits)); }
                if(plan.integer_transform_count != 0u && (floating || !transform_copied_integer(plan,bits,narrow ? 32u : 64u)))
                { out.reason = numeric_unavailable_reason::unsupported_plan; return; }
            }
            else if(plan.kind == plan_kind::constant_value)
            {
                if(plan.implicit_constant)
                {
                    if(plan.byte_count != type.byte_count || !implicit_bits(plan, bits))
                    { out.reason = numeric_unavailable_reason::implicit_width_mismatch; return; }
                }
                else
                {
                    // constu/consts/lit are integer scalar values. No integer-to-
                    // float conversion or address-width truncation is guessed.
                    if(floating) { out.reason = numeric_unavailable_reason::unsupported_plan; return; }
                    bits = plan.constant_bits;
                }
            }
            else if(plan.kind == plan_kind::composite_value)
            {
                composite_value copied{};
                piece_query_limits cap{}; cap.max_object_bytes = 8u; cap.max_copied_memory_bytes = 0u;
                auto const status{materialize_location_pieces(plan, locals, total_count, {}, type.byte_count, copied, cap)};
                if(status != piece_query_error::none || !copied.fully_available || copied.bytes.size() != type.byte_count)
                { out.reason = numeric_unavailable_reason::incomplete_composite; return; }
                location_plan implicit{}; implicit.byte_count = type.byte_count;
                for(::std::size_t i{}; i != type.byte_count; ++i)
                {
                    // [owned complete composite bytes / owned 16-byte array] end
                    // [safe                                                ] widths
                    // are 1/2/4/8 from classify(), checked length before each copy.
                    implicit.implicit_bytes[i] = copied.bytes[i];
                }
                if(!implicit_bits(implicit, bits)) { out.reason = numeric_unavailable_reason::incomplete_composite; return; }
            }
            else { out.reason = numeric_unavailable_reason::unsupported_plan; return; }
            out.kind = kind; out.reason = numeric_unavailable_reason::none;
            out.byte_count = type.byte_count; out.bits = bits & numeric_mask(type.byte_count);
        }
    }
    // Finite metadata + copied-scalar query only. The caller must FIRST bind a
    // real paused participant/source/code owner/generation. This API never loads
    // a frame, executes guest DWARF, reads memory/registers or opens source files.
    // Failure clears out; no partial variable list is published.
    [[nodiscard]] inline inline_query_error query_numeric_variables(::std::span<scope_record const> scopes,
        ::std::span<type_record const> types, ::std::span<variable_record const> variables, ::std::uint64_t pc,
        ::std::span<copied_numeric_local const> locals, ::std::size_t total_count,
        ::std::vector<numeric_variable>& out, numeric_query_limits const& cap = {}) noexcept
    {
        out.clear();
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            if(types.size() > cap.max_types || variables.size() > cap.max_variables ||
               locals.size() > cap.max_captured_locals) { return inline_query_error::limit_exceeded; }
            if(locals.size() > total_count) { return inline_query_error::malformed; }
            ::std::vector<inline_frame> chain{};
            auto const proven{query_inline_frames(scopes, pc, chain, cap.inline_limits)};
            if(proven != inline_query_error::none) { return proven; }
            ::std::size_t strings{}, locations{};
            for(auto const& scope : scopes)
            { if(!budget::charge(scope.name.size(), cap.max_metadata_string_bytes, strings) ||
                 !budget::charge(scope.call_file.size(), cap.max_metadata_string_bytes, strings)) { return inline_query_error::limit_exceeded; } }
            for(auto const& type : types)
            { if(!budget::charge(type.name.size(), cap.max_metadata_string_bytes, strings)) { return inline_query_error::limit_exceeded; } }
            for(auto const& variable : variables)
            {
                if(variable.scope >= scopes.size() || (variable.type != no_record && variable.type >= types.size()))
                { return inline_query_error::malformed; }
                if(!budget::charge(variable.name.size(), cap.max_metadata_string_bytes, strings) ||
                   !budget::charge(variable.qualified_name.size(), cap.max_metadata_string_bytes, strings) ||
                   !budget::charge(variable.declaration_file.size(), cap.max_metadata_string_bytes, strings) ||
                   !budget::charge(variable.locations.size(), cap.max_locations, locations)) { return inline_query_error::limit_exceeded; }
                ::std::size_t defaults{};
                for(auto const& location : variable.locations)
                {
                    if(!location.range) { if(++defaults > 1u) { return inline_query_error::ambiguous; } }
                    else if(location.range->begin > location.range->end) { return inline_query_error::malformed; }
                }
            }
            auto const contains{[&](::std::size_t i) noexcept
            {
                // [immutable scopes] end; i is a previously checked record index.
                // [safe            ] ranges were validated by query_inline_frames.
                for(auto const& range : scopes[i].ranges)
                { if(range.begin <= pc && pc < range.end) { return true; } }
                return false;
            }};
            ::std::size_t physical{no_record};
            for(::std::size_t i{}; i != scopes.size(); ++i)
            { if(scopes[i].kind == scope_kind::subprogram && scopes[i].concrete && contains(i)) { physical = i; break; } }
            if(physical == no_record) { return inline_query_error::unavailable; }
            auto const active{[&](::std::size_t i) noexcept
            {
                for(::std::size_t depth{}; i != no_record; ++depth)
                {
                    if(depth > cap.inline_limits.max_depth || i >= scopes.size()) { return false; }
                    auto const& scope{scopes[i]};
                    if(i == physical) { return true; }
                    if(scope.kind == scope_kind::lexical_block)
                    { if((scope.own_ranges_declared || !scope.ranges.empty()) && !contains(i)) { return false; } }
                    else if(scope.kind == scope_kind::inline_subprogram)
                    { if(!scope.concrete || !contains(i)) { return false; } }
                    else { return false; }
                    i = scope.parent; // checked preceding metadata index; never a frame pointer.
                }
                return false;
            }};
            ::std::vector<numeric_variable> pending{};
            ::std::size_t result_strings{};
            for(auto const& variable : variables)
            {
                if(variable.global || variable.declaration || !active(variable.scope)) { continue; }
                if(pending.size() == cap.max_results ||
                   !budget::charge(variable.name.size(), cap.max_result_string_bytes, result_strings))
                { return inline_query_error::limit_exceeded; }
                auto const* type{variable.type == no_record ? nullptr : ::std::addressof(types[variable.type])};
                // [immutable type records] end; checked optional borrow retained
                // only through this synchronous scalar copy, never in output.
                ::fast_io::string formatted_type{};
                ::std::string_view spelling{type == nullptr ? ::std::string_view{} : ::std::string_view{type->name}};
                if(type != nullptr && type_declarators::uses_c_spelling(*type))
                {
                    // Display the same owned producer type as object reads,
                    // including outer qualifiers and named aliases. Formatting
                    // consumes only the remaining shared result-string budget.
                    auto const named{type_declarators::format(types, variable.type, formatted_type,
                        {cap.inline_limits.max_depth, 4096u, cap.max_result_string_bytes - result_strings})};
                    if(named != inline_query_error::none) { return named; }
                    spelling = {formatted_type.data(), formatted_type.size()};
                }
                if(!budget::charge(spelling.size(), cap.max_result_string_bytes, result_strings))
                { return inline_query_error::limit_exceeded; }
                numeric_variable saved{variable.identity, ::fast_io::concat_std(::std::string_view{variable.name}),
                    ::fast_io::concat_std(spelling), variable.parameter};
                location_plan const* selected{}; location_plan const* fallback{};
                for(auto const& location : variable.locations)
                {
                    if(!location.range) { fallback = ::std::addressof(location.plan); } // checked owned-plan borrow, no address is exposed.
                    else if(location.range->begin <= pc && pc < location.range->end)
                    {
                        if(selected != nullptr) { return inline_query_error::ambiguous; }
                        selected = ::std::addressof(location.plan); // checked active immutable location, lifetime ends at this query.
                    }
                }
                if(selected == nullptr) { selected = fallback; } // optional owned-plan borrow; never evaluated as guest code.
                if(type == nullptr) { saved.reason = numeric_unavailable_reason::unsupported_type; }
                else if(selected != nullptr) { value_details::copy_value(*selected, *type, locals, total_count, saved); }
                else if(!variable.locations.empty()) { saved.reason = numeric_unavailable_reason::inactive_location; }
                pending.push_back(::std::move(saved));
            }
            out = ::std::move(pending); return inline_query_error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out.clear(); return inline_query_error::allocation_failure; }
#endif
    }
}
