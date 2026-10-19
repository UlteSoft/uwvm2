/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include "language_debug_features.h"
#if UWVM_DEBUG_LANGUAGE_CPP
#ifndef UWVM_MODULE
# include "source_pretty_printer.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_pretty_printer::details
{
    namespace value_details
    {
        // Distinct typedef records may identify one owned final declaration.
        // A spelling/size match alone never establishes an active payload type.
        [[nodiscard]] inline bool same_value_type(::std::span<dwarf::type_record const> types,
            ::std::size_t left,::std::size_t right) noexcept
        {
            return dwarf::same_cpp_value_type(types,left,right);
        }
        [[nodiscard]] inline bool known_void(::std::span<dwarf::type_record const> types,::std::size_t index) noexcept
        {
            if(index==dwarf::no_record) { return true; }
            if(index>=types.size()) { return false; }
            auto const& type{types[index]};
            return type.void_type && type.kind==dwarf::type_kind::unavailable && !type.size_known &&
                type.byte_size==0u && type.byte_count==0u && type.encoding==0u && !type.atomic_scalar &&
                !type.reference_type && !type.rvalue_reference_type && type.referenced_type==dwarf::no_record &&
                dwarf::cxx_language(type.language) && !type.tinygo_producer && !type.zig_producer;
        }
        [[nodiscard]] inline bool value_field(::std::span<dwarf::type_record const> types, field owner,
            ::std::string_view name, ::std::size_t target, field& result)
        {
            if(!member(types,owner,name,result)) { return false; }
            if(same_value_type(types,result.type,target)) { return true; }
            // The library wrapper's template DIE establishes T; its named
            // storage array establishes the exact offset. Never infer T from
            // the number of bytes in an unrelated character array.
            if(owner.type >= types.size() || target >= types.size() || types[owner.type].template_types.empty() ||
               !same_value_type(types,types[owner.type].template_types[0u],target) || types[result.type].kind != dwarf::type_kind::array ||
               !types[result.type].size_known || !types[target].size_known || types[result.type].byte_size < types[target].byte_size)
            { return false; }
            result.type = target; return true;
        }
        [[nodiscard]] inline bool boolean(::std::span<dwarf::type_record const> types,field flag,
            ::std::span<::std::byte const> bytes,::std::span<::std::byte const> known,bool& result)
        {
            ::std::uint64_t value{};
            if(!integer(types,flag,1u) || !bits(types,flag,bytes,known,value) || value > 1u) { return false; }
            result = value != 0u; return true;
        }
        template<class Session>
        [[nodiscard]] outcome selected(Session& work,field payload,::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,
            ::std::vector<dwarf::object_node>& tree,::std::size_t depth,::std::string text,::std::string_view profile)
        {
            if(payload.type >= work.types.size() || !work.types[payload.type].size_known) { return outcome::unavailable; }
            auto const extent{work.types[payload.type].byte_size};
            if(payload.offset > bytes.size() || extent > bytes.size()-payload.offset || extent > 65536u) { return outcome::unavailable; }
            auto const first{static_cast<::std::size_t>(payload.offset)}; auto const count{static_cast<::std::size_t>(extent)};
            if(work.elements < work.cap.max_elements)
            {
                auto const before{tree.size()}; ++work.elements;
                if(!work.append(tree,payload.type,bytes.subspan(first,count),"value",depth,
                    known.empty() ? known : known.subspan(first,count),eligible.empty() ? eligible : eligible.subspan(first,count)))
                { return work.limited ? outcome::limit_exceeded : outcome::unavailable; }
                text += " = "; scalar_text(tree[before],text);
            }
            else
            {
                // The summary may reuse a scalar already in the raw tree.
                // This path does not create a payload read or a synthetic node.
                auto const base{tree[0u].byte_offset};
                for(auto const& node : tree)
                {
                    if(node.type == payload.type && node.byte_offset >= base && node.byte_offset-base == payload.offset)
                    { text += " = "; scalar_text(node,text); break; }
                }
            }
            bool const clipped{text.size() > 4096u};
            if(clipped) { text.resize(4093u); text += "..."; }
            summary(tree[0u],profile,::std::move(text)); tree[0u].pretty_truncated = tree[0u].pretty_truncated || clipped; return outcome::formatted;
        }
        template<class Session>
        [[nodiscard]] outcome adapters(Session& work,::std::size_t root,::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,
            ::std::vector<dwarf::object_node>& tree,::std::size_t depth)
        {
            auto const& type{work.types[root]}; field payload{};
            bool const stack{family(type,"stack")}, queue{family(type,"queue")}, priority{family(type,"priority_queue")};
            if(stack || queue || priority)
            {
                if(type.template_types.size() < 2u || !path(work.types,root,"c",payload) ||
                   !same_value_type(work.types,payload.type,type.template_types[1u])) { return outcome::not_matched; }
                return selected(work,payload,bytes,known,eligible,tree,depth,
                    stack ? "stack" : queue ? "queue" : "priority_queue","C++ container adaptor");
            }
            if(family(type,"duration"))
            {
                if(type.template_types.empty() || !any_path(work.types,root,{"__r","__rep_"},payload) ||
                   !same_value_type(work.types,payload.type,type.template_types[0u])) { return outcome::not_matched; }
                return selected(work,payload,bytes,known,eligible,tree,depth,"duration ticks","C++ chrono duration");
            }
            if(family(type,"time_point"))
            {
                if(type.template_types.size() < 2u || !any_path(work.types,root,{"__d","__d_"},payload) ||
                   !same_value_type(work.types,payload.type,type.template_types[1u])) { return outcome::not_matched; }
                return selected(work,payload,bytes,known,eligible,tree,depth,"time_point since epoch","C++ chrono time_point");
            }
            return outcome::not_matched;
        }
        template<class Session>
        [[nodiscard]] outcome sums(Session& work,::std::size_t root,::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,
            ::std::vector<dwarf::object_node>& tree,::std::size_t depth)
        {
            auto const& type{work.types[root]}; field flag{},payload{};
            if(family(type,"optional"))
            {
                if(type.template_types.empty() || type.template_types[0u] >= work.types.size()) { return outcome::not_matched; }
                if(!any_path(work.types,root,{"_M_payload._M_engaged","__engaged_"},flag)) { return outcome::not_matched; }
                bool engaged{}; if(!boolean(work.types,flag,bytes,known,engaged)) { return outcome::unavailable; }
                if(!engaged) { summary(tree[0u],"C++ optional","nullopt"); return outcome::formatted; }
                if(!any_path(work.types,root,{"_M_payload._M_payload._M_value","__val_"},payload) || !same_value_type(work.types,payload.type,type.template_types[0u]))
                { return outcome::unavailable; }
                return selected(work,payload,bytes,known,eligible,tree,depth,"engaged","C++ optional");
            }
            if(family(type,"variant"))
            {
                if(type.template_types.empty()) { return outcome::not_matched; }
                if(!any_path(work.types,root,{"_M_index","__impl_.__index"},flag) || !integer(work.types,flag)) { return outcome::not_matched; }
                ::std::uint64_t index{}; if(!bits(work.types,flag,bytes,known,index)) { return outcome::unavailable; }
                if(index == dwarf::numeric_mask(work.types[flag.type].byte_count))
                { summary(tree[0u],"C++ variant","valueless_by_exception"); return outcome::formatted; }
                if(index >= type.template_types.size() || index >= 64u) { return outcome::unavailable; }
                auto const target{type.template_types[static_cast<::std::size_t>(index)]};
                if(target >= work.types.size()) { return outcome::unavailable; }
                field branch{}; bool libcxx{};
                if(!path(work.types,root,"_M_u",branch))
                { if(!path(work.types,root,"__impl_.__data",branch)) { return outcome::not_matched; } libcxx = true; }
                for(::std::uint64_t i{}; i != index; ++i)
                { field next{}; if(!member(work.types,branch,libcxx ? "__tail" : "_M_rest",next)) { return outcome::unavailable; } branch = next; }
                field head{};
                if(!member(work.types,branch,libcxx ? "__head" : "_M_first",head) ||
                   !value_field(work.types,head,libcxx ? "__value" : "_M_storage",target,payload)) { return outcome::unavailable; }
                ::std::string text{"index="}; number(text,index); text += " "; text += work.types[target].name;
                return selected(work,payload,bytes,known,eligible,tree,depth,::std::move(text),libcxx ? "libc++ active variant" : "libstdc++ active variant");
            }
            if(family(type,"expected"))
            {
                if(type.template_types.size() < 2u) { return outcome::not_matched; }
                if(!any_path(work.types,root,{"_M_has_value","__has_val_","__repr_.__v.__has_val_"},flag)) { return outcome::not_matched; }
                bool has_value{}; if(!boolean(work.types,flag,bytes,known,has_value)) { return outcome::unavailable; }
                auto const target{type.template_types[has_value ? 0u : 1u]};
                // An absent template DW_AT_type denotes void, like shared_ptr<void>.
                // A proven active void value has no payload and grants no read.
                if(has_value && target == dwarf::no_record)
                { summary(tree[0u],"C++ expected","value (void)"); return outcome::formatted; }
                if(target >= work.types.size()) { return outcome::unavailable; }
                if(has_value && work.types[target].kind == dwarf::type_kind::unavailable && work.types[target].name == "void")
                { summary(tree[0u],"C++ expected","value (void)"); return outcome::formatted; }
                bool const found{has_value ? any_path(work.types,root,{"_M_val","__union_.__val_","__repr_.__v.__union_.__v.__val_"},payload) :
                    any_path(work.types,root,{"_M_unex","__union_.__unex_","__repr_.__v.__union_.__v.__unex_"},payload)};
                if(!found || !same_value_type(work.types,payload.type,target)) { return outcome::unavailable; }
                return selected(work,payload,bytes,known,eligible,tree,depth,has_value ? "value" : "error","C++ expected");
            }
            return outcome::not_matched;
        }
        template<class Session>
        [[nodiscard]] outcome shared(Session& work,::std::size_t root,::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,
            ::std::vector<dwarf::object_node>& tree,::std::size_t depth)
        {
            auto const& type{work.types[root]}; bool const weak{family(type,"weak_ptr")};
            if(!weak && !family(type,"shared_ptr")) { return outcome::not_matched; }
            field stored{},control{}; bool libcxx{};
            if(!path(work.types,root,"_M_ptr",stored) || !path(work.types,root,"_M_refcount._M_pi",control))
            {
                if(!path(work.types,root,"__ptr_",stored) || !path(work.types,root,"__cntrl_",control)) { return outcome::not_matched; }
                libcxx = true;
            }
            if(type.template_types.empty()) { return outcome::unavailable; }
            auto const declared{type.template_types[0u]};
            bool const array{declared < work.types.size() && work.types[declared].kind == dwarf::type_kind::array};
            auto const expected{array ? work.types[declared].referenced_type : declared};
            if((expected != dwarf::no_record && expected >= work.types.size()) || stored.type >= work.types.size() ||
               !pointer(work.types,control,work.width)) { return outcome::unavailable; }
            auto const& stored_type{work.types[stored.type]};
            if(stored_type.kind != dwarf::type_kind::pointer || stored_type.reference_type || stored_type.rvalue_reference_type ||
               !stored_type.size_known || stored_type.byte_size != work.width || stored_type.byte_count != work.width ||
               (stored_type.address_class_known && stored_type.address_class != 0u) || (stored_type.referenced_type != expected && !same_value_type(work.types,stored_type.referenced_type,expected) &&
                !(known_void(work.types,expected) && known_void(work.types,stored_type.referenced_type))))
            { return outcome::unavailable; }
            ::std::uint64_t address{},block{};
            if(!bits(work.types,stored,bytes,known,address) || !bits(work.types,control,bytes,known,block) ||
               (!eligible.empty() && (!bits(work.types,stored,bytes,eligible,address) || !bits(work.types,control,bytes,eligible,block))))
            { return outcome::unavailable; }
            ::std::uint64_t use{},weak_count{};
            if(block != 0u)
            {
                auto target{work.types[control.type].referenced_type};
                if(target>=work.types.size()) { return outcome::unavailable; }
                if(!work.types[target].size_known)
                {
                    ::std::size_t completed{};
                    if(!libcxx || !dwarf::complete_cpp_class(work.types,target,completed)) { return outcome::unavailable; }
                    target=completed;
                }
                auto const& block_type{work.types[target]};
                if(!block_type.size_known || block_type.byte_size == 0u || block_type.byte_size > work.cap.max_bytes) { return outcome::not_matched; }
                field owners{},observers{};
                if(!path(work.types,target,libcxx ? "__shared_owners_" : "_M_use_count",owners) ||
                   !path(work.types,target,libcxx ? "__shared_weak_owners_" : "_M_weak_count",observers) ||
                   !integer(work.types,owners,libcxx ? work.width : 4u) || !integer(work.types,observers,libcxx ? work.width : 4u) ||
                   work.types[owners.type].encoding != 5u || work.types[observers.type].encoding != 5u ||
                   work.types[owners.type].atomic_scalar || work.types[observers.type].atomic_scalar)
                { return outcome::unavailable; }
                ::std::vector<::std::byte> data{};
                if(!work.read(block,static_cast<::std::size_t>(block_type.byte_size),data)) { return work.limited ? outcome::limit_exceeded : outcome::unavailable; }
                if(!bits(work.types,owners,data,{},use) || !bits(work.types,observers,data,{},weak_count)) { return outcome::unavailable; }
                auto const maximum{dwarf::numeric_mask(work.types[owners.type].byte_count)};
                if(libcxx)
                {
                    // libc++ counters store owners minus one (including -1
                    // for expired). This is a copied ABI counter, not a lock().
                    if(use == maximum) { use = 0u; } else if(use < maximum/2u) { ++use; } else { return outcome::unavailable; }
                    auto const weak_max{dwarf::numeric_mask(work.types[observers.type].byte_count)};
                    if(weak_count == weak_max) { if(use != 0u) { return outcome::unavailable; } weak_count = 0u; }
                    else if(weak_count < weak_max/2u) { if(use == 0u) { ++weak_count; } }
                    else { return outcome::unavailable; }
                }
                else
                {
                    if(use > maximum/2u || weak_count > dwarf::numeric_mask(work.types[observers.type].byte_count)/2u ||
                       (use != 0u && weak_count == 0u)) { return outcome::unavailable; }
                    if(use != 0u) { --weak_count; }
                }
            }
            ::std::string text{weak ? "weak_ptr" : "shared_ptr"}; text += " use_count="; number(text,use); text += " weak_count="; number(text,weak_count);
            if(weak) { text += use == 0u ? " expired" : " observed (pointee not read)"; }
            else if(address == 0u) { text += block == 0u ? " empty" : " null stored pointer with ownership"; }
            else if(use == 0u) { text += " stored pointer without ownership"; }
            else if(array) { text += " array extent unavailable"; }
            else if(known_void(work.types,expected)) { text += " stored pointer to void"; }
            else if(work.elements < work.cap.max_elements)
            {
                auto const target{work.types[stored.type].referenced_type}; auto const& value{work.types[target]};
                if(value.size_known && value.byte_size != 0u && value.byte_size <= work.cap.max_bytes)
                {
                    ::std::vector<::std::byte> object{};
                    if(!work.read(address,static_cast<::std::size_t>(value.byte_size),object)) { return work.limited ? outcome::limit_exceeded : outcome::unavailable; }
                    auto const before{tree.size()}; ++work.elements;
                    if(!work.append(tree,target,object,"*",depth)) { return work.limited ? outcome::limit_exceeded : outcome::unavailable; }
                    text += " = "; scalar_text(tree[before],text);
                }
            }
            summary(tree[0u],libcxx ? "libc++ shared ownership" : "libstdc++ shared ownership",::std::move(text)); return outcome::formatted;
        }
    }
    template<class Session>
    [[nodiscard]] outcome extended_values(Session& work,::std::size_t root,::std::span<::std::byte const> bytes,
        ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,
        ::std::vector<dwarf::object_node>& tree,::std::size_t depth)
    {
        auto const adapted{value_details::adapters(work,root,bytes,known,eligible,tree,depth)};
        if(adapted != outcome::not_matched) { return adapted; }
        auto const sum{value_details::sums(work,root,bytes,known,eligible,tree,depth)};
        if(sum != outcome::not_matched) { return sum; }
        return value_details::shared(work,root,bytes,known,eligible,tree,depth);
    }
}
#endif // UWVM_DEBUG_LANGUAGE_CPP
