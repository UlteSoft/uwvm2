/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_selectors.h"
# include <fast_io.h>
# include <cstddef>
# include <cstdint>
# include <span>
# include <string>
# include <string_view>
# include <utility>
# include <vector>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_dwarf
{
    enum class immutable_local_status { intact, overwritten, unavailable };
    // Display validity only. The bitmap is an owned copy of the same genuine
    // instruction-granularity runtime source image, not an opcode byte scan.
    // No guest read, pointer, pause ticket or native capability is created.
    [[nodiscard]] inline immutable_local_status immutable_wasm_local(
        ::std::span<::std::byte const> expression, ::std::span<::std::uint_least8_t const> boundaries,
        ::std::uint64_t local_index) noexcept
    {
        if(expression.empty() || expression.size() > 65536u || local_index >= 256u ||
           boundaries.size() != expression.size()/8u + (expression.size()%8u != 0u))
        { return immutable_local_status::unavailable; }
        if(expression.size()%8u != 0u &&
           (boundaries.back() >> (expression.size()%8u)) != 0u)
        { return immutable_local_status::unavailable; }
        for(::std::size_t offset{}; offset != expression.size(); ++offset)
        {
            if((boundaries[offset/8u] & (1u << (offset%8u))) == 0u) { continue; }
            auto const op{::std::to_integer<::std::uint8_t>(expression[offset])};
            if(op != 0x21u && op != 0x22u) { continue; } // local.set / local.tee
            if(expression.size()-offset <= 1u) { return immutable_local_status::unavailable; }
            auto const* first{reinterpret_cast<unsigned char const*>(expression.data()) + offset + 1u};
            auto const* end{reinterpret_cast<unsigned char const*>(expression.data()) + expression.size()};
            ::std::uint32_t written{};
            auto const parsed{::fast_io::parse_by_scan(first,end,::fast_io::mnp::leb128_get(written))};
            if(parsed.code != ::fast_io::parse_code::ok || parsed.iter <= first || parsed.iter > end ||
               parsed.iter-first > ::std::ptrdiff_t{5}) { return immutable_local_status::unavailable; }
            auto const after{offset+1u+static_cast<::std::size_t>(parsed.iter-first)};
            for(auto immediate{offset+1u}; immediate != after; ++immediate)
            { if((boundaries[immediate/8u] & (1u << (immediate%8u))) != 0u) { return immutable_local_status::unavailable; } }
            if(written == local_index) { return immutable_local_status::overwritten; }
        }
        return immutable_local_status::intact;
    }
    enum class source_expression_step_kind : unsigned { member, index, dereference, go_length, go_capacity };
    struct source_expression_step
    {
        source_expression_step_kind kind{};
        ::std::string member{};
        ::std::int64_t index{};
    };
    struct source_expression
    {
        ::std::string root_name{};
        ::std::vector<source_expression_step> steps{};
    };
    struct source_expression_limits
    {
        // Callers may LOWER these limits. Raising them cannot widen the hard
        // byte/operation/recursive-nesting ceilings used by this parser.
        ::std::size_t max_expression_bytes{4096u}, max_steps{32u}, max_nesting{32u};
    };
    namespace expression_details
    {
        [[nodiscard]] inline constexpr ::std::size_t bounded(::std::size_t value, ::std::size_t ceiling) noexcept
        { return value < ceiling ? value : ceiling; }
        struct parser
        {
            ::std::string_view text{};
            source_expression pending{};
            ::std::size_t cursor{}, max_steps{}, max_nesting{};
            void space() noexcept
            {
                while(cursor < text.size() && ::fast_io::char_category::is_c_space(text[cursor]))
                { ++cursor; } // [safe] checked byte precedes each scalar advance; may reach end.
            }
            [[nodiscard]] bool identifier(bool numeric_member, ::std::string_view& out) noexcept
            {
                if(cursor >= text.size()) { return false; }
                auto const begin{cursor}; auto const first{text[cursor]}; // [safe] cursor < size before read.
                bool const numeric{numeric_member && ::fast_io::char_category::is_c_digit(first)};
                if(!numeric && first != '_' && !::fast_io::char_category::is_c_alpha(first)) { return false; }
                ++cursor; // [safe] one checked byte consumed; cursor <= text.size().
                while(cursor < text.size())
                {
                    auto const value{text[cursor]}; // [safe] scalar index checked before byte read.
                    if(numeric ? !::fast_io::char_category::is_c_digit(value) :
                        (value != '_' && !::fast_io::char_category::is_c_alnum(value))) { break; }
                    ++cursor; // [safe] advance exactly one checked identifier byte.
                }
                // [input ... begin ... cursor ... text_end]
                // [safe                                  ] unsafe (one-past)
                //            ^^ begin <= cursor <= size before this local bounded borrow.
                out = text.substr(begin, cursor - begin); return true;
            }
            [[nodiscard]] object_selector_error append(source_expression_step&& step)
            {
                if(pending.steps.size() >= max_steps) { return object_selector_error::limit_exceeded; }
                pending.steps.push_back(::std::move(step)); return object_selector_error::none;
            }
            [[nodiscard]] object_selector_error primary(::std::size_t depth)
            {
                space();
                if(cursor >= text.size()) { return object_selector_error::malformed; }
                if(text[cursor] == '(')
                {
                    if(depth >= max_nesting) { return object_selector_error::limit_exceeded; }
                    ++cursor; // [safe] opening parenthesis checked at cursor < size.
                    auto const result{unary(depth + 1u)}; // [safe] depth < max_nesting <= 32 before increase.
                    if(result != object_selector_error::none) { return result; }
                    space();
                    if(cursor >= text.size() || text[cursor] != ')') { return object_selector_error::malformed; }
                    ++cursor; // [safe] checked closing parenthesis consumed, may reach end.
                    return object_selector_error::none;
                }
                auto const begin{cursor}; ::std::string_view root{};
                if(!identifier(false, root)) { return object_selector_error::unsupported_expression; }
                while(cursor < text.size() && text[cursor] == ':')
                {
                    if(text.size() - cursor < 2u || text[cursor + 1u] != ':') { return object_selector_error::malformed; }
                    cursor += 2u; // [safe] both namespace colons proved in this same input before advance.
                    ::std::string_view segment{};
                    if(!identifier(false, segment)) { return object_selector_error::malformed; }
                }
                if((root == "len" || root == "cap") && cursor - begin == root.size())
                {
                    auto const root_end{cursor};
                    space();
                    if(cursor < text.size() && text[cursor] == '(')
                    {
                        if(depth >= max_nesting) { return object_selector_error::limit_exceeded; }
                        ++cursor;
                        auto const result{unary(depth + 1u)};
                        if(result != object_selector_error::none) { return result; }
                        // Only a producer reference is an argument. No nested
                        // builtin, call, assignment or arithmetic is executed.
                        for(auto const& step : pending.steps)
                        {
                            if(step.kind == source_expression_step_kind::go_length || step.kind == source_expression_step_kind::go_capacity)
                            { return object_selector_error::unsupported_expression; }
                        }
                        space();
                        if(cursor >= text.size() || text[cursor] != ')') { return object_selector_error::malformed; }
                        ++cursor;
                        return append({root == "len" ? source_expression_step_kind::go_length : source_expression_step_kind::go_capacity, {}, {}});
                    }
                    cursor = root_end; // Failed builtin lookahead retains the exact variable spelling.
                }
                // [input ... begin(namespace root) ... cursor ... end]
                // [safe                                             ] unsafe (one-past)
                //            ^^ exact spelling, bounded by identifier/namespace scans; no rewrite/fallback.
                pending.root_name = ::fast_io::concat_std(text.substr(begin, cursor - begin));
                return object_selector_error::none;
            }
            [[nodiscard]] object_selector_error postfix(::std::size_t depth)
            {
                auto const result{primary(depth)}; if(result != object_selector_error::none) { return result; }
                return postfix_tail();
            }
            // Continue only a previously parsed producer-reference path. The
            // scalar caller leaves a non-arrow minus for its arithmetic parser.
            [[nodiscard]] object_selector_error postfix_tail(bool scalar_boundary = false)
            {
                for(;;)
                {
                    space(); if(cursor >= text.size()) { return object_selector_error::none; }
                    auto const value{text[cursor]}; // [safe] input byte checked before selector classification.
                    if(!pending.steps.empty() && (pending.steps.back().kind == source_expression_step_kind::go_length ||
                        pending.steps.back().kind == source_expression_step_kind::go_capacity))
                    {
                        bool const postfix{value == '.' || value == '[' ||
                            (value == '-' && (!scalar_boundary || text.substr(cursor).starts_with("->")))};
                        return postfix ? object_selector_error::unsupported_expression : object_selector_error::none;
                    }
                    if(value == '.' && text.size() - cursor >= 2u && text[cursor + 1u] == '*')
                    {
                        // Zig's postfix pointer dereference uses the same bounded
                        // copied-guest-pointer plan as unary *. It grants no read.
                        cursor += 2u;
                        auto const status{append({source_expression_step_kind::dereference, {}, {}})};
                        if(status != object_selector_error::none) { return status; }
                    }
                    else if(value == '.' || value == '-')
                    {
                        bool const arrow{value == '-'};
                        if(arrow && (text.size() - cursor < 2u || text[cursor + 1u] != '>'))
                        { return scalar_boundary ? object_selector_error::none : object_selector_error::unsupported_expression; }
                        auto const cost{arrow ? 2u : 1u};
                        if(pending.steps.size() > max_steps || cost > max_steps - pending.steps.size())
                        { return object_selector_error::limit_exceeded; }
                        cursor += cost; // [safe] dot or complete two-byte arrow checked before scalar advance.
                        space(); ::std::string_view name{};
                        if(!identifier(true, name)) { return object_selector_error::malformed; }
                        if(arrow) { pending.steps.push_back({source_expression_step_kind::dereference, {}, {}}); }
                        pending.steps.push_back({source_expression_step_kind::member, ::fast_io::concat_std(name), {}});
                    }
                    else if(value == '[')
                    {
                        if(pending.steps.size() >= max_steps) { return object_selector_error::limit_exceeded; }
                        ++cursor; // [safe] checked '[' at cursor < size before advance.
                        space(); auto const begin{cursor};
                        if(cursor < text.size() && text[cursor] == '-') { ++cursor; } // [safe] checked optional sign.
                        auto const digits{cursor};
                        while(cursor < text.size() && ::fast_io::char_category::is_c_digit(text[cursor]))
                        { ++cursor; } // [safe] each decimal byte checked before scalar advance.
                        if(cursor == digits) { return object_selector_error::malformed; }
                        auto const token_end{cursor}; space();
                        if(cursor >= text.size() || text[cursor] != ']') { return object_selector_error::malformed; }
                        // [input ... begin ... token_end ... ']' ... input_end]
                        // [safe                                              ] unsafe (one-past)
                        //            ^^ nonempty signed token bounded BEFORE either pointer derivation.
                        auto const first{text.data() + begin}; auto const last{text.data() + token_end};
                        ::std::int64_t index{};
                        auto const parsed{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::dec_get<true, true>(index))};
                        if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last) { return object_selector_error::malformed; }
                        ++cursor; // [safe] closing bracket checked at cursor < size before advance.
                        pending.steps.push_back({source_expression_step_kind::index, {}, index});
                    }
                    else { return object_selector_error::none; } // ')' is consumed only by its owning primary.
                }
            }
            [[nodiscard]] object_selector_error unary(::std::size_t depth)
            {
                space(); if(cursor >= text.size()) { return object_selector_error::malformed; }
                if(text[cursor] != '*') { return postfix(depth); } // [safe] cursor checked before byte classification.
                if(depth >= max_nesting) { return object_selector_error::limit_exceeded; }
                ++cursor; // [safe] checked '*' consumed; no source pointer is modified.
                auto const result{unary(depth + 1u)}; // [safe] depth < max_nesting <= 32 before increase.
                if(result != object_selector_error::none) { return result; }
                if(!pending.steps.empty() && (pending.steps.back().kind == source_expression_step_kind::go_length ||
                    pending.steps.back().kind == source_expression_step_kind::go_capacity))
                { return object_selector_error::unsupported_expression; }
                // Postfix binds more tightly: *p[1] => [1], dereference;
                // (*p)[1] => dereference, [1]. This is intent, NOT a read.
                return append({source_expression_step_kind::dereference, {}, {}});
            }
        };
    }
    // Cold read-only syntax only. Exact ASCII namespace roots, Rust numeric
    // members, signed decimal indices, unary *, parentheses and -> are accepted.
    // len/cap accept one reference argument; their Go metadata is checked by
    // the evaluator. Calls, address-of, assignments, casts, arithmetic and numeric roots are
    // rejected. No evaluator, native address, source-stop or memory authority
    // is created. root_name is exact producer lookup spelling, not a fallback.
    [[nodiscard]] inline object_selector_error parse_source_expression(::std::string_view text,
        source_expression& out, source_expression_limits const& cap = {}) noexcept
    {
        out = {};
        if(text.empty()) { return object_selector_error::malformed; }
        if(text.size() > expression_details::bounded(cap.max_expression_bytes, 4096u))
        { return object_selector_error::limit_exceeded; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            expression_details::parser state{text, {}, 0u, expression_details::bounded(cap.max_steps, 32u),
                expression_details::bounded(cap.max_nesting, 32u)};
            auto const status{state.unary(0u)}; if(status != object_selector_error::none) { return status; }
            state.space();
            if(state.cursor != text.size()) { return object_selector_error::unsupported_expression; }
            out = ::std::move(state.pending); return object_selector_error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out = {}; return object_selector_error::allocation_failure; }
#endif
    }
    struct copied_guest_pointer_plan
    {
        // Scalar metadata only. This is neither a native pointer nor a read
        // token. A runtime must authenticate a genuine current stopped guest
        // activation and memory identity, check the complete guest range, copy
        // it, then revalidate stop/owner/generation BEFORE publishing a result.
        ::std::uint64_t guest_offset{};
        ::std::size_t type{no_record};
        ::std::uint64_t extent{};
        ::std::uint8_t address_bytes{};
    };
    // segment contains ONLY the member/index steps BEFORE one dereference.
    // The immutable root bytes have already been captured from a guest object;
    // this pure helper does no read, callback, native cast or pointer addition.
    // Wasm linear memory bytes are little endian independently of host width.
    // Null has no dereference plan and explicitly returns unavailable.
    namespace expression_details
    {
        [[nodiscard]] inline inline_query_error plan_pointer(::std::span<type_record const> types,
            ::std::size_t root_type, ::std::span<object_selector_step const> segment,
            ::std::span<::std::byte const> copied_root_bytes, ::std::span<::std::byte const> known_bits, bool with_known_bits,
            copied_guest_pointer_plan& out, object_selector_limits const& cap, bool cpp_reference = false) noexcept
        {
            out = {};
            if(with_known_bits && (known_bits.empty() || known_bits.size() != copied_root_bytes.size()))
            { return inline_query_error::malformed; }
            if(segment.size() > 32u || copied_root_bytes.size() > 65536u) { return inline_query_error::limit_exceeded; }
            auto bounded_cap{cap};
            bounded_cap.max_steps = expression_details::bounded(cap.max_steps, 32u);
            bounded_cap.max_expression_bytes = expression_details::bounded(cap.max_expression_bytes, 4096u);
            bounded_cap.max_lookup_depth = expression_details::bounded(cap.max_lookup_depth, 32u);
            bounded_cap.max_lookup_edges = expression_details::bounded(cap.max_lookup_edges, 65536u);
            bounded_cap.object_limits.max_types = expression_details::bounded(cap.object_limits.max_types, 65536u);
            bounded_cap.object_limits.max_edges = expression_details::bounded(cap.object_limits.max_edges, 65536u);
            bounded_cap.object_limits.max_depth = expression_details::bounded(cap.object_limits.max_depth, 32u);
            bounded_cap.object_limits.max_results = expression_details::bounded(cap.object_limits.max_results, 1024u);
            bounded_cap.object_limits.max_array_elements = expression_details::bounded(cap.object_limits.max_array_elements, 200u);
            bounded_cap.object_limits.max_object_bytes = expression_details::bounded(cap.object_limits.max_object_bytes, 65536u);
            bounded_cap.object_limits.max_metadata_string_bytes = expression_details::bounded(cap.object_limits.max_metadata_string_bytes, 1024u * 1024u);
            bounded_cap.object_limits.max_result_string_bytes = expression_details::bounded(cap.object_limits.max_result_string_bytes, 65536u);
            ::std::vector<object_node> nodes{};
            auto const status{with_known_bits ?
                query_selected_object_value_with_known_bits(types, root_type, segment, copied_root_bytes, known_bits, nodes, bounded_cap) :
                query_selected_object_value(types, root_type, segment, copied_root_bytes, nodes, bounded_cap)};
            if(status != inline_query_error::none) { return status; }
            if(nodes.size() != 1u) { return inline_query_error::unavailable; }
            auto const& node{nodes.front()}; // [safe] exactly one owned result proved before borrow.
            if(node.type >= types.size()) { return inline_query_error::malformed; }
            auto const& pointer_type{types[node.type]}; // [safe] graph and scalar type index checked before metadata borrow.
            if(node.kind != type_kind::pointer || pointer_type.kind != type_kind::pointer ||
               (cpp_reference ? (!pointer_type.reference_type ||
                    !object_details::cxx_reference_spelling(pointer_type)) :
                    (pointer_type.reference_type || pointer_type.rvalue_reference_type)) || node.bit_field ||
               (pointer_type.address_class_known && pointer_type.address_class != 0u) ||
               !node.value_available || node.reason != object_unavailable_reason::none ||
               (node.scalar_bytes != 4u && node.scalar_bytes != 8u) || node.byte_size != node.scalar_bytes ||
               !pointer_type.size_known || pointer_type.byte_size != node.scalar_bytes || pointer_type.byte_count != node.scalar_bytes)
            { return inline_query_error::unavailable; }
            if(node.bits == 0u) { return inline_query_error::unavailable; }
            if(pointer_type.referenced_type == no_record) { return inline_query_error::unavailable; }
            if(pointer_type.referenced_type >= types.size()) { return inline_query_error::malformed; }
            auto const& pointee{types[pointer_type.referenced_type]}; // [safe] pointee type index checked before borrow.
            if(pointee.kind == type_kind::unavailable || !pointee.size_known || pointee.byte_size == 0u)
            { return inline_query_error::unavailable; }
            if(pointee.byte_size > bounded_cap.object_limits.max_object_bytes) { return inline_query_error::limit_exceeded; }
            out = {node.bits, pointer_type.referenced_type, pointee.byte_size, node.scalar_bytes};
            return inline_query_error::none;
        }
    } // namespace expression_details
    [[nodiscard]] inline inline_query_error plan_copied_guest_pointer(::std::span<type_record const> types,
        ::std::size_t root_type, ::std::span<object_selector_step const> segment,
        ::std::span<::std::byte const> copied_root_bytes, copied_guest_pointer_plan& out,
        object_selector_limits const& cap = {}) noexcept
    { return expression_details::plan_pointer(types, root_type, segment, copied_root_bytes, {}, false, out, cap); }
    // An explicit nonempty known-bit mask must cover the EXACT complete copied
    // object. Unknown pointer/discriminant bits remain unavailable; zero-filling
    // missing pieces never turns an unavailable value into a guest offset.
    [[nodiscard]] inline inline_query_error plan_copied_guest_pointer_with_known_bits(::std::span<type_record const> types,
        ::std::size_t root_type, ::std::span<object_selector_step const> segment,
        ::std::span<::std::byte const> copied_root_bytes, ::std::span<::std::byte const> known_bits,
        copied_guest_pointer_plan& out, object_selector_limits const& cap = {}) noexcept
    { return expression_details::plan_pointer(types, root_type, segment, copied_root_bytes, known_bits, true, out, cap); }
    // C++ reference member access has an explicit profile. It consumes complete
    // owned ABI-width carrier bits, never a native address/read capability.
    [[nodiscard]] inline inline_query_error plan_copied_guest_cpp_reference(::std::span<type_record const> types,
        ::std::size_t root_type, ::std::span<object_selector_step const> segment,
        ::std::span<::std::byte const> bytes, copied_guest_pointer_plan& out,
        object_selector_limits const& cap = {}) noexcept
    { return expression_details::plan_pointer(types,root_type,segment,bytes,{},false,out,cap,true); }
    [[nodiscard]] inline inline_query_error plan_copied_guest_cpp_reference_with_known_bits(::std::span<type_record const> types,
        ::std::size_t root_type, ::std::span<object_selector_step const> segment,
        ::std::span<::std::byte const> bytes, ::std::span<::std::byte const> known_bits,
        copied_guest_pointer_plan& out, object_selector_limits const& cap = {}) noexcept
    { return expression_details::plan_pointer(types,root_type,segment,bytes,known_bits,true,out,cap,true); }

}
