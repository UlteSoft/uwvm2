/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_expression.h"
# include <cstddef>
# include <cstdint>
# include <limits>
# include <span>
# include <utility>
# include <vector>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_language_expression
{
    namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
    // This finite evaluator never owns a stop, module or memory authority.
    // Its initial bytes are OWNED DATA and reader is the caller's synchronous
    // policy. Production invokes it only inside the actual runtime canonical
    // capture/source-binding/cohort/publication/memory transaction. Each read
    // returns another owned complete object; no raw pointer/span is retained.
    struct limits { ::std::size_t maximum_reads{32u}; };
    namespace details
    {
        [[nodiscard]] inline dwarf::inline_query_error validate_steps(::std::span<dwarf::source_expression_step const> steps) noexcept
        {
            if(steps.size() > 32u) { return dwarf::inline_query_error::limit_exceeded; }
            ::std::size_t charged{};
            for(::std::size_t i{}; i != steps.size(); ++i)
            {
                auto const& step{steps[i]};
                if(step.kind == dwarf::source_expression_step_kind::member)
                {
                    if(step.member.empty()) { return dwarf::inline_query_error::malformed; }
                    if(step.member.size() > 4096u - charged) { return dwarf::inline_query_error::limit_exceeded; }
                    charged += step.member.size(); // bounded subtraction proved BEFORE copying any member string.
                }
                else if(step.kind == dwarf::source_expression_step_kind::go_length || step.kind == dwarf::source_expression_step_kind::go_capacity)
                {
                    if(i + 1u != steps.size() || !step.member.empty() || step.index != 0) { return dwarf::inline_query_error::malformed; }
                }
                else if(step.kind != dwarf::source_expression_step_kind::index && step.kind != dwarf::source_expression_step_kind::dereference)
                { return dwarf::inline_query_error::malformed; }
                else if(!step.member.empty()) { return dwarf::inline_query_error::malformed; }
            }
            return dwarf::inline_query_error::none;
        }
        [[nodiscard]] inline bool conventional_pointer(::std::span<dwarf::type_record const> types,
            dwarf::object_node const& node, ::std::uint8_t address_bytes, ::std::size_t& target, bool cpp_reference = false) noexcept
        {
            if(node.type >= types.size()) { return false; }
            auto const& type{types[node.type]}; // [safe] checked metadata index before immutable borrow.
            if(node.kind != dwarf::type_kind::pointer || type.kind != dwarf::type_kind::pointer ||
               (cpp_reference ? (!type.reference_type || !dwarf::object_details::cxx_reference_spelling(type)) :
                    (type.reference_type || type.rvalue_reference_type)) || node.bit_field ||
               (type.address_class_known && type.address_class != 0u) ||
               !type.size_known || type.byte_size != address_bytes || type.byte_count != address_bytes ||
               (address_bytes != 4u && address_bytes != 8u) || type.referenced_type >= types.size()) { return false; }
            auto const& pointee{types[type.referenced_type]}; // [safe] exact bounded graph target, no address calculation.
            if(!pointee.size_known || pointee.byte_size == 0u || pointee.byte_size > 65536u ||
               pointee.kind == dwarf::type_kind::unavailable) { return false; }
            target = type.referenced_type; return true;
        }
        [[nodiscard]] inline bool rust_sequence(::std::span<dwarf::type_record const> types, ::std::size_t root,
            ::std::span<dwarf::object_selector_step const> segment, ::std::uint8_t width, ::std::size_t& target, bool& string)
        {
            ::std::vector<dwarf::object_node> layout{};
            if(dwarf::query_selected_object_type(types,root,segment,layout) != dwarf::inline_query_error::none || layout.empty() || layout[0u].type >= types.size()) { return false; }
            auto const& type{types[layout[0u].type]}; ::std::string_view name{type.name};
            string = name == "&str" || name == "&mut str";
            if(type.language != 0x1cu || type.kind != dwarf::type_kind::structure || !type.size_known || type.byte_size != width*2u ||
               (!string && !(name.starts_with("&[") || name.starts_with("&mut [")))) { return false; }
            if(type.members.size() != 2u) { return false; }
            dwarf::member_record const* pointer{}; dwarf::member_record const* length{};
            for(auto const& member : type.members)
            { if(member.name == "data_ptr") { pointer = ::std::addressof(member); } else if(member.name == "length") { length = ::std::addressof(member); } }
            if(pointer == nullptr || length == nullptr || !pointer->offset_known || !length->offset_known || pointer->bit_field || length->bit_field ||
               pointer->byte_offset != 0u || length->byte_offset != width || pointer->type >= types.size() || length->type >= types.size()) { return false; }
            auto const& pt{types[pointer->type]}; auto const& lt{types[length->type]};
            if(pt.kind != dwarf::type_kind::pointer || pt.reference_type || pt.rvalue_reference_type || !pt.size_known || pt.byte_size != width ||
               (pt.address_class_known && pt.address_class != 0u) || pt.referenced_type >= types.size() ||
               lt.kind != dwarf::type_kind::scalar || lt.encoding != 7u || !lt.size_known || lt.byte_size != width || lt.byte_count != width) { return false; }
            auto const& element{types[pt.referenced_type]};
            if(!element.size_known || element.byte_size == 0u || element.byte_size > 65536u || element.kind == dwarf::type_kind::unavailable ||
               (string && (element.kind != dwarf::type_kind::scalar || element.byte_size != 1u))) { return false; }
            target = pt.referenced_type; return true;
        }
        [[nodiscard]] inline bool go_language(dwarf::type_record const& type) noexcept
        { return type.language == 0x16u || (type.language == 0x0cu && type.tinygo_producer); }
        // TinyGo labels Go compilation units C99 with the exact TinyGo producer;
        // preserve that raw language and adapt only the owned producer metadata.
        [[nodiscard]] inline bool implicit_member(dwarf::type_record const& type) noexcept
        { return dwarf::object_details::cxx_reference_spelling(type) || go_language(type) ||
            (type.language == 0x1cu && ::std::string_view{type.name}.starts_with("&")); }
        struct sequence_layout { ::std::size_t element{}; bool string{}; ::std::string_view pointer{}, length{}; };
        [[nodiscard]] inline bool sequence(::std::span<dwarf::type_record const> types, ::std::size_t root,
            ::std::span<dwarf::object_selector_step const> segment, ::std::uint8_t width, sequence_layout& out)
        {
            out = {};
            if(rust_sequence(types,root,segment,width,out.element,out.string))
            { out.pointer = "data_ptr"; out.length = "length"; return true; }
            ::std::vector<dwarf::object_node> layout{};
            if(dwarf::query_selected_object_type(types,root,segment,layout) != dwarf::inline_query_error::none ||
               layout.empty() || layout[0u].type >= types.size()) { return false; }
            auto const& type{types[layout[0u].type]};
            out.string = type.name == "string";
            if(!go_language(type) || type.kind != dwarf::type_kind::structure || !type.size_known ||
               (!out.string && !::std::string_view{type.name}.starts_with("[]")) ||
               type.byte_size != width*(out.string ? 2u : 3u) || type.members.size() != (out.string ? 2u : 3u)) { return false; }
            dwarf::member_record const* pointer{}; dwarf::member_record const* length{}; dwarf::member_record const* capacity{};
            for(auto const& member : type.members)
            { if(member.name == "ptr") { pointer = ::std::addressof(member); } else if(member.name == "len") { length = ::std::addressof(member); }
              else if(member.name == "cap") { capacity = ::std::addressof(member); } else { return false; } }
            auto const integer{[&](dwarf::member_record const* member, ::std::uint64_t offset)
            {
                if(member == nullptr || !member->offset_known || member->bit_field || member->byte_offset != offset || member->type >= types.size()) { return false; }
                auto const& value{types[member->type]};
                return value.kind == dwarf::type_kind::scalar && (value.encoding == 5u || value.encoding == 7u) &&
                    value.size_known && value.byte_size == width && value.byte_count == width;
            }};
            if(!integer(length,width) || (!out.string && !integer(capacity,width*2u)) || pointer == nullptr ||
               !pointer->offset_known || pointer->bit_field || pointer->byte_offset != 0u || pointer->type >= types.size()) { return false; }
            auto const& pt{types[pointer->type]};
            if(pt.kind != dwarf::type_kind::pointer || pt.reference_type || pt.rvalue_reference_type ||
               (pt.address_class_known && pt.address_class != 0u) || !pt.size_known || pt.byte_size != width || pt.byte_count != width ||
               pt.referenced_type >= types.size()) { return false; }
            auto const& element{types[pt.referenced_type]};
            if(!element.size_known || element.byte_size == 0u || element.byte_size > 65536u || element.kind == dwarf::type_kind::unavailable ||
               (out.string && (element.kind != dwarf::type_kind::scalar || element.byte_size != 1u))) { return false; }
            out.element = pt.referenced_type; out.pointer = "ptr"; out.length = "len"; return true;
        }
        [[nodiscard]] inline bool sequence_carriers(::std::span<dwarf::type_record const> types, ::std::size_t root,
            ::std::vector<dwarf::object_selector_step> segment, ::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known, ::std::span<::std::byte const> qualified,
            ::std::uint64_t& address, ::std::uint64_t& length, sequence_layout const& layout)
        {
            ::std::vector<dwarf::object_node> out{};
            segment.push_back({dwarf::object_selector_kind::member,::fast_io::concat_std(layout.length),{}});
            auto const length_status{known.empty() ? dwarf::query_selected_object_value(types,root,segment,bytes,out) :
                dwarf::query_selected_object_value_with_known_bits(types,root,segment,bytes,known,out)};
            if(length_status != dwarf::inline_query_error::none || out.size() != 1u || !out[0u].value_available) { return false; }
            if(out[0u].scalar_kind == dwarf::numeric_kind::signed_integer && (out[0u].bits & (::std::uint64_t{1u} << (out[0u].byte_size*8u-1u))) != 0u) { return false; }
            length = out[0u].bits;
            if(length == 0u) { address = 0u; return true; }
            segment.back().member = ::fast_io::concat_std(layout.pointer);
            auto const pointer_status{qualified.empty() ? dwarf::query_selected_object_value(types,root,segment,bytes,out) :
                dwarf::query_selected_object_value_with_known_bits(types,root,segment,bytes,qualified,out)};
            if(pointer_status != dwarf::inline_query_error::none || out.size() != 1u || !out[0u].value_available) { return false; }
            address = out[0u].bits; return true;
        }
        // The Go result is int, independently of the producer's unsigned
        // descriptor fields. Neither len nor cap follows the data pointer.
        [[nodiscard]] inline bool go_array_length(::std::span<dwarf::type_record const> types,
            dwarf::object_node const& selected, ::std::uint8_t width, ::std::uint64_t& length) noexcept
        {
            if(selected.type >= types.size() || selected.bit_field || (width != 4u && width != 8u)) { return false; }
            auto index{selected.type};
            auto const& argument{types[index]};
            if(!go_language(argument)) { return false; }
            if(argument.kind == dwarf::type_kind::pointer)
            {
                // Metadata alone identifies *[N]T. Its address may be nil,
                // missing, or ineligible as a read carrier: none is followed.
                if(argument.reference_type || argument.rvalue_reference_type || !argument.size_known ||
                   argument.byte_size != width || argument.byte_count != width ||
                   (argument.address_class_known && argument.address_class != 0u) ||
                   argument.referenced_type >= types.size()) { return false; }
                index = argument.referenced_type;
            }
            auto const& array{types[index]};
            if(!go_language(array) || array.kind != dwarf::type_kind::array || !array.size_known ||
               !array.contiguous_array || !array.row_major_array || array.dimensions.size() != 1u ||
               array.referenced_type >= types.size()) { return false; }
            auto const& bound{array.dimensions[0u]};
            auto const& element{types[array.referenced_type]};
            auto const maximum{width == 4u ? 0x7fffffffull : 0x7fffffffffffffffull};
            if(!bound.count_known || !bound.lower_bound_known || bound.lower_bound != 0 || bound.count > maximum ||
               !element.size_known || element.kind == dwarf::type_kind::unavailable ||
               (element.byte_size == 0u && element.kind != dwarf::type_kind::structure && element.kind != dwarf::type_kind::array) ||
               (element.byte_size != 0u && bound.count > (::std::numeric_limits<::std::uint64_t>::max)() / element.byte_size) ||
               array.byte_size != bound.count * element.byte_size) { return false; }
            length = bound.count; return true;
        }
        [[nodiscard]] inline bool go_builtin_layout(::std::span<dwarf::type_record const> types, ::std::size_t root,
            ::std::span<dwarf::object_selector_step const> segment, ::std::uint8_t width, bool capacity, sequence_layout& layout)
        {
            ::std::vector<dwarf::object_node> selected{};
            return dwarf::query_selected_object_type(types,root,segment,selected) == dwarf::inline_query_error::none &&
                !selected.empty() && selected[0u].type < types.size() && go_language(types[selected[0u].type]) &&
                sequence(types,root,segment,width,layout) && !(capacity && layout.string);
        }
        struct go_reference_layout { ::std::size_t target{}; bool channel{}; };
        [[nodiscard]] inline bool tinygo_reference_layout(::std::span<dwarf::type_record const> types,
            ::std::size_t root, ::std::span<dwarf::object_selector_step const> segment,
            ::std::uint8_t width, bool capacity, go_reference_layout& layout)
        {
            ::std::vector<dwarf::object_node> selected{}; ::std::size_t target{};
            if(dwarf::query_selected_object_type(types,root,segment,selected) != dwarf::inline_query_error::none ||
               selected.size() != 1u || selected[0u].type >= types.size() ||
               !conventional_pointer(types,selected[0u],width,target)) { return false; }
            auto const tinygo{[](dwarf::type_record const& t) { return t.language == 0x0cu && t.tinygo_producer; }};
            auto const& pointer{types[selected[0u].type]}; auto const& object{types[target]};
            if(!tinygo(pointer) || !tinygo(object) || object.kind != dwarf::type_kind::structure) { return false; }
            bool const channel{object.name == "runtime.channel"};
            if(!channel && (object.name != "runtime.hashmap" || capacity)) { return false; }
            // TinyGo erases source map/channel spellings into these runtime
            // pointer DIEs (including named typedefs). Accept only complete,
            // checked ABI layouts; neither a name nor a field mints a read.
            auto const field{[&](::std::string_view name, ::std::uint64_t offset,
                                dwarf::type_kind kind, ::std::uint64_t size, ::std::uint64_t encoding = 0u)
            {
                dwarf::member_record const* found{};
                for(auto const& member : object.members)
                { if(member.name == name) { if(found != nullptr) { return false; } found = ::std::addressof(member); } }
                if(found == nullptr || !found->offset_known || found->bit_field || found->byte_offset != offset ||
                   found->type >= types.size() || offset > object.byte_size || size > object.byte_size-offset) { return false; }
                auto const& type{types[found->type]};
                if(!tinygo(type) || type.kind != kind || !type.size_known || type.byte_size != size) { return false; }
                if(kind == dwarf::type_kind::scalar)
                { return type.encoding == encoding && type.byte_count == size; }
                if(kind == dwarf::type_kind::pointer)
                { return type.byte_count == width && !type.reference_type && !type.rvalue_reference_type &&
                    (!type.address_class_known || type.address_class == 0u); }
                return true;
            }};
            auto const integer{[&](::std::string_view name, unsigned index)
            { return field(name,width*index,dwarf::type_kind::scalar,width,7u); }};
            if(channel)
            {
                // The qualified wasm/no-scheduler ABI has a zero-sized PMutex.
                // Other mutex/scheduler layouts remain explicit unavailable.
                if(object.byte_size != width*9u || object.members.size() != 11u ||
                   !field("closed",0u,dwarf::type_kind::scalar,1u,2u) ||
                   !field("selectLocked",1u,dwarf::type_kind::scalar,1u,2u) ||
                   !integer("elementSize",1u) || !integer("bufCap",2u) || !integer("bufLen",3u) ||
                   !integer("bufHead",4u) || !integer("bufTail",5u) ||
                   !field("senders",width*6u,dwarf::type_kind::structure,width) ||
                   !field("receivers",width*7u,dwarf::type_kind::structure,width) ||
                   !field("lock",width*8u,dwarf::type_kind::structure,0u) ||
                   !field("buf",width*8u,dwarf::type_kind::pointer,width)) { return false; }
            }
            else
            {
                bool const slots{object.members.size() == 11u}; unsigned const trailing{slots ? 7u : 5u};
                if((!slots && object.members.size() != 9u) || object.byte_size != width*(trailing+5u) ||
                   !field("buckets",0u,dwarf::type_kind::pointer,width) || !integer("seed",1u) ||
                   !integer("count",2u) || !integer("keySize",3u) || !integer("valueSize",4u) ||
                   (slots && (!integer("keySlotSize",5u) || !integer("valueSlotSize",6u))) ||
                   !field("bucketBits",width*trailing,dwarf::type_kind::scalar,1u,7u) ||
                   !field("flags",width*trailing+1u,dwarf::type_kind::scalar,1u,7u) ||
                   !field("keyEqual",width*(trailing+1u),dwarf::type_kind::structure,width*2u) ||
                   !field("keyHash",width*(trailing+3u),dwarf::type_kind::structure,width*2u)) { return false; }
            }
            layout = {target,channel}; return true;
        }
        inline void go_builtin_result(::std::uint8_t width, bool capacity, ::std::uint64_t bits, bool available,
            ::std::vector<dwarf::object_node>& out)
        {
            dwarf::object_node result{};
            result.name = ::fast_io::concat_std(capacity ? "$cap" : "$len");
            result.type_name = ::fast_io::concat_std("int"); result.kind = dwarf::type_kind::scalar;
            result.scalar_kind = dwarf::numeric_kind::signed_integer; result.scalar_bytes = width;
            result.byte_size = width; result.bits = bits; result.value_available = available;
            out = {}; out.push_back(::std::move(result));
        }
        [[nodiscard]] inline dwarf::inline_query_error go_builtin_value(::std::span<dwarf::type_record const> types,
            ::std::size_t root, ::std::vector<dwarf::object_selector_step> segment, ::std::uint8_t width, bool capacity,
            ::std::span<::std::byte const> bytes, ::std::span<::std::byte const> known, ::std::vector<dwarf::object_node>& out)
        {
            ::std::vector<dwarf::object_node> selected{}; ::std::uint64_t length{};
            if(dwarf::query_selected_object_type(types,root,segment,selected) == dwarf::inline_query_error::none &&
               !selected.empty() && go_array_length(types,selected[0u],width,length))
            { go_builtin_result(width,capacity,length,true,out); return dwarf::inline_query_error::none; }
            sequence_layout layout{};
            if(!go_builtin_layout(types,root,segment,width,capacity,layout)) { return dwarf::inline_query_error::unavailable; }
            segment.push_back({dwarf::object_selector_kind::member,::fast_io::concat_std(capacity ? "cap" : layout.length),{}});
            ::std::vector<dwarf::object_node> field{};
            auto const status{known.empty() ? dwarf::query_selected_object_value(types,root,segment,bytes,field) :
                dwarf::query_selected_object_value_with_known_bits(types,root,segment,bytes,known,field)};
            if(status != dwarf::inline_query_error::none) { return status; }
            auto const maximum{width == 4u ? 0x7fffffffull : 0x7fffffffffffffffull};
            if(field.size() != 1u || !field[0u].value_available || field[0u].scalar_bytes != width || field[0u].bits > maximum)
            { return dwarf::inline_query_error::unavailable; }
            auto const bits{field[0u].bits};
            // A missing independent descriptor field does not fabricate data
            // or hide an otherwise complete length/capacity. If both fields
            // are present, reject a contradictory slice descriptor.
            if(!layout.string)
            {
                segment.back().member = ::fast_io::concat_std(capacity ? layout.length : "cap");
                auto const other{known.empty() ? dwarf::query_selected_object_value(types,root,segment,bytes,field) :
                    dwarf::query_selected_object_value_with_known_bits(types,root,segment,bytes,known,field)};
                if(other == dwarf::inline_query_error::none && field.size() == 1u && field[0u].value_available &&
                    (field[0u].bits > maximum || (capacity ? field[0u].bits > bits : bits > field[0u].bits)))
                { return dwarf::inline_query_error::unavailable; }
            }
            go_builtin_result(width,capacity,bits,true,out); return dwarf::inline_query_error::none;
        }
        inline void add(::std::vector<dwarf::object_selector_step>& segment, dwarf::source_expression_step const& step)
        { segment.push_back({step.kind == dwarf::source_expression_step_kind::member ? dwarf::object_selector_kind::member :
                dwarf::object_selector_kind::index, step.member, step.index}); }
    }
    // Type queries inspect authenticated producer metadata only. Even a null
    // pointer can have a known pointee type; this operation performs no read.
    [[nodiscard]] inline dwarf::inline_query_error type(::std::span<dwarf::type_record const> types,
        ::std::size_t root, ::std::span<dwarf::source_expression_step const> steps, ::std::uint8_t address_bytes,
        ::std::vector<dwarf::object_node>& out) noexcept
    {
        out = {};
        auto const validated{details::validate_steps(steps)};
        if(validated != dwarf::inline_query_error::none) { return validated; }
        if(address_bytes != 4u && address_bytes != 8u) { return dwarf::inline_query_error::malformed; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            ::std::vector<dwarf::object_selector_step> segment{};
            for(auto const& step : steps)
            {
                if(step.kind == dwarf::source_expression_step_kind::go_length || step.kind == dwarf::source_expression_step_kind::go_capacity)
                {
                    details::sequence_layout layout{}; bool const capacity{step.kind == dwarf::source_expression_step_kind::go_capacity};
                    ::std::vector<dwarf::object_node> selected{}; ::std::uint64_t length{};
                    if(dwarf::query_selected_object_type(types,root,segment,selected) == dwarf::inline_query_error::none &&
                       !selected.empty() && details::go_array_length(types,selected[0u],address_bytes,length))
                    { details::go_builtin_result(address_bytes,capacity,0u,false,out); return dwarf::inline_query_error::none; }
                    details::go_reference_layout reference{};
                    if(details::tinygo_reference_layout(types,root,segment,address_bytes,capacity,reference))
                    { details::go_builtin_result(address_bytes,capacity,0u,false,out); return dwarf::inline_query_error::none; }
                    if(!details::go_builtin_layout(types,root,segment,address_bytes,capacity,layout)) { return dwarf::inline_query_error::unavailable; }
                    details::go_builtin_result(address_bytes,capacity,0u,false,out); return dwarf::inline_query_error::none;
                }
                if(step.kind == dwarf::source_expression_step_kind::index)
                {
                    details::sequence_layout sequence{};
                    if(details::sequence(types,root,segment,address_bytes,sequence))
                    { if(step.index < 0) { return dwarf::inline_query_error::unavailable; } root = sequence.element; segment.clear(); continue; }
                }
                bool implicit{}, cpp_reference{};
                if(step.kind == dwarf::source_expression_step_kind::member)
                {
                    ::std::vector<dwarf::object_node> selected{}; ::std::size_t target{};
                    if(dwarf::query_selected_object_type(types,root,segment,selected) == dwarf::inline_query_error::none && selected.size() == 1u &&
                       selected[0u].type < types.size() && details::implicit_member(types[selected[0u].type]))
                    {
                        cpp_reference=dwarf::object_details::cxx_reference_spelling(types[selected[0u].type]);
                        implicit=details::conventional_pointer(types,selected[0u],address_bytes,target,cpp_reference);
                    }
                }
                if(step.kind != dwarf::source_expression_step_kind::dereference && !implicit)
                {
                    if(step.kind != dwarf::source_expression_step_kind::member && step.kind != dwarf::source_expression_step_kind::index)
                    { return dwarf::inline_query_error::malformed; }
                    details::add(segment, step); continue;
                }
                ::std::vector<dwarf::object_node> selected{};
                auto const status{dwarf::query_selected_object_type(types, root, segment, selected)};
                if(status != dwarf::inline_query_error::none) { return status; }
                ::std::size_t target{};
                if(selected.size() != 1u) { return dwarf::inline_query_error::unavailable; }
                if(!details::conventional_pointer(types, selected[0u], address_bytes, target,cpp_reference))
                {
                    ::std::uint64_t length{};
                    if(selected[0u].type >= types.size() || types[selected[0u].type].kind != dwarf::type_kind::pointer ||
                       !details::go_array_length(types,selected[0u],address_bytes,length)) { return dwarf::inline_query_error::unavailable; }
                    target = types[selected[0u].type].referenced_type;
                }
                root = target; segment.clear(); // checked scalar graph transition, not a native/guest pointer.
                if(implicit) { details::add(segment,step); }
            }
            return dwarf::query_selected_object_type(types, root, segment, out);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out = {}; return dwarf::inline_query_error::allocation_failure; }
#endif
    }
    template<typename Reader>
    [[nodiscard]] inline dwarf::inline_query_error value(::std::span<dwarf::type_record const> types,
        ::std::size_t root, ::std::span<dwarf::source_expression_step const> steps,
        ::std::vector<::std::byte> bytes, ::std::vector<::std::byte> known_bits,
        ::std::uint8_t address_bytes, Reader&& read, ::std::vector<dwarf::object_node>& out, limits const& cap = {},
        ::std::span<::std::byte const> pointer_carrier_known_bits = {}) noexcept
    {
        out = {};
        auto const validated{details::validate_steps(steps)};
        if(validated != dwarf::inline_query_error::none) { return validated; }
        if(bytes.size() > 65536u) { return dwarf::inline_query_error::limit_exceeded; }
        if((address_bytes != 4u && address_bytes != 8u) || (!known_bits.empty() && known_bits.size() != bytes.size()) ||
           (!pointer_carrier_known_bits.empty() && pointer_carrier_known_bits.size() != bytes.size()))
        { return dwarf::inline_query_error::malformed; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            // This grammar contains no calls or channel receives. For an
            // array or *array, Go len/cap is therefore constant and does not
            // evaluate any member/index/dereference prefix. Walk only the
            // authenticated type graph, including nil pointers, before the
            // ordinary dynamic string/slice path can request a guest read.
            if(!steps.empty() && (steps.back().kind == dwarf::source_expression_step_kind::go_length ||
                steps.back().kind == dwarf::source_expression_step_kind::go_capacity))
            {
                ::std::vector<dwarf::object_node> selected{}; ::std::uint64_t length{};
                if(type(types,root,steps.first(steps.size()-1u),address_bytes,selected) == dwarf::inline_query_error::none &&
                   !selected.empty() && details::go_array_length(types,selected[0u],address_bytes,length))
                {
                    details::go_builtin_result(address_bytes,steps.back().kind == dwarf::source_expression_step_kind::go_capacity,
                        length,true,out); return dwarf::inline_query_error::none;
                }
            }
            ::std::vector<dwarf::object_selector_step> segment{}; ::std::size_t reads{};
            // Source bytes and pointer-carrier eligibility differ: a legitimate
            // aggregate float member can remain displayable while its floating
            // storage bits cannot mint a conventional source pointer. Intersect
            // eligibility with actual availability before pointer planning.
            ::std::vector<::std::byte> pointer_known{};
            if(!pointer_carrier_known_bits.empty())
            {
                pointer_known.resize(bytes.size());
                for(::std::size_t i{}; i != bytes.size(); ++i)
                {
                    // [owned exact-size value/mask spans ... i<size] end
                    // [safe                                         ] equal checked indices only.
                    pointer_known[i] = pointer_carrier_known_bits[i] &
                        (known_bits.empty() ? ::std::byte{0xffu} : known_bits[i]);
                }
            }
            auto const allowed{cap.maximum_reads < 32u ? cap.maximum_reads : 32u};
            for(auto const& step : steps)
            {
                if(step.kind == dwarf::source_expression_step_kind::go_length || step.kind == dwarf::source_expression_step_kind::go_capacity)
                {
                    bool const capacity{step.kind == dwarf::source_expression_step_kind::go_capacity};
                    details::go_reference_layout reference{};
                    if(details::tinygo_reference_layout(types,root,segment,address_bytes,capacity,reference))
                    {
                        auto const& qualified{pointer_known.empty() ? known_bits : pointer_known};
                        ::std::vector<dwarf::object_node> pointer{};
                        auto const selected{qualified.empty() ? dwarf::query_selected_object_value(types,root,segment,bytes,pointer) :
                            dwarf::query_selected_object_value_with_known_bits(types,root,segment,bytes,qualified,pointer)};
                        if(selected != dwarf::inline_query_error::none || pointer.size() != 1u || !pointer[0u].value_available ||
                           pointer[0u].reason != dwarf::object_unavailable_reason::none || pointer[0u].scalar_bytes != address_bytes)
                        { return dwarf::inline_query_error::unavailable; }
                        if(pointer[0u].bits == 0u)
                        { details::go_builtin_result(address_bytes,capacity,0u,true,out); return dwarf::inline_query_error::none; }
                        dwarf::copied_guest_pointer_plan plan{};
                        auto const planned{qualified.empty() ? dwarf::plan_copied_guest_pointer(types,root,segment,bytes,plan) :
                            dwarf::plan_copied_guest_pointer_with_known_bits(types,root,segment,bytes,qualified,plan)};
                        auto const maximum_address{address_bytes == 4u ? 0xffffffffull : ~::std::uint64_t{}};
                        if(planned != dwarf::inline_query_error::none || plan.type != reference.target ||
                           plan.address_bytes != address_bytes || plan.extent == 0u || plan.extent > 65536u ||
                           plan.guest_offset > maximum_address || plan.extent-1u > maximum_address-plan.guest_offset)
                        { return dwarf::inline_query_error::unavailable; }
                        if(reads >= allowed) { return dwarf::inline_query_error::limit_exceeded; } ++reads;
                        ::std::vector<::std::byte> header{};
                        if(!read(plan.guest_offset,static_cast<::std::size_t>(plan.extent),header) || header.size() != plan.extent)
                        { return dwarf::inline_query_error::unavailable; }
                        // One ordinary guest-pointer copy, in the caller's
                        // existing stopped-guest transaction. Never follow a
                        // bucket/buffer/function/queue pointer or acquire locks.
                        ::std::vector<dwarf::object_selector_step> member{{dwarf::object_selector_kind::member,
                            ::fast_io::concat_std(::std::string_view{reference.channel ? (capacity ? "bufCap" : "bufLen") : "count"}),{}}};
                        ::std::vector<dwarf::object_node> field{};
                        auto const value{dwarf::query_selected_object_value(types,reference.target,member,header,field)};
                        auto const maximum_int{address_bytes == 4u ? 0x7fffffffull : 0x7fffffffffffffffull};
                        if(value != dwarf::inline_query_error::none || field.size() != 1u || !field[0u].value_available ||
                           field[0u].scalar_bytes != address_bytes || field[0u].bits > maximum_int)
                        { return dwarf::inline_query_error::unavailable; }
                        auto const bits{field[0u].bits};
                        if(reference.channel)
                        {
                            member[0u].member = ::fast_io::concat_std(capacity ? "bufLen" : "bufCap");
                            if(dwarf::query_selected_object_value(types,reference.target,member,header,field) != dwarf::inline_query_error::none ||
                               field.size() != 1u || !field[0u].value_available || field[0u].bits > maximum_int ||
                               (capacity ? field[0u].bits > bits : bits > field[0u].bits))
                            { return dwarf::inline_query_error::unavailable; }
                        }
                        details::go_builtin_result(address_bytes,capacity,bits,true,out); return dwarf::inline_query_error::none;
                    }
                    return details::go_builtin_value(types,root,segment,address_bytes,capacity,
                        bytes,known_bits,out);
                }
                if(step.kind == dwarf::source_expression_step_kind::index)
                {
                    details::sequence_layout sequence{};
                    if(details::sequence(types,root,segment,address_bytes,sequence))
                    {
                        ::std::uint64_t address{}, length{};
                        auto const& qualified{pointer_known.empty() ? known_bits : pointer_known};
                        if(!details::sequence_carriers(types,root,segment,bytes,known_bits,qualified,address,length,sequence) ||
                           step.index < 0 || static_cast<::std::uint64_t>(step.index) >= length || address == 0u)
                        { return dwarf::inline_query_error::unavailable; }
                        auto const target{sequence.element}; auto const extent{types[target].byte_size}; auto const index{static_cast<::std::uint64_t>(step.index)};
                        auto const maximum{address_bytes == 4u ? 0xffffffffull : ~::std::uint64_t{}};
                        // Prove the complete declared slice without multiplying length*extent.
                        // The selected element may fit even when a later element crosses the guest width.
                        if(address > maximum || extent-1u > maximum-address ||
                           length-1u > (maximum-address-(extent-1u))/extent ||
                           index > (maximum-address)/extent) { return dwarf::inline_query_error::unavailable; }
                        auto const offset{address+index*extent};
                        if(extent-1u > maximum-offset) { return dwarf::inline_query_error::unavailable; }
                        if(reads >= allowed) { return dwarf::inline_query_error::limit_exceeded; } ++reads;
                        ::std::vector<::std::byte> copied{};
                        if(!read(offset,static_cast<::std::size_t>(extent),copied) || copied.size() != extent) { return dwarf::inline_query_error::unavailable; }
                        bytes = ::std::move(copied); root = target; segment.clear(); known_bits.clear(); pointer_known.clear(); continue;
                    }
                }
                bool implicit{}, cpp_reference{};
                if(step.kind == dwarf::source_expression_step_kind::member)
                {
                    ::std::vector<dwarf::object_node> selected{}; ::std::size_t target{};
                    if(dwarf::query_selected_object_type(types,root,segment,selected) == dwarf::inline_query_error::none && selected.size() == 1u &&
                       selected[0u].type < types.size() && details::implicit_member(types[selected[0u].type]))
                    {
                        cpp_reference=dwarf::object_details::cxx_reference_spelling(types[selected[0u].type]);
                        implicit=details::conventional_pointer(types,selected[0u],address_bytes,target,cpp_reference);
                    }
                }
                if(step.kind != dwarf::source_expression_step_kind::dereference && !implicit)
                {
                    if(step.kind != dwarf::source_expression_step_kind::member && step.kind != dwarf::source_expression_step_kind::index)
                    { return dwarf::inline_query_error::malformed; }
                    details::add(segment, step); continue;
                }
                dwarf::copied_guest_pointer_plan plan{};
                auto const& required_known{pointer_known.empty() ? known_bits : pointer_known};
                auto const status{cpp_reference ?
                    (required_known.empty() ? dwarf::plan_copied_guest_cpp_reference(types,root,segment,bytes,plan) :
                        dwarf::plan_copied_guest_cpp_reference_with_known_bits(types,root,segment,bytes,required_known,plan)) :
                    (required_known.empty() ? dwarf::plan_copied_guest_pointer(types,root,segment,bytes,plan) :
                        dwarf::plan_copied_guest_pointer_with_known_bits(types,root,segment,bytes,required_known,plan))};
                if(status != dwarf::inline_query_error::none) { return status; }
                if(plan.address_bytes != address_bytes || plan.extent == 0u || plan.extent > 65536u ||
                   plan.extent > static_cast<::std::uint64_t>((::std::numeric_limits<::std::size_t>::max)()))
                { return dwarf::inline_query_error::unavailable; }
                if(reads >= allowed) { return dwarf::inline_query_error::limit_exceeded; }
                ++reads; // [safe] bounded read budget charged BEFORE invoking the supplied cold policy.
                ::std::vector<::std::byte> target{}; auto const extent{static_cast<::std::size_t>(plan.extent)};
                // guest_offset is a checked scalar plan from owned bytes. It is
                // never cast to native memory here. Actual runtime checks its
                // own retained memory bounds BEFORE every address derivation.
                if(!read(plan.guest_offset, extent, target) || target.size() != extent)
                { return dwarf::inline_query_error::unavailable; }
                bytes = ::std::move(target); known_bits.clear(); pointer_known.clear(); root = plan.type; segment.clear();
                if(implicit) { details::add(segment,step); }
                // Complete returned owned bytes replace the old image. No stale
                // validity mask, backend pointer or independently borrowed span.
            }
            auto const status{known_bits.empty() ? dwarf::query_selected_object_value(types, root, segment, bytes, out) :
                dwarf::query_selected_object_value_with_known_bits(types, root, segment, bytes, known_bits, out)};
            details::sequence_layout sequence{};
            if(status == dwarf::inline_query_error::none && !out.empty() && details::sequence(types,root,segment,address_bytes,sequence) && sequence.string)
            {
                ::std::uint64_t address{},length{}; auto const& qualified{pointer_known.empty() ? known_bits : pointer_known};
                if(details::sequence_carriers(types,root,segment,bytes,known_bits,qualified,address,length,sequence))
                {
                    // Validate the WHOLE declared guest string before a read,
                    // including when only a bounded display prefix is copied.
                    // Empty strings require neither a pointer nor read budget.
                    auto const maximum{address_bytes == 4u ? 0xffffffffull : ~::std::uint64_t{}};
                    auto const copied_bytes{static_cast<::std::size_t>(length < 4096u ? length : 4096u)};
                    ::std::vector<::std::byte> copied{};
                    if(length == 0u || (address != 0u && address <= maximum && length-1u <= maximum-address &&
                        reads < allowed && (++reads, read(address,copied_bytes,copied)) && copied.size() == copied_bytes))
                    {
                        out[0u].display_text_available = true;
                        out[0u].display_text_truncated = length > copied_bytes;
                        if(copied_bytes != 0u) { out[0u].display_text = ::fast_io::concat_std(::std::string_view{reinterpret_cast<char const*>(copied.data()),copied.size()}); }
                    }
                }
            }
            if(status == dwarf::inline_query_error::none && out.size() == 1u && out[0u].kind == dwarf::type_kind::pointer)
            {
                ::std::size_t target{};
                if(details::conventional_pointer(types,out[0u],address_bytes,target) && types[target].byte_size == 1u && types[target].name == "char")
                {
                    dwarf::copied_guest_pointer_plan plan{}; auto const& qualified{pointer_known.empty() ? known_bits : pointer_known};
                    auto const planned{qualified.empty() ? dwarf::plan_copied_guest_pointer(types,root,segment,bytes,plan) :
                        dwarf::plan_copied_guest_pointer_with_known_bits(types,root,segment,bytes,qualified,plan)};
                    if(planned == dwarf::inline_query_error::none)
                    {
                        ::std::string text{}; ::fast_io::ostring_ref_std output{::std::addressof(text)}; bool complete{};
                        auto const maximum{address_bytes == 4u ? 0xffffffffull : ~::std::uint64_t{}};
                        for(::std::size_t i{}; i < 256u && reads < allowed && plan.guest_offset <= maximum && i <= maximum-plan.guest_offset; ++i)
                        {
                            ++reads; ::std::vector<::std::byte> copied{};
                            if(!read(plan.guest_offset+i,1u,copied) || copied.size() != 1u) { break; }
                            if(copied[0u] == ::std::byte{}) { complete = true; break; }
                            ::fast_io::io::print(output,::fast_io::mnp::chvw(static_cast<char>(::std::to_integer<unsigned char>(copied[0u]))));
                        }
                        if(complete || !text.empty())
                        { out[0u].display_text = ::std::move(text); out[0u].display_text_available = true; out[0u].display_text_truncated = !complete; }
                    }
                }
            }
            return status;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out = {}; return dwarf::inline_query_error::allocation_failure; }
#endif
    }
}
