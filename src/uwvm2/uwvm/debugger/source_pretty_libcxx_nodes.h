/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include "language_debug_features.h"
#if UWVM_DEBUG_LANGUAGE_CPP
#ifndef UWVM_MODULE
# include "source_pretty_sequences.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_pretty_printer::details
{
    // A standalone include of source_pretty_sequences.h enters the core before
    // its own helper definitions. The core includes this nodes header last;
    // forward declarations keep that valid include order independent of which
    // public extension header the caller selected first.
    namespace sequence_details
    {
        [[nodiscard]] inline bool unsigned_type(::std::span<dwarf::type_record const>,::std::size_t,unsigned) noexcept;
        template<class Session> [[nodiscard]] bool pointer_bits(Session&,field,::std::span<::std::byte const>,
            ::std::span<::std::byte const>,::std::span<::std::byte const>,::std::uint64_t&) noexcept;
        [[nodiscard]] inline member_lookup base_walk(::std::span<dwarf::type_record const>,::std::size_t,::std::size_t,
            ::std::size_t&,unsigned,member_ancestor const*) noexcept;
        [[nodiscard]] inline bool first_path(::std::span<dwarf::type_record const>,::std::size_t,
            ::std::initializer_list<::std::string_view>,field&) noexcept;
        inline void add_preview(::std::vector<dwarf::object_node> const&,::std::size_t,::std::uint64_t,::std::string&);
    }
    namespace libcxx_node_details
    {
        [[nodiscard]] inline bool same_declaration(::std::span<dwarf::type_record const> types,::std::size_t left,::std::size_t right) noexcept
        {
            if(left>=types.size() || right>=types.size()) { return false; }
            auto const& a{types[left]};auto const& b{types[right]};
            if(a.atomic_scalar || b.atomic_scalar) { return false; }
            if(left==right) { return true; }
            // Typedef/cv aliases can have distinct record indices while naming
            // one final CU-owned DIE. Equal names or sizes are not evidence.
            return a.declaration_identity!=dwarf::die_key{} && a.declaration_identity==b.declaration_identity &&
                a.kind==b.kind && a.size_known==b.size_known && a.byte_size==b.byte_size &&
                a.byte_count==b.byte_count && a.encoding==b.encoding && a.reference_type==b.reference_type &&
                a.rvalue_reference_type==b.rvalue_reference_type && a.language==b.language &&
                dwarf::cxx_language(a.language) && !a.tinygo_producer && !b.tinygo_producer && !a.zig_producer && !b.zig_producer;
        }
        template<class Session> [[nodiscard]] outcome apply(Session& work,::std::size_t root,::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,::std::vector<dwarf::object_node>& tree,::std::size_t depth,
            bool ordered,bool hashed,bool forward)
        {
            auto const& type{work.types[root]};field owner{root,0u},head{},tail{},size{};::std::size_t element{dwarf::no_record};
            bool const counted{!forward};
            if(ordered || hashed)
            {
                if(!path(work.types,root,ordered ? "__tree_" : "__table_",owner)) { return outcome::not_matched; }
                if(work.types[owner.type].template_types.empty()) { return outcome::unavailable; }
                element=work.types[owner.type].template_types[0u];
                field first{};
                if(ordered)
                {
                    if(!sequence_details::first_path(work.types,root,{"__tree_.__end_node_","__tree_.__pair1_"},first) ||
                       !member(work.types,first,"__left_",head) ||
                       !sequence_details::first_path(work.types,root,{"__tree_.__size_","__tree_.__pair3_"},size))
                    { return outcome::unavailable; }
                }
                else if(!sequence_details::first_path(work.types,root,{"__table_.__first_node_","__table_.__p1_"},first) ||
                    !member(work.types,first,"__next_",head) ||
                    !sequence_details::first_path(work.types,root,{"__table_.__size_","__table_.__p2_"},size))
                { return outcome::unavailable; }
            }
            else
            {
                if(type.template_types.empty()) { return outcome::unavailable; }element=type.template_types[0u];
                if(forward)
                {
                    field first{};
                    if(!sequence_details::first_path(work.types,root,{"__before_begin_"},first)) { return outcome::not_matched; }
                    if(!member(work.types,first,"__next_",head)) { return outcome::unavailable; }
                }
                else if(!path(work.types,root,"__end_.__next_",head)) { return outcome::not_matched; }
                else if(!path(work.types,root,"__end_.__prev_",tail) ||
                    !sequence_details::first_path(work.types,root,{"__size_","__size_alloc_"},size))
                { return outcome::unavailable; }
            }
            if(element>=work.types.size() || !pointer(work.types,head,work.width) ||
               (counted && !sequence_details::unsigned_type(work.types,size.type,work.width))) { return outcome::unavailable; }
            ::std::uint64_t current{},count{},last{};
            if(!sequence_details::pointer_bits(work,head,bytes,known,eligible,current) ||
               (counted && !bits(work.types,size,bytes,known,count)) || (!ordered && !hashed && !forward &&
                   (!pointer(work.types,tail,work.width) || !sequence_details::pointer_bits(work,tail,bytes,known,eligible,last)))) { return outcome::unavailable; }
            if((forward && current==0u) || (counted && count==0u))
            {
                if(!forward && ((ordered || hashed) ? current!=0u : current!=last)) { return outcome::unavailable; }
                summary(tree[0u],"libc++ linked container","size=0 []");return outcome::formatted;
            }
            ::std::string_view const node_name{ordered ? "__tree_node" : hashed ? "__hash_node" : forward ? "__forward_list_node" : "__list_node"};
            ::std::size_t node{dwarf::no_record};
            for(::std::size_t i{};i!=work.types.size();++i)
            {
                auto const& candidate{work.types[i]};
                if(!family(candidate,node_name) || candidate.template_types.empty() || !same_declaration(work.types,candidate.template_types[0u],element) ||
                   !candidate.size_known || candidate.byte_size==0u || candidate.byte_size>work.cap.max_bytes) { continue; }
                if(node!=dwarf::no_record && (candidate.declaration_identity==dwarf::die_key{} ||
                    candidate.declaration_identity!=work.types[node].declaration_identity)) { return outcome::unavailable; }node=i;
            }
            ::std::size_t walked{};field payload{},next{},previous{},left{},right{};
            if(node==dwarf::no_record || sequence_details::base_walk(work.types,node,work.types[head.type].referenced_type,walked,0u,nullptr)!=member_lookup::found ||
               !path(work.types,node,"__value_",payload)) { return outcome::unavailable; }
            bool const wrapper{family(work.types[element],"__value_type") || family(work.types[element],"__hash_value_type")};
            if(wrapper)
            {
                field pair{};auto const& nominal{work.types[element]};
                bool const has_pair{any_path(work.types,element,{"__cc","__cc_"},pair)};
                if(same_declaration(work.types,payload.type,element))
                {
                    if(!has_pair || pair.offset>(~::std::uint64_t{})-payload.offset) { return outcome::unavailable; }
                    payload={pair.type,payload.offset+pair.offset};
                }
                else if((has_pair && !same_declaration(work.types,pair.type,payload.type)) || (!has_pair && (nominal.size_known || !nominal.members.empty())))
                { return outcome::unavailable; }
                // Old libc++ stores a nominal wrapper containing __cc_; new
                // libc++ stores pair<const K,V> directly behind an incomplete
                // wrapper declaration. Both require the owning map's real K/V.
                auto const& stored{work.types[payload.type]};
                if(type.template_types.size()<2u || stored.atomic_scalar || !family(stored,"pair") || stored.template_types.size()!=2u ||
                   !same_declaration(work.types,type.template_types[0u],stored.template_types[0u]) ||
                   !same_declaration(work.types,type.template_types[1u],stored.template_types[1u])) { return outcome::unavailable; }
                element=payload.type;
            }
            else if(!same_declaration(work.types,payload.type,element)) { return outcome::unavailable; }
            auto const& value_type{work.types[element]};
            if(!value_type.size_known || value_type.byte_size==0u || value_type.byte_size>work.cap.max_bytes ||
               payload.offset>work.types[node].byte_size || value_type.byte_size>work.types[node].byte_size-payload.offset) { return outcome::unavailable; }
            if(ordered)
            {
                if(!path(work.types,node,"__left_",left) || !path(work.types,node,"__right_",right) || !pointer(work.types,left,work.width) || !pointer(work.types,right,work.width) ||
                   work.types[left.type].referenced_type!=work.types[head.type].referenced_type ||
                   work.types[right.type].referenced_type!=work.types[head.type].referenced_type) { return outcome::unavailable; }
            }
            else if(!path(work.types,node,"__next_",next) || !pointer(work.types,next,work.width) ||
                work.types[next.type].referenced_type!=work.types[head.type].referenced_type) { return outcome::unavailable; }
            if(!ordered && !hashed && !forward && (!path(work.types,node,"__prev_",previous) || !pointer(work.types,previous,work.width) ||
                work.types[previous.type].referenced_type!=work.types[head.type].referenced_type)) { return outcome::unavailable; }
            struct saved { ::std::uint64_t address{};::std::vector<::std::byte> bytes{}; };
            ::std::vector<saved> stack{};::std::vector<::std::uint64_t> visited{};::std::uint64_t shown{},prior{},sentinel{};bool incomplete{};::std::string text{"["};
            while((!counted || shown<count) && (ordered ? current!=0u || !stack.empty() : current!=0u) &&
                work.elements<work.cap.max_elements && work.nodes<work.cap.max_nodes)
            {
                ::std::vector<::std::byte> copied{};auto address{current};
                if(ordered)
                {
                    while(current!=0u)
                    {
                        if(::std::find(visited.begin(),visited.end(),current)!=visited.end()) { return outcome::unavailable; }
                        if(!work.read(current,static_cast<::std::size_t>(work.types[node].byte_size),copied))
                        { if(work.limited) { incomplete=true;break; }return outcome::unavailable; }
                        visited.push_back(current);::std::uint64_t child{};if(!bits(work.types,left,copied,{},child)) { return outcome::unavailable; }
                        stack.push_back({current,::std::move(copied)});current=child;
                    }
                    if(incomplete || stack.empty()) { break; }address=stack.back().address;copied=::std::move(stack.back().bytes);stack.pop_back();
                    if(!bits(work.types,right,copied,{},current)) { return outcome::unavailable; }
                }
                else
                {
                    if(::std::find(visited.begin(),visited.end(),current)!=visited.end()) { return outcome::unavailable; }
                    if(!work.read(current,static_cast<::std::size_t>(work.types[node].byte_size),copied))
                    { if(work.limited) { incomplete=true;break; }return outcome::unavailable; }
                    visited.push_back(current);if(!bits(work.types,next,copied,{},current)) { return outcome::unavailable; }
                    if(!hashed && !forward)
                    {
                        ::std::uint64_t back{};if(!bits(work.types,previous,copied,{},back)) { return outcome::unavailable; }
                        if(shown==0u) { sentinel=back;if(sentinel==0u || sentinel==address) { return outcome::unavailable; } }
                        else if(back!=prior || address==sentinel) { return outcome::unavailable; }
                    }
                    prior=address;
                }
                ::std::string label{"["};number(label,shown);label+=']';auto const before{tree.size()};++work.elements;
                if(!work.append(tree,element,::std::span<::std::byte const>{copied}.subspan(static_cast<::std::size_t>(payload.offset),static_cast<::std::size_t>(value_type.byte_size)),::std::move(label),depth))
                { incomplete=true;break; }
                sequence_details::add_preview(tree,before,shown,text);++shown;
            }
            if(counted && shown==count)
            {
                if(ordered ? current!=0u || !stack.empty() : hashed ? current!=0u : prior!=last || current!=sentinel ||
                    ::std::find(visited.begin(),visited.end(),sentinel)!=visited.end()) { return outcome::unavailable; }
            }
            else if(!forward && !incomplete && work.elements<work.cap.max_elements && work.nodes<work.cap.max_nodes)
            { return outcome::unavailable; }
            if(forward && current!=0u && ::std::find(visited.begin(),visited.end(),current)!=visited.end()) { return outcome::unavailable; }
            bool const truncated{incomplete || (counted ? shown<count : current!=0u)};
            if(truncated || shown>8u) { if(shown!=0u) { text+=", "; }text+="..."; }text+=']';
            ::std::string result{counted || !truncated ? "size=" : "size=unknown (copied prefix) "};
            if(counted || !truncated) { number(result,counted ? count : shown);result+=' '; }result+=text;
            summary(tree[0u],ordered ? "libc++ ordered tree" : hashed ? "libc++ hash chain" : forward ? "libc++ forward_list" : "libc++ list",
                ::std::move(result),counted ? count-shown : 0u);tree[0u].pretty_truncated|=truncated;return outcome::formatted;
        }
    }
    template<class Session> [[nodiscard]] outcome extended_libcxx_nodes(Session& work,::std::size_t root,
        ::std::span<::std::byte const> bytes,::std::span<::std::byte const> known,::std::span<::std::byte const> pointer_known,
        ::std::vector<dwarf::object_node>& tree,::std::size_t depth)
    {
        if(root>=work.types.size() || tree.empty() || tree[0u].type!=root || (work.width!=4u && work.width!=8u) ||
           (!known.empty() && known.size()!=bytes.size()) || (!pointer_known.empty() && pointer_known.size()!=bytes.size())) { return outcome::unavailable; }
        auto const& type{work.types[root]};bool const ordered{family(type,"map") || family(type,"multimap") || family(type,"set") || family(type,"multiset")};
        bool const hashed{family(type,"unordered_map") || family(type,"unordered_multimap") || family(type,"unordered_set") || family(type,"unordered_multiset")};
        bool const forward{family(type,"forward_list")};if(!ordered && !hashed && !forward && !family(type,"list")) { return outcome::not_matched; }
        auto pending{tree};auto const result{libcxx_node_details::apply(work,root,bytes,known,pointer_known,pending,depth,ordered,hashed,forward)};
        if(result==outcome::formatted) { tree=::std::move(pending); }return result;
    }
}
#endif // UWVM_DEBUG_LANGUAGE_CPP
