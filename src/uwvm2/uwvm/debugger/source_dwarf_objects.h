/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_values.h"
# include "source_dwarf_variants.h"
# include "source_frames.h"
# include "source_type_declarators.h"
# include <algorithm>
# include <bit>
# include <limits>
# include <memory>
# include <utility>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_dwarf
{
    enum class object_unavailable_reason
    {
        none, unsupported_type, unknown_size, unknown_member_offset, dynamic_array,
        unsupported_array_stride, object_bounds, unsupported_scalar, unsupported_bit_field,
        incomplete_value, unavailable_discriminant, ambiguous_variant, unsupported_variant_layout
    };
    struct object_node
    {
        ::std::size_t parent{no_record}, type{no_record}, depth{};
        ::std::string name{}, type_name{}, enumerator{};
        type_kind kind{};
        numeric_kind scalar_kind{numeric_kind::unavailable};
        object_unavailable_reason reason{object_unavailable_reason::none};
        ::std::uint64_t byte_offset{}, byte_size{}, bits{}, omitted_children{};
        ::std::uint8_t scalar_bytes{}, bit_width{};
        bool value_available{}, inherited{}, bit_field{};
        ::std::string display_text{}; bool display_text_available{}, display_text_truncated{};
        bool variant_part{}, variant_case{}, active_variant{}, default_variant{};
    };
    struct object_query_limits
    {
        ::std::size_t max_types{65536u}, max_edges{65536u}, max_depth{32u}, max_results{1024u};
        ::std::size_t max_array_elements{200u}, max_object_bytes{65536u};
        ::std::size_t max_metadata_string_bytes{1024u * 1024u}, max_result_string_bytes{65536u};
    };
    [[nodiscard]] inline constexpr ::std::string_view object_reason_text(object_unavailable_reason reason) noexcept
    {
        switch(reason)
        {
            case object_unavailable_reason::none: return "none";
            case object_unavailable_reason::unsupported_type: return "unsupported source type";
            case object_unavailable_reason::unknown_size: return "source object size is unavailable";
            case object_unavailable_reason::unknown_member_offset: return "member needs dynamic or unsupported offset evaluation";
            case object_unavailable_reason::dynamic_array: return "array has an unavailable bound";
            case object_unavailable_reason::unsupported_array_stride: return "array has a non-contiguous or column-major layout";
            case object_unavailable_reason::object_bounds: return "source layout exceeds the copied object bounds";
            case object_unavailable_reason::unsupported_scalar: return "source scalar encoding is unsupported";
            case object_unavailable_reason::unsupported_bit_field: return "bit-field encoding or width is unsupported";
            case object_unavailable_reason::incomplete_value: return "copied value contains unavailable bits";
            case object_unavailable_reason::unavailable_discriminant: return "variant discriminant or selector is unavailable";
            case object_unavailable_reason::ambiguous_variant: return "variant selectors are ambiguous";
            case object_unavailable_reason::unsupported_variant_layout: return "nested variant layout is unavailable";
        }
        return "invalid object metadata";
    }
    [[nodiscard]] inline constexpr ::std::string_view object_type_kind_text(type_kind kind) noexcept
    {
        switch(kind)
        {
            case type_kind::scalar: return "scalar";
            case type_kind::pointer: return "pointer";
            case type_kind::structure: return "struct";
            case type_kind::class_type: return "class";
            case type_kind::union_type: return "union";
            case type_kind::array: return "array";
            case type_kind::enumeration: return "enum";
            case type_kind::subroutine: return "function";
            case type_kind::member_pointer: return "member-pointer";
            case type_kind::unavailable: return "unavailable";
        }
        return "invalid type";
    }
    namespace object_details
    {
        [[nodiscard]] inline constexpr bool c_type_spelling(type_record const& type) noexcept
        { return type_declarators::uses_c_spelling(type); }
        [[nodiscard]] inline bool cxx_reference_spelling(type_record const& type) noexcept
        {
            if(!type.reference_type || type.tinygo_producer || type.zig_producer) { return false; }
            switch(type.language)
            { case 0x04u: case 0x19u: case 0x1au: case 0x21u: case 0x2au: case 0x2bu: case 0x3au: return true;
              default: return false; }
        }
        [[nodiscard]] inline inline_query_error display_type_name(::std::span<type_record const> types,
            ::std::size_t index, ::std::string& out, object_query_limits const& cap)
        {
            out.clear();
            if(index >= types.size()) { return inline_query_error::malformed; }
            if(!c_type_spelling(types[index]))
            {
                if(types[index].name.size() > cap.max_result_string_bytes) { return inline_query_error::limit_exceeded; }
                out = ::fast_io::concat_std(::std::string_view{types[index].name}); return inline_query_error::none;
            }
            ::fast_io::string spelling{};
            auto const error{type_declarators::format(types,index,spelling,
                {cap.max_depth,cap.max_edges,cap.max_result_string_bytes})};
            if(error != inline_query_error::none) { return error; }
            out = ::fast_io::concat_std(::fast_io::mnp::strvw(spelling));
            return inline_query_error::none;
        }
        [[nodiscard]] inline bool size(type_record const& type, ::std::uint64_t& out) noexcept
        {
            if(type.size_known) { out = type.byte_size; return true; }
            // Existing hand-built scalar metadata predates byte_size. This
            // fallback is restricted to its already checked finite width.
            if((type.kind == type_kind::scalar || type.kind == type_kind::pointer || type.kind == type_kind::enumeration) &&
               (type.byte_count == 1u || type.byte_count == 2u || type.byte_count == 4u || type.byte_count == 8u || type.byte_count == 16u))
            { out = type.byte_count; return true; }
            return false;
        }
        [[nodiscard]] inline inline_query_error validate(::std::span<type_record const> types, object_query_limits const& cap) noexcept
        {
            if(types.size() > cap.max_types) { return inline_query_error::limit_exceeded; }
            ::std::size_t strings{}, edges{};
            auto const validate_member{[&](member_record const& member) noexcept
            {
                if(member.type != no_record && member.type >= types.size()) { return inline_query_error::malformed; }
                if(!budget::charge(member.name.size(), cap.max_metadata_string_bytes, strings)) { return inline_query_error::limit_exceeded; }
                return inline_query_error::none;
            }};
            for(auto const& type : types)
            {
                if((type.display_qualifiers & ~0x0fu) != 0u || (type.named_type_alias && type.name.empty()) ||
                   static_cast<unsigned>(type.kind) > static_cast<unsigned>(type_kind::member_pointer) ||
                   (type.method_qualifiers & ~3u) != 0u ||
                   (type.containing_type != no_record && type.containing_type >= types.size()) ||
                   (type.referenced_type != no_record && type.referenced_type >= types.size()) ||
                   (type.size_known && type.byte_count != 0u &&
                    (type.kind == type_kind::scalar || type.kind == type_kind::pointer || type.kind == type_kind::enumeration) &&
                    type.byte_size != type.byte_count)) { return inline_query_error::malformed; }
                if(!budget::charge(type.name.size(), cap.max_metadata_string_bytes, strings) ||
                   !budget::charge(type.parameter_types.size(), cap.max_edges, edges) ||
                   !budget::charge(type.members.size(), cap.max_edges, edges) ||
                   !budget::charge(type.dimensions.size(), cap.max_edges, edges) ||
                   !budget::charge(type.enumerators.size(), cap.max_edges, edges) ||
                   !budget::charge(type.variant_parts.size(), cap.max_edges, edges)) { return inline_query_error::limit_exceeded; }
                for(auto parameter : type.parameter_types)
                { if(parameter != no_record && parameter >= types.size()) { return inline_query_error::malformed; } }
                for(auto const& member : type.members)
                {
                    auto const checked{validate_member(member)}; if(checked != inline_query_error::none) { return checked; }
                }
                for(auto const& value : type.enumerators)
                { if(!budget::charge(value.name.size(), cap.max_metadata_string_bytes, strings)) { return inline_query_error::limit_exceeded; } }
                for(auto const& part : type.variant_parts)
                {
                    if(!budget::charge(1u, cap.max_edges, edges) || !budget::charge(part.variants.size(), cap.max_edges, edges))
                    { return inline_query_error::limit_exceeded; }
                    auto const discriminant{validate_member(part.discriminant)}; if(discriminant != inline_query_error::none) { return discriminant; }
                    if(part.discriminant_supported)
                    {
                        if(!part.has_discriminant || !part.discriminant.offset_known || part.discriminant.type == no_record ||
                           !variant_integer_width(part.discriminant_bytes)) { return inline_query_error::malformed; }
                        auto const& scalar{types[part.discriminant.type]};
                        bool const integer{scalar.encoding == 0x02u || scalar.encoding == 0x05u || scalar.encoding == 0x06u || scalar.encoding == 0x07u || scalar.encoding == 0x08u};
                        if(!integer || (scalar.kind != type_kind::scalar && scalar.kind != type_kind::enumeration) ||
                           scalar.byte_count != part.discriminant_bytes || part.discriminant_signed != (scalar.encoding == 0x05u || scalar.encoding == 0x06u))
                        { return inline_query_error::malformed; }
                    }
                    for(auto const& variant : part.variants)
                    {
                        if(static_cast<unsigned>(variant.selector_kind) > static_cast<unsigned>(variant_selector_kind::intervals)) { return inline_query_error::malformed; }
                        if(!budget::charge(variant.name.size(), cap.max_metadata_string_bytes, strings) ||
                           !budget::charge(variant.selectors.size(), cap.max_edges, edges) ||
                           !budget::charge(variant.members.size(), cap.max_edges, edges)) { return inline_query_error::limit_exceeded; }
                        if(variant.selector_kind == variant_selector_kind::default_case && !variant.selectors.empty()) { return inline_query_error::malformed; }
                        if(variant.selector_kind == variant_selector_kind::intervals && variant.selectors.empty()) { return inline_query_error::malformed; }
                        for(auto const& member : variant.members)
                        { auto const checked{validate_member(member)}; if(checked != inline_query_error::none) { return checked; } }
                    }
                }
            }
            return inline_query_error::none;
        }
        struct builder
        {
            ::std::span<type_record const> types{};
            ::std::span<::std::byte const> bytes{};
            ::std::vector<object_node> nodes{};
            object_query_limits const& cap;
            bool read_values{};
            ::std::size_t strings{};
            inline_query_error failure{};
            ::std::span<::std::byte const> known_bits{};
            [[nodiscard]] bool known_byte(::std::size_t offset) const noexcept
            {
                // Caller has checked the owned copied-byte range first. A
                // supplied mask must have the exact same bounded length.
                return known_bits.empty() || (offset < known_bits.size() && known_bits[offset] == ::std::byte{0xffu});
            }
            [[nodiscard]] bool fail(inline_query_error value) noexcept { failure = value; return false; }
            [[nodiscard]] bool append(object_node&& value, ::std::size_t& index)
            {
                if(nodes.size() >= cap.max_results || value.depth > cap.max_depth ||
                   !budget::charge(value.name.size(), cap.max_result_string_bytes, strings) ||
                   !budget::charge(value.type_name.size(), cap.max_result_string_bytes, strings))
                { return fail(inline_query_error::limit_exceeded); }
                index = nodes.size(); nodes.push_back(::std::move(value)); return true;
            }
            [[nodiscard]] bool scalar(::std::size_t index, type_record const& type)
            {
                auto& node{nodes[index]}; // checked append index; no recursive growth in this helper.
                if(type.kind == type_kind::pointer)
                { node.scalar_kind = numeric_kind::unsigned_integer; }
                else
                {
                    auto scalar_type{type_record{}};
                    scalar_type.kind = type_kind::scalar; scalar_type.encoding = type.encoding;
                    scalar_type.byte_count = type.byte_count;
                    node.scalar_kind = value_details::classify(scalar_type);
                }
                if(node.scalar_kind == numeric_kind::unavailable ||
                   (node.byte_size != 1u && node.byte_size != 2u && node.byte_size != 4u && node.byte_size != 8u))
                { node.reason = object_unavailable_reason::unsupported_scalar; return true; }
                node.scalar_bytes = static_cast<::std::uint8_t>(node.byte_size);
                if(!read_values) { return true; }
                if(node.byte_offset > bytes.size() || node.byte_size > bytes.size() - node.byte_offset)
                { node.reason = object_unavailable_reason::object_bounds; return true; }
                for(::std::uint64_t i{}; i != node.byte_size; ++i)
                {
                    if(!known_byte(static_cast<::std::size_t>(node.byte_offset + i)))
                    { node.reason = object_unavailable_reason::incomplete_value; return true; }
                }
                // [owned copied guest bytes ... offset ... offset+width ... end]
                // [safe                                                       ] unsafe (one-past)
                //                               ^^ checked BEFORE deriving either borrow.
                auto const first{reinterpret_cast<char const*>(bytes.data() + static_cast<::std::size_t>(node.byte_offset))};
                auto const last{first + node.scalar_bytes};
                ::fast_io::parse_result<char const*> parsed{};
                switch(node.scalar_bytes)
                {
                    case 1u: parsed = ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<8>(node.bits)); break;
                    case 2u: parsed = ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<16>(node.bits)); break;
                    case 4u: parsed = ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<32>(node.bits)); break;
                    case 8u: parsed = ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<64>(node.bits)); break;
                    default: return fail(inline_query_error::malformed);
                }
                if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last) { return fail(inline_query_error::malformed); }
                node.value_available = true;
                if(type.kind == type_kind::enumeration) { return enum_name(index, type); }
                return true;
            }
            [[nodiscard]] bool enum_name(::std::size_t index, type_record const& type)
            {
                auto& node{nodes[index]};
                auto const mask{numeric_mask(node.scalar_bytes)};
                for(auto const& item : type.enumerators)
                {
                    if((item.bits & mask) != node.bits) { continue; }
                    if(!budget::charge(item.name.size(), cap.max_result_string_bytes, strings)) { return fail(inline_query_error::limit_exceeded); }
                    node.enumerator = ::fast_io::concat_std(::std::string_view{item.name}); break;
                }
                return true;
            }
            [[nodiscard]] bool bit_field(type_record const& owner, member_record const& member, ::std::uint64_t base,
                ::std::size_t parent, ::std::size_t depth)
            {
                object_node node{}; node.parent = parent; node.depth = depth; node.type = member.type;
                node.name = ::fast_io::concat_std(::std::string_view{member.name}); node.bit_field = true; node.inherited = member.inherited;
                if(member.type != no_record)
                {
                    auto const named{display_type_name(types,member.type,node.type_name,cap)};
                    if(named!=inline_query_error::none) { return fail(named); }
                    node.kind=types[member.type].kind;
                }
                ::std::uint64_t owner_size{};
                if(!member.offset_known || member.bit_size == 0u || member.bit_size > 64u || !size(owner, owner_size) ||
                   member.data_bit_offset / 8u > owner_size ||
                   (member.bit_size + member.data_bit_offset % 8u + 7u) / 8u > owner_size - member.data_bit_offset / 8u)
                { node.reason = object_unavailable_reason::unsupported_bit_field; }
                else if(member.type == no_record) { node.reason = object_unavailable_reason::unsupported_type; }
                else
                {
                    auto scalar_type{type_record{}};
                    auto const& type{types[member.type]}; scalar_type.kind = type_kind::scalar;
                    scalar_type.encoding = type.encoding; scalar_type.byte_count = type.byte_count;
                    node.scalar_kind = value_details::classify(scalar_type);
                    if((type.kind != type_kind::scalar && type.kind != type_kind::enumeration) || member.bit_size > static_cast<::std::uint64_t>(type.byte_count) * 8u ||
                       node.scalar_kind == numeric_kind::unavailable || node.scalar_kind == numeric_kind::f32_bits || node.scalar_kind == numeric_kind::f64_bits)
                    { node.reason = object_unavailable_reason::unsupported_bit_field; }
                    else if(member.data_bit_offset / 8u > (::std::numeric_limits<::std::uint64_t>::max)() - base)
                    { node.reason = object_unavailable_reason::object_bounds; }
                    else
                    {
                        node.bit_width = static_cast<::std::uint8_t>(member.bit_size); node.scalar_bytes = type.byte_count;
                        node.byte_offset = base + member.data_bit_offset / 8u;
                        node.byte_size = (member.bit_size + member.data_bit_offset % 8u + 7u) / 8u;
                        if(read_values)
                        {
                            if(node.byte_offset > bytes.size() || node.byte_size > bytes.size() - node.byte_offset)
                            { node.reason = object_unavailable_reason::object_bounds; }
                            else
                            {
                                // Wasm linear memory is LITTLE endian on every
                                // host. Each checked byte is scanned by fast_io;
                                // bit positions are scalar arithmetic only.
                                for(::std::uint64_t bit{}; bit != member.bit_size; ++bit)
                                {
                                    auto const source_bit{member.data_bit_offset % 8u + bit};
                                    auto const offset{node.byte_offset + source_bit / 8u};
                                    if(!known_bits.empty() &&
                                       (::std::to_integer<unsigned char>(known_bits[static_cast<::std::size_t>(offset)]) & (1u << (source_bit % 8u))) == 0u)
                                    { node.reason = object_unavailable_reason::incomplete_value; break; }
                                    // [copied guest bytes ... offset ... end]
                                    // [safe                                ] unsafe (one-past)
                                    //                         ^^ entire byte range proved above.
                                    auto const first{reinterpret_cast<char const*>(bytes.data() + static_cast<::std::size_t>(offset))};
                                    auto const last{first + 1u}; ::std::uint8_t value{};
                                    auto const parsed{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<8>(value))};
                                    if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last) { return fail(inline_query_error::malformed); }
                                    node.bits |= static_cast<::std::uint64_t>((value >> (source_bit % 8u)) & 1u) << bit;
                                }
                                if(node.reason == object_unavailable_reason::none && node.scalar_kind == numeric_kind::signed_integer && member.bit_size != 64u &&
                                   (node.bits & (::std::uint64_t{1u} << (member.bit_size - 1u))) != 0u)
                                { node.bits |= ~((::std::uint64_t{1u} << member.bit_size) - 1u); }
                                node.bits &= numeric_mask(node.scalar_bytes); node.value_available = node.reason == object_unavailable_reason::none;
                            }
                        }
                    }
                }
                ::std::size_t index{}; if(!append(::std::move(node), index)) { return false; }
                if(nodes[index].value_available && member.type != no_record && types[member.type].kind == type_kind::enumeration)
                { return enum_name(index, types[member.type]); }
                return true;
            }
            [[nodiscard]] bool visit_member(type_record const& owner, member_record const& member, ::std::uint64_t base,
                ::std::uint64_t extent, ::std::size_t parent, ::std::size_t depth)
            {
                if(member.bit_field) { return bit_field(owner, member, base, parent, depth); }
                if(!member.offset_known || member.byte_offset > extent || member.byte_offset > (::std::numeric_limits<::std::uint64_t>::max)() - base)
                {
                    object_node unavailable{}; unavailable.parent = parent; unavailable.depth = depth;
                    unavailable.type = member.type; unavailable.name = ::fast_io::concat_std(::std::string_view{member.name}); unavailable.inherited = member.inherited;
                    if(member.type != no_record)
                    {
                        auto const named{display_type_name(types,member.type,unavailable.type_name,cap)};
                        if(named!=inline_query_error::none) { return fail(named); }
                        unavailable.kind=types[member.type].kind;
                    }
                    unavailable.reason = member.offset_known ? object_unavailable_reason::object_bounds : object_unavailable_reason::unknown_member_offset;
                    ::std::size_t ignored{}; return append(::std::move(unavailable), ignored);
                }
                return visit(member.type, base + member.byte_offset, extent - member.byte_offset, parent, depth,
                    ::fast_io::concat_std(::std::string_view{member.name}), member.inherited);
            }
            [[nodiscard]] bool variant_part(type_record const& owner, variant_part_record const& part, ::std::uint64_t base,
                ::std::uint64_t extent, ::std::size_t parent, ::std::size_t depth)
            {
                object_node group{}; group.parent = parent; group.depth = depth; group.name = ::fast_io::concat_std("<variant-part>");
                group.variant_part = true; group.byte_offset = base; group.byte_size = extent;
                ::std::size_t group_index{}; if(!append(::std::move(group), group_index)) { return false; }
                ::std::uint64_t discriminant_bits{}; bool discriminant_known{};
                if(part.has_discriminant)
                {
                    auto const discriminant_index{nodes.size()};
                    if(!visit_member(owner, part.discriminant, base, extent, group_index, depth + 1u)) { return false; }
                    // [owned result nodes ... discriminant_index ... end]
                    // [safe                                             ] helper
                    //  ^^ appended at least its root; no vector borrow survives it.
                    discriminant_known = nodes[discriminant_index].value_available &&
                        nodes[discriminant_index].reason == object_unavailable_reason::none && part.discriminant_supported;
                    discriminant_bits = nodes[discriminant_index].bits;
                }
                ::std::size_t selected{no_record};
                if(read_values)
                {
                    auto const status{select_variant(part, discriminant_bits, discriminant_known, selected, cap.max_edges)};
                    if(status == variant_query_error::malformed) { return fail(inline_query_error::malformed); }
                    if(status == variant_query_error::limit_exceeded) { return fail(inline_query_error::limit_exceeded); }
                    if(status != variant_query_error::none)
                    {
                        nodes[group_index].reason = status == variant_query_error::ambiguous ?
                            object_unavailable_reason::ambiguous_variant : object_unavailable_reason::unavailable_discriminant;
                        return true; // Unknown/ambiguous discriminants never expose a guessed payload.
                    }
                }
                for(::std::size_t i{}; i != part.variants.size(); ++i)
                {
                    if(read_values && i != selected) { continue; }
                    auto const& variant{part.variants[i]};
                    object_node branch{}; branch.parent = group_index; branch.depth = depth + 1u;
                    branch.name = ::fast_io::concat_std(::std::string_view{variant.name}); branch.byte_offset = base; branch.byte_size = extent;
                    branch.variant_case = true; branch.active_variant = read_values; branch.default_variant = variant.selector_kind == variant_selector_kind::default_case;
                    if(variant.selector_kind == variant_selector_kind::unavailable) { branch.reason = object_unavailable_reason::unavailable_discriminant; }
                    else if(!variant.layout_supported) { branch.reason = object_unavailable_reason::unsupported_variant_layout; }
                    ::std::size_t branch_index{}; if(!append(::std::move(branch), branch_index)) { return false; }
                    // Known direct fields remain inspectable even when a nested
                    // variant subtree is unavailable; no whole payload zeroing.
                    for(auto const& member : variant.members)
                    { if(!visit_member(owner, member, base, extent, branch_index, depth + 2u)) { return false; } }
                }
                return true;
            }
            [[nodiscard]] bool visit(::std::size_t type_index, ::std::uint64_t offset, ::std::uint64_t extent,
                ::std::size_t parent, ::std::size_t depth, ::std::string name, bool inherited = false, ::std::size_t dimension = 0u)
            {
                object_node node{}; node.parent = parent; node.depth = depth; node.type = type_index;
                node.name = ::std::move(name); node.byte_offset = offset; node.inherited = inherited;
                if(type_index == no_record) { node.reason = object_unavailable_reason::unsupported_type; }
                else
                {
                    auto const& type{types[type_index]};
                    auto const named{display_type_name(types,type_index,node.type_name,cap)};
                    if(named!=inline_query_error::none) { return fail(named); }
                    node.kind=type.kind;
                    if(!size(type, node.byte_size)) { node.reason = object_unavailable_reason::unknown_size; }
                    else if(dimension == 0u && node.byte_size > extent) { node.reason = object_unavailable_reason::object_bounds; }
                    if(dimension != 0u) { node.byte_size = extent; }
                }
                ::std::size_t index{}; if(!append(::std::move(node), index)) { return false; }
                if(nodes[index].reason != object_unavailable_reason::none || type_index == no_record) { return true; }
                auto const& type{types[type_index]};
                if(type.kind == type_kind::scalar || type.kind == type_kind::pointer || type.kind == type_kind::enumeration)
                { return scalar(index, type); } // Pointers are displayed as guest offsets, NEVER followed.
                if(type.kind == type_kind::array)
                {
                    if(!type.contiguous_array || !type.row_major_array)
                    { nodes[index].reason = object_unavailable_reason::unsupported_array_stride; return true; }
                    if(type.referenced_type == no_record || dimension >= type.dimensions.size())
                    { nodes[index].reason = object_unavailable_reason::dynamic_array; return true; }
                    ::std::uint64_t stride{};
                    if(!size(types[type.referenced_type], stride)) { nodes[index].reason = object_unavailable_reason::unknown_size; return true; }
                    for(::std::size_t i{dimension + 1u}; i != type.dimensions.size(); ++i)
                    {
                        auto const& bound{type.dimensions[i]};
                        if(!bound.count_known || (stride != 0u && bound.count > (::std::numeric_limits<::std::uint64_t>::max)() / stride))
                        { nodes[index].reason = object_unavailable_reason::dynamic_array; return true; }
                        stride *= bound.count;
                    }
                    auto const& bound{type.dimensions[dimension]};
                    if(!bound.lower_bound_known || !bound.count_known ||
                       (stride != 0u && bound.count > nodes[index].byte_size / stride))
                    { nodes[index].reason = object_unavailable_reason::dynamic_array; return true; }
                    auto const count{(::std::min)(bound.count, static_cast<::std::uint64_t>(cap.max_array_elements))};
                    nodes[index].omitted_children = bound.count - count;
                    for(::std::uint64_t i{}; i != count; ++i)
                    {
                        auto const delta{i * stride}; // checked count*stride <= object size above.
                        if(delta > (::std::numeric_limits<::std::uint64_t>::max)() - offset) { return fail(inline_query_error::malformed); }
                        auto const index_bits{static_cast<::std::uint64_t>(bound.lower_bound) + i};
                        if(i > static_cast<::std::uint64_t>((::std::numeric_limits<::std::int64_t>::max)()) - static_cast<::std::uint64_t>(bound.lower_bound))
                        { return fail(inline_query_error::malformed); }
                        auto label{::fast_io::concat_std("[", ::fast_io::mnp::dec(::std::bit_cast<::std::int64_t>(index_bits)), "]")};
                        auto const child_type{dimension + 1u == type.dimensions.size() ? type.referenced_type : type_index};
                        if(!visit(child_type, offset + delta, stride, index, depth + 1u, ::std::move(label), false,
                                  child_type == type_index ? dimension + 1u : 0u)) { return false; }
                    }
                    return true;
                }
                if(type.kind != type_kind::structure && type.kind != type_kind::class_type && type.kind != type_kind::union_type)
                { nodes[index].reason = object_unavailable_reason::unsupported_type; return true; }
                for(auto const& member : type.members)
                {
                    if(!visit_member(type, member, offset, nodes[index].byte_size, index, depth + 1u)) { return false; }
                }
                for(auto const& part : type.variant_parts)
                { if(!variant_part(type, part, offset, nodes[index].byte_size, index, depth + 1u)) { return false; } }
                return true;
            }
        };
        [[nodiscard]] inline inline_query_error query(::std::span<type_record const> types, ::std::size_t type,
            ::std::span<::std::byte const> bytes, bool values, ::std::vector<object_node>& out, object_query_limits const& cap,
            ::std::span<::std::byte const> known_bits = {}) noexcept
        {
            out.clear();
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
#endif
                auto const checked{validate(types, cap)};
                if(checked != inline_query_error::none) { return checked; }
                if(type >= types.size()) { return inline_query_error::malformed; }
                if(bytes.size() > cap.max_object_bytes) { return inline_query_error::limit_exceeded; }
                if(!known_bits.empty() && known_bits.size() != bytes.size()) { return inline_query_error::malformed; }
                ::std::uint64_t extent{};
                if(!size(types[type], extent)) { extent = 0u; }
                if(values) { extent = bytes.size(); }
                builder pending{types, bytes, {}, cap, values};
                pending.known_bits = known_bits;
                if(!pending.visit(type, 0u, extent, no_record, 0u, {})) { return pending.failure; }
                out = ::std::move(pending.nodes); return inline_query_error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            }
            catch(...) { out.clear(); return inline_query_error::allocation_failure; }
#endif
        }
    }
    // ptype-style layout is cold metadata only. Byte offsets/sizes do not give
    // permission to read memory. Self-referential POINTERS terminate as leaves.
    [[nodiscard]] inline inline_query_error query_type_layout(::std::span<type_record const> types, ::std::size_t type,
        ::std::vector<object_node>& out, object_query_limits const& cap = {}) noexcept
    { return object_details::query(types, type, {}, false, out, cap); }
    // Caller supplies an OWNED COPY made under an authenticated stopped-frame
    // execution lease. This API cannot read guest/native memory, evaluate guest
    // expressions, dereference pointers, open files or retain either input span.
    [[nodiscard]] inline inline_query_error query_object_value(::std::span<type_record const> types, ::std::size_t type,
        ::std::span<::std::byte const> copied_guest_bytes, ::std::vector<object_node>& out, object_query_limits const& cap = {}) noexcept
    { return object_details::query(types, type, copied_guest_bytes, true, out, cap); }
    // Composite locations retain their per-bit availability. An unknown bit in
    // one field does not erase unrelated fully known fields; a variant selector
    // still requires every discriminant bit. This grants no guest read authority.
    [[nodiscard]] inline inline_query_error query_object_value_with_known_bits(::std::span<type_record const> types, ::std::size_t type,
        ::std::span<::std::byte const> copied_guest_bytes, ::std::span<::std::byte const> known_bits,
        ::std::vector<object_node>& out, object_query_limits const& cap = {}) noexcept
    {
        if(known_bits.size() != copied_guest_bytes.size()) { out.clear(); return inline_query_error::malformed; }
        return object_details::query(types, type, copied_guest_bytes, true, out, cap, known_bits);
    }

    struct variable_selection
    {
        die_key identity{};
        ::std::size_t type{no_record}, scope{no_record}, physical_scope{no_record};
        location_plan location{};
        bool location_available{};
        bool global{}, static_storage{};
        bool static_location{}, immutable_cpp_object_pointer{};
    };
    // Resolve lexical shadowing at the actual Code-relative stop. Names do not
    // authorize memory access; the selected result is copied metadata only.
    [[nodiscard]] inline inline_query_error query_named_variable(::std::span<scope_record const> scopes,
        ::std::span<type_record const> types, ::std::span<variable_record const> variables, ::std::uint64_t pc,
        ::std::string_view name, variable_selection& out, numeric_query_limits const& cap = {},
        ::std::optional<::std::size_t> selected_frame = {}) noexcept
    {
        out = {};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            if(name.empty() || name.size() > cap.max_result_string_bytes) { return inline_query_error::unavailable; }
            ::std::vector<numeric_variable> active{};
            auto const status{query_numeric_variables(scopes, types, variables, pc, {}, 0u, active, cap)};
            if(status != inline_query_error::none) { return status; }
            ::std::size_t selected{no_record}, selected_depth{};
            bool ambiguous{};
            if(selected_frame)
            {
                ::uwvm2::uwvm::debugger::source_frame_variables::selection frame_variable{};
                auto const frame_status{::uwvm2::uwvm::debugger::source_frame_variables::named(scopes, variables, types.size(),
                    pc, *selected_frame, name, frame_variable)};
                using frame_error = ::uwvm2::uwvm::debugger::source_frame_variables::error;
                switch(frame_status)
                {
                    case frame_error::none: selected = frame_variable.variable_index; break;
                    case frame_error::unavailable: break; // existing same-CU/global fallback remains below.
                    case frame_error::ambiguous: return inline_query_error::ambiguous;
                    case frame_error::limit_exceeded: return inline_query_error::limit_exceeded;
                    case frame_error::allocation_failure: return inline_query_error::allocation_failure;
                    case frame_error::malformed: case frame_error::bounds: return inline_query_error::malformed;
                    default: return inline_query_error::malformed;
                }
            }
            if(!selected_frame) for(auto const& candidate : active)
            {
                ::std::size_t found{no_record};
                for(::std::size_t i{}; i != variables.size(); ++i)
                { if(variables[i].identity == candidate.identity) { if(found != no_record) { return inline_query_error::ambiguous; } found = i; } }
                if(found == no_record) { return inline_query_error::malformed; }
                if(candidate.name != name && variables[found].qualified_name != name) { continue; }
                auto scope{variables[found].scope}; ::std::size_t depth{};
                while(scope != no_record)
                {
                    if(scope >= scopes.size() || depth >= cap.inline_limits.max_depth) { return inline_query_error::malformed; }
                    ++depth; scope = scopes[scope].parent; // checked metadata index; no runtime frame pointer changes.
                }
                if(selected == no_record || depth > selected_depth) { selected = found; selected_depth = depth; ambiguous = false; }
                else if(depth == selected_depth) { ambiguous = true; }
            }
            ::std::size_t actual_physical{no_record};
            for(::std::size_t i{}; i != scopes.size(); ++i)
            {
                if(scopes[i].kind != scope_kind::subprogram || !scopes[i].concrete) { continue; }
                for(auto const& range : scopes[i].ranges)
                { if(range.begin <= pc && pc < range.end) { actual_physical = i; break; } }
                if(actual_physical != no_record) { break; }
            }
            if(actual_physical == no_record) { return inline_query_error::unavailable; }
            auto const compile_unit{[&](::std::size_t scope) noexcept
            {
                for(::std::size_t depth{}; scope != no_record; ++depth)
                {
                    if(scope >= scopes.size() || depth >= cap.inline_limits.max_depth) { return no_record; }
                    if(scopes[scope].kind == scope_kind::compile_unit) { return scope; }
                    scope = scopes[scope].parent; // bounded preceding metadata scope index only.
                }
                return no_record;
            }};
            if(selected == no_record)
            {
                auto const actual_unit{compile_unit(actual_physical)};
                if(actual_unit == no_record) { return inline_query_error::unavailable; }
                // Prefer globals/file statics in the current actual CU. Only
                // explicit external definitions may be considered across CUs;
                // duplicate names remain ambiguous, never a guessed address.
                bool selected_same_unit{};
                for(::std::size_t i{}; i != variables.size(); ++i)
                {
                    auto const& variable{variables[i]};
                    if(!variable.global || variable.declaration || (variable.name != name && variable.qualified_name != name)) { continue; }
                    auto const unit{compile_unit(variable.scope)};
                    if(unit == no_record) { return inline_query_error::malformed; }
                    bool const same_unit{unit == actual_unit};
                    if(!same_unit && (!variable.external || selected_same_unit)) { continue; }
                    if(selected == no_record || (same_unit && !selected_same_unit))
                    { selected = i; selected_same_unit = same_unit; ambiguous = false; }
                    else { ambiguous = true; }
                }
            }
            if(selected == no_record) { return inline_query_error::unavailable; }
            if(ambiguous) { return inline_query_error::ambiguous; }
            auto const& variable{variables[selected]}; variable_selection pending{};
            pending.identity = variable.identity; pending.type = variable.type; pending.scope = variable.scope;
            pending.global = variable.global; pending.static_storage = variable.static_storage;
            pending.immutable_cpp_object_pointer = variable.immutable_cpp_object_pointer;
            if(variable.global) { pending.physical_scope = actual_physical; }
            auto scope{variable.scope};
            for(::std::size_t depth{}; scope != no_record; ++depth)
            {
                if(scope >= scopes.size() || depth >= cap.inline_limits.max_depth) { return inline_query_error::malformed; }
                if(scopes[scope].kind == scope_kind::subprogram) { pending.physical_scope = scope; break; }
                scope = scopes[scope].parent; // checked parent metadata, not a native frame.
            }
            location_plan const* selected_location{}; location_plan const* fallback{};
            for(auto const& location : variable.locations)
            {
                if(!location.range) { fallback = ::std::addressof(location.plan); }
                else if(location.range->begin <= pc && pc < location.range->end)
                {
                    if(selected_location != nullptr) { return inline_query_error::ambiguous; }
                    selected_location = ::std::addressof(location.plan); // synchronous immutable, checked active plan borrow.
                }
            }
            if(selected_location == nullptr) { selected_location = fallback; }
            if(selected_location != nullptr) { pending.location = *selected_location; pending.location_available = true; pending.static_location = selected_location == fallback; }
            out = pending; return inline_query_error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out = {}; return inline_query_error::allocation_failure; }
#endif
    }
    enum class object_location_error
    { none, unsupported_location, unavailable_frame_base, ambiguous_frame_base, local_not_captured, carrier_mismatch, address_overflow, malformed_metadata, local_unavailable };
    [[nodiscard]] inline object_location_error resolve_absolute_guest_offset(location_plan const& location, ::std::uint64_t& guest_offset) noexcept
    {
        guest_offset = 0u;
        if(location.kind != plan_kind::absolute_guest_offset) { return object_location_error::unsupported_location; }
        if(location.reason != unavailable_reason::none || !location.pieces.empty() || (location.address_bytes != 4u && location.address_bytes != 8u))
        { return object_location_error::malformed_metadata; }
        if(location.address_bytes == 4u && location.constant_bits > (::std::numeric_limits<::std::uint32_t>::max)())
        { return object_location_error::address_overflow; }
        guest_offset = location.constant_bits; return object_location_error::none;
        // This scalar is valid only under a proven single-memory Wasm producer
        // ABI. No relocation, host-symbol lookup, pointer or memory read occurs.
    }
    // DW_OP_fbreg + a real captured Wasm local frame base becomes a Wasm
    // linear-memory OFFSET. It never becomes a host address. The caller must
    // still authenticate its capture ticket/source owner/generation, select the
    // producer ABI's memory, and copy bytes with the runtime memory host API.
    [[nodiscard]] inline object_location_error resolve_frame_relative_offset(location_plan const& location,
        ::std::span<location_record const> frame_base, ::std::uint64_t pc,
        ::std::span<copied_numeric_local const> locals, ::std::size_t total_count, ::std::uint64_t& guest_offset,
        ::std::size_t max_frame_base_entries = 65536u) noexcept
    {
        guest_offset = 0u;
        if(location.kind != plan_kind::frame_relative_offset) { return object_location_error::unsupported_location; }
        if(location.address_bytes != 4u && location.address_bytes != 8u) { return object_location_error::malformed_metadata; }
        if(locals.size() > total_count) { return object_location_error::malformed_metadata; }
        if(frame_base.size() > max_frame_base_entries) { return object_location_error::malformed_metadata; }
        location_plan const* selected{}; location_plan const* fallback{};
        for(auto const& entry : frame_base)
        {
            if(!entry.range)
            { if(fallback != nullptr) { return object_location_error::ambiguous_frame_base; } fallback = ::std::addressof(entry.plan); }
            else
            {
                if(entry.range->begin > entry.range->end) { return object_location_error::malformed_metadata; }
                if(entry.range->begin <= pc && pc < entry.range->end)
                { if(selected != nullptr) { return object_location_error::ambiguous_frame_base; } selected = ::std::addressof(entry.plan); }
            }
        }
        if(selected == nullptr) { selected = fallback; } // synchronous owned-metadata borrow only.
        if(selected == nullptr || selected->kind != plan_kind::wasm_local_frame_base)
        { return object_location_error::unavailable_frame_base; }
        if(selected->address_bytes != location.address_bytes) { return object_location_error::malformed_metadata; }
        if(selected->storage != wasm_location_space::local || selected->local_index >= total_count || selected->local_index >= locals.size())
        { return object_location_error::local_not_captured; }
        auto const& local{locals[static_cast<::std::size_t>(selected->local_index)]};
        if(!local.available) { return object_location_error::local_unavailable; }
        ::std::uint64_t base{};
        if(location.address_bytes == 4u)
        {
            if(local.wasm_type != 0x7fu) { return object_location_error::carrier_mismatch; }
            // [owned 16-byte local snapshot] end
            // [safe                        ] exact native i32 carrier copy;
            //  ^^ do not treat Wasm32 bits as any host pointer or dereference.
            ::std::uint32_t value{}; ::std::memcpy(::std::addressof(value), local.bytes.data(), sizeof(value)); base = value;
        }
        else
        {
            if(local.wasm_type != 0x7eu) { return object_location_error::carrier_mismatch; }
            // [safe] exact native i64 carrier copy from the checked owned slot.
            ::std::memcpy(::std::addressof(base), local.bytes.data(), sizeof(base));
        }
        auto const maximum{location.address_bytes == 4u ? static_cast<::std::uint64_t>((::std::numeric_limits<::std::uint32_t>::max)()) :
                                                        (::std::numeric_limits<::std::uint64_t>::max)()};
        if(location.displacement >= 0)
        {
            auto const delta{static_cast<::std::uint64_t>(location.displacement)};
            if(delta > maximum - base) { return object_location_error::address_overflow; }
            guest_offset = base + delta;
        }
        else
        {
            // INT64_MIN is supported without negating a signed minimum.
            auto const delta{::std::uint64_t{} - static_cast<::std::uint64_t>(location.displacement)};
            if(delta > base) { return object_location_error::address_overflow; }
            guest_offset = base - delta;
        }
        return object_location_error::none;
    }
    struct escaped_metadata_text { ::std::string_view text{}; };
    template<typename char_type, typename output>
    inline void print_define(::fast_io::io_reserve_type_t<char_type, escaped_metadata_text>, output&& stream, escaped_metadata_text value)
    {
        auto const count{(::std::min)(value.text.size(), ::std::size_t{4096u})};
        for(::std::size_t i{}; i != count; ++i)
        {
            // [immutable owned metadata text ... i ... end]
            // [safe                                      ] unsafe (one-past)
            //                                  ^^ i<count<=size before read.
            auto const byte{static_cast<unsigned char>(value.text[i])};
            if(byte >= 0x20u && byte < 0x7fu && byte != '\\' && byte != '"')
            { ::fast_io::io::print(stream, ::fast_io::mnp::chvw(static_cast<char_type>(byte))); }
            else if(byte == '\\') { ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(::std::string_view{"\\\\"})); }
            else if(byte == '"') { ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(::std::string_view{"\\\""})); }
            else
            {
                // Escape non-ASCII bytes as well: invalid UTF-8, C1 terminal
                // controls and Unicode bidi controls cannot become terminal
                // instructions after a character-domain conversion. Hex is
                // emitted by fast_io, never by a handwritten numeric printer.
                ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(::std::string_view{"\\x"}), ::fast_io::mnp::hex<false, true>(byte));
            }
        }
        if(count != value.text.size()) { ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(::std::string_view{"... (truncated)"})); }
    }
    struct object_node_details { object_node const* node{}; };
    [[nodiscard]] inline object_node_details object_details_of(object_node const& node) noexcept
    { return {::std::addressof(node)}; }
    template<typename char_type, typename output>
    inline void print_define(::fast_io::io_reserve_type_t<char_type, object_node_details>, output&& stream, object_node_details value)
    {
        if(value.node == nullptr) { ::fast_io::fast_terminate(); }
        auto const& node{*value.node};
        auto const depth{(::std::min)(node.depth, ::std::size_t{32u})};
        for(::std::size_t i{}; i != depth; ++i) { ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(::std::string_view{"  "})); }
        ::fast_io::io::print(stream, escaped_metadata_text{node.name.empty() ? ::std::string_view{"object"} : ::std::string_view{node.name}},
            ::fast_io::mnp::code_cvt(::std::string_view{": type="}), escaped_metadata_text{node.type_name.empty() ? object_type_kind_text(node.kind) : ::std::string_view{node.type_name}},
            ::fast_io::mnp::code_cvt(::std::string_view{", offset="}), ::fast_io::mnp::dec(node.byte_offset),
            ::fast_io::mnp::code_cvt(::std::string_view{", bytes="}), ::fast_io::mnp::dec(node.byte_size));
        if(node.variant_part) { ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(::std::string_view{", variant-part"})); }
        if(node.variant_case)
        { ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(node.active_variant ? ::std::string_view{", active-variant"} : ::std::string_view{", variant"}),
            ::fast_io::mnp::code_cvt(node.default_variant ? ::std::string_view{", default"} : ::std::string_view{})); }
        if(node.reason != object_unavailable_reason::none)
        { ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(::std::string_view{", unavailable="}), ::fast_io::mnp::code_cvt(object_reason_text(node.reason))); }
        else if(node.value_available)
        {
            ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(::std::string_view{", value="}));
            if(node.kind == type_kind::pointer)
            { ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(::std::string_view{"guest:"}), ::fast_io::mnp::hex0x(node.bits)); }
            else if(node.scalar_kind == numeric_kind::signed_integer)
            {
                numeric_variable scalar{}; scalar.bits = node.bits; scalar.byte_count = node.scalar_bytes;
                ::fast_io::io::print(stream, ::fast_io::mnp::dec(numeric_signed_value(scalar)));
            }
            else if(node.scalar_kind == numeric_kind::f32_bits)
            { ::fast_io::io::print(stream, ::std::bit_cast<float>(static_cast<::std::uint32_t>(node.bits))); }
            else if(node.scalar_kind == numeric_kind::f64_bits)
            { ::fast_io::io::print(stream, ::std::bit_cast<double>(node.bits)); }
            else if(node.scalar_kind == numeric_kind::boolean)
            { ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(node.bits != 0u ? ::std::string_view{"true"} : ::std::string_view{"false"})); }
            else { ::fast_io::io::print(stream, ::fast_io::mnp::dec(node.bits)); }
            if(!node.enumerator.empty())
            { ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(::std::string_view{" ("}), escaped_metadata_text{::std::string_view{node.enumerator}}, ::fast_io::mnp::code_cvt(::std::string_view{")"})); }
        }
        if(node.display_text_available)
        { ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(::std::string_view{", text="}), escaped_metadata_text{node.display_text});
          if(node.display_text_truncated) { ::fast_io::io::print(stream,::fast_io::mnp::code_cvt(::std::string_view{" (truncated)"})); } }
        if(node.omitted_children != 0u)
        { ::fast_io::io::print(stream, ::fast_io::mnp::code_cvt(::std::string_view{", omitted-elements="}), ::fast_io::mnp::dec(node.omitted_children)); }
    }
}
