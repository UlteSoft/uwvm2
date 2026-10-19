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
    namespace sequence_details
    {
        [[nodiscard]] inline bool unsigned_type(::std::span<dwarf::type_record const> types,::std::size_t index,unsigned width=0u) noexcept
        {
            if(index>=types.size()) { return false; }auto const& t{types[index]};
            return t.kind==dwarf::type_kind::scalar && t.size_known && t.byte_count==t.byte_size && !t.atomic_scalar &&
                (t.byte_size==1u || t.byte_size==2u || t.byte_size==4u || t.byte_size==8u) &&
                (width==0u || t.byte_size==width) && (t.encoding==7u || t.encoding==8u);
        }
        [[nodiscard]] inline bool template_count(::std::span<dwarf::type_record const> types,::std::size_t root,
            ::std::size_t position,unsigned width,::std::uint64_t& count) noexcept
        {
            if(root>=types.size()) { return false; }bool found{};
            for(auto const& value:types[root].template_values)
            {
                if(value.position!=position) { continue; }
                if(found || !value.available || !unsigned_type(types,value.type,width) ||
                   (value.signed_value && (value.bits>>63u)!=0u) ||
                   (value.bits & ~dwarf::numeric_mask(static_cast<::std::uint8_t>(width)))!=0u) { return false; }
                found=true;count=value.bits;
            }
            return found;
        }
        // LLVM 18 compressed pairs inherit two element carriers. A nonempty
        // allocator/comparator gives both carriers a member named __value_.
        // Select ordinal zero from actual template DIEs, never global lookup.
        [[nodiscard]] inline bool compressed_first(::std::span<dwarf::type_record const> types,field owner,field& out) noexcept
        {
            if(owner.type>=types.size()) { return false; }auto const& pair{types[owner.type]};
            if(!family(pair,"__compressed_pair") || !pair.size_known || pair.template_types.size()!=2u ||
               pair.template_types[0u]>=types.size()) { return false; }
            auto const first{pair.template_types[0u]};bool found{};::std::size_t visited{};field pending{};
            for(auto const& base:pair.members)
            {
                if(++visited>4096u) { return false; }if(!base.inherited) { continue; }
                if(base.type>=types.size() || !base.offset_known || base.bit_field || !types[base.type].size_known ||
                   base.byte_offset>pair.byte_size || types[base.type].byte_size>pair.byte_size-base.byte_offset ||
                   base.byte_offset>(~::std::uint64_t{})-owner.offset) { return false; }
                auto const& carrier{types[base.type]};
                if(!family(carrier,"__compressed_pair_elem") || carrier.template_types.size()!=1u || carrier.template_types[0u]!=first) { continue; }
                bool ordinal{};
                for(auto const& value:carrier.template_values)
                {
                    if(value.position!=1u) { continue; }
                    if(ordinal || !value.available || !integer(types,{value.type,0u},4u) || types[value.type].encoding!=5u || types[value.type].atomic_scalar || value.bits!=0u) { return false; }
                    ordinal=true;
                }
                if(!ordinal) { return false; }field selected{};
                if(found || !member(types,{base.type,owner.offset+base.byte_offset},"__value_",selected) || selected.type!=first) { return false; }
                pending=selected;found=true;
            }
            if(found) { out=pending; }return found;
        }
        [[nodiscard]] inline bool first_path(::std::span<dwarf::type_record const> types,::std::size_t root,
            ::std::initializer_list<::std::string_view> paths,field& out) noexcept
        {
            for(auto name:paths)
            {
                field owner{};if(!path(types,root,name,owner)) { continue; }
                if(family(types[owner.type],"__compressed_pair")) { return compressed_first(types,owner,out); }
                out=owner;return true;
            }
            return false;
        }
        template<class Session> [[nodiscard]] bool pointer_bits(Session& work,field value,::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,::std::uint64_t& result) noexcept
        {
            if(!pointer(work.types,value,work.width) || !bits(work.types,value,bytes,known,result)) { return false; }
            ::std::uint64_t checked{};
            return eligible.empty() || (bits(work.types,value,bytes,eligible,checked) && checked==result);
        }
        [[nodiscard]] inline bool range(::std::uint64_t address,::std::uint64_t count,::std::uint64_t extent,unsigned width) noexcept
        {
            auto const maximum{width==4u ? 0xffffffffull : ~::std::uint64_t{}};
            return count==0u || (extent!=0u && address!=0u && address<=maximum && extent-1u<=maximum-address &&
                count-1u<=(maximum-address-(extent-1u))/extent);
        }
        [[nodiscard]] inline ::std::uint64_t little(::std::span<::std::byte const> bytes) noexcept
        {
            ::std::uint64_t result{};
            for(::std::size_t i{};i!=bytes.size() && i<8u;++i)
            { result|=static_cast<::std::uint64_t>(::std::to_integer<unsigned char>(bytes[i]))<<(i*8u); }return result;
        }
        template<class Session> [[nodiscard]] bool append_bit(Session& work,::std::vector<dwarf::object_node>& tree,
            ::std::uint64_t index,bool value,::std::size_t depth)
        {
            if(work.elements>=work.cap.max_elements || work.nodes>=work.cap.max_nodes || tree.size()>=work.cap.max_nodes || depth>work.cap.max_depth)
            { work.limited=true;return false; }
            dwarf::object_node node{};node.parent=0u;node.depth=1u;node.name="[";number(node.name,index);node.name+=']';
            node.kind=dwarf::type_kind::scalar;node.type_name="bool";node.scalar_kind=dwarf::numeric_kind::boolean;
            node.scalar_bytes=1u;node.byte_size=1u;node.bits=value;node.value_available=true;node.detached_copy=true;
            tree.push_back(::std::move(node));++work.elements;++work.nodes;return true;
        }
        inline void add_preview(::std::vector<dwarf::object_node> const& tree,::std::size_t before,::std::uint64_t index,::std::string& text)
        { if(index<8u && text.size()<3000u) { if(index!=0u) { text+=", "; }scalar_text(tree[before],text); } }
        inline void finish(::std::vector<dwarf::object_node>& tree,::std::string_view profile,::std::string text,
            ::std::uint64_t count,::std::uint64_t shown)
        {
            if(shown<count || shown>8u) { if(shown!=0u) { text+=", "; }text+="..."; }text+=']';
            summary(tree[0u],profile,::std::move(text),count-shown);
        }
        template<class Session> [[nodiscard]] outcome static_span(Session& work,::std::size_t root,::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,::std::vector<dwarf::object_node>& tree,::std::size_t depth)
        {
            ::std::uint64_t count{};
            if(!template_count(work.types,root,1u,work.width,count)) { return outcome::not_matched; }
            if(count==dwarf::numeric_mask(static_cast<::std::uint8_t>(work.width))) { return outcome::not_matched; }
            field data{};::std::size_t element{};
            if(!static_span_schema(work.types,root,work.width,data,element,count) ||
               work.types[element].byte_size>work.cap.max_bytes) { return outcome::unavailable; }
            auto const& t{work.types[element]};
            ::std::uint64_t address{};
            if(!pointer_bits(work,data,bytes,known,eligible,address) || !range(address,count,t.byte_size,work.width)) { return outcome::unavailable; }
            ::std::string text{"size="};number(text,count);text+=" [";::std::uint64_t shown{};
            auto const remaining{(std::min)(work.elements<work.cap.max_elements ? work.cap.max_elements-work.elements : 0u,
                work.nodes<work.cap.max_nodes ? work.cap.max_nodes-work.nodes : 0u)};
            auto const available{work.copied<work.cap.max_bytes ? work.cap.max_bytes-work.copied : 0u};
            auto const planned{(std::min)(count,static_cast<::std::uint64_t>((std::min)(remaining,static_cast<::std::size_t>(available/t.byte_size))))};
            ::std::vector<::std::byte> copied{};
            if(planned!=0u && !work.read(address,static_cast<::std::size_t>(planned*t.byte_size),copied))
            { if(!work.limited) { return outcome::unavailable; } }
            else for(;shown<planned && work.elements<work.cap.max_elements;++shown)
            {
                ::std::string name{"["};number(name,shown);name+=']';auto const before{tree.size()};++work.elements;
                if(!work.append(tree,element,::std::span<::std::byte const>{copied}.subspan(static_cast<::std::size_t>(shown*t.byte_size),static_cast<::std::size_t>(t.byte_size)),::std::move(name),depth)) { break; }
                add_preview(tree,before,shown,text);
            }
            finish(tree,"C++ static span",::std::move(text),count,shown);return outcome::formatted;
        }
        template<class Session> [[nodiscard]] outcome bitset(Session& work,::std::size_t root,::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known,::std::vector<dwarf::object_node>& tree,::std::size_t depth)
        {
            ::std::uint64_t count{};
            if(!template_count(work.types,root,0u,work.width,count)) { return outcome::unavailable; }
            if(count==0u) { summary(tree[0u],"C++ bitset","size=0 []");return outcome::formatted; }
            field storage{};
            if(!any_path(work.types,root,{"_M_w","__first_"},storage)) { return outcome::unavailable; }
            auto word{storage.type};::std::uint64_t words{1u};auto const& t{work.types[word]};
            if(t.kind==dwarf::type_kind::array)
            {
                if(!t.contiguous_array || !t.row_major_array || t.dimensions.size()!=1u || !t.dimensions[0u].count_known ||
                   !t.dimensions[0u].lower_bound_known || t.dimensions[0u].lower_bound!=0 || t.referenced_type>=work.types.size()) { return outcome::unavailable; }
                words=t.dimensions[0u].count;word=t.referenced_type;
            }
            if(!unsigned_type(work.types,word) || words==0u) { return outcome::unavailable; }
            auto const extent{work.types[word].byte_size};auto const word_bits{extent*8u};
            if((count-1u)/word_bits+1u!=words || !t.size_known || words>t.byte_size/extent || words*extent!=t.byte_size ||
               storage.offset>bytes.size() || t.byte_size>bytes.size()-storage.offset) { return outcome::unavailable; }
            ::std::string text{"size="};number(text,count);text+=" [";::std::uint64_t shown{};
            while(shown<count && work.elements<work.cap.max_elements && work.nodes<work.cap.max_nodes)
            {
                auto const at{static_cast<::std::size_t>(storage.offset+shown/8u)};auto const mask{1u<<(shown%8u)};
                if(!known.empty() && (::std::to_integer<unsigned char>(known[at])&mask)==0u) { return outcome::unavailable; }
                auto const before{tree.size()};if(!append_bit(work,tree,shown,(::std::to_integer<unsigned char>(bytes[at])&mask)!=0u,depth)) { break; }
                add_preview(tree,before,shown,text);++shown;
            }
            finish(tree,"C++ bitset",::std::move(text),count,shown);return outcome::formatted;
        }
        template<class Session> [[nodiscard]] outcome vector_bool(Session& work,::std::size_t root,::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,::std::vector<dwarf::object_node>& tree,::std::size_t depth)
        {
            field start{},finish_pointer{},start_bit{},finish_bit{},capacity{};
            if(!path(work.types,root,"_M_impl._M_start._M_p",start) || !path(work.types,root,"_M_impl._M_start._M_offset",start_bit) ||
               !path(work.types,root,"_M_impl._M_finish._M_p",finish_pointer) || !path(work.types,root,"_M_impl._M_finish._M_offset",finish_bit) ||
               !path(work.types,root,"_M_impl._M_end_of_storage",capacity) || !pointer(work.types,start,work.width) ||
               !pointer(work.types,finish_pointer,work.width) || !pointer(work.types,capacity,work.width) ||
               work.types[finish_pointer.type].referenced_type!=work.types[start.type].referenced_type ||
               work.types[capacity.type].referenced_type!=work.types[start.type].referenced_type ||
               !unsigned_type(work.types,start_bit.type) || !unsigned_type(work.types,finish_bit.type)) { return outcome::unavailable; }
            auto const word{work.types[start.type].referenced_type};
            if(!unsigned_type(work.types,word)) { return outcome::unavailable; }auto const extent{work.types[word].byte_size};auto const width{extent*8u};
            ::std::uint64_t address{},end{},allocated{},first_offset{},last_offset{};
            if(!pointer_bits(work,start,bytes,known,eligible,address) || !pointer_bits(work,finish_pointer,bytes,known,eligible,end) ||
               !pointer_bits(work,capacity,bytes,known,eligible,allocated) || !bits(work.types,start_bit,bytes,known,first_offset) ||
               !bits(work.types,finish_bit,bytes,known,last_offset) || first_offset>=width || last_offset>=width || end<address || allocated<end ||
               (end-address)%extent!=0u || (allocated-address)%extent!=0u || (end==allocated && last_offset!=0u) ||
               (address==0u && (end!=0u || allocated!=0u || first_offset!=0u || last_offset!=0u))) { return outcome::unavailable; }
            auto const words{(end-address)/extent};
            if(words>(~::std::uint64_t{}-last_offset)/width) { return outcome::unavailable; }
            auto const total{words*width+last_offset};if(total<first_offset) { return outcome::unavailable; }auto const count{total-first_offset};
            ::std::string text{"size="};number(text,count);text+=" [";::std::uint64_t shown{};
            auto const remaining{(std::min)(work.elements<work.cap.max_elements ? work.cap.max_elements-work.elements : 0u,
                work.nodes<work.cap.max_nodes ? work.cap.max_nodes-work.nodes : 0u)};
            auto planned{(std::min)(count,static_cast<::std::uint64_t>(remaining))};
            auto const available{work.copied<work.cap.max_bytes ? work.cap.max_bytes-work.copied : 0u};
            auto const max_words{available/extent};
            if(planned!=0u && ((first_offset+planned-1u)/width+1u)>max_words)
            { planned=max_words==0u ? 0u : (std::min)(planned,max_words*width-first_offset); }
            ::std::vector<::std::byte> data{};
            auto const copied_words{planned==0u ? 0u : (first_offset+planned-1u)/width+1u};
            if(copied_words!=0u && (!range(address,copied_words,extent,work.width) || !work.read(address,static_cast<::std::size_t>(copied_words*extent),data)))
            { if(!work.limited) { return outcome::unavailable; } }
            else while(shown<planned)
            {
                auto const offset{first_offset+shown};auto const before{tree.size()};
                if(!append_bit(work,tree,shown,(::std::to_integer<unsigned char>(data[static_cast<::std::size_t>(offset/8u)])&(1u<<(offset%8u)))!=0u,depth)) { break; }
                add_preview(tree,before,shown,text);++shown;
            }
            finish(tree,"libstdc++ vector<bool>",::std::move(text),count,shown);return outcome::formatted;
        }
        [[nodiscard]] inline member_lookup base_walk(::std::span<dwarf::type_record const> types,::std::size_t node,::std::size_t base,
            ::std::size_t& work,unsigned depth,member_ancestor const* ancestors) noexcept
        {
            if(node>=types.size() || base>=types.size() || depth>=16u) { return member_lookup::invalid; }
            for(auto p{ancestors};p!=nullptr;p=p->parent) { if(p->type==node) { return member_lookup::invalid; } }
            if(node==base) { return member_lookup::found; }member_ancestor const active{node,ancestors};bool found{};
            for(auto const& field:types[node].members)
            {
                if(++work>4096u) { return member_lookup::invalid; }if(!field.inherited) { continue; }
                if(!field.offset_known || field.bit_field || field.type>=types.size()) { return member_lookup::invalid; }
                if(field.byte_offset!=0u) { continue; }
                auto const status{base_walk(types,field.type,base,work,depth+1u,::std::addressof(active))};
                if(status==member_lookup::invalid) { return status; }if(status==member_lookup::found)
                { if(found) { return member_lookup::invalid; }found=true; }
            }
            return found ? member_lookup::found : member_lookup::absent;
        }
        template<class Session> [[nodiscard]] outcome forward_list(Session& work,::std::size_t root,::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,::std::vector<dwarf::object_node>& tree,::std::size_t depth)
        {
            auto const& type{work.types[root]};field head{};
            if(type.template_types.empty() || type.template_types[0u]>=work.types.size() ||
               !path(work.types,root,"_M_impl._M_head._M_next",head) || !pointer(work.types,head,work.width)) { return outcome::unavailable; }
            auto const element{type.template_types[0u]};auto const& value_type{work.types[element]};
            if(!value_type.size_known || value_type.byte_size==0u || value_type.byte_size>work.cap.max_bytes) { return outcome::unavailable; }
            ::std::uint64_t current{};if(!pointer_bits(work,head,bytes,known,eligible,current)) { return outcome::unavailable; }
            if(current==0u) { summary(tree[0u],"libstdc++ forward_list","size=0 []");return outcome::formatted; }
            ::std::size_t node{dwarf::no_record};
            for(::std::size_t i{};i!=work.types.size();++i)
            {
                auto const& candidate{work.types[i]};
                if(!family(candidate,"_Fwd_list_node") || candidate.template_types.empty() || candidate.template_types[0u]!=element ||
                   !candidate.size_known || candidate.byte_size==0u || candidate.byte_size>work.cap.max_bytes) { continue; }
                if(node!=dwarf::no_record && (candidate.declaration_identity==dwarf::die_key{} ||
                    work.types[node].declaration_identity!=candidate.declaration_identity)) { return outcome::unavailable; }node=i;
            }
            ::std::size_t checked{};
            if(node==dwarf::no_record || base_walk(work.types,node,work.types[head.type].referenced_type,checked,0u,nullptr)!=member_lookup::found)
            { return outcome::unavailable; }
            field next{},payload{};
            if(!path(work.types,node,"_M_next",next) || !pointer(work.types,next,work.width) ||
               work.types[next.type].referenced_type!=work.types[head.type].referenced_type ||
               !any_path(work.types,node,{"_M_value","_M_storage"},payload)) { return outcome::unavailable; }
            if(payload.type!=element)
            {
                auto const& storage{work.types[payload.type]};field data{};
                if(storage.template_types.empty() || storage.template_types[0u]!=element ||
                   !member(work.types,payload,"_M_storage",data) || work.types[data.type].kind!=dwarf::type_kind::array ||
                   !work.types[data.type].size_known || work.types[data.type].byte_size<value_type.byte_size) { return outcome::unavailable; }
                auto const& array{work.types[data.type]};
                if(!array.contiguous_array || !array.row_major_array || array.dimensions.size()!=1u ||
                   !array.dimensions[0u].lower_bound_known || array.dimensions[0u].lower_bound!=0 || !array.dimensions[0u].count_known ||
                   array.referenced_type>=work.types.size() || !unsigned_type(work.types,array.referenced_type,1u) ||
                   array.byte_size!=array.dimensions[0u].count) { return outcome::unavailable; }
                payload={element,data.offset};
            }
            if(payload.offset>work.types[node].byte_size || value_type.byte_size>work.types[node].byte_size-payload.offset) { return outcome::unavailable; }
            ::std::vector<::std::uint64_t> visited{};::std::uint64_t shown{};::std::string text{"["};bool incomplete{};
            while(current!=0u && work.elements<work.cap.max_elements && work.nodes<work.cap.max_nodes)
            {
                if(::std::find(visited.begin(),visited.end(),current)!=visited.end()) { return outcome::unavailable; }
                ::std::vector<::std::byte> copied{};
                if(!work.read(current,static_cast<::std::size_t>(work.types[node].byte_size),copied))
                { if(work.limited) { break; }return outcome::unavailable; }
                visited.push_back(current);if(!bits(work.types,next,copied,{},current)) { return outcome::unavailable; }
                ::std::string name{"["};number(name,shown);name+=']';auto const before{tree.size()};++work.elements;
                if(!work.append(tree,element,::std::span<::std::byte const>{copied}.subspan(static_cast<::std::size_t>(payload.offset),static_cast<::std::size_t>(value_type.byte_size)),::std::move(name),depth)) { incomplete=true;break; }
                add_preview(tree,before,shown,text);++shown;
            }
            if(current!=0u && ::std::find(visited.begin(),visited.end(),current)!=visited.end()) { return outcome::unavailable; }
            incomplete=incomplete || current!=0u;
            if(incomplete || shown>8u) { if(shown!=0u) { text+=", "; }text+="..."; }text+=']';
            ::std::string summary_text{!incomplete ? "size=" : "size=unknown (copied prefix) "};
            if(!incomplete) { number(summary_text,shown);summary_text+=' '; }summary_text+=text;
            summary(tree[0u],"libstdc++ forward_list",::std::move(summary_text));tree[0u].pretty_truncated|=incomplete;return outcome::formatted;
        }
        struct deque_iterator
        { field cur{},first{},last{},node{};::std::uint64_t c{},f{},l{},n{}; };
        template<class Session> [[nodiscard]] bool deque_metadata(Session& work,::std::size_t root,::std::string_view prefix,
            ::std::span<::std::byte const> bytes,::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,
            ::std::size_t element,deque_iterator& out)
        {
            auto select{[&](::std::string_view name,field& value)
            { ::std::string path_name{prefix};path_name+=name;return path(work.types,root,path_name,value); }};
            if(!select("._M_cur",out.cur) || !select("._M_first",out.first) || !select("._M_last",out.last) || !select("._M_node",out.node)) { return false; }
            for(auto pointer_field:{out.cur,out.first,out.last})
            { if(!pointer(work.types,pointer_field,work.width) || work.types[pointer_field.type].referenced_type!=element) { return false; } }
            if(!pointer(work.types,out.node,work.width)) { return false; }
            auto const slot{work.types[out.node.type].referenced_type};
            return pointer(work.types,{slot,0u},work.width) && work.types[slot].referenced_type==element &&
                pointer_bits(work,out.cur,bytes,known,eligible,out.c) && pointer_bits(work,out.first,bytes,known,eligible,out.f) &&
                pointer_bits(work,out.last,bytes,known,eligible,out.l) && pointer_bits(work,out.node,bytes,known,eligible,out.n);
        }
        template<class Session> [[nodiscard]] outcome deque(Session& work,::std::size_t root,::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,::std::vector<dwarf::object_node>& tree,::std::size_t depth)
        {
            auto const& type{work.types[root]};field map{},map_size{};
            if(type.template_types.empty() || type.template_types[0u]>=work.types.size() ||
               !path(work.types,root,"_M_impl._M_map",map) || !path(work.types,root,"_M_impl._M_map_size",map_size) ||
               !pointer(work.types,map,work.width) || !unsigned_type(work.types,map_size.type,work.width)) { return outcome::unavailable; }
            auto const element{type.template_types[0u]};auto const& t{work.types[element]};
            if(!t.size_known || t.byte_size==0u || t.byte_size>work.cap.max_bytes) { return outcome::unavailable; }
            auto const slot{work.types[map.type].referenced_type};
            if(!pointer(work.types,{slot,0u},work.width) || work.types[slot].referenced_type!=element) { return outcome::unavailable; }
            deque_iterator first{},last{};::std::uint64_t map_address{},slots{};
            if(!deque_metadata(work,root,"_M_impl._M_start",bytes,known,eligible,element,first) ||
               !deque_metadata(work,root,"_M_impl._M_finish",bytes,known,eligible,element,last) ||
               !pointer_bits(work,map,bytes,known,eligible,map_address) || !bits(work.types,map_size,bytes,known,slots)) { return outcome::unavailable; }
            if(slots==0u || !range(map_address,slots,work.width,work.width) || first.n<map_address || last.n<first.n ||
               (first.n-map_address)%work.width!=0u || (last.n-map_address)%work.width!=0u ||
               (first.n-map_address)/work.width>=slots || (last.n-map_address)/work.width>=slots ||
               first.f==0u || last.f==0u || first.l<=first.f || last.l<=last.f || first.l-first.f!=last.l-last.f ||
               (first.l-first.f)%t.byte_size!=0u || first.c<first.f || first.c>=first.l || last.c<last.f || last.c>=last.l ||
               (first.c-first.f)%t.byte_size!=0u || (last.c-last.f)%t.byte_size!=0u ||
               (first.n==last.n && (first.f!=last.f || first.l!=last.l || last.c<first.c)) ||
               (first.n!=last.n && first.f==last.f)) { return outcome::unavailable; }
            auto const block_bytes{first.l-first.f};auto const block_elements{block_bytes/t.byte_size};
            if(block_bytes>65536u || !range(first.f,block_elements,t.byte_size,work.width) || !range(last.f,block_elements,t.byte_size,work.width)) { return outcome::unavailable; }
            auto const distance{(last.n-first.n)/work.width};auto const start_index{(first.c-first.f)/t.byte_size};auto const finish_index{(last.c-last.f)/t.byte_size};
            if(distance>(~::std::uint64_t{})/block_elements) { return outcome::unavailable; }
            auto count{distance*block_elements};
            if(count<start_index) { if(distance!=0u || finish_index<start_index) { return outcome::unavailable; } count=finish_index-start_index; }
            else { count-=start_index;if(finish_index>(~::std::uint64_t{})-count) { return outcome::unavailable; }count+=finish_index; }
            ::std::string text{"size="};number(text,count);text+=" [";::std::uint64_t shown{};
            if(count==0u) { finish(tree,"libstdc++ deque",::std::move(text),0u,0u);return outcome::formatted; }
            bool last_verified{};
            if(first.n!=last.n && work.elements<work.cap.max_elements && work.nodes<work.cap.max_nodes)
            {
                ::std::vector<::std::byte> entry{};
                if(!work.read(last.n,work.width,entry))
                { if(!work.limited) { return outcome::unavailable; }finish(tree,"libstdc++ deque",::std::move(text),count,0u);return outcome::formatted; }
                if(little(entry)!=last.f) { return outcome::unavailable; }last_verified=true;
            }
            ::std::uint64_t current_node{first.n},index{start_index};::std::vector<::std::uint64_t> visited{};
            while(shown<count && work.elements<work.cap.max_elements && work.nodes<work.cap.max_nodes)
            {
                ::std::uint64_t address{last.f};
                if(current_node!=last.n || !last_verified)
                {
                    ::std::vector<::std::byte> entry{};
                    if(!work.read(current_node,work.width,entry)) { if(work.limited) { break; }return outcome::unavailable; }
                    address=little(entry);
                }
                if(!range(address,block_elements,t.byte_size,work.width) ||
                   (current_node==first.n && address!=first.f) || (current_node==last.n && address!=last.f) ||
                   (current_node!=last.n && address==last.f) ||
                   ::std::find(visited.begin(),visited.end(),address)!=visited.end()) { return outcome::unavailable; }visited.push_back(address);
                auto const remaining{work.elements<work.cap.max_elements ? work.cap.max_elements-work.elements : 0u};
                auto const available{work.copied<work.cap.max_bytes ? work.cap.max_bytes-work.copied : 0u};
                auto batch{(std::min)(block_elements-index,count-shown)};
                batch=(std::min)(batch,static_cast<::std::uint64_t>((std::min)(remaining,static_cast<::std::size_t>(available/t.byte_size))));
                if(batch==0u) { work.limited=true;break; }::std::vector<::std::byte> data{};
                if(!work.read(address+index*t.byte_size,static_cast<::std::size_t>(batch*t.byte_size),data))
                { if(work.limited) { break; }return outcome::unavailable; }
                ::std::uint64_t consumed{};
                for(;consumed<batch && work.elements<work.cap.max_elements;++consumed)
                {
                    ::std::string name{"["};number(name,shown);name+=']';auto const before{tree.size()};++work.elements;
                    if(!work.append(tree,element,::std::span<::std::byte const>{data}.subspan(static_cast<::std::size_t>(consumed*t.byte_size),static_cast<::std::size_t>(t.byte_size)),::std::move(name),depth)) { break; }
                    add_preview(tree,before,shown,text);++shown;
                }
                if(consumed!=batch || batch<block_elements-index || shown==count) { break; }
                current_node+=work.width;index=0u;
            }
            finish(tree,"libstdc++ deque",::std::move(text),count,shown);return outcome::formatted;
        }
        template<class Session> [[nodiscard]] outcome libcxx_packed(Session& work,::std::size_t root,::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,::std::vector<dwarf::object_node>& tree,::std::size_t depth)
        {
            field data{},size{},cap{};
            if(!path(work.types,root,"__begin_",data) || !path(work.types,root,"__size_",size) ||
               !first_path(work.types,root,{"__cap_","__cap_alloc_"},cap) || !pointer(work.types,data,work.width) ||
               !unsigned_type(work.types,size.type,work.width) || !unsigned_type(work.types,cap.type,work.width)) { return outcome::unavailable; }
            auto const word{work.types[data.type].referenced_type};if(!unsigned_type(work.types,word)) { return outcome::unavailable; }
            auto const extent{work.types[word].byte_size};auto const word_bits{extent*8u};::std::uint64_t address{},count{},words{};
            if(!pointer_bits(work,data,bytes,known,eligible,address) || !bits(work.types,size,bytes,known,count) || !bits(work.types,cap,bytes,known,words) ||
               words>(~::std::uint64_t{})/word_bits || count>words*word_bits || !range(address,words,extent,work.width) ||
               (address==0u && (count!=0u || words!=0u))) { return outcome::unavailable; }
            auto const remaining{(std::min)(work.elements<work.cap.max_elements ? work.cap.max_elements-work.elements : 0u,
                work.nodes<work.cap.max_nodes ? work.cap.max_nodes-work.nodes : 0u)};
            auto planned{(std::min)(count,static_cast<::std::uint64_t>(remaining))};auto const available{work.copied<work.cap.max_bytes ? work.cap.max_bytes-work.copied : 0u};
            planned=(std::min)(planned,static_cast<::std::uint64_t>(available/extent)*word_bits);
            auto const copy_words{planned==0u ? 0u : (planned-1u)/word_bits+1u};::std::vector<::std::byte> copied{};::std::uint64_t shown{};
            ::std::string text{"size="};number(text,count);text+=" [";
            if(copy_words!=0u && !work.read(address,static_cast<::std::size_t>(copy_words*extent),copied))
            { if(!work.limited) { return outcome::unavailable; } }
            else while(shown<planned)
            {
                auto const before{tree.size()};if(!append_bit(work,tree,shown,(::std::to_integer<unsigned char>(copied[shown/8u])&(1u<<(shown%8u)))!=0u,depth)) { break; }
                add_preview(tree,before,shown,text);++shown;
            }
            finish(tree,"libc++ vector<bool>",::std::move(text),count,shown);return outcome::formatted;
        }
        template<class Session> [[nodiscard]] bool libcxx_block_count(Session& work,::std::size_t root,::std::size_t element,::std::uint64_t& result)
        {
            if(root>=work.types.size() || element>=work.types.size() || !work.types[element].size_known ||
               work.types[element].byte_size==0u || (work.width!=4u && work.width!=8u) || work.types[root].atomic_scalar)
            { return false; }
            auto const extent{work.types[element].byte_size};
            auto const normal{extent<256u ? 4096u/extent : 16u};auto const small{extent<128u ? 512u/extent : 4u};
            auto const declared{[](::std::span<dwarf::static_constant_record const> values) noexcept
            { for(auto const& value:values) { if(value.name=="__block_size") { return true; } }return false; }};
            auto const decode{[&](::std::span<dwarf::static_constant_record const> values,::std::uint64_t& decoded) noexcept
            {
                bool found{};
                for(auto const& value:values)
                {
                    if(value.name!="__block_size") { continue; }
                    if(found || !value.available || value.type>=work.types.size() || value.bits==0u ||
                       (value.signed_value && value.bits>>63u)!=0u || (value.bits!=normal && value.bits!=small)) { return false; }
                    auto const& integral{work.types[value.type]};
                    if(integral.kind!=dwarf::type_kind::scalar || integral.atomic_scalar || !integral.size_known ||
                       integral.byte_size!=work.width || integral.byte_count!=work.width ||
                       (integral.encoding!=5u && integral.encoding!=7u) ||
                       (value.bits & ~dwarf::numeric_mask(static_cast<::std::uint8_t>(work.width)))!=0u) { return false; }
                    decoded=value.bits;found=true;
                }
                return found;
            }};
            auto const& owner{work.types[root]};
            // A same-T iterator elsewhere in the CU does not establish this
            // deque's ABI. Prefer a constant owned by this exact root type.
            if(declared(owner.static_constants)) { return decode(owner.static_constants,result); }
            if(family(owner,"deque") && (owner.libcxx18_deque_blocks || owner.libcxx23_abi2_small_deque_blocks))
            {
                if(owner.libcxx18_deque_blocks && owner.libcxx23_abi2_small_deque_blocks) { return false; }
                result=owner.libcxx23_abi2_small_deque_blocks ? small : normal;return true;
            }
            if(owner.atomic_scalar || (!owner.named_type_alias && owner.display_qualifiers==0u) ||
               owner.declaration_identity==dwarf::die_key{}) { return false; }
            bool found{};
            for(auto const& candidate:work.types)
            {
                // Typedef/cv records can precede static definition collection.
                // Only the same final aggregate DIE can supply missing data;
                // same-name and same-element types never supply ABI evidence.
                if(candidate.named_type_alias || candidate.atomic_scalar || candidate.display_qualifiers!=0u || candidate.kind!=owner.kind ||
                   candidate.declaration_identity!=owner.declaration_identity || !declared(candidate.static_constants)) { continue; }
                ::std::uint64_t decoded{};
                if(!decode(candidate.static_constants,decoded) || (found && result!=decoded)) { return false; }
                result=decoded;found=true;
            }
            return found;
        }
        template<class Session> [[nodiscard]] outcome libcxx_deque(Session& work,::std::size_t root,::std::span<::std::byte const> bytes,
            ::std::span<::std::byte const> known,::std::span<::std::byte const> eligible,::std::vector<dwarf::object_node>& tree,::std::size_t depth)
        {
            auto const& type{work.types[root]};field first{},begin{},end{},capacity{},start{},size{};
            if(type.template_types.empty() || type.template_types[0u]>=work.types.size() ||
               !any_path(work.types,root,{"__map_.__front_cap_","__map_.__first_"},first) || !path(work.types,root,"__map_.__begin_",begin) ||
               !path(work.types,root,"__map_.__end_",end) || !first_path(work.types,root,{"__map_.__back_cap_","__map_.__end_cap_"},capacity) ||
               !path(work.types,root,"__start_",start) || !first_path(work.types,root,{"__size_","__size_alloc_"},size) ||
               !unsigned_type(work.types,start.type,work.width) || !unsigned_type(work.types,size.type,work.width)) { return outcome::unavailable; }
            auto const element{type.template_types[0u]};auto const extent{work.types[element].byte_size};
            if(!work.types[element].size_known || extent==0u || extent>work.cap.max_bytes || !pointer(work.types,begin,work.width)) { return outcome::unavailable; }
            auto const slot{work.types[begin.type].referenced_type};
            if(!pointer(work.types,{slot,0u},work.width) || work.types[slot].referenced_type!=element) { return outcome::unavailable; }
            for(auto value:{first,end,capacity})
            { if(!pointer(work.types,value,work.width) || work.types[value.type].referenced_type!=slot) { return outcome::unavailable; } }
            ::std::uint64_t allocation{},map{},finish_map{},allocated_end{},offset{},count{};
            if(!pointer_bits(work,first,bytes,known,eligible,allocation) || !pointer_bits(work,begin,bytes,known,eligible,map) ||
               !pointer_bits(work,end,bytes,known,eligible,finish_map) || !pointer_bits(work,capacity,bytes,known,eligible,allocated_end) ||
               !bits(work.types,start,bytes,known,offset) || !bits(work.types,size,bytes,known,count) ||
               map<allocation || finish_map<map || allocated_end<finish_map || (map-allocation)%work.width!=0u ||
               (finish_map-map)%work.width!=0u || (allocated_end-allocation)%work.width!=0u) { return outcome::unavailable; }
            if(allocation==0u && (map!=0u || finish_map!=0u || allocated_end!=0u)) { return outcome::unavailable; }
            if(map==0u && (allocation!=0u || finish_map!=0u || allocated_end!=0u || offset!=0u || count!=0u)) { return outcome::unavailable; }
            auto const slots{(finish_map-map)/work.width};
            if(slots==0u)
            { if(offset!=0u || count!=0u) { return outcome::unavailable; }summary(tree[0u],"libc++ deque","size=0 []");return outcome::formatted; }
            ::std::uint64_t block{};
            if(!libcxx_block_count(work,root,element,block) || slots>(~::std::uint64_t{})/block ||
               count>=slots*block || offset>=slots*block-count || !range(map,slots,work.width,work.width)) { return outcome::unavailable; }
            ::std::uint64_t shown{},logical{offset};::std::string text{"size="};number(text,count);text+=" [";::std::vector<::std::uint64_t> visited{};
            while(shown<count && work.elements<work.cap.max_elements && work.nodes<work.cap.max_nodes)
            {
                auto const map_index{logical/block};auto const in_block{logical%block};::std::vector<::std::byte> entry{};
                if(!work.read(map+map_index*work.width,work.width,entry)) { if(work.limited) { break; }return outcome::unavailable; }
                auto const address{little(entry)};
                if(!range(address,block,extent,work.width) || ::std::find(visited.begin(),visited.end(),address)!=visited.end()) { return outcome::unavailable; }visited.push_back(address);
                auto const remaining{work.elements<work.cap.max_elements ? work.cap.max_elements-work.elements : 0u};
                auto const available{work.copied<work.cap.max_bytes ? work.cap.max_bytes-work.copied : 0u};
                auto batch{(std::min)(block-in_block,count-shown)};batch=(std::min)(batch,static_cast<::std::uint64_t>((std::min)(remaining,static_cast<::std::size_t>(available/extent))));
                if(batch==0u) { work.limited=true;break; }::std::vector<::std::byte> copied{};
                if(!work.read(address+in_block*extent,static_cast<::std::size_t>(batch*extent),copied)) { if(work.limited) { break; }return outcome::unavailable; }
                ::std::uint64_t used{};
                for(;used<batch && work.elements<work.cap.max_elements;++used)
                {
                    ::std::string label{"["};number(label,shown);label+=']';auto const before{tree.size()};++work.elements;
                    if(!work.append(tree,element,::std::span<::std::byte const>{copied}.subspan(static_cast<::std::size_t>(used*extent),static_cast<::std::size_t>(extent)),::std::move(label),depth)) { break; }
                    add_preview(tree,before,shown,text);++shown;++logical;
                }
                if(used!=batch || batch<block-in_block) { break; }
            }
            finish(tree,"libc++ deque",::std::move(text),count,shown);return outcome::formatted;
        }
    }
    template<class Session> [[nodiscard]] outcome extended_sequences(Session& work,::std::size_t root,
        ::std::span<::std::byte const> bytes,::std::span<::std::byte const> known,::std::span<::std::byte const> pointer_known,
        ::std::vector<dwarf::object_node>& tree,::std::size_t depth)
    {
        if(root>=work.types.size() || tree.empty() || tree[0u].type!=root || (work.width!=4u && work.width!=8u) ||
           (!known.empty() && known.size()!=bytes.size()) || (!pointer_known.empty() && pointer_known.size()!=bytes.size())) { return outcome::unavailable; }
        auto const& type{work.types[root]};bool const span{family(type,"span")};bool const set{family(type,"bitset")};
        bool const deque{family(type,"deque")};bool const forward{family(type,"forward_list")};
        bool const packed{family(type,"vector") && !type.template_types.empty() && type.template_types[0u]<work.types.size() &&
            work.types[type.template_types[0u]].kind==dwarf::type_kind::scalar && work.types[type.template_types[0u]].encoding==2u &&
            work.types[type.template_types[0u]].byte_size==1u};
        if(!span && !set && !packed && !deque && !forward) { return outcome::not_matched; }
        auto pending{tree};outcome result{outcome::not_matched};
        if(span) { result=sequence_details::static_span(work,root,bytes,known,pointer_known,pending,depth); }
        else if(set) { result=sequence_details::bitset(work,root,bytes,known,pending,depth); }
        else if(deque)
        { field probe{};result=path(work.types,root,"_M_impl._M_map",probe) ? sequence_details::deque(work,root,bytes,known,pointer_known,pending,depth) :
            sequence_details::libcxx_deque(work,root,bytes,known,pointer_known,pending,depth); }
        else if(forward)
        { field probe{};result=path(work.types,root,"_M_impl._M_head._M_next",probe) ? sequence_details::forward_list(work,root,bytes,known,pointer_known,pending,depth) : outcome::not_matched; }
        else
        { field probe{};result=path(work.types,root,"_M_impl._M_start._M_p",probe) ? sequence_details::vector_bool(work,root,bytes,known,pointer_known,pending,depth) :
            sequence_details::libcxx_packed(work,root,bytes,known,pointer_known,pending,depth); }
        if(result==outcome::formatted) { tree=::std::move(pending); }return result;
    }
}
#endif // UWVM_DEBUG_LANGUAGE_CPP
