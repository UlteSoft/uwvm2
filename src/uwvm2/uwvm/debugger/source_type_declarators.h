/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_types.h"
# include "source_dwarf_query.h"
# include <fast_io_dsal/string.h>
# include <fast_io_dsal/string_view.h>
# include <fast_io_dsal/array.h>
# include <memory>
# include <utility>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_dwarf::type_declarators
{
    using view = ::fast_io::string_view;
    struct limits { ::std::size_t depth{32u}, edges{4096u}, bytes{16384u}; };
    [[nodiscard]] inline constexpr bool uses_c_spelling(type_record const& type) noexcept
    {
        if(type.tinygo_producer || type.zig_producer) { return false; }
        // Actual CU language only; names never select a language grammar.
        switch(type.language)
        {
            case 0x01u: case 0x02u: case 0x04u: case 0x0cu: case 0x10u: case 0x11u:
            case 0x19u: case 0x1au: case 0x1du: case 0x21u: case 0x2au: case 0x2bu:
            case 0x2cu: case 0x3au: case 0x3eu: return true;
            default: return false;
        }
    }
    // Owned metadata only. Function signatures/member-pointer declarations are
    // neither call targets nor permission to inspect a native/guest pointer.
    struct builder
    {
        ::std::span<type_record const> types{};
        limits cap{};
        ::fast_io::array<::std::size_t,32u> path{};
        ::std::size_t edges{};
        bool opaque{};
        inline_query_error error{};
        [[nodiscard]] bool put(::fast_io::string& output, view piece)
        {
            if(output.size() > cap.bytes || piece.size() > cap.bytes-output.size())
            { error = inline_query_error::limit_exceeded; return false; }
            ::fast_io::ostring_ref_fast_io stream{::std::addressof(output)};
            ::fast_io::io::print(stream,piece); return true;
        }
        [[nodiscard]] bool cv(::fast_io::string& output, ::std::uint8_t mask, bool prefix)
        {
            if(mask&8u) { opaque=true; return false; }
            if(mask&~0x0fu) { error=inline_query_error::malformed; return false; }
            if((mask&1u) && !put(output,prefix ? view{"const "} : view{" const"})) { return false; }
            if((mask&2u) && !put(output,prefix ? view{"volatile "} : view{" volatile"})) { return false; }
            if((mask&4u) && !put(output,prefix ? view{"restrict "} : view{" restrict"})) { return false; }
            return true;
        }
        [[nodiscard]] bool base(view name, ::std::uint8_t mask, view declaration, ::fast_io::string& output, bool suffix=false)
        {
            if(!suffix && !cv(output,mask,true)) { return false; }
            if(!put(output,name) || (suffix && !cv(output,mask,false))) { return false; }
            if(!declaration.empty() && (!put(output," ") || !put(output,declaration))) { return false; }
            return true;
        }
        [[nodiscard]] bool build(::std::size_t index, view declaration, ::fast_io::string& output, ::std::size_t depth, ::std::uint8_t inherited_qualifiers=0u, ::std::size_t member_owner=no_record)
        {
            if(index==no_record) { return base("void",inherited_qualifiers,declaration,output); }
            if(index>=types.size()) { error=inline_query_error::malformed; return false; }
            if(depth>=path.size() || depth>=cap.depth || edges==cap.edges)
            { error=inline_query_error::limit_exceeded; return false; }
            ++edges;
            for(::std::size_t i{};i!=depth;++i) { if(path[i]==index) { opaque=true;return false; } }
            path[depth]=index;
            auto const& type{types[index]};
            auto const qualifiers{static_cast<::std::uint8_t>(type.display_qualifiers|inherited_qualifiers)};
            if(qualifiers&8u) { opaque=true;return false; }
            if(!type.name.empty())
            {
                return base(view{type.name.data(),type.name.size()},qualifiers,declaration,output,
                    type.kind==type_kind::pointer && !type.named_type_alias);
            }
            ::fast_io::string next{};
            if(type.kind==type_kind::pointer || type.kind==type_kind::member_pointer)
            {
                if(type.kind==type_kind::member_pointer)
                {
                    ::fast_io::string containing{};
                    if(type.containing_type==no_record || !build(type.containing_type,{},containing,depth+1u) ||
                       containing.empty() || !put(next,containing.subview(0u)) || !put(next,"::*")) { opaque=true;return false; }
                }
                else if(!put(next,type.rvalue_reference_type ? view{"&&"} : type.reference_type ? view{"&"} : view{"*"})) { return false; }
                if(!cv(next,qualifiers,false)) { return false; }
                if(!declaration.empty())
                {
                    bool const separator{declaration.front()!='[' && declaration.front()!='('};
                    if((separator && !put(next," ")) || !put(next,declaration)) { return false; }
                }
                if(type.referenced_type!=no_record && type.referenced_type<types.size() &&
                   (types[type.referenced_type].kind==type_kind::array || types[type.referenced_type].kind==type_kind::subroutine))
                {
                    ::fast_io::string grouped{};
                    if(!put(grouped,"(") || !put(grouped,next.subview(0u)) || !put(grouped,")")) { return false; }
                    next=::std::move(grouped);
                }
                return build(type.referenced_type,next.subview(0u),output,depth+1u,0u,
                    type.kind==type_kind::member_pointer ? type.containing_type : no_record);
            }
            if(type.kind==type_kind::array)
            {
                if(!type.contiguous_array || !type.row_major_array || type.referenced_type==no_record || type.dimensions.empty())
                { opaque=true;return false; }
                if(!put(next,declaration)) { return false; }
                for(auto const& bound:type.dimensions)
                {
                    if(bound.lower_bound_known && bound.lower_bound!=0) { opaque=true;return false; }
                    if(!put(next,"[")) { return false; }
                    if(bound.count_known)
                    {
                        auto const count{::fast_io::concat_fast_io(::fast_io::mnp::dec(bound.count))};
                        if(!put(next,count.subview(0u))) { return false; }
                    }
                    if(!put(next,"]")) { return false; }
                }
                return build(type.referenced_type,next.subview(0u),output,depth+1u,qualifiers);
            }
            if(type.kind==type_kind::subroutine)
            {
                if(!type.signature_complete || qualifiers!=0u ||
                   (type.calling_convention_known && type.calling_convention!=1u)) { opaque=true;return false; }
                ::std::size_t parameter_begin{};auto method_cv{type.method_qualifiers};
                if(type.first_parameter_artificial)
                {
                    // Clang's subroutine-type producer emits artificial this,
                    // without DW_AT_object_pointer. Remove it ONLY in an owned
                    // member-pointer declaration whose final class DIE matches.
                    // A hidden ABI argument of an ordinary function stays opaque.
                    if(member_owner>=types.size() || type.parameter_types.empty() ||
                       type.parameter_types.front()>=types.size()) { opaque=true;return false; }
                    auto const& pointer{types[type.parameter_types.front()]};
                    if(pointer.kind!=type_kind::pointer || pointer.reference_type ||
                       pointer.referenced_type>=types.size()) { opaque=true;return false; }
                    auto const& object{types[pointer.referenced_type]};auto const& owner{types[member_owner]};
                    if((object.kind!=type_kind::structure && object.kind!=type_kind::class_type) ||
                       object.declaration_identity!=owner.declaration_identity ||
                       object.declaration_identity==die_key{} || (object.display_qualifiers&~3u)!=0u)
                    { opaque=true;return false; }
                    method_cv|=object.display_qualifiers;parameter_begin=1u;
                }
                if(!put(next,declaration) || !put(next,"(")) { return false; }
                bool first{true};
                for(auto position{parameter_begin};position!=type.parameter_types.size();++position)
                {
                    auto const parameter{type.parameter_types[position]};
                    if(parameter==no_record) { opaque=true;return false; }
                    ::fast_io::string text{};
                    if(!build(parameter,{},text,depth+1u) || text.empty()) { return false; }
                    if((!first && !put(next,", ")) || !put(next,text.subview(0u))) { return false; }
                    first=false;
                }
                if(type.variadic && ((!first && !put(next,", ")) || !put(next,"..."))) { return false; }
                if(!put(next,")") || !cv(next,method_cv,false)) { return false; }
                if(type.method_lvalue_reference && type.method_rvalue_reference) { opaque=true;return false; }
                if(type.method_lvalue_reference && !put(next," &")) { return false; }
                if(type.method_rvalue_reference && !put(next," &&")) { return false; }
                return build(type.referenced_type,next.subview(0u),output,depth+1u);
            }
            opaque=true;return false;
        }
    };
    [[nodiscard]] inline inline_query_error format(::std::span<type_record const> types,::std::size_t index,
        ::fast_io::string& output,limits cap={})
    {
        output.clear();builder render{types,cap};
        if(!render.build(index,{},output,0u))
        { output.clear();return render.error; } // Opaque is an explicit empty spelling.
        return inline_query_error::none;
    }
}
