/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include "language_debug_features.h"
#ifndef UWVM_MODULE
# include "source_dwarf_objects.h"
# if UWVM_DEBUG_LANGUAGE_CPP
#  include "source_pretty_coroutine.h"
# endif
# include "source_pretty_registry.h"
# include <algorithm>
# include <charconv>
# include <initializer_list>
# include <memory>
# include <span>
# include <string>
# include <string_view>
# include <type_traits>
# include <utility>
# include <vector>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_pretty_printer
{
    namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
    enum class outcome { not_matched, formatted, unavailable, limit_exceeded, allocation_failure };
    struct limits
    {
        ::std::size_t maximum_reads{32u}, max_elements{200u}, max_nodes{1024u};
        ::std::size_t max_bytes{65536u}, max_depth{8u};
        source_pretty_registry::registry const* registry{};
    };
    // Library internals are profiles of owned DWARF, not host C++ casts. Guest
    // bytes are always little endian (including on BE debugger hosts). Reader
    // is supplied by the existing stopped source-memory transaction; no printer
    // can create a read capability, call guest code or keep a borrowed address.
    namespace details
    {
        inline void number(::std::string& text, ::std::uint64_t value)
        {
            char buffer[24u]; auto const converted{::std::to_chars(buffer,buffer+sizeof(buffer),value)};
            text.append(buffer,converted.ptr);
        }
        [[nodiscard]] inline bool family(dwarf::type_record const& type, ::std::string_view name) noexcept
        {
            if constexpr (!(UWVM_DEBUG_LANGUAGE_CPP)) { return false; }
            else
            {
            if(!dwarf::cxx_language(type.language) || type.tinygo_producer || type.zig_producer ||
               (type.kind != dwarf::type_kind::structure && type.kind != dwarf::type_kind::class_type)) { return false; }
            ::std::string_view spelling{type.aggregate_name.empty() ? type.name : type.aggregate_name};
            // DW_AT_name is unqualified on Clang/GCC; namespace qualification
            // is accepted too. Matching the name alone never selects offsets.
            // A nested template argument cannot select the outer object's ABI:
            // time_point<clock, std::chrono::duration<...>> is a time_point.
            ::std::size_t component{}, nesting{};
            for(::std::size_t i{};i!=spelling.size();++i)
            {
                if(spelling[i]=='<') { ++nesting; }
                else if(spelling[i]=='>')
                { if(nesting==0u) { return false; } --nesting; }
                else if(nesting==0u && spelling[i]==':' && i+1u<spelling.size() && spelling[i+1u]==':')
                { component=i+2u;++i; }
            }
            if(nesting!=0u) { return false; }
            spelling.remove_prefix(component);
            return spelling.substr(0u,spelling.find('<')) == name;
        
            }
        }
        struct field { ::std::size_t type{dwarf::no_record}; ::std::uint64_t offset{}; };
        namespace sequence_details
        {
#if UWVM_DEBUG_LANGUAGE_CPP
            [[nodiscard]] inline bool compressed_first(::std::span<dwarf::type_record const>,field,field&) noexcept;
            [[nodiscard]] inline bool template_count(::std::span<dwarf::type_record const>,::std::size_t,::std::size_t,unsigned,::std::uint64_t&) noexcept;
#else
            [[nodiscard]] inline bool compressed_first(::std::span<dwarf::type_record const>,field,field&) noexcept { return false; }
            [[nodiscard]] inline bool template_count(::std::span<dwarf::type_record const>,::std::size_t,::std::size_t,unsigned,::std::uint64_t&) noexcept { return false; }
#endif
        }
        enum class member_lookup { absent, found, invalid };
        struct member_ancestor { ::std::size_t type{}; member_ancestor const* parent{}; };
        // Lookup failure and genuine absence are distinct internally. An
        // ambiguous/unsupported base must never disappear merely because a
        // different inheritance branch provides one attractive field.
        [[nodiscard]] inline member_lookup member_walk(::std::span<dwarf::type_record const> types, field owner,
            ::std::string_view name, field& out, ::std::size_t& work, ::std::size_t depth,
            member_ancestor const* ancestors) noexcept
        {
            if(depth >= 16u || owner.type >= types.size()) { return member_lookup::invalid; }
            for(auto current{ancestors}; current != nullptr; current=current->parent)
            { if(current->type == owner.type) { return member_lookup::invalid; } }
            member_ancestor const active{owner.type,ancestors};
            auto const& type{types[owner.type]}; bool found{};
            for(auto const& item : type.members)
            {
                if(++work > 4096u) { return member_lookup::invalid; }
                if(item.name != name && !item.inherited && !item.name.empty()) { continue; }
                if(!item.offset_known || item.bit_field || item.type >= types.size() ||
                   !types[item.type].size_known || !type.size_known || item.byte_offset > type.byte_size ||
                   types[item.type].byte_size > type.byte_size-item.byte_offset ||
                   item.byte_offset > (~::std::uint64_t{})-owner.offset) { return member_lookup::invalid; }
                field candidate{item.type,owner.offset+item.byte_offset};
                if(item.name == name) { if(found) { return member_lookup::invalid; } out = candidate; found = true; }
                else if(item.inherited || item.name.empty())
                {
                    field nested{};
                    auto const status{member_walk(types,candidate,name,nested,work,depth+1u,::std::addressof(active))};
                    if(status == member_lookup::invalid) { return status; }
                    if(status == member_lookup::found)
                    { if(found) { return member_lookup::invalid; } out = nested; found = true; }
                }
            }
            return found ? member_lookup::found : member_lookup::absent;
        }
        [[nodiscard]] inline bool member(::std::span<dwarf::type_record const> types, field owner,
            ::std::string_view name, field& out, ::std::size_t depth = 0u) noexcept
        {
            ::std::size_t work{}; field pending{};
            if(member_walk(types,owner,name,pending,work,depth,nullptr) != member_lookup::found) { return false; }
            out=pending; return true;
        }
        [[nodiscard]] inline bool path(::std::span<dwarf::type_record const> types, ::std::size_t root,
            ::std::string_view name, field& out) noexcept
        {
            field current{root,0u}; ::std::size_t work{},parts{};
            while(!name.empty())
            {
                auto const dot{name.find('.')}; auto const part{name.substr(0u,dot)}; field next{};
                if(++parts > 16u || part.empty() || member_walk(types,current,part,next,work,0u,nullptr) != member_lookup::found) { return false; }
                current = next; if(dot == ::std::string_view::npos) { break; } name.remove_prefix(dot+1u);
            }
            out = current; return true;
        }
        [[nodiscard]] inline bool any_path(::std::span<dwarf::type_record const> types, ::std::size_t root,
            ::std::initializer_list<::std::string_view> paths, field& out) noexcept
        { for(auto name : paths) { if(path(types,root,name,out)) { return true; } } return false; }
        [[nodiscard]] inline bool integer(::std::span<dwarf::type_record const> types, field value, unsigned width = 0u) noexcept
        {
            if(value.type >= types.size()) { return false; } auto const& t{types[value.type]};
            return t.kind == dwarf::type_kind::scalar && t.size_known && t.byte_size == t.byte_count &&
                (width == 0u || t.byte_size == width) && (t.byte_size == 1u || t.byte_size == 2u || t.byte_size == 4u || t.byte_size == 8u) &&
                (t.encoding == 2u || t.encoding == 5u || t.encoding == 6u || t.encoding == 7u || t.encoding == 8u);
        }
        [[nodiscard]] inline bool pointer(::std::span<dwarf::type_record const> types, field value, unsigned width) noexcept
        {
            if(value.type >= types.size()) { return false; } auto const& t{types[value.type]};
            return t.kind == dwarf::type_kind::pointer && !t.reference_type && !t.rvalue_reference_type &&
                t.size_known && t.byte_size == width && t.byte_count == width &&
                (!t.address_class_known || t.address_class == 0u) && t.referenced_type < types.size();
        }
        [[nodiscard]] inline bool bits(::std::span<dwarf::type_record const> types, field value,
            ::std::span<::std::byte const> bytes, ::std::span<::std::byte const> known, ::std::uint64_t& result) noexcept
        {
            if(value.type >= types.size()) { return false; }
            auto const count{types[value.type].byte_size};
            if(count == 0u || count > 8u || value.offset > bytes.size() || count > bytes.size()-value.offset ||
               (!known.empty() && known.size() != bytes.size())) { return false; }
            result = 0u;
            for(::std::size_t i{}; i != count; ++i)
            {
                auto const at{static_cast<::std::size_t>(value.offset)+i};
                if(!known.empty() && known[at] != ::std::byte{0xffu}) { return false; }
                result |= static_cast<::std::uint64_t>(::std::to_integer<unsigned char>(bytes[at])) << (8u*i);
            }
            return true;
        }
        [[nodiscard]] inline bool vector_capacity(::std::span<dwarf::type_record const> types, ::std::size_t root,
            unsigned width, field& out) noexcept
        {
            if constexpr (!(UWVM_DEBUG_LANGUAGE_CPP)) { return false; }
            else
            {
            field selected{};
            if(any_path(types,root,{"_M_impl._M_end_of_storage","__cap_.__value_","__cap_.__first_","__end_cap_.__value_","__end_cap_.__first_","__cap_",
                "__layout_.__capacity_.__value_","__layout_.__capacity_.__first_","__layout_.__capacity_",
                "__layout_.__cap_.__value_","__layout_.__cap_.__first_","__layout_.__cap_"},selected))
            {
                if(family(types[selected.type],"__compressed_pair"))
                {
                    field first{};
                    if(!sequence_details::compressed_first(types,selected,first) || !pointer(types,first,width)) { return false; }
                    selected=first;
                }
                out=selected;return true;
            }
            // A stateful LLVM18 allocator gives both compressed-pair bases a
            // __value_. Keep generic ambiguity rejection; prove ordinal zero.
            for(auto name : {"__end_cap_","__cap_","__layout_.__capacity_","__layout_.__cap_"})
            {
                field carrier{};
                if(!path(types,root,name,carrier) || !family(types[carrier.type],"__compressed_pair")) { continue; }
                if(!sequence_details::compressed_first(types,carrier,selected) || !pointer(types,selected,width)) { return false; }
                out=selected;return true;
            }
            return false;
        
            }
        }
        inline void summary(dwarf::object_node& root, ::std::string_view profile, ::std::string text,
            ::std::uint64_t omitted = 0u)
        {
            bool const clipped{text.size() > 4096u};
            if(clipped) { text.resize(4093u); text += "..."; }
            root.pretty_printer.assign(profile); root.pretty_summary = ::std::move(text);
            root.pretty_available = true; root.pretty_omitted = omitted; root.pretty_truncated = omitted != 0u || clipped;
        }
        inline void scalar_text(dwarf::object_node const& value, ::std::string& text)
        {
            if(value.pretty_available) { text += value.pretty_summary; return; }
            if(value.display_text_available) { text += '"'; text += ::fast_io::concat_std(dwarf::escaped_metadata_text{value.display_text}); text += '"'; return; }
            if(!value.value_available)
            {
                if(value.kind == dwarf::type_kind::scalar || value.kind == dwarf::type_kind::pointer || value.kind == dwarf::type_kind::enumeration)
                { text += "<unavailable>"; }
                else { text += value.type_name.empty() ? "{...}" : value.type_name; }
                return;
            }
            if(value.kind == dwarf::type_kind::pointer)
            {
                text += "guest:0x"; char buffer[24u]; auto const converted{::std::to_chars(buffer,buffer+sizeof(buffer),value.bits,16)};
                text.append(buffer,converted.ptr); return;
            }
            if(value.scalar_kind == dwarf::numeric_kind::boolean) { text += value.bits ? "true" : "false"; return; }
            if(value.scalar_kind == dwarf::numeric_kind::f32_bits)
            { text += ::fast_io::concat_std(::std::bit_cast<float>(static_cast<::std::uint32_t>(value.bits))); return; }
            if(value.scalar_kind == dwarf::numeric_kind::f64_bits)
            { text += ::fast_io::concat_std(::std::bit_cast<double>(value.bits)); return; }
            if(value.scalar_kind == dwarf::numeric_kind::signed_integer)
            {
                dwarf::numeric_variable n{}; n.bits = value.bits; n.byte_count = value.scalar_bytes;
                auto const signed_value{dwarf::numeric_signed_value(n)};
                if(signed_value < 0) { text += '-'; number(text,0u-static_cast<::std::uint64_t>(signed_value)); }
                else { number(text,static_cast<::std::uint64_t>(signed_value)); } return;
            }
            if(value.scalar_kind == dwarf::numeric_kind::unsigned_integer) { number(text,value.bits); return; }
            text += value.type_name.empty() ? "{...}" : value.type_name;
        }
        [[nodiscard]] inline member_lookup unique_pointer_walk(::std::span<dwarf::type_record const> types,
            field owner, ::std::size_t target, unsigned width, field& out,
            ::std::size_t& work, unsigned depth, member_ancestor const* ancestors) noexcept
        {
            if(owner.type >= types.size() || depth >= 16u) { return member_lookup::invalid; }
            for(auto current{ancestors}; current != nullptr; current=current->parent)
            { if(current->type == owner.type) { return member_lookup::invalid; } }
            member_ancestor const active{owner.type,ancestors}; auto const& type{types[owner.type]}; bool found{};
            for(auto const& item : type.members)
            {
                if(++work > 4096u) { return member_lookup::invalid; }
                if(item.name != "_M_head_impl" && !item.inherited && !item.name.empty()) { continue; }
                if(item.type >= types.size() || !item.offset_known || item.bit_field || !type.size_known ||
                   !types[item.type].size_known || item.byte_offset > type.byte_size || types[item.type].byte_size > type.byte_size-item.byte_offset ||
                   item.byte_offset > (~::std::uint64_t{})-owner.offset) { return member_lookup::invalid; }
                field candidate{item.type,owner.offset+item.byte_offset};
                if(item.name == "_M_head_impl")
                {
                    // The deleter occupies a different tuple head, often with
                    // the identical member spelling. Its class/integer value
                    // is not a second pointer carrier. Pointer heads of T are
                    // accepted only when exactly one is proved by metadata.
                    auto const& value{types[item.type]};
                    if(value.kind != dwarf::type_kind::pointer || value.referenced_type != target) { continue; }
                    if(!pointer(types,candidate,width) || found) { return member_lookup::invalid; }
                    out=candidate;found=true;
                }
                else
                {
                    field nested{}; auto const status{unique_pointer_walk(types,candidate,target,width,nested,work,depth+1u,::std::addressof(active))};
                    if(status == member_lookup::invalid) { return status; }
                    if(status == member_lookup::found)
                    { if(found) { return member_lookup::invalid; } out=nested;found=true; }
                }
            }
            return found ? member_lookup::found : member_lookup::absent;
        }
        [[nodiscard]] inline bool unique_pointer_head(::std::span<dwarf::type_record const> types,
            field tuple, ::std::size_t target, unsigned width, field& result) noexcept
        {
            ::std::size_t work{};field pending{};
            if(target >= types.size() || unique_pointer_walk(types,tuple,target,width,pending,work,0u,nullptr) != member_lookup::found) { return false; }
            result=pending;return true;
        }
        struct indexed_sequence
        {
            ::std::size_t element{dwarf::no_record}; ::std::uint64_t address{}, count{};
            bool copied_storage{}; ::std::uint64_t copied_offset{};
            ::std::uint64_t ring_capacity{}, ring_head{};
        };
        [[nodiscard]] inline bool indexed_address(indexed_sequence const& plan,::std::uint64_t index,
            ::std::uint64_t extent,unsigned width,::std::uint64_t& address) noexcept
        {
            address=0u;
            if(plan.copied_storage || index>=plan.count || extent==0u || (width!=4u && width!=8u)) { return false; }
            auto position{index};
            if(plan.ring_capacity!=0u)
            {
                if(plan.count>plan.ring_capacity || plan.ring_head>=plan.ring_capacity) { return false; }
                auto const tail{plan.ring_capacity-plan.ring_head};
                position=index>=tail ? index-tail : plan.ring_head+index;
            }
            auto const maximum{width==4u ? 0xffffffffull : ~::std::uint64_t{}};
            if(plan.address==0u || plan.address>maximum || extent-1u>maximum-plan.address ||
               position>(maximum-plan.address-(extent-1u))/extent) { return false; }
            address=plan.address+position*extent;return true;
        }
        struct libcxx_string_field
        {
            field value{};
            ::std::size_t node{dwarf::no_record};
            dwarf::member_record const* declaration{};
        };
        enum class libcxx_lookup { absent, found, invalid };
        // Bind a metadata path to its EXACT raw child ancestry. Union aliases
        // with the same leaf spelling are never interchangeable. Transparent
        // inherited/anonymous wrappers retain their own checked parent nodes.
        [[nodiscard]] inline libcxx_lookup libcxx_string_member(::std::span<dwarf::type_record const> types,
            ::std::vector<dwarf::object_node> const& tree, libcxx_string_field owner,
            ::std::string_view name, libcxx_string_field& out, ::std::size_t& work, unsigned depth = 0u) noexcept
        {
            if(depth >= 16u || owner.value.type >= types.size() || owner.node >= tree.size()) { return libcxx_lookup::invalid; }
            auto const& type{types[owner.value.type]}; bool found{};
            bool const typed_first{name=="__value_" && family(type,"__compressed_pair") && !type.template_types.empty()};
            field first{};
            if(typed_first && !sequence_details::compressed_first(types,owner.value,first)) { return libcxx_lookup::invalid; }
            for(auto const& item : type.members)
            {
                if(++work > 65536u) { return libcxx_lookup::invalid; }
                if(item.name != name && !item.inherited && !item.name.empty()) { continue; }
                if(!item.offset_known || item.type >= types.size() || !type.size_known || !types[item.type].size_known)
                { return libcxx_lookup::invalid; }
                if(typed_first && item.inherited && (!family(types[item.type],"__compressed_pair_elem") ||
                    types[item.type].template_types.empty() || types[item.type].template_types[0u]!=first.type)) { continue; }
                auto const relative{item.bit_field ? item.data_bit_offset/8u : item.byte_offset};
                auto const extent{item.bit_field ? (item.bit_size+item.data_bit_offset%8u+7u)/8u : types[item.type].byte_size};
                if((item.bit_field && (item.bit_size == 0u || item.bit_size > 64u)) || relative > type.byte_size || extent > type.byte_size-relative ||
                   relative > (~::std::uint64_t{})-owner.value.offset) { return libcxx_lookup::invalid; }
                libcxx_string_field candidate{{item.type,owner.value.offset+relative},dwarf::no_record,::std::addressof(item)};
                if(candidate.value.offset > (~::std::uint64_t{})-tree[0u].byte_offset) { return libcxx_lookup::invalid; }
                auto const absolute{tree[0u].byte_offset+candidate.value.offset};
                for(::std::size_t i{owner.node+1u}; i < tree.size() && tree[i].depth > tree[owner.node].depth; ++i)
                {
                    auto const& node{tree[i]};
                    if(node.parent != owner.node || node.name != item.name || node.type != item.type) { continue; }
                    if(candidate.node != dwarf::no_record || node.detached_copy || node.kind != types[item.type].kind ||
                       node.byte_offset != absolute || node.byte_size != extent || node.bit_field != item.bit_field ||
                       node.inherited != item.inherited || (item.bit_field && node.bit_width != item.bit_size))
                    { return libcxx_lookup::invalid; }
                    candidate.node=i;
                }
                if(candidate.node == dwarf::no_record) { return libcxx_lookup::invalid; }
                if(item.name == name)
                { if(found) { return libcxx_lookup::invalid; } out=candidate; found=true; }
                else
                {
                    if(item.bit_field) { return libcxx_lookup::invalid; }
                    libcxx_string_field nested{};
                    auto const status{libcxx_string_member(types,tree,candidate,name,nested,work,depth+1u)};
                    if(status == libcxx_lookup::invalid) { return status; }
                    if(status == libcxx_lookup::found)
                    { if(found) { return libcxx_lookup::invalid; } out=nested; found=true; }
                }
            }
            if(found && typed_first && (out.value.type!=first.type || out.value.offset!=first.offset)) { return libcxx_lookup::invalid; }
            return found ? libcxx_lookup::found : libcxx_lookup::absent;
        }
        [[nodiscard]] inline bool libcxx_string_field_path(::std::span<dwarf::type_record const> types,
            ::std::vector<dwarf::object_node> const& tree, libcxx_string_field current,
            ::std::string_view path_name, libcxx_string_field& out) noexcept
        {
            ::std::size_t work{};
            while(!path_name.empty())
            {
                auto const dot{path_name.find('.')}; auto const name{path_name.substr(0u,dot)}; libcxx_string_field next{};
                if(name.empty() || libcxx_string_member(types,tree,current,name,next,work) != libcxx_lookup::found) { return false; }
                current=next; if(dot == ::std::string_view::npos) { break; }
                if(current.declaration->bit_field) { return false; } path_name.remove_prefix(dot+1u);
            }
            out=current; return true;
        }
        [[nodiscard]] inline bool libcxx_string_path(::std::span<dwarf::type_record const> types,
            ::std::vector<dwarf::object_node> const& tree, ::std::size_t root,
            ::std::string_view path_name, libcxx_string_field& out) noexcept
        { return libcxx_string_field_path(types,tree,{{root,0u},0u,{}},path_name,out); }
        [[nodiscard]] inline bool libcxx_string_unsigned(::std::span<dwarf::type_record const> types,
            ::std::vector<dwarf::object_node> const& tree, libcxx_string_field selected,
            ::std::span<::std::byte const> bytes, ::std::span<::std::byte const> known, ::std::uint64_t& result) noexcept
        {
            if(selected.declaration == nullptr || selected.value.type >= types.size() || selected.node >= tree.size()) { return false; }
            auto const& type{types[selected.value.type]}; auto const& node{tree[selected.node]}; auto const& member{*selected.declaration};
            if(type.kind != dwarf::type_kind::scalar || !type.size_known || type.byte_size != type.byte_count ||
               (type.byte_count != 1u && type.byte_count != 2u && type.byte_count != 4u && type.byte_count != 8u) ||
               (type.encoding != 2u && type.encoding != 7u && type.encoding != 8u) ||
               node.reason != dwarf::object_unavailable_reason::none || !node.value_available || node.scalar_bytes != type.byte_count ||
               (node.scalar_kind != dwarf::numeric_kind::boolean && node.scalar_kind != dwarf::numeric_kind::unsigned_integer)) { return false; }
            auto const count{member.bit_field ? member.bit_size : type.byte_size*8u}; auto const start{member.bit_field ? member.data_bit_offset%8u : 0u};
            if(count == 0u || count > type.byte_size*8u || selected.value.offset > bytes.size() ||
               (start+count+7u)/8u > bytes.size()-selected.value.offset || (!known.empty() && known.size() != bytes.size())) { return false; }
            result=0u;
            for(::std::uint64_t bit{}; bit != count; ++bit)
            {
                auto const at{static_cast<::std::size_t>(selected.value.offset+(start+bit)/8u)};
                auto const mask{1u << ((start+bit)%8u)};
                if(!known.empty() && (::std::to_integer<unsigned char>(known[at]) & mask) == 0u) { return false; }
                if((::std::to_integer<unsigned char>(bytes[at]) & mask) != 0u) { result |= ::std::uint64_t{1u} << bit; }
            }
            return result == node.bits;
        }
        struct libcxx_string_layout
        {
            libcxx_string_field long_data{},long_size{},long_cap{},short_data{},short_size{},short_flag{};
            ::std::size_t element{dwarf::no_record}; ::std::uint64_t unit{},short_count{}; bool alternate{};
        };
        // Clang emits distinct typedef records for pointer char and the SSO
        // array's value_type. Only their owned final same-CU declaration can
        // prove equivalence; equal names, sizes or encodings are insufficient.
        [[nodiscard]] inline bool libcxx_same_character(::std::span<dwarf::type_record const> types,
            ::std::size_t left,::std::size_t right) noexcept
        {
            if(left>=types.size() || right>=types.size()) { return false; }
            if(left==right) { return true; }
            auto const& a{types[left]};auto const& b{types[right]};
            return a.declaration_identity!=dwarf::die_key{} && a.declaration_identity==b.declaration_identity &&
                a.kind==dwarf::type_kind::scalar && b.kind==dwarf::type_kind::scalar &&
                a.size_known && b.size_known && a.byte_size==b.byte_size && a.byte_count==b.byte_count &&
                a.encoding==b.encoding && a.language==b.language && dwarf::cxx_language(a.language) &&
                !a.atomic_scalar && !b.atomic_scalar && !a.reference_type && !b.reference_type &&
                !a.tinygo_producer && !b.tinygo_producer && !a.zig_producer && !b.zig_producer;
        }
        // One exact schema proof serves metadata-only type queries, value
        // indexing and rendering. No mode bits, Guest addresses or reads here.
        [[nodiscard]] inline bool libcxx_string_schema(::std::span<dwarf::type_record const> types,
            ::std::size_t root,::std::vector<dwarf::object_node> const& tree,unsigned width,libcxx_string_layout& out) noexcept
        {
            if(root>=types.size() || tree.empty() || (width!=4u && width!=8u)) { return false; }
            libcxx_string_field rep{},long_rep{},short_rep{},long_data{},long_size{},long_cap{},long_flag{},short_data{},short_size{},short_flag{};
            bool matched{};
            for(auto const prefix : {::std::string_view{"__rep_.__value_"},::std::string_view{"__rep_"},
                ::std::string_view{"__r_.__value_"},::std::string_view{"__r_.__first_"},::std::string_view{"__r_"}})
            {
                libcxx_string_field candidate{},l{},s{};
                if(!libcxx_string_path(types,tree,root,prefix,candidate) ||
                   !libcxx_string_field_path(types,tree,candidate,"__l",l) ||
                   !libcxx_string_field_path(types,tree,candidate,"__s",s)) { continue; }
                // SDK24 libc++ keeps its anonymous union inside a __rep
                // struct. Bind that exact raw child; equal offsets alone do not
                // identify which union supplies the two alternative records.
                if(types[candidate.value.type].kind != dwarf::type_kind::union_type)
                {
                    auto const owner_kind{types[candidate.value.type].kind};
                    if(owner_kind!=dwarf::type_kind::structure && owner_kind!=dwarf::type_kind::class_type) { return false; }
                    libcxx_string_field wrapped{};::std::size_t work{};
                    if(libcxx_string_member(types,tree,candidate,"",wrapped,work)!=libcxx_lookup::found ||
                       wrapped.declaration==nullptr || wrapped.declaration->inherited || wrapped.declaration->bit_field ||
                       types[wrapped.value.type].kind!=dwarf::type_kind::union_type ||
                       wrapped.value.offset!=candidate.value.offset ||
                       types[wrapped.value.type].byte_size!=types[candidate.value.type].byte_size ||
                       l.node>=tree.size() || s.node>=tree.size() ||
                       tree[l.node].parent!=wrapped.node || tree[s.node].parent!=wrapped.node)
                    { return false; }
                    candidate=wrapped;
                }
                if(matched && candidate.node != rep.node) { return false; }
                matched=true; rep=candidate; long_rep=l; short_rep=s;
                auto select{[&](::std::string_view suffix,libcxx_string_field& selected)
                { return libcxx_string_field_path(types,tree,candidate,suffix,selected); }};
                if(!select("__l.__data_",long_data) || !select("__l.__size_",long_size) ||
                   !select("__l.__cap_",long_cap) || !select("__l.__is_long_",long_flag) ||
                   !select("__s.__data_",short_data) || !select("__s.__size_",short_size) || !select("__s.__is_long_",short_flag))
                { return false; }
            }
            if(!matched || types[rep.value.type].kind != dwarf::type_kind::union_type ||
               types[rep.value.type].byte_size != width*3u || types[long_rep.value.type].byte_size != width*3u ||
               types[short_rep.value.type].byte_size != width*3u || long_rep.value.offset != rep.value.offset || short_rep.value.offset != rep.value.offset ||
               !pointer(types,long_data.value,width) || !integer(types,long_size.value,width) || long_size.declaration->bit_field ||
               !long_cap.declaration->bit_field || long_cap.declaration->bit_size != width*8u-1u ||
               !long_flag.declaration->bit_field || long_flag.declaration->bit_size != 1u ||
               !short_flag.declaration->bit_field || short_flag.declaration->bit_size != 1u ||
               !short_size.declaration->bit_field || short_size.declaration->bit_size != 7u ||
               types[short_size.value.type].byte_size != 1u || types[long_cap.value.type].byte_size != width)
            { return false; }
            auto const& array{types[short_data.value.type]};
            if(array.kind != dwarf::type_kind::array || !array.size_known || !array.contiguous_array || !array.row_major_array ||
               array.dimensions.size() != 1u || array.referenced_type >= types.size() ||
               !libcxx_same_character(types,types[long_data.value.type].referenced_type,array.referenced_type) || !array.dimensions[0u].count_known ||
               !array.dimensions[0u].lower_bound_known || array.dimensions[0u].lower_bound != 0 || array.dimensions[0u].count == 0u)
            { return false; }
            auto const& element{types[array.referenced_type]}; auto const unit{element.byte_size};
            if(element.kind != dwarf::type_kind::scalar || !element.size_known || element.byte_count != unit ||
               (unit != 1u && unit != 2u && unit != 4u) ||
               (element.encoding != 5u && element.encoding != 6u && element.encoding != 7u && element.encoding != 8u && element.encoding != 0x10u) ||
               array.dimensions[0u].count > array.byte_size/unit || array.dimensions[0u].count*unit != array.byte_size ||
               short_data.declaration->bit_field || short_data.value.offset < rep.value.offset)
            { return false; }
            bool const alternate{long_data.value.offset == rep.value.offset};
            auto const control_offset{rep.value.offset+(alternate ? width*3u-1u : 0u)};
            if(long_size.value.offset != rep.value.offset+width ||
               long_cap.value.offset != rep.value.offset+(alternate ? width*2u : 0u) ||
               long_flag.value.offset != control_offset || short_flag.value.offset != control_offset || short_size.value.offset != control_offset ||
               (alternate ?
                   (long_cap.declaration->data_bit_offset != width*16u || long_flag.declaration->data_bit_offset != width*24u-1u ||
                    short_flag.declaration->data_bit_offset != width*24u-1u || short_size.declaration->data_bit_offset != (width*3u-1u)*8u ||
                    short_data.value.offset != rep.value.offset) :
                   (long_data.value.offset != rep.value.offset+width*2u || long_cap.declaration->data_bit_offset != 1u ||
                    long_flag.declaration->data_bit_offset != 0u || short_flag.declaration->data_bit_offset != 0u ||
                    short_size.declaration->data_bit_offset != 1u || short_data.value.offset != rep.value.offset+unit)))
            { return false; }
            out={long_data,long_size,long_cap,short_data,short_size,short_flag,array.referenced_type,unit,array.dimensions[0u].count,alternate};
            return true;
        }
        [[nodiscard]] inline bool libcxx_string_extent(::std::span<dwarf::type_record const> types,
            ::std::vector<dwarf::object_node> const& tree,libcxx_string_layout const& layout,
            ::std::span<::std::byte const> bytes,::std::span<::std::byte const> known,
            ::std::uint64_t& length,bool& long_storage) noexcept
        {
            ::std::uint64_t mode{};
            if(!libcxx_string_unsigned(types,tree,layout.short_flag,bytes,known,mode) || mode>1u) { return false; }
            long_storage=mode!=0u;
            if(long_storage)
            {
                ::std::uint64_t capacity{};
                if(!libcxx_string_unsigned(types,tree,layout.long_size,bytes,known,length) ||
                   !libcxx_string_unsigned(types,tree,layout.long_cap,bytes,known,capacity)) { return false; }
                if(!layout.alternate)
                { if(capacity>(~::std::uint64_t{})/2u) { return false; } capacity*=2u; }
                return capacity!=0u && length<capacity;
            }
            return libcxx_string_unsigned(types,tree,layout.short_size,bytes,known,length) && length<layout.short_count &&
                layout.short_data.value.offset<=bytes.size() && length<=(bytes.size()-layout.short_data.value.offset)/layout.unit;
        }
        [[nodiscard]] inline bool static_span_schema(::std::span<dwarf::type_record const> types,::std::size_t root,
            unsigned width,field& data,::std::size_t& element,::std::uint64_t& count) noexcept
        {
            if(root>=types.size() || !family(types[root],"span") || (width!=4u && width!=8u) ||
               !sequence_details::template_count(types,root,1u,width,count) ||
               count==dwarf::numeric_mask(static_cast<::std::uint8_t>(width))) { return false; }
            auto const& type{types[root]};
            if(type.template_types.empty() || type.template_types[0u]>=types.size() ||
               !any_path(types,root,{"_M_ptr","__data_"},data) || !pointer(types,data,width)) { return false; }
            element=types[data.type].referenced_type;auto const& value{types[element]};
            return element==type.template_types[0u] && value.size_known && value.byte_size!=0u && value.byte_size<=65536u;
        }
        // Counted libc++ vector layout: derive every field offset and type
        // from the owned DWARF layout. No release/sizeof-dependent offsets.
        [[nodiscard]] inline bool counted_vector_schema(::std::span<dwarf::type_record const> types,::std::size_t root,
            unsigned width,field& data,field& size,field& capacity,::std::size_t& element) noexcept
        {
            if constexpr (!(UWVM_DEBUG_LANGUAGE_CPP)) { return false; }
            if(root>=types.size() || (width!=4u && width!=8u) || !family(types[root],"vector")) { return false; }
            auto const& owner{types[root]};field layout{};
            if(!owner.size_known || owner.atomic_scalar || owner.template_types.size()<2u || !path(types,root,"__layout_",layout) ||
               !family(types[layout.type],"__vector_layout") || types[layout.type].atomic_scalar) { return false; }
            auto const& schema{types[layout.type]};
            if(schema.template_types.size()<2u || owner.template_types[1u]>=types.size() || schema.template_types[1u]>=types.size() ||
               !dwarf::same_cpp_value_type(types,owner.template_types[0u],schema.template_types[0u])) { return false; }
            auto const allocator{owner.template_types[1u]},storage_allocator{schema.template_types[1u]};
            if(allocator!=storage_allocator && (types[allocator].declaration_identity==dwarf::die_key{} ||
               types[allocator].declaration_identity!=types[storage_allocator].declaration_identity)) { return false; }
            if(!path(types,root,"__layout_.__begin_",data) || !path(types,root,"__layout_.__size_",size) ||
               !path(types,root,"__layout_.__capacity_",capacity) || !pointer(types,data,width) ||
               !integer(types,size,width) || !integer(types,capacity,width)) { return false; }
            auto const& count{types[size.type]};auto const& cap{types[capacity.type]};
            if(types[data.type].atomic_scalar || count.atomic_scalar || cap.atomic_scalar ||
               (count.encoding!=7u && count.encoding!=8u) || (cap.encoding!=7u && cap.encoding!=8u)) { return false; }
            element=types[data.type].referenced_type;auto const& value{types[element]};
            if(!dwarf::same_cpp_value_type(types,element,owner.template_types[0u]) || value.atomic_scalar ||
               value.byte_size==0u || value.byte_size>65536u) { return false; }
            field const fields[]{data,size,capacity};
            for(auto const& f:fields)
            { if(f.offset>owner.byte_size || width>owner.byte_size-f.offset) { return false; } }
            for(unsigned i{};i!=3u;++i)for(unsigned j{i+1u};j!=3u;++j)
            { if(fields[i].offset<fields[j].offset+width && fields[j].offset<fields[i].offset+width) { return false; } }
            return true;
        }
#if UWVM_DEBUG_LANGUAGE_RUST
        struct rust_sequence_layout
        {
            field data{},length{},capacity{};
            ::std::size_t element{dwarf::no_record};
            bool string{},zero_sized{};
        };
        [[nodiscard]] inline bool rust_family(dwarf::type_record const& type,::std::string_view family_name) noexcept
        {
            if(type.language!=0x1cu || (type.kind!=dwarf::type_kind::structure && type.kind!=dwarf::type_kind::class_type)) { return false; }
            ::std::string_view name{type.aggregate_name.empty()?type.name:type.aggregate_name};
            auto const angle{name.find('<')};auto const outer{name.substr(0u,angle)};
            auto const colon{outer.rfind("::")};auto const leaf{colon==outer.npos?outer:outer.substr(colon+2u)};
            return leaf==family_name && (angle==name.npos || name.back()=='>');
        }
        [[nodiscard]] inline bool rust_same_type(::std::span<dwarf::type_record const> types,::std::size_t a,::std::size_t b) noexcept
        {
            if(a>=types.size() || b>=types.size()) { return false; }
            auto const& x{types[a]};auto const& y{types[b]};
            return (a==b || (x.declaration_identity!=dwarf::die_key{} && x.declaration_identity==y.declaration_identity)) &&
                x.language==0x1cu && y.language==0x1cu && x.kind!=dwarf::type_kind::unavailable && x.kind==y.kind &&
                x.size_known && y.size_known && x.byte_size==y.byte_size && x.byte_count==y.byte_count &&
                x.encoding==y.encoding && x.atomic_scalar==y.atomic_scalar && x.rust_unit_type==y.rust_unit_type;
        }
        [[nodiscard]] inline bool rust_byte(dwarf::type_record const& type) noexcept
        {
            return type.language==0x1cu && type.kind==dwarf::type_kind::scalar && type.size_known &&
                type.byte_size==1u && type.byte_count==1u && type.encoding==7u && !type.atomic_scalar;
        }
        struct rust_shared_layout
        {
            field data{},strong{},weak{},value{};
            ::std::size_t inner{dwarf::no_record};
            ::std::size_t header_bytes{};
            bool atomic{},weak_owner{};
        };
        // Rc/Arc are structural profiles of the actual owned Rust DWARF.
        // In particular, neither the compiler release nor the host ABI selects
        // the allocation offsets. Unsized/fat-pointer layouts fail this proof.
        [[nodiscard]] inline bool rust_shared_schema(::std::span<dwarf::type_record const> types,
            ::std::size_t root,unsigned width,rust_shared_layout& out) noexcept
        {
            out={};
            if(root>=types.size() || (width!=4u && width!=8u)) { return false; }
            auto const& owner{types[root]};bool const weak_owner{rust_family(owner,"Weak")};
            if((!weak_owner && !rust_family(owner,"Rc") && !rust_family(owner,"Arc")) ||
               !owner.size_known || owner.atomic_scalar || owner.template_types.empty()) { return false; }
            field carrier{},data{};
            if(!path(types,root,"ptr",carrier) || !rust_family(types[carrier.type],"NonNull") ||
               types[carrier.type].atomic_scalar || types[carrier.type].template_types.empty() ||
               !path(types,root,"ptr.pointer",data) || !pointer(types,data,width) || types[data.type].atomic_scalar) { return false; }
            auto const inner{types[data.type].referenced_type};auto const& allocation{types[inner]};
            bool const atomic{rust_family(allocation,"ArcInner")};
            if((!atomic && !rust_family(allocation,"RcInner")) ||
               (!weak_owner && atomic!=rust_family(owner,"Arc")) || !allocation.size_known || allocation.atomic_scalar ||
               allocation.template_types.empty() || !rust_same_type(types,inner,types[carrier.type].template_types[0u])) { return false; }
            field strong{},weak{},value{};
            if(!path(types,inner,atomic?"strong.v.value":"strong.value.value",strong) ||
               !path(types,inner,atomic?"weak.v.value":"weak.value.value",weak) ||
               !path(types,inner,atomic?"data":"value",value) ||
               !rust_same_type(types,owner.template_types[0u],value.type) ||
               !rust_same_type(types,allocation.template_types[0u],value.type)) { return false; }
            auto const counter{[&](::std::string_view name,field terminal)
            {
                field wrapper{},cell{};
                if(!path(types,inner,name,wrapper) || !rust_family(types[wrapper.type],atomic?"AtomicUsize":"Cell") ||
                   types[wrapper.type].atomic_scalar || !member(types,wrapper,atomic?"v":"value",cell) ||
                   !rust_family(types[cell.type],"UnsafeCell") || types[cell.type].atomic_scalar ||
                   !integer(types,terminal,width)) { return false; }
                auto const& scalar{types[terminal.type]};
                return scalar.language==0x1cu && scalar.name=="usize" && scalar.encoding==7u && !scalar.atomic_scalar &&
                    !types[cell.type].template_types.empty() &&
                    rust_same_type(types,terminal.type,types[cell.type].template_types[0u]) &&
                    (atomic || (!types[wrapper.type].template_types.empty() &&
                        rust_same_type(types,terminal.type,types[wrapper.type].template_types[0u])));
            }};
            if(!counter("strong",strong) || !counter("weak",weak) ||
               (strong.offset<weak.offset+width && weak.offset<strong.offset+width)) { return false; }
            auto const& target{types[value.type]};
            auto const header{(std::max)(strong.offset+width,weak.offset+width)};
            // Read the control prefix first. An expired Weak must never copy
            // the already-destroyed T, even when it happens to remain mapped.
            if(target.kind==dwarf::type_kind::unavailable || target.atomic_scalar || !target.size_known ||
               target.byte_size>65536u || header>65536u || value.offset<header ||
               value.offset>allocation.byte_size || target.byte_size>allocation.byte_size-value.offset) { return false; }
            out={data,strong,weak,value,inner,static_cast<::std::size_t>(header),atomic,weak_owner};return true;
        }
        [[nodiscard]] inline bool rust_shared_address(::std::span<dwarf::type_record const> types,
            rust_shared_layout const& layout,::std::span<::std::byte const> bytes,::std::span<::std::byte const> known,
            ::std::span<::std::byte const> eligible,unsigned width,::std::uint64_t& address,bool& empty) noexcept
        {
            empty=false;address=0u;
            if(layout.inner>=types.size() || (width!=4u && width!=8u) ||
               !bits(types,layout.data,bytes,known,address) || (!eligible.empty() && !bits(types,layout.data,bytes,eligible,address))) { return false; }
            auto const maximum{width==4u?0xffffffffull:~::std::uint64_t{}};
            if(layout.weak_owner && address==maximum) { empty=true;return true; } // Rust Weak::new's dangling sentinel.
            auto const extent{types[layout.inner].byte_size};
            return address!=0u && address<=maximum && extent!=0u && extent-1u<=maximum-address;
        }
        [[nodiscard]] inline bool rust_shared_counts(::std::span<dwarf::type_record const> types,
            rust_shared_layout const& layout,::std::span<::std::byte const> header,unsigned width,
            ::std::uint64_t& strong,::std::uint64_t& weak) noexcept
        {
            ::std::uint64_t stored{};
            if((width!=4u && width!=8u) || !bits(types,layout.strong,header,{},strong) ||
               !bits(types,layout.weak,header,{},stored) || stored==0u || (!layout.weak_owner && strong==0u) ||
               (layout.atomic && stored==(width==4u?0xffffffffull:~::std::uint64_t{}))) { return false; }
            // std's public weak_count excludes the implicit weak held by the
            // live strong cohort; expired Weak::weak_count returns zero.
            weak=strong!=0u?stored-1u:0u;return true;
        }
        [[nodiscard]] inline bool rust_buffer_schema(::std::span<dwarf::type_record const> types,field owner,
            unsigned width,rust_sequence_layout& out) noexcept
        {
            if(owner.type>=types.size() || (!rust_family(types[owner.type],"Vec") &&
               !rust_family(types[owner.type],"VecDeque"))) { return false; }
            auto const& vec{types[owner.type]};field data{},length{},capacity{};
            bool const erased{path(types,owner.type,"buf.inner.ptr.pointer.pointer",data)};
            if(!erased && !any_path(types,owner.type,{"buf.ptr.pointer.pointer","buf.ptr.pointer"},data)) { return false; }
            if(!path(types,owner.type,"len",length) ||
               !any_path(types,owner.type,{"buf.inner.cap.__0","buf.inner.cap","buf.cap.__0","buf.cap"},capacity) ||
               !pointer(types,data,width) || !integer(types,length,width) || !integer(types,capacity,width)) { return false; }
            auto const storage{types[data.type].referenced_type};auto element{storage};
            if(!vec.template_types.empty())
            {
                element=vec.template_types[0u];field buffer{};
                if(element>=types.size() || !path(types,owner.type,"buf",buffer) ||
                   !rust_family(types[buffer.type],"RawVec") || types[buffer.type].template_types.empty() ||
                   !rust_same_type(types,element,types[buffer.type].template_types[0u])) { return false; }
                if(!rust_same_type(types,element,storage) && (!erased || !rust_byte(types[storage]))) { return false; }
            }
            else if(erased) { return false; } // An erased pointer never proves T.
            if(element>=types.size() || types[element].kind==dwarf::type_kind::unavailable ||
               types[element].language!=0x1cu || !types[element].size_known || types[element].byte_size>65536u) { return false; }
            field fields[]{data,length,capacity};
            for(unsigned i{};i!=3u;++i)
            {
                if(fields[i].offset>vec.byte_size || width>vec.byte_size-fields[i].offset ||
                   fields[i].offset>(~::std::uint64_t{})-owner.offset) { return false; }
                for(unsigned j{i+1u};j!=3u;++j)
                { if(fields[i].offset<fields[j].offset+width && fields[j].offset<fields[i].offset+width) { return false; } }
                fields[i].offset+=owner.offset;
            }
            out={fields[0],fields[1],fields[2],element,false,types[element].byte_size==0u};return true;
        }
        [[nodiscard]] inline bool rust_vector_schema(::std::span<dwarf::type_record const> types,field owner,
            unsigned width,rust_sequence_layout& out) noexcept
        {
            return owner.type<types.size() && rust_family(types[owner.type],"Vec") &&
                rust_buffer_schema(types,owner,width,out);
        }
        struct rust_deque_layout { rust_sequence_layout buffer{};field head{}; };
        [[nodiscard]] inline bool rust_deque_schema(::std::span<dwarf::type_record const> types,::std::size_t root,
            unsigned width,rust_deque_layout& out) noexcept
        {
            out={};
            if(root>=types.size() || (width!=4u && width!=8u) || !rust_family(types[root],"VecDeque") ||
               types[root].atomic_scalar) { return false; }
            rust_sequence_layout buffer{};field head{};
            if(!rust_buffer_schema(types,{root,0u},width,buffer) || !path(types,root,"head",head) ||
               !integer(types,head,width) || types[buffer.data.type].atomic_scalar ||
               types[buffer.element].atomic_scalar) { return false; }
            field const fields[]{buffer.data,buffer.length,buffer.capacity,head};
            for(unsigned i{};i!=4u;++i)
            {
                auto const& f{fields[i]};auto const& t{types[f.type]};
                if(t.language!=0x1cu || t.atomic_scalar || (i!=0u && t.encoding!=7u && t.encoding!=8u) ||
                   f.offset>types[root].byte_size || width>types[root].byte_size-f.offset) { return false; }
                for(unsigned j{i+1u};j!=4u;++j)
                { if(f.offset<fields[j].offset+width && fields[j].offset<f.offset+width) { return false; } }
            }
            out={buffer,head};return true;
        }
        struct rust_deque_extent
        {
            indexed_sequence sequence{};::std::uint64_t capacity{};bool zero_sized{};
        };
        [[nodiscard]] inline bool rust_deque_plan(::std::span<dwarf::type_record const> types,::std::size_t root,
            ::std::span<::std::byte const> bytes,::std::span<::std::byte const> known,
            ::std::span<::std::byte const> eligible,unsigned width,rust_deque_extent& out) noexcept
        {
            out={};rust_deque_layout layout{};
            if(!rust_deque_schema(types,root,width,layout)) { return false; }
            auto const& buffer{layout.buffer};::std::uint64_t address{},count{},capacity{},head{},discarded{};
            if(!bits(types,buffer.data,bytes,known,address) ||
               (!eligible.empty() && (!bits(types,buffer.data,bytes,eligible,discarded) || discarded!=address)) ||
               !bits(types,buffer.length,bytes,known,count) || !bits(types,buffer.capacity,bytes,known,capacity) ||
               !bits(types,layout.head,bytes,known,head)) { return false; }
            auto const maximum{width==4u ? 0xffffffffull : ~::std::uint64_t{}};
            if(buffer.zero_sized) { capacity=maximum; }
            if(count>capacity || (capacity==0u ? head!=0u : head>=capacity)) { return false; }
            auto const extent{types[buffer.element].byte_size};
            // Validate the entire allocation, including the wrapped-away part,
            // before issuing a single bounded read of the displayed prefix.
            if(capacity!=0u && (address==0u || address>maximum || (!buffer.zero_sized &&
               (extent-1u>maximum-address || capacity-1u>(maximum-address-(extent-1u))/extent)))) { return false; }
            out.sequence={buffer.element,address,count,buffer.zero_sized,0u,
                buffer.zero_sized ? 0u : capacity,buffer.zero_sized ? 0u : head};
            out.capacity=capacity;out.zero_sized=buffer.zero_sized;return true;
        }
        [[nodiscard]] inline bool rust_sequence_schema(::std::span<dwarf::type_record const> types,::std::size_t root,
            unsigned width,rust_sequence_layout& out) noexcept
        {
            out={};if(root>=types.size() || (width!=4u && width!=8u)) { return false; }
            if(rust_family(types[root],"Vec")) { return rust_vector_schema(types,{root,0u},width,out); }
            if(rust_family(types[root],"String"))
            {
                field vec{};if(!path(types,root,"vec",vec) || !rust_vector_schema(types,vec,width,out) ||
                    !rust_byte(types[out.element])) { return false; }
                out.string=true;return true;
            }
            auto const& type{types[root]};
            if(type.language!=0x1cu || (type.kind!=dwarf::type_kind::structure && type.kind!=dwarf::type_kind::class_type)) { return false; }
            ::std::string_view name{type.aggregate_name.empty()?type.name:type.aggregate_name};
            bool const string{name=="&str" || name=="&mut str"};
            if(!string && !((name.starts_with("&[") || name.starts_with("&mut [")) && name.ends_with("]"))) { return false; }
            field data{},length{};
            if(!path(types,root,"data_ptr",data) || !path(types,root,"length",length) ||
               !pointer(types,data,width) || !integer(types,length,width) ||
               (data.offset<length.offset+width && length.offset<data.offset+width)) { return false; }
            auto const element{types[data.type].referenced_type};auto const& value{types[element]};
            if(value.language!=0x1cu || value.kind==dwarf::type_kind::unavailable || !value.size_known ||
               value.byte_size>65536u || (string && !rust_byte(value))) { return false; }
            out={data,length,{},element,string,value.byte_size==0u};return true;
        }
#endif
        [[nodiscard]] inline bool index_element_type(::std::span<dwarf::type_record const> types,::std::size_t root,
            unsigned width,::std::size_t& element,::std::uint64_t& count,bool& bounded) noexcept
        {
            if(root>=types.size()) { return false; }field data{};
#if UWVM_DEBUG_LANGUAGE_RUST
            rust_deque_layout deque{};
            if(rust_deque_schema(types,root,width,deque))
            { element=deque.buffer.element;count=0u;bounded=false;return true; }
            rust_sequence_layout rust{};
            if(rust_sequence_schema(types,root,width,rust))
            { element=rust.element;count=0u;bounded=false;return true; }
#endif
            if(static_span_schema(types,root,width,data,element,count)) { bounded=true;return true; }
            field length{},capacity{};
            if(counted_vector_schema(types,root,width,data,length,capacity,element)) { count=0u;bounded=false;return true; }
            if((!family(types[root],"basic_string") && !family(types[root],"string")) ||
               !any_path(types,root,{"__r_","__rep_"},data)) { return false; }
            ::std::vector<dwarf::object_node> layout{};libcxx_string_layout schema{};
            if(dwarf::query_type_layout(types,root,layout)!=dwarf::inline_query_error::none ||
               !libcxx_string_schema(types,root,layout,width,schema)) { return false; }
            element=schema.element;count=0u;bounded=false;return true;
        }
        // An operator[] plan consists solely of scalar guest offsets and owned
        // metadata. Applying it still requires the caller's genuine read policy.
        [[nodiscard]] inline bool index_plan(::std::span<dwarf::type_record const> types, ::std::size_t root,
            ::std::span<::std::byte const> bytes, ::std::span<::std::byte const> known,
            ::std::span<::std::byte const> pointer_known, unsigned width, indexed_sequence& result) noexcept
        {
            result = {}; if(root >= types.size() || (width != 4u && width != 8u)) { return false; }
            auto const& type{types[root]}; field first{},length{},end{},capacity{}; bool endpoints{},counted_vector{};
            ::std::size_t selected_element{dwarf::no_record};bool rust_zero_sized{};
#if UWVM_DEBUG_LANGUAGE_RUST
            if(rust_family(type,"VecDeque"))
            {
                rust_deque_extent deque{};
                if(!rust_deque_plan(types,root,bytes,known,pointer_known,width,deque)) { return false; }
                result=deque.sequence;return true;
            }
#endif
            ::std::size_t span_element{};::std::uint64_t span_count{};
            if(static_span_schema(types,root,width,first,span_element,span_count))
            {
                auto const& value{types[span_element]};::std::uint64_t address{},discarded{};
                if(!bits(types,first,bytes,known,address) ||
                   (!pointer_known.empty() && (!bits(types,first,bytes,pointer_known,discarded) || address!=discarded))) { return false; }
                auto const maximum{width==4u ? 0xffffffffull : ~::std::uint64_t{}};
                if(span_count!=0u && (address==0u || address>maximum || value.byte_size-1u>maximum-address ||
                    span_count-1u>(maximum-address-(value.byte_size-1u))/value.byte_size)) { return false; }
                result={span_element,address,span_count};return true;
            }
            if((family(type,"basic_string") || family(type,"string")) && any_path(types,root,{"__r_","__rep_"},first))
            {
                ::std::vector<dwarf::object_node> tree{};libcxx_string_layout layout{};
                auto const copied{known.empty() ? dwarf::query_object_value(types,root,bytes,tree) :
                    dwarf::query_object_value_with_known_bits(types,root,bytes,known,tree)};
                if(copied==dwarf::inline_query_error::none && libcxx_string_schema(types,root,tree,width,layout))
                {
                    ::std::uint64_t count{};bool long_storage{};
                    if(!libcxx_string_extent(types,tree,layout,bytes,known,count,long_storage)) { return false; }
                    if(!long_storage)
                    { result={layout.element,0u,count,true,layout.short_data.value.offset};return true; }
                    ::std::uint64_t address{},discarded{};
                    if(!bits(types,layout.long_data.value,bytes,known,address) ||
                       (!pointer_known.empty() && (!bits(types,layout.long_data.value,bytes,pointer_known,discarded) || address!=discarded))) { return false; }
                    auto const maximum{width==4u ? 0xffffffffull : ~::std::uint64_t{}};
                    if(count!=0u && (address==0u || address>maximum || layout.unit-1u>maximum-address ||
                        count-1u>(maximum-address-(layout.unit-1u))/layout.unit)) { return false; }
                    result={layout.element,address,count};return true;
                }
            }
            if(family(type,"vector"))
            {
                ::std::size_t counted_element{};
                counted_vector=counted_vector_schema(types,root,width,first,length,capacity,counted_element);
                if(!counted_vector)
                {
                    endpoints=true;
                    if(!(any_path(types,root,{"_M_impl._M_start","__begin_","__layout_.__begin_"},first) &&
                         any_path(types,root,{"_M_impl._M_finish","__end_","__layout_.__end_"},end) &&
                         vector_capacity(types,root,width,capacity))) { return false; }
                }
            }
            else if(family(type,"span") || family(type,"basic_string_view") || family(type,"string_view") || family(type,"basic_string") || family(type,"string"))
            {
                if(!(any_path(types,root,{"_M_dataplus._M_p","_M_str","_M_ptr","__data_"},first) &&
                     any_path(types,root,{"_M_string_length","_M_len","_M_extent._M_extent_value","_M_extent","__size_"},length))) { return false; }
            }
#if UWVM_DEBUG_LANGUAGE_RUST
            else if(type.language == 0x1cu)
            {
                rust_sequence_layout rust{};
                if(!rust_sequence_schema(types,root,width,rust)) { return false; }
                first=rust.data;length=rust.length;capacity=rust.capacity;selected_element=rust.element;rust_zero_sized=rust.zero_sized;
            }
#endif
            else { return false; }
            if(!pointer(types,first,width)) { return false; }
            auto const target{selected_element==dwarf::no_record?types[first.type].referenced_type:selected_element}; auto const& element{types[target]};
            if(!element.size_known || (element.byte_size == 0u && !rust_zero_sized) || element.byte_size > 65536u) { return false; }
            auto pointer_mask{pointer_known.empty() ? known : pointer_known};
            if(!known.empty() && !pointer_known.empty())
            {
                if(known.size() != bytes.size() || pointer_known.size() != bytes.size()) { return false; }
                // No allocation needed: require both masks over the pointer
                // fields before interpreting either field's scalar bits.
                auto const proven{[&](field f)
                { ::std::uint64_t discarded{}; return bits(types,f,bytes,known,discarded) && bits(types,f,bytes,pointer_known,discarded); }};
                if(!proven(first) || (endpoints && (!proven(end) || !proven(capacity)))) { return false; }
            }
            ::std::uint64_t address{},count{},last{},cap{};
            if(!bits(types,first,bytes,pointer_mask,address)) { return false; }
            if(endpoints)
            {
                if(!pointer(types,end,width) || !pointer(types,capacity,width) || types[end.type].referenced_type != target ||
                   types[capacity.type].referenced_type != target || !bits(types,end,bytes,pointer_mask,last) ||
                   !bits(types,capacity,bytes,pointer_mask,cap) || address > last || last > cap ||
                   (last-address)%element.byte_size != 0u || (cap-address)%element.byte_size != 0u ||
                   (address == 0u && (last != 0u || cap != 0u))) { return false; }
                count = (last-address)/element.byte_size;
            }
            else
            {
                if(!integer(types,length,width) || !bits(types,length,bytes,known,count)) { return false; }
                if(capacity.type != dwarf::no_record && (!integer(types,capacity,width) || !bits(types,capacity,bytes,known,cap) || (!rust_zero_sized && count > cap))) { return false; }
            }
            auto const maximum{width == 4u ? 0xffffffffull : ~::std::uint64_t{}};
            if(rust_zero_sized)
            {
                if(count!=0u && (address==0u || address>maximum)) { return false; }
                // The selected value has no bytes. It is already represented by
                // an empty subspan of the authenticated copied descriptor.
                result={target,0u,count,true,0u};return true;
            }
            if(count != 0u && (address == 0u || address > maximum || element.byte_size-1u > maximum-address ||
                count-1u > (maximum-address-(element.byte_size-1u))/element.byte_size)) { return false; }
            if(counted_vector && cap!=0u && (address==0u || element.byte_size-1u>maximum-address ||
               cap-1u>(maximum-address-(element.byte_size-1u))/element.byte_size)) { return false; }
            result = {target,address,count}; return true;
        }
        template<typename Session>
        [[nodiscard]] outcome extended_sequences(Session&, ::std::size_t, ::std::span<::std::byte const>,
            ::std::span<::std::byte const>, ::std::span<::std::byte const>, ::std::vector<dwarf::object_node>&, ::std::size_t);
        template<typename Session>
        [[nodiscard]] outcome extended_values(Session&, ::std::size_t, ::std::span<::std::byte const>,
            ::std::span<::std::byte const>, ::std::span<::std::byte const>, ::std::vector<dwarf::object_node>&, ::std::size_t);
        template<typename Session>
        [[nodiscard]] outcome extended_libcxx_nodes(Session&, ::std::size_t, ::std::span<::std::byte const>,
            ::std::span<::std::byte const>, ::std::span<::std::byte const>, ::std::vector<dwarf::object_node>&, ::std::size_t);
        template<typename Session>
        [[nodiscard]] outcome extended_array(Session&, ::std::size_t, ::std::span<::std::byte const>,
            ::std::span<::std::byte const>, ::std::span<::std::byte const>, ::std::vector<dwarf::object_node>&, ::std::size_t);
        template<typename Reader> struct session
        {
            ::std::span<dwarf::type_record const> types;
            unsigned width;
            Reader& reader;
            limits cap;
            ::std::size_t reads{}, copied{}, elements{}, nodes{}, text_bytes{};
            bool limited{};
            [[nodiscard]] bool read(::std::uint64_t address, ::std::size_t count, ::std::vector<::std::byte>& bytes)
            {
                bytes.clear(); auto const maximum{width == 4u ? 0xffffffffull : ~::std::uint64_t{}};
                if(count == 0u) { return true; }
                if(address == 0u || address > maximum || count-1u > maximum-address) { return false; }
                if(reads >= cap.maximum_reads || copied > cap.max_bytes || count > cap.max_bytes-copied)
                { limited = true; return false; }
                ++reads; copied += count;
                return reader(address,count,bytes) && bytes.size() == count;
            }
            [[nodiscard]] bool append(::std::vector<dwarf::object_node>& tree, ::std::size_t type,
                ::std::span<::std::byte const> bytes, ::std::string name, ::std::size_t depth,
                ::std::span<::std::byte const> known = {}, ::std::span<::std::byte const> eligible = {})
            {
                if((!known.empty() && known.size() != bytes.size()) || (!eligible.empty() && eligible.size() != bytes.size())) { return false; }
                if(type >= types.size() || nodes >= cap.max_nodes || depth > cap.max_depth) { limited = true; return false; }
                dwarf::object_query_limits object_cap{};
                object_cap.max_results = cap.max_nodes-nodes;
                object_cap.max_array_elements = elements < cap.max_elements ? cap.max_elements-elements : 0u;
                object_cap.max_depth = (std::min)(::std::size_t{24u},cap.max_depth-depth+1u);
                // Validate immutable metadata once at format entry. Repeating a
                // whole CU graph scan for every vector element makes printing
                // quadratic in DWARF size; builder walks only the selected type.
                dwarf::object_details::builder builder{types,bytes,{},object_cap,true};
                builder.known_bits = known;
                if(!builder.visit(type,0u,bytes.size(),dwarf::no_record,0u,{})) { limited = true; return false; }
                auto child{::std::move(builder.nodes)};
                if(child.empty()) { return false; }
                nodes += child.size();
                if(depth < cap.max_depth) { (void)format_owned(type,bytes,known,eligible,child,depth+1u); }
                if(child.size() > cap.max_nodes-tree.size()) { limited = true; return false; }
                auto const base{tree.size()};
                child[0u].name = ::std::move(name); child[0u].detached_copy = true;
                for(::std::size_t i{}; i != child.size(); ++i)
                {
                    auto& n{child[i]};
                    if(i == 0u) { n.parent = 0u; }
                    else if(n.parent != dwarf::no_record) { n.parent += base; }
                    ++n.depth; tree.push_back(::std::move(n));
                }
                return true;
            }
            [[nodiscard]] outcome sequence(::std::size_t, ::std::span<::std::byte const> bytes,
                ::std::span<::std::byte const> known, ::std::span<::std::byte const> pointer_known,
                ::std::vector<dwarf::object_node>& tree, ::std::size_t depth,
                field start, field size, field finish, field capacity, bool endpoints, bool string, ::std::string_view profile,
                ::std::size_t selected_element=dwarf::no_record,bool zero_sized=false)
            {
                if(!pointer(types,start,width)) { return outcome::unavailable; }
                if(!known.empty() && !pointer_known.empty())
                {
                    auto const proven{[&](field value)
                    { ::std::uint64_t discarded{};return bits(types,value,bytes,known,discarded) && bits(types,value,bytes,pointer_known,discarded); }};
                    if(!proven(start) || (endpoints && (!proven(finish) || !proven(capacity)))) { return outcome::unavailable; }
                }
                ::std::uint64_t address{}, count{}, end{}, allocated{};
                if(!bits(types,start,bytes,pointer_known.empty() ? known : pointer_known,address)) { return outcome::unavailable; }
                auto const target{selected_element==dwarf::no_record?types[start.type].referenced_type:selected_element};
                if(target>=types.size()) { return outcome::unavailable; }auto const& element{types[target]};
                if(!element.size_known || (element.byte_size == 0u && (!zero_sized || string)) || element.byte_size > cap.max_bytes ||
                   (string && (element.kind != dwarf::type_kind::scalar ||
                    (element.byte_size != 1u && element.byte_size != 2u && element.byte_size != 4u)))) { return outcome::unavailable; }
                auto const extent{element.byte_size};
                if(endpoints)
                {
                    if(!pointer(types,finish,width) || !pointer(types,capacity,width) ||
                       types[finish.type].referenced_type != target || types[capacity.type].referenced_type != target ||
                       !bits(types,finish,bytes,pointer_known.empty() ? known : pointer_known,end) ||
                       !bits(types,capacity,bytes,pointer_known.empty() ? known : pointer_known,allocated) ||
                       end < address || allocated < end || (end-address)%extent != 0u || (allocated-address)%extent != 0u)
                    { return outcome::unavailable; }
                    if(address == 0u && (end != 0u || allocated != 0u)) { return outcome::unavailable; }
                    count = (end-address)/extent; allocated = (allocated-address)/extent;
                }
                else
                {
                    if(!integer(types,size,width) || !bits(types,size,bytes,known,count)) { return outcome::unavailable; }
                    if(capacity.type != dwarf::no_record)
                    {
                        if(!integer(types,capacity,width) || !bits(types,capacity,bytes,known,allocated) || (!zero_sized && allocated < count))
                        { return outcome::unavailable; }
                    }
                }
                auto const maximum{width == 4u ? 0xffffffffull : ~::std::uint64_t{}};
                if(zero_sized) { allocated=maximum; }
                // Prove the complete declared range before copying a prefix.
                if(count != 0u && (address == 0u || address > maximum || (!zero_sized && count > (maximum-address)/extent+((maximum-address)%extent == extent-1u ? 1u : 0u))))
                { return outcome::unavailable; }
                ::std::string text{string ? "string length=" : "size="}; number(text,count);
                if(capacity.type != dwarf::no_record || endpoints) { text += " capacity="; number(text,allocated); }
                auto remaining{elements <= cap.max_elements ? cap.max_elements-elements : 0u};
                auto const available_bytes{copied <= cap.max_bytes ? cap.max_bytes-copied : 0u};
                auto const display_elements{string ? (std::min)(remaining,static_cast<::std::size_t>(4096u/extent)) : remaining};
                ::std::size_t shown{static_cast<::std::size_t>((std::min)(count,static_cast<::std::uint64_t>(display_elements)))};
                if(!zero_sized) { shown = (std::min)(shown,static_cast<::std::size_t>(available_bytes/extent)); }
                ::std::vector<::std::byte> data{};
                if(shown != 0u && !zero_sized && !read(address,shown*static_cast<::std::size_t>(extent),data)) { return limited ? outcome::limit_exceeded : outcome::unavailable; }
                if(string)
                {
                    elements += shown;
                    text += ' '; text += '"';
                    // Preserve code-unit values independently of locale. UTF-8
                    // bytes use the existing escaped display channel; UTF16/32
                    // are rendered explicitly without invalid Unicode guesses.
                    if(extent == 1u && !data.empty())
                    {
                        tree[0u].display_text.assign(reinterpret_cast<char const*>(data.data()),data.size());
                        tree[0u].display_text_available = true; tree[0u].display_text_truncated = count > shown;
                        text += ::fast_io::concat_std(dwarf::escaped_metadata_text{tree[0u].display_text});
                    }
                    else if(extent == 1u) { tree[0u].display_text_available = true; tree[0u].display_text_truncated = count != 0u; }
                    else
                    {
                        text += extent == 2u ? "UTF16 code units: " : "UTF32 code units: ";
                        for(::std::size_t i{}; i != shown; ++i)
                        { ::std::uint64_t unit{}; for(::std::size_t b{}; b != extent; ++b) { unit |= static_cast<::std::uint64_t>(::std::to_integer<unsigned char>(data[i*extent+b])) << (b*8u); }
                          if(i != 0u) { text += ','; } number(text,unit); if(text.size() > 3500u) { shown = i+1u; break; } }
                    }
                    text += '"';
                }
                else
                {
                    text += " ["; ::std::size_t appended{};
                    for(; appended != shown; ++appended)
                    {
                        if(elements >= cap.max_elements) { limited = true; break; }
                        ::std::string label{"["}; number(label,appended); label += ']'; auto const before{tree.size()};
                        ++elements;
                        if(!append(tree,target,zero_sized ? ::std::span<::std::byte const>{} : ::std::span<::std::byte const>{data.data()+appended*extent,static_cast<::std::size_t>(extent)},::std::move(label),depth)) { break; }
                        if(appended < 8u && text.size() < 3000u)
                        { if(appended != 0u) { text += ", "; } scalar_text(tree[before],text); }
                    }
                    shown = appended; if(shown > 8u || count > shown) { if(shown != 0u) { text += ", "; } text += "..."; } text += ']';
                }
                if(text.size() > 4096u) { text.resize(4096u); }
                summary(tree[0u],profile,::std::move(text),count-shown); return outcome::formatted;
            }
            // A same-CU value type can instantiate both cached and uncached
            // GNU hash nodes. Bind the cache bit through the owning table's
            // actual traits template parameter before selecting its node DIE.
            [[nodiscard]] bool template_boolean(::std::size_t owner, ::std::size_t position, bool& result) const noexcept
            {
                if(owner >= types.size()) { return false; }
                bool found{}, pending{};
                for(auto const& parameter : types[owner].template_values)
                {
                    if(parameter.position != position) { continue; }
                    if(found || !parameter.available || parameter.bits > 1u ||
                       parameter.type >= types.size()) { return false; }
                    auto const& scalar{types[parameter.type]};
                    if(!dwarf::cxx_language(scalar.language) || scalar.tinygo_producer || scalar.zig_producer ||
                       scalar.kind != dwarf::type_kind::scalar || !scalar.size_known ||
                       scalar.atomic_scalar || scalar.encoding != 2u || scalar.byte_count != 1u || scalar.byte_size != 1u)
                    { return false; }
                    pending = parameter.bits != 0u; found = true;
                }
                if(found) { result = pending; } return found;
            }
            [[nodiscard]] bool node_type(::std::string_view stem, ::std::size_t element, ::std::size_t& result,
                bool cache_known = false, bool cache_value = false)
            {
                result = dwarf::no_record;
                for(::std::size_t i{}; i != types.size(); ++i)
                {
                    auto const& type{types[i]};
                    if(!family(type,stem) || type.template_types.empty() || type.template_types[0u] != element || !type.size_known ||
                       type.byte_size == 0u || type.byte_size > cap.max_bytes) { continue; }
                    bool candidate_cache{};
                    if(cache_known && (!template_boolean(i,1u,candidate_cache) || candidate_cache != cache_value)) { continue; }
                    if(result != dwarf::no_record && (type.declaration_identity == dwarf::die_key{} ||
                       types[result].declaration_identity != type.declaration_identity)) { return false; }
                    result = i;
                }
                return result != dwarf::no_record;
            }
            [[nodiscard]] bool payload(::std::size_t node, ::std::size_t element, field& result)
            {
                if(!any_path(types,node,{"_M_data","_M_value_field","_M_storage"},result)) { return false; }
                if(result.type == element) { return true; }
                // libstdc++ __aligned_membuf<T> represents placement storage.
                // Its template DIE must prove T; byte-array size alone cannot.
                auto const& storage{types[result.type]};
                if(storage.template_types.empty() || storage.template_types[0u] != element || !storage.size_known ||
                   !types[element].size_known || storage.byte_size < types[element].byte_size) { return false; }
                field inner{};
                if(!member(types,result,"_M_storage",inner) || types[inner.type].kind != dwarf::type_kind::array ||
                   types[inner.type].byte_size < types[element].byte_size) { return false; }
                result = {element,inner.offset}; return true;
            }
            [[nodiscard]] member_lookup base_zero_walk(::std::size_t node, ::std::size_t base, ::std::size_t& work,
                ::std::size_t depth, member_ancestor const* ancestors) noexcept
            {
                if(node >= types.size() || base >= types.size() || depth >= 16u) { return member_lookup::invalid; }
                for(auto current{ancestors}; current != nullptr; current=current->parent)
                { if(current->type == node) { return member_lookup::invalid; } }
                if(node == base) { return member_lookup::found; }
                member_ancestor const active{node,ancestors}; bool found{};
                for(auto const& member : types[node].members)
                {
                    if(++work > 4096u) { return member_lookup::invalid; }
                    if(!member.inherited) { continue; }
                    if(!member.offset_known || member.bit_field || member.type >= types.size() || !types[node].size_known ||
                       !types[member.type].size_known || member.byte_offset > types[node].byte_size ||
                       types[member.type].byte_size > types[node].byte_size-member.byte_offset) { return member_lookup::invalid; }
                    if(member.byte_offset != 0u) { continue; }
                    auto const status{base_zero_walk(member.type,base,work,depth+1u,::std::addressof(active))};
                    if(status == member_lookup::invalid) { return status; }
                    if(status == member_lookup::found)
                    { if(found) { return member_lookup::invalid; } found=true; }
                }
                return found ? member_lookup::found : member_lookup::absent;
            }
            [[nodiscard]] bool base_zero(::std::size_t node, ::std::size_t base, ::std::size_t depth = 0u) noexcept
            {
                ::std::size_t work{}; return base_zero_walk(node,base,work,depth,nullptr) == member_lookup::found;
            }
            [[nodiscard]] outcome linked(::std::size_t root, ::std::span<::std::byte const> bytes,
                ::std::span<::std::byte const> known, ::std::span<::std::byte const> pointer_known,
                ::std::vector<dwarf::object_node>& tree, ::std::size_t depth, bool tree_order, bool hash)
            {
                field head{},count_field{},tail{}; ::std::size_t element{dwarf::no_record}, hash_impl{dwarf::no_record};
                bool cache_known{}, cache_value{};
                auto const& type{types[root]};
                if(tree_order || hash)
                {
                    field impl{};
                    if(!path(types,root,hash ? "_M_h" : "_M_t",impl) || types[impl.type].template_types.size() < 2u) { return outcome::unavailable; }
                    element = types[impl.type].template_types[1u];
                    if(hash) { hash_impl = impl.type; }
                    if(!(path(types,root,hash ? "_M_h._M_before_begin._M_nxt" : "_M_t._M_impl._M_header._M_parent",head) &&
                         path(types,root,hash ? "_M_h._M_element_count" : "_M_t._M_impl._M_node_count",count_field))) { return outcome::unavailable; }
                }
                else
                {
                    if(type.template_types.empty()) { return outcome::unavailable; } element = type.template_types[0u];
                    if(!(path(types,root,"_M_impl._M_node._M_next",head) && path(types,root,"_M_impl._M_node._M_size",count_field) &&
                         path(types,root,"_M_impl._M_node._M_prev",tail))) { return outcome::unavailable; }
                }
                if(element >= types.size() || !types[element].size_known || !pointer(types,head,width) || !integer(types,count_field,width)) { return outcome::unavailable; }
                ::std::uint64_t current{},count{},last{};
                if(!bits(types,head,bytes,pointer_known.empty() ? known : pointer_known,current) || !bits(types,count_field,bytes,known,count) ||
                   (!tree_order && !hash && (!pointer(types,tail,width) || !bits(types,tail,bytes,pointer_known.empty() ? known : pointer_known,last)))) { return outcome::unavailable; }
                ::std::string text{"size="}; number(text,count); text += " [";
                if(count == 0u)
                { if((tree_order || hash) ? current != 0u : current != last) { return outcome::unavailable; } text += ']'; summary(tree[0u],"libstdc++ linked container",::std::move(text)); return outcome::formatted; }
                // Empty tables above need no node ABI. A nonempty modern
                // table must establish its own cache bit before a node copy.
                if(hash && family(types[hash_impl],"_Hashtable") && types[hash_impl].template_types.size() == 10u)
                {
                    auto const traits{types[hash_impl].template_types[9u]};
                    if(traits >= types.size() || !family(types[traits],"_Hashtable_traits") ||
                       !types[traits].template_types.empty() || types[traits].template_values.size() != 3u ||
                       !template_boolean(traits,0u,cache_value)) { return outcome::unavailable; }
                    cache_known = true;
                }
                ::std::size_t node{};
                if(!node_type(tree_order ? "_Rb_tree_node" : hash ? "_Hash_node" : "_List_node",element,node,cache_known,cache_value)) { return outcome::unavailable; }
                if(!base_zero(node,types[head.type].referenced_type)) { return outcome::unavailable; }
                field value{},left{},right{},next{},previous{};
                if(!payload(node,element,value)) { return outcome::unavailable; }
                if(tree_order)
                { if(!path(types,node,"_M_left",left) || !path(types,node,"_M_right",right) || !pointer(types,left,width) || !pointer(types,right,width)) { return outcome::unavailable; } }
                else if(!path(types,node,hash ? "_M_nxt" : "_M_next",next) || !pointer(types,next,width)) { return outcome::unavailable; }
                if(!tree_order && !hash && (!path(types,node,"_M_prev",previous) || !pointer(types,previous,width))) { return outcome::unavailable; }
                struct pending_node { ::std::uint64_t address{}; ::std::vector<::std::byte> bytes{}; };
                ::std::vector<pending_node> stack{}; ::std::vector<::std::uint64_t> visited{};
                ::std::uint64_t emitted{},prior{},sentinel{};
                while(emitted < count && elements < cap.max_elements)
                {
                    ::std::vector<::std::byte> data{}; ::std::uint64_t address{current};
                    if(tree_order)
                    {
                        while(current != 0u)
                        {
                            if(::std::find(visited.begin(),visited.end(),current) != visited.end()) { return outcome::unavailable; }
                            if(!read(current,static_cast<::std::size_t>(types[node].byte_size),data))
                            { if(limited) { break; } return outcome::unavailable; }
                            visited.push_back(current); ::std::uint64_t child{};
                            if(!bits(types,left,data,{},child)) { return outcome::unavailable; }
                            stack.push_back({current,::std::move(data)}); current = child;
                        }
                        if(limited || stack.empty()) { break; }
                        address = stack.back().address; data = ::std::move(stack.back().bytes); stack.pop_back();
                        if(!bits(types,right,data,{},current)) { return outcome::unavailable; }
                    }
                    else
                    {
                        if(current == 0u || ::std::find(visited.begin(),visited.end(),current) != visited.end()) { return outcome::unavailable; }
                        if(!read(current,static_cast<::std::size_t>(types[node].byte_size),data))
                        { if(limited) { break; } return outcome::unavailable; }
                        visited.push_back(current); if(!bits(types,next,data,{},current)) { return outcome::unavailable; }
                        if(!hash)
                        {
                            ::std::uint64_t back{}; if(!bits(types,previous,data,{},back)) { return outcome::unavailable; }
                            if(emitted == 0u) { sentinel = back; if(sentinel == 0u || sentinel == address) { return outcome::unavailable; } }
                            else if(back != prior || address == sentinel) { return outcome::unavailable; }
                        }
                        prior = address;
                    }
                    if(value.offset > data.size() || types[element].byte_size > data.size()-value.offset) { return outcome::unavailable; }
                    ::std::string label{"["}; number(label,emitted); label += ']'; auto const before{tree.size()}; ++elements;
                    if(!append(tree,element,{data.data()+value.offset,static_cast<::std::size_t>(types[element].byte_size)},::std::move(label),depth)) { break; }
                    if(emitted < 8u && text.size() < 3000u) { if(emitted != 0u) { text += ", "; } scalar_text(tree[before],text); }
                    ++emitted;
                }
                if(emitted == count && ((hash && current != 0u) || (tree_order && (current != 0u || !stack.empty())) ||
                   (!tree_order && !hash && (prior != last || current != sentinel ||
                       ::std::find(visited.begin(),visited.end(),sentinel) != visited.end())))) { return outcome::unavailable; }
                if(emitted < count || emitted > 8u) { if(emitted != 0u) { text += ", "; } text += "..."; } text += ']';
                summary(tree[0u],tree_order ? "libstdc++ ordered tree" : hash ? "libstdc++ hash chain" : "libstdc++ list",::std::move(text),count-emitted);
                return outcome::formatted;
            }
#if UWVM_DEBUG_LANGUAGE_RUST
            [[nodiscard]] outcome rust_shared(::std::size_t root,::std::span<::std::byte const> bytes,
                ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,
                ::std::vector<dwarf::object_node>& tree,::std::size_t depth)
            {
                rust_shared_layout layout{};::std::uint64_t address{},strong{},weak{};bool empty{};
                if(!rust_shared_schema(types,root,width,layout) ||
                   !rust_shared_address(types,layout,bytes,known,eligible,width,address,empty)) { return outcome::unavailable; }
                auto const profile{layout.atomic?(layout.weak_owner?"Rust Arc Weak":"Rust Arc"):(layout.weak_owner?"Rust Rc Weak":"Rust Rc")};
                if(empty) { summary(tree[0u],profile,"empty strong=0 weak=0");return outcome::formatted; }
                ::std::vector<::std::byte> header{};
                if(!read(address,layout.header_bytes,header)) { return limited?outcome::limit_exceeded:outcome::unavailable; }
                if(!rust_shared_counts(types,layout,header,width,strong,weak)) { return outcome::unavailable; }
                ::std::string text{strong==0u?"expired strong=":"strong="};number(text,strong);text+=" weak=";number(text,weak);
                auto const available{elements<cap.max_elements?cap.max_elements-elements:0u};::std::size_t shown{};
                if(strong!=0u && available!=0u)
                {
                    auto const size{static_cast<::std::size_t>(types[layout.value.type].byte_size)};::std::vector<::std::byte> value{};
                    if(size!=0u && !read(address+layout.value.offset,size,value))
                    { return limited?outcome::limit_exceeded:outcome::unavailable; }
                    auto const before{tree.size()};++elements;
                    if(!append(tree,layout.value.type,value,"value",depth)) { return limited?outcome::limit_exceeded:outcome::unavailable; }
                    ++shown;text+=" = ";scalar_text(tree[before],text);
                }
                // Counter children are detached values from that same owned
                // prefix, never writes to the actual reference count.
                for(unsigned i{};i!=2u && elements<cap.max_elements;++i)
                {
                    auto const counter{i==0u?layout.strong:layout.weak};auto const count{i==0u?strong:weak};
                    ::std::vector<::std::byte> value(width);
                    for(unsigned j{};j!=width;++j) { value[j]=static_cast<::std::byte>((count>>(8u*j))&255u); }
                    ++elements;if(!append(tree,counter.type,value,i==0u?"strong":"weak",depth))
                    { return limited?outcome::limit_exceeded:outcome::unavailable; }++shown;
                }
                auto const children{strong==0u?2u:3u};
                summary(tree[0u],profile,::std::move(text),children-shown);return outcome::formatted;
            }
            [[nodiscard]] outcome rust_deque(::std::size_t root,::std::span<::std::byte const> bytes,
                ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,
                ::std::vector<dwarf::object_node>& tree,::std::size_t depth)
            {
                rust_deque_extent layout{};
                if(!rust_deque_plan(types,root,bytes,known,eligible,width,layout)) { return outcome::unavailable; }
                auto const& plan{layout.sequence};auto const extent{types[plan.element].byte_size};
                auto const remaining{elements<=cap.max_elements ? cap.max_elements-elements : 0u};
                auto shown{static_cast<::std::size_t>((std::min)(plan.count,static_cast<::std::uint64_t>(remaining)))};
                if(!layout.zero_sized)
                {
                    auto const available{copied<=cap.max_bytes ? cap.max_bytes-copied : 0u};
                    shown=(std::min)(shown,static_cast<::std::size_t>(available/extent));
                }
                ::std::vector<::std::byte> data{};
                if(shown!=0u && !layout.zero_sized)
                {
                    auto const first{static_cast<::std::size_t>((std::min)(static_cast<::std::uint64_t>(shown),
                        plan.ring_capacity-plan.ring_head))};
                    auto const needed{first==shown ? 1u : 2u};
                    if(reads>cap.maximum_reads || needed>cap.maximum_reads-reads)
                    { limited=true;return outcome::limit_exceeded; }
                    ::std::uint64_t address{};
                    if(!indexed_address(plan,0u,extent,width,address)) { return outcome::unavailable; }
                    if(!read(address,first*static_cast<::std::size_t>(extent),data))
                    { return limited ? outcome::limit_exceeded : outcome::unavailable; }
                    if(first!=shown)
                    {
                        ::std::vector<::std::byte> tail{};
                        if(!read(plan.address,(shown-first)*static_cast<::std::size_t>(extent),tail))
                        { return limited ? outcome::limit_exceeded : outcome::unavailable; }
                        data.insert(data.end(),tail.begin(),tail.end());
                    }
                }
                ::std::string text{"size="};number(text,plan.count);text+=" capacity=";number(text,layout.capacity);text+=" [";
                ::std::size_t appended{};
                for(;appended!=shown;++appended)
                {
                    ::std::string label{"["};number(label,appended);label+=']';auto const before{tree.size()};++elements;
                    if(!append(tree,plan.element,layout.zero_sized ? ::std::span<::std::byte const>{} :
                        ::std::span<::std::byte const>{data}.subspan(appended*extent,extent),::std::move(label),depth)) { break; }
                    if(appended<8u && text.size()<3000u)
                    { if(appended!=0u) { text+=", "; }scalar_text(tree[before],text); }
                }
                if(appended>8u || plan.count>appended) { if(appended!=0u) { text+=", "; }text+="..."; }text+=']';
                summary(tree[0u],"Rust VecDeque",::std::move(text),plan.count-appended);return outcome::formatted;
            }
#endif
            [[nodiscard]] outcome format_owned(::std::size_t root, ::std::span<::std::byte const> bytes,
                ::std::span<::std::byte const> known, ::std::span<::std::byte const> pointer_known,
                ::std::vector<dwarf::object_node>& tree, ::std::size_t depth)
            {
                if(root >= types.size() || tree.empty()) { return outcome::unavailable; }
                auto const kind{types[root].kind};
                if(kind != dwarf::type_kind::structure && kind != dwarf::type_kind::class_type && kind != dwarf::type_kind::array)
                { return outcome::not_matched; }
                auto pending{tree};
                auto const result{format_dispatch(root,bytes,known,pointer_known,pending,depth)};
                if(result == outcome::formatted) { tree = ::std::move(pending); }
                return result;
            }
            [[nodiscard]] outcome format_dispatch(::std::size_t root, ::std::span<::std::byte const> bytes,
                ::std::span<::std::byte const> known, ::std::span<::std::byte const> pointer_known,
                ::std::vector<dwarf::object_node>& tree, ::std::size_t depth)
            {
                if(root >= types.size() || tree.empty() || tree[0u].type != root) { return outcome::unavailable; }
                auto const& type{types[root]}; field a{},b{},c{};
                if(cap.registry != nullptr)
                {
                    if(!source_pretty_registry::valid(*cap.registry)) { return outcome::unavailable; }
                    if(auto const profile{source_pretty_registry::find(*cap.registry,type.name)})
                    {
                        using enum source_pretty_registry::profile_kind;
                        if(profile->kind == sequence || profile->kind == string)
                        {
                            if(!path(types,root,profile->data_path,a) || !path(types,root,profile->size_path,b) ||
                               (!profile->capacity_path.empty() && !path(types,root,profile->capacity_path,c))) { return outcome::unavailable; }
                            return this->sequence(root,bytes,known,pointer_known,tree,depth,a,b,{},c,false,profile->kind == string,profile->name);
                        }
                        if(profile->kind == pointer)
                        {
                            if(!path(types,root,profile->data_path,a) || !details::pointer(types,a,width)) { return outcome::unavailable; }
                            ::std::uint64_t address{};
                            if(!bits(types,a,bytes,known,address) || (!pointer_known.empty() && !bits(types,a,bytes,pointer_known,address))) { return outcome::unavailable; }
                            ::std::string text{address == 0u ? "empty" : "stored pointer"};
                            if(address != 0u && elements < cap.max_elements)
                            {
                                auto const target{types[a.type].referenced_type}; auto const& value{types[target]};
                                if(!value.size_known || value.byte_size == 0u || value.byte_size > cap.max_bytes) { return outcome::unavailable; }
                                ::std::vector<::std::byte> copied_value{};
                                if(!read(address,static_cast<::std::size_t>(value.byte_size),copied_value)) { return limited ? outcome::limit_exceeded : outcome::unavailable; }
                                auto const before{tree.size()}; ++elements;
                                if(!append(tree,target,copied_value,"*",depth)) { return limited ? outcome::limit_exceeded : outcome::unavailable; }
                                text += " = "; scalar_text(tree[before],text);
                            }
                            summary(tree[0u],profile->name,::std::move(text),address != 0u && cap.max_elements == 0u ? 1u : 0u);
                            return outcome::formatted;
                        }
                        if(profile->kind == optional)
                        {
                            if(!path(types,root,profile->engaged_path,a) || !path(types,root,profile->value_path,b) || !integer(types,a,1u)) { return outcome::unavailable; }
                            ::std::uint64_t engaged{}; if(!bits(types,a,bytes,known,engaged) || engaged > 1u) { return outcome::unavailable; }
                            if(engaged == 0u) { summary(tree[0u],profile->name,"none"); return outcome::formatted; }
                            auto const extent{types[b.type].byte_size};
                            if(b.offset > bytes.size() || extent > bytes.size()-b.offset) { return outcome::unavailable; }
                            ::std::string text{"engaged"};
                            if(elements < cap.max_elements)
                            {
                                auto const first{static_cast<::std::size_t>(b.offset)}; auto const count{static_cast<::std::size_t>(extent)};
                                auto const before{tree.size()}; ++elements;
                                if(!append(tree,b.type,bytes.subspan(first,count),"value",depth,
                                    known.empty() ? known : known.subspan(first,count),pointer_known.empty() ? pointer_known : pointer_known.subspan(first,count)))
                                { return limited ? outcome::limit_exceeded : outcome::unavailable; }
                                text += " = "; scalar_text(tree[before],text);
                            }
                            summary(tree[0u],profile->name,::std::move(text)); return outcome::formatted;
                        }
                        return outcome::unavailable;
                    }
                    if(!cap.registry->builtins_enabled) { return outcome::not_matched; }
                }
#if UWVM_DEBUG_LANGUAGE_CPP
                auto const array_result{extended_array(*this,root,bytes,known,pointer_known,tree,depth)};
                if(array_result != outcome::not_matched) { return array_result; }
                auto const extended_sequence{extended_sequences(*this,root,bytes,known,pointer_known,tree,depth)};
                if(extended_sequence != outcome::not_matched) { return extended_sequence; }
                auto const extended_value{extended_values(*this,root,bytes,known,pointer_known,tree,depth)};
                if(extended_value != outcome::not_matched) { return extended_value; }
                auto const extended_nodes{extended_libcxx_nodes(*this,root,bytes,known,pointer_known,tree,depth)};
                if(extended_nodes != outcome::not_matched) { return extended_nodes; }
                auto const choose{[&](field& value, ::std::initializer_list<::std::string_view> paths)
                { return any_path(types,root,paths,value); }};
                if(family(type,"vector"))
                {
                    ::std::size_t counted_element{};
                    if(counted_vector_schema(types,root,width,a,b,c,counted_element))
                    {
                        indexed_sequence checked{};
                        if(!index_plan(types,root,bytes,known,pointer_known,width,checked)) { return outcome::unavailable; }
                        return sequence(root,bytes,known,pointer_known,tree,depth,a,b,{},c,false,false,"libc++ vector counted layout");
                    }
                    if(choose(a,{"_M_impl._M_start"}) && choose(b,{"_M_impl._M_finish"}) && choose(c,{"_M_impl._M_end_of_storage"}))
                    { return sequence(root,bytes,known,pointer_known,tree,depth,a,{},b,c,true,false,"libstdc++ vector"); }
                    if(choose(a,{"__begin_","__layout_.__begin_"}) && choose(b,{"__end_","__layout_.__end_"}) &&
                       vector_capacity(types,root,width,c))
                    { return sequence(root,bytes,known,pointer_known,tree,depth,a,{},b,c,true,false,"libc++ vector"); }
                    return outcome::unavailable;
                }
                bool const string{family(type,"basic_string") || family(type,"string") || family(type,"basic_string_view") || family(type,"string_view")};
                if(string)
                {
                    if(choose(a,{"_M_dataplus._M_p"}) && choose(b,{"_M_string_length"}))
                    { return sequence(root,bytes,known,pointer_known,tree,depth,a,b,{}, {},false,true,"libstdc++ C++11 basic_string"); }
                    if(choose(a,{"_M_str"}) && choose(b,{"_M_len"}))
                    { return sequence(root,bytes,known,pointer_known,tree,depth,a,b,{}, {},false,true,"libstdc++ string_view"); }
                    if(choose(a,{"__data_"}) && choose(b,{"__size_"}))
                    { return sequence(root,bytes,known,pointer_known,tree,depth,a,b,{}, {},false,true,"libc++ string_view"); }
                    libcxx_string_layout layout{};::std::uint64_t length{};bool long_storage{};
                    if(!libcxx_string_schema(types,root,tree,width,layout) ||
                       !libcxx_string_extent(types,tree,layout,bytes,known,length,long_storage)) { return outcome::unavailable; }
                    if(long_storage)
                    { return sequence(root,bytes,known,pointer_known,tree,depth,layout.long_data.value,layout.long_size.value,{}, {},false,true,"libc++ long basic_string"); }
                    auto const& short_data{layout.short_data};auto const unit{layout.unit};
                    if(length>cap.max_bytes/unit) { return outcome::unavailable; }
                    auto const start{static_cast<::std::size_t>(short_data.value.offset)};
                    auto const shown{(::std::min)(length,static_cast<::std::uint64_t>(elements < cap.max_elements ? cap.max_elements-elements : 0u))};
                    auto const byte_count{static_cast<::std::size_t>(shown*unit)};
                    for(::std::size_t i{}; i != byte_count; ++i)
                    { if(!known.empty() && known[start+i] != ::std::byte{0xffu}) { return outcome::unavailable; } }
                    ::std::string text{"string length="}; number(text,length); text += " \"";
                    if(unit == 1u)
                    {
                        tree[0u].display_text.clear();
                        if(byte_count != 0u) { tree[0u].display_text.assign(reinterpret_cast<char const*>(bytes.data()+start),byte_count); }
                        tree[0u].display_text_available=true; tree[0u].display_text_truncated=shown<length; text += ::fast_io::concat_std(dwarf::escaped_metadata_text{tree[0u].display_text});
                    }
                    else
                    {
                        text += unit == 2u ? "UTF16 code units: " : "UTF32 code units: ";
                        for(::std::size_t i{}; i != shown; ++i)
                        { ::std::uint64_t value{}; for(::std::size_t j{}; j != unit; ++j)
                          { value |= static_cast<::std::uint64_t>(::std::to_integer<unsigned char>(bytes[start+i*unit+j])) << (j*8u); }
                          if(i != 0u) { text += ','; } number(text,value); }
                    }
                    elements += static_cast<::std::size_t>(shown);
                    text += '"'; summary(tree[0u],"libc++ short basic_string",::std::move(text),length-shown); return outcome::formatted;
                }
                if(family(type,"span"))
                {
                    if(choose(a,{"_M_ptr","__data_"}) && choose(b,{"_M_extent._M_extent_value","_M_extent","__size_"}))
                    { return sequence(root,bytes,known,pointer_known,tree,depth,a,b,{}, {},false,false,"C++ span"); }
                    return outcome::unavailable;
                }
                if(family(type,"list")) { return linked(root,bytes,known,pointer_known,tree,depth,false,false); }
                if(family(type,"map") || family(type,"multimap") || family(type,"set") || family(type,"multiset"))
                { return linked(root,bytes,known,pointer_known,tree,depth,true,false); }
                if(family(type,"unordered_map") || family(type,"unordered_multimap") || family(type,"unordered_set") || family(type,"unordered_multiset"))
                { return linked(root,bytes,known,pointer_known,tree,depth,false,true); }
#endif
#if UWVM_DEBUG_LANGUAGE_RUST
                if(type.language==0x1cu)
                {
                    if(rust_family(type,"Rc") || rust_family(type,"Arc") || rust_family(type,"Weak"))
                    { return rust_shared(root,bytes,known,pointer_known,tree,depth); }
                    if(rust_family(type,"VecDeque")) { return rust_deque(root,bytes,known,pointer_known,tree,depth); }
                    rust_sequence_layout rust{};
                    if(rust_sequence_schema(types,root,width,rust))
                    {
                        auto const profile{rust.string ? "Rust string" : rust.capacity.type==dwarf::no_record ? "Rust slice" : "Rust Vec"};
                        return sequence(root,bytes,known,pointer_known,tree,depth,rust.data,rust.length,{},rust.capacity,
                            false,rust.string,profile,rust.element,rust.zero_sized);
                    }
                }
#endif
#if UWVM_DEBUG_LANGUAGE_CPP
                if(family(type,"optional"))
                {
                    if(!choose(a,{"_M_payload._M_engaged","__engaged_"}) || !integer(types,a,1u)) { return outcome::unavailable; }
                    ::std::uint64_t engaged{};
                    if(!bits(types,a,bytes,known,engaged) || engaged > 1u) { return outcome::unavailable; }
                    ::std::string text{engaged ? "engaged" : "nullopt"};
                    if(engaged && choose(b,{"_M_payload._M_payload._M_value","__val_"}))
                    { for(auto const& n : tree) { if(n.type == b.type && n.byte_offset >= tree[0u].byte_offset &&
                        n.byte_offset-tree[0u].byte_offset == b.offset) { text += " = "; scalar_text(n,text); break; } } }
                    summary(tree[0u],"C++ optional",::std::move(text)); return outcome::formatted;
                }
                if(family(type,"variant"))
                {
                    if(!choose(a,{"_M_index","__impl_.__index"}) || !integer(types,a)) { return outcome::unavailable; }
                    ::std::uint64_t index{}; if(!bits(types,a,bytes,known,index)) { return outcome::unavailable; }
                    ::std::string text{"index="}; number(text,index);
                    if(index == dwarf::numeric_mask(types[a.type].byte_count)) { text = "valueless_by_exception"; }
                    summary(tree[0u],"C++ variant",::std::move(text)); return outcome::formatted;
                }
                if(family(type,"unique_ptr") || family(type,"shared_ptr") || family(type,"weak_ptr"))
                {
                    bool const unique{family(type,"unique_ptr")}; bool const weak{family(type,"weak_ptr")};
                    if(unique)
                    {
                        if(type.template_types.empty() || type.template_types[0u] >= types.size()) { return outcome::unavailable; }
                        field tuple{};auto const target{type.template_types[0u]};
                        if(path(types,root,"_M_t._M_t",tuple))
                        { if(!unique_pointer_head(types,tuple,target,width,a)) { return outcome::unavailable; } }
                        else if(!choose(a,{"__ptr_.__value_","__ptr_.__first_","__ptr_"}) ||
                            !pointer(types,a,width) || types[a.type].referenced_type != target) { return outcome::unavailable; }
                    }
                    else if(!choose(a,{"_M_ptr","__ptr_"}) || !pointer(types,a,width) || type.template_types.empty() ||
                        types[a.type].referenced_type != type.template_types[0u]) { return outcome::unavailable; }
                    ::std::uint64_t address{}; if(!bits(types,a,bytes,pointer_known.empty() ? known : pointer_known,address)) { return outcome::unavailable; }
                    ::std::string text{address == 0u ? "empty" : unique ? "stored pointer" : "stored pointer (ownership/liveness unknown)"};
                    // Weak pointers must never follow a potentially expired
                    // pointee; shared counts need their own proven ABI profile.
                    if(address != 0u && unique)
                    {
                        auto const target{types[a.type].referenced_type}; auto const& t{types[target]};
                        if(t.size_known && t.byte_size != 0u && t.byte_size <= cap.max_bytes && elements < cap.max_elements)
                        {
                            ::std::vector<::std::byte> object{};
                            if(read(address,static_cast<::std::size_t>(t.byte_size),object))
                            { ++elements; auto const before{tree.size()}; if(append(tree,target,object,"*",depth)) { text += " = "; scalar_text(tree[before],text); } }
                        }
                    }
                    summary(tree[0u],unique ? "C++ unique_ptr" : weak ? "C++ weak_ptr" : "C++ shared_ptr",::std::move(text)); return outcome::formatted;
                }
                if(family(type,"array") || family(type,"pair") || family(type,"tuple") || type.kind == dwarf::type_kind::array)
                {
                    ::std::string text{family(type,"pair") ? "pair" : family(type,"tuple") ? "tuple" : "array"};
                    text += " {"; ::std::size_t shown{};bool incomplete{tree[0u].omitted_children != 0u};
                    for(::std::size_t i{1u}; i != tree.size(); ++i)
                    {
                        auto const& node{tree[i]};
                        if(node.omitted_children != 0u || node.reason != dwarf::object_unavailable_reason::none) { incomplete=true; }
                        if(node.kind != dwarf::type_kind::scalar && node.kind != dwarf::type_kind::enumeration &&
                           node.kind != dwarf::type_kind::pointer && !node.bit_field) { continue; }
                        if(!node.value_available || node.kind == dwarf::type_kind::pointer || node.bit_field || shown >= 8u || text.size() >= 3000u)
                        { incomplete=true;continue; }
                        if(shown != 0u) { text += ", "; } text += node.name; text += '='; scalar_text(node,text); ++shown;
                    }
                    if(shown == 0u) { text += "no scalar preview"; }
                    if(incomplete) { text += "; additional/unavailable raw fields"; }
                    text += '}';if(text.size() > 4096u) { text.resize(4096u);incomplete=true; }
                    summary(tree[0u],"C++ fixed aggregate",::std::move(text));tree[0u].pretty_truncated=tree[0u].pretty_truncated||incomplete;return outcome::formatted;
                }
                if(coroutine_details::summarize(types,tree,width))
                {
                    (void)coroutine_details::enrich_handle(*this,root,bytes,known,pointer_known,tree,depth);
                    return outcome::formatted;
                }
#endif
                return outcome::not_matched;
            }
        };
    }
    template<typename Reader>
    [[nodiscard]] inline outcome format(::std::span<dwarf::type_record const> types, ::std::size_t root,
        ::std::span<::std::byte const> bytes, ::std::span<::std::byte const> known,
        ::std::span<::std::byte const> pointer_known, ::std::uint8_t address_bytes, Reader&& read,
        ::std::vector<dwarf::object_node>& tree, limits const& cap = {}) noexcept
    {
        if(root >= types.size() || tree.empty() || (address_bytes != 4u && address_bytes != 8u) || bytes.size() > 65536u ||
           (!known.empty() && known.size() != bytes.size()) || (!pointer_known.empty() && pointer_known.size() != bytes.size()) ||
           cap.max_nodes == 0u || cap.max_nodes > 1024u || tree.size() > cap.max_nodes || cap.max_bytes > 65536u || cap.max_depth > 16u)
        { return outcome::unavailable; }
        dwarf::object_query_limits check{};
        if(dwarf::object_details::validate(types,check) != dwarf::inline_query_error::none) { return outcome::unavailable; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            // Commit a complete result only; read failures and exhausted budgets
            // leave the original raw object and existing summaries untouched.
            ::std::vector<::std::byte> qualified{};
            if(!pointer_known.empty() && !known.empty())
            {
                qualified.resize(known.size());
                for(::std::size_t i{}; i != known.size(); ++i) { qualified[i] = pointer_known[i] & known[i]; }
                pointer_known = qualified;
            }
            details::session<::std::remove_reference_t<Reader>> work{types,address_bytes,read,cap};
            work.nodes = tree.size();
            return work.format_owned(root,bytes,known,pointer_known,tree,0u);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        } catch(...) { return outcome::allocation_failure; }
#endif
    }
}
#if UWVM_DEBUG_LANGUAGE_CPP
#include "source_pretty_sequences.h"
#include "source_pretty_libcxx_nodes.h"
#include "source_pretty_values.h"
#include "source_pretty_array.h"
#endif
