/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_objects.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_dwarf
{
    enum class object_selector_kind : unsigned { member, index };
    struct object_selector_step
    {
        object_selector_kind kind{};
        ::std::string member{};
        ::std::int64_t index{};
    };
    struct object_selector
    {
        ::std::string root_name{};
        ::std::vector<object_selector_step> steps{};
    };
    struct object_selector_limits
    {
        ::std::size_t max_expression_bytes{4096u}, max_steps{32u}, max_lookup_depth{32u}, max_lookup_edges{65536u};
        object_query_limits object_limits{};
    };
    enum class object_selector_error { none, malformed, unsupported_expression, limit_exceeded, allocation_failure };
    // Read-only lexical selectors only. ASCII producer names, qualified root
    // names, exact member names and signed decimal array indices are accepted.
    // Calls/assignments/casts/operators/pointer dereference never enter an
    // evaluator. Parsing creates no source-stop, memory or execution authority.
    [[nodiscard]] inline object_selector_error parse_source_object_selector(::std::string_view text,
        object_selector& out, object_selector_limits const& cap = {}) noexcept
    {
        out = {};
        if(text.empty()) { return object_selector_error::malformed; }
        if(text.size() > cap.max_expression_bytes) { return object_selector_error::limit_exceeded; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            ::std::size_t cursor{}; object_selector pending{};
            auto const identifier{[&](bool numeric_member, ::std::string_view& name) noexcept
            {
                if(cursor >= text.size()) { return false; }
                auto const begin{cursor}; auto const first{text[cursor]}; // checked scalar index before read.
                bool const numeric{numeric_member && ::fast_io::char_category::is_c_digit(first)};
                if(!numeric && first != '_' && !::fast_io::char_category::is_c_alpha(first)) { return false; }
                ++cursor; // [safe] cursor was < size, advancing exactly one byte may reach text_end.
                while(cursor < text.size())
                {
                    auto const value{text[cursor]}; // [safe] cursor < size before each checked byte.
                    if(numeric ? !::fast_io::char_category::is_c_digit(value) : (value != '_' && !::fast_io::char_category::is_c_alnum(value))) { break; }
                    ++cursor; // checked scalar position only; never a raw source pointer.
                }
                // [input selector ... begin ... cursor ... text_end]
                // [safe                                           ] unsafe (one-past)
                //                     ^^ both scalar positions were checked; this borrow stays local.
                name = text.substr(begin, cursor - begin); return true;
            }};
            ::std::string_view root{};
            if(!identifier(false, root)) { return object_selector_error::unsupported_expression; }
            while(cursor < text.size() && text[cursor] == ':')
            {
                if(text.size() - cursor < 2u || text[cursor + 1u] != ':') { return object_selector_error::malformed; }
                cursor += 2u; // [safe] two colons were checked before this scalar advance.
                ::std::string_view segment{}; if(!identifier(false, segment)) { return object_selector_error::malformed; }
                // [safe] root began at text[0]; all parsed namespace segments are within text.
                root = text.substr(0u, cursor);
            }
            pending.root_name = ::fast_io::concat_std(root);
            while(cursor < text.size())
            {
                if(pending.steps.size() >= cap.max_steps) { return object_selector_error::limit_exceeded; }
                object_selector_step step{};
                auto const opcode{text[cursor++]}; // [safe] cursor < size checked before byte read/advance.
                if(opcode == '.')
                {
                    ::std::string_view member{}; if(!identifier(true, member)) { return object_selector_error::malformed; }
                    step.kind = object_selector_kind::member; step.member = ::fast_io::concat_std(member);
                }
                else if(opcode == '[')
                {
                    step.kind = object_selector_kind::index;
                    auto const begin{cursor};
                    if(cursor < text.size() && text[cursor] == '-') { ++cursor; } // checked optional sign.
                    auto const digits{cursor};
                    while(cursor < text.size() && ::fast_io::char_category::is_c_digit(text[cursor])) { ++cursor; } // checked decimal token bounds.
                    if(cursor == digits || cursor >= text.size() || text[cursor] != ']') { return object_selector_error::malformed; }
                    // [input selector ... begin ... cursor (']') ... end]
                    // [safe                                            ] unsafe (one-past)
                    //                     ^^ nonempty token bounded BEFORE either pointer derivation.
                    auto const first{text.data() + begin}; auto const last{text.data() + cursor};
                    auto const parsed{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::dec_get<true, true>(step.index))};
                    if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last) { return object_selector_error::malformed; }
                    ++cursor; // [safe] closing bracket was checked at cursor < size.
                }
                else { return object_selector_error::unsupported_expression; }
                pending.steps.push_back(::std::move(step));
            }
            out = ::std::move(pending); return object_selector_error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out = {}; return object_selector_error::allocation_failure; }
#endif
    }
    namespace selector_details
    {
        struct selected_object
        {
            ::std::size_t type{no_record}, dimension{}, bit_field_owner{no_record};
            ::std::uint64_t offset{}, extent{}, bit_field_base{};
            // Synchronous immutable metadata borrow only, never returned by a
            // public query or saved in output; it carries no memory authority.
            member_record const* bit_field{};
        };
        struct resolver
        {
            ::std::span<type_record const> types{};
            ::std::span<::std::byte const> bytes{}, known_bits{};
            object_selector_limits const& cap;
            bool values{};
            ::std::size_t edges{}; // Shared across every base branch AND selector step.
            [[nodiscard]] inline_query_error charge(::std::size_t amount = 1u) noexcept
            { return budget::charge(amount, cap.max_lookup_edges, edges) ? inline_query_error::none : inline_query_error::limit_exceeded; }
            [[nodiscard]] inline_query_error member_position(selected_object const& owner, member_record const& member, selected_object& out) noexcept
            {
                if(member.type == no_record) { return inline_query_error::unavailable; }
                if(member.type >= types.size()) { return inline_query_error::malformed; }
                if(!member.offset_known) { return inline_query_error::unavailable; }
                out = {}; out.type = member.type;
                if(member.bit_field)
                {
                    out.bit_field_owner = owner.type; out.bit_field_base = owner.offset;
                    // [bounded immutable member in this same typegraph] end
                    // [safe                                           ] owner lives through query only.
                    //  ^^ no host/guest object pointer or borrowed string escapes.
                    out.bit_field = ::std::addressof(member); return inline_query_error::none;
                }
                ::std::uint64_t size{};
                if(!object_details::size(types[member.type], size)) { return inline_query_error::unavailable; }
                if(member.byte_offset > owner.extent || size > owner.extent - member.byte_offset ||
                   member.byte_offset > (::std::numeric_limits<::std::uint64_t>::max)() - owner.offset)
                { return inline_query_error::malformed; }
                out.offset = owner.offset + member.byte_offset; out.extent = size; return inline_query_error::none;
            }
            [[nodiscard]] inline_query_error active_variant(selected_object const& owner, variant_part_record const& part, ::std::size_t& index)
            {
                ::std::uint64_t bits{}; bool known{};
                if(part.has_discriminant && part.discriminant_supported)
                {
                    if(!values) { return inline_query_error::unavailable; }
                    object_details::builder display{types, bytes, {}, cap.object_limits, true}; display.known_bits = known_bits;
                    if(!display.visit_member(types[owner.type], part.discriminant, owner.offset, owner.extent, no_record, 0u)) { return display.failure; }
                    if(display.nodes.empty()) { return inline_query_error::malformed; }
                    bits = display.nodes.front().bits; known = display.nodes.front().value_available;
                }
                auto const status{select_variant(part, bits, known, index, cap.max_lookup_edges)};
                switch(status)
                {
                    case variant_query_error::none: return inline_query_error::none;
                    case variant_query_error::ambiguous: return inline_query_error::ambiguous;
                    case variant_query_error::limit_exceeded: return inline_query_error::limit_exceeded;
                    case variant_query_error::malformed: return inline_query_error::malformed;
                    default: return inline_query_error::unavailable;
                }
            }
            // rustc represents tuple and tuple-struct fields as __0, __1, ... .
            // Numeric selectors are aliases only in the original Rust CU. Never
            // infer a language or a field offset from a name or the host ABI.
            [[nodiscard]] static bool member_matches(type_record const& type, ::std::string_view producer,
                ::std::string_view requested) noexcept
            {
                if(producer == requested) { return true; }
                if(type.language != 0x1cu || requested.empty() ||
                   (requested.size() > 1u && requested.front() == '0') ||
                   producer.size() < 3u || !producer.starts_with("__") || producer.substr(2u) != requested)
                { return false; }
                for(auto c : requested) { if(!::fast_io::char_category::is_c_digit(c)) { return false; } }
                return true;
            }
            [[nodiscard]] inline_query_error find_member(selected_object const& owner, ::std::string_view name,
                selected_object& out, bool& found, ::std::size_t depth = 0u)
            {
                found = false;
                if(depth >= cap.max_lookup_depth || depth > cap.object_limits.max_depth) { return inline_query_error::limit_exceeded; }
                if(owner.type >= types.size()) { return inline_query_error::malformed; }
                if(owner.bit_field || owner.dimension != 0u) { return inline_query_error::unavailable; }
                auto const& type{types[owner.type]};
                if(type.kind != type_kind::structure && type.kind != type_kind::class_type && type.kind != type_kind::union_type) { return inline_query_error::unavailable; }
                // C++ ordinary directly-declared members hide every base member.
                // Duplicate direct fields stay ambiguous; no first-name guessing.
                for(auto const& member : type.members)
                {
                    auto const counted{charge()}; if(counted != inline_query_error::none) { return counted; }
                    if(member.inherited || !member_matches(type, member.name, name)) { continue; }
                    if(found) { return inline_query_error::ambiguous; }
                    auto const position{member_position(owner, member, out)}; if(position != inline_query_error::none) { return position; }
                    found = true;
                }
                if(found) { return inline_query_error::none; }
                for(auto const& part : type.variant_parts)
                {
                    auto const counted{charge()}; if(counted != inline_query_error::none) { return counted; }
                    if(part.has_discriminant && member_matches(type, part.discriminant.name, name))
                    {
                        if(found) { return inline_query_error::ambiguous; }
                        auto const position{member_position(owner, part.discriminant, out)}; if(position != inline_query_error::none) { return position; }
                        found = true; continue;
                    }
                    bool possible{};
                    for(auto const& variant : part.variants)
                    {
                        for(auto const amount : {::std::size_t{1u}, variant.members.size(), variant.selectors.size()})
                        { auto const cost{charge(amount)}; if(cost != inline_query_error::none) { return cost; } }
                        for(auto const& member : variant.members) { if(member_matches(type, member.name, name)) { possible = true; } }
                    }
                    if(!possible) { continue; }
                    ::std::size_t selected{}; auto const selected_status{active_variant(owner, part, selected)};
                    if(selected_status != inline_query_error::none) { return selected_status; }
                    // [validated immutable variants ... selected ... end]
                    // [safe                                           ] select_variant returned one bounded index.
                    auto const& variant{part.variants[selected]};
                    if(!variant.layout_supported) { return inline_query_error::unavailable; }
                    for(auto const& member : variant.members)
                    {
                        if(!member_matches(type, member.name, name)) { continue; }
                        if(found) { return inline_query_error::ambiguous; }
                        auto const position{member_position(owner, member, out)}; if(position != inline_query_error::none) { return position; }
                        found = true;
                    }
                }
                if(found) { return inline_query_error::none; }
                for(auto const& base : type.members)
                {
                    if(!base.inherited) { continue; }
                    auto const counted{charge()}; if(counted != inline_query_error::none) { return counted; }
                    // A dynamic/virtual base may contribute this name. Without
                    // its layout proof a known sibling cannot be called unique.
                    if(base.bit_field || !base.offset_known) { return inline_query_error::unavailable; }
                    selected_object base_object{}; auto const position{member_position(owner, base, base_object)};
                    if(position != inline_query_error::none) { return position; }
                    selected_object candidate{}; bool matches{};
                    auto const result{find_member(base_object, name, candidate, matches, depth + 1u)};
                    if(result != inline_query_error::none) { return result; }
                    if(matches) { if(found) { return inline_query_error::ambiguous; } out = candidate; found = true; }
                }
                return inline_query_error::none;
            }
            [[nodiscard]] inline_query_error index(selected_object& object, ::std::int64_t index) noexcept
            {
                if(object.type >= types.size()) { return inline_query_error::malformed; }
                auto const& type{types[object.type]};
                if(object.bit_field || type.kind != type_kind::array || !type.contiguous_array || !type.row_major_array ||
                   type.referenced_type == no_record || object.dimension >= type.dimensions.size()) { return inline_query_error::unavailable; }
                auto const& bound{type.dimensions[object.dimension]};
                if(!bound.lower_bound_known || !bound.count_known || bound.count == 0u || index < bound.lower_bound) { return inline_query_error::unavailable; }
                if(bound.count - 1u > static_cast<::std::uint64_t>((::std::numeric_limits<::std::int64_t>::max)()) - static_cast<::std::uint64_t>(bound.lower_bound))
                { return inline_query_error::malformed; } // Never wrap an unrepresentable signed index domain.
                auto const ordinal{static_cast<::std::uint64_t>(index) - static_cast<::std::uint64_t>(bound.lower_bound)};
                if(ordinal >= bound.count) { return inline_query_error::unavailable; }
                ::std::uint64_t stride{};
                if(!object_details::size(types[type.referenced_type], stride)) { return inline_query_error::unavailable; }
                for(::std::size_t i{object.dimension + 1u}; i != type.dimensions.size(); ++i)
                {
                    auto const cost{charge()}; if(cost != inline_query_error::none) { return cost; }
                    auto const& rest{type.dimensions[i]};
                    if(!rest.count_known || !rest.lower_bound_known ||
                       (rest.count != 0u && rest.count - 1u > static_cast<::std::uint64_t>((::std::numeric_limits<::std::int64_t>::max)()) - static_cast<::std::uint64_t>(rest.lower_bound)))
                    { return inline_query_error::unavailable; }
                    if(stride != 0u && rest.count > (::std::numeric_limits<::std::uint64_t>::max)() / stride) { return inline_query_error::malformed; }
                    stride *= rest.count;
                }
                if(stride != 0u && bound.count > object.extent / stride) { return inline_query_error::malformed; }
                auto const delta{ordinal * stride}; // ordinal < count and count*stride <= extent above.
                if(delta > object.extent || stride > object.extent - delta || delta > (::std::numeric_limits<::std::uint64_t>::max)() - object.offset)
                { return inline_query_error::malformed; }
                object.offset += delta; object.extent = stride; // checked scalar guest-offset replacement only.
                ++object.dimension; // checked dimension < dimensions.size() before the scalar advance.
                if(object.dimension == type.dimensions.size()) { object.type = type.referenced_type; object.dimension = 0u; }
                return inline_query_error::none;
            }
        };
        [[nodiscard]] inline inline_query_error query(::std::span<type_record const> types, ::std::size_t root_type,
            ::std::span<object_selector_step const> steps, ::std::span<::std::byte const> bytes, ::std::span<::std::byte const> known_bits,
            bool values, ::std::vector<object_node>& out, object_selector_limits const& cap) noexcept
        {
            out.clear();
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
#endif
                auto const graph{object_details::validate(types, cap.object_limits)}; if(graph != inline_query_error::none) { return graph; }
                if(root_type >= types.size()) { return inline_query_error::malformed; }
                if(steps.size() > cap.max_steps || bytes.size() > cap.object_limits.max_object_bytes) { return inline_query_error::limit_exceeded; }
                if(!known_bits.empty() && known_bits.size() != bytes.size()) { return inline_query_error::malformed; }
                ::std::size_t strings{};
                for(auto const& step : steps)
                {
                    if(step.kind != object_selector_kind::member && step.kind != object_selector_kind::index) { return inline_query_error::malformed; }
                    if(!budget::charge(step.member.size(), cap.max_expression_bytes, strings)) { return inline_query_error::limit_exceeded; }
                    if(step.kind == object_selector_kind::member && step.member.empty()) { return inline_query_error::malformed; }
                }
                selected_object selected{}; selected.type = root_type;
                if(!object_details::size(types[root_type], selected.extent)) { return inline_query_error::unavailable; }
                if(values && selected.extent > bytes.size()) { return inline_query_error::malformed; }
                resolver lookup{types, bytes, known_bits, cap, values}; ::std::string label{};
                for(auto const& step : steps)
                {
                    auto const cost{lookup.charge()}; if(cost != inline_query_error::none) { return cost; }
                    if(selected.bit_field) { return inline_query_error::unavailable; }
                    if(step.kind == object_selector_kind::index)
                    {
                        auto const result{lookup.index(selected, step.index)}; if(result != inline_query_error::none) { return result; }
                        label = ::fast_io::concat_std("[", ::fast_io::mnp::dec(step.index), "]");
                    }
                    else
                    {
                        selected_object pending{}; bool found{};
                        auto const result{lookup.find_member(selected, step.member, pending, found)};
                        if(result != inline_query_error::none) { return result; }
                        if(!found) { return inline_query_error::unavailable; }
                        selected = pending; label = ::fast_io::concat_std(::std::string_view{step.member});
                    }
                }
                object_details::builder display{types, bytes, {}, cap.object_limits, values}; display.known_bits = known_bits;
                if(selected.bit_field)
                {
                    if(!display.bit_field(types[selected.bit_field_owner], *selected.bit_field, selected.bit_field_base, no_record, 0u)) { return display.failure; }
                }
                else if(!display.visit(selected.type, selected.offset, selected.extent, no_record, 0u, ::std::move(label), false, selected.dimension)) { return display.failure; }
                out = ::std::move(display.nodes); return inline_query_error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            }
            catch(...) { out.clear(); return inline_query_error::allocation_failure; }
#endif
        }
    }
    // The runtime/controller FIRST authenticates the root variable, captures
    // its complete bounded bytes under one actual stop ticket, and revalidates
    // the source/activation binding. This pure API only selects that owned copy;
    // it cannot widen it, follow pointers, execute code or perform a new read.
    [[nodiscard]] inline inline_query_error query_selected_object_value(::std::span<type_record const> types, ::std::size_t root_type,
        ::std::span<object_selector_step const> steps, ::std::span<::std::byte const> copied_root_bytes,
        ::std::vector<object_node>& out, object_selector_limits const& cap = {}) noexcept
    { return selector_details::query(types, root_type, steps, copied_root_bytes, {}, true, out, cap); }
    [[nodiscard]] inline inline_query_error query_selected_object_value_with_known_bits(::std::span<type_record const> types, ::std::size_t root_type,
        ::std::span<object_selector_step const> steps, ::std::span<::std::byte const> copied_root_bytes, ::std::span<::std::byte const> known_bits,
        ::std::vector<object_node>& out, object_selector_limits const& cap = {}) noexcept
    {
        if(known_bits.size() != copied_root_bytes.size()) { out.clear(); return inline_query_error::malformed; }
        return selector_details::query(types, root_type, steps, copied_root_bytes, known_bits, true, out, cap);
    }
    [[nodiscard]] inline inline_query_error query_selected_object_type(::std::span<type_record const> types, ::std::size_t root_type,
        ::std::span<object_selector_step const> steps, ::std::vector<object_node>& out, object_selector_limits const& cap = {}) noexcept
    {
        // A root declaration has no offset/index selection and needs no ABI
        // extent. Retain unknown-size metadata (e.g. Clang member pointers)
        // without inventing a carrier size or granting object-value access.
        if(steps.empty()) { return query_type_layout(types,root_type,out,cap.object_limits); }
        return selector_details::query(types, root_type, steps, {}, {}, false, out, cap);
    }
}
