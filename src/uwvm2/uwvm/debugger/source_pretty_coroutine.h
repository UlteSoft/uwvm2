/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include "language_debug_features.h"
#if UWVM_DEBUG_LANGUAGE_CPP
#ifndef UWVM_MODULE
# include "source_dwarf_objects.h"
# include <memory>
# include <span>
# include <string>
# include <string_view>
# include <utility>
# include <vector>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_pretty_printer::coroutine_details
{
    namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
    // Only already copied DWARF objects are accepted here. In particular, a
    // function-pointer value in Wasm is a table index, NOT a linear-memory
    // address. Raw summaries have no reader. Separate versioned enrichment
    // uses the current bounded session and exact owned compiler metadata.
    namespace details
    {
        [[nodiscard]] inline bool cpp(dwarf::type_record const& type) noexcept
        { return dwarf::cxx_language(type.language) && !type.tinygo_producer && !type.zig_producer; }

        [[nodiscard]] inline bool pointer(dwarf::type_record const& type, unsigned width = 0u) noexcept
        {
            return cpp(type) && type.kind == dwarf::type_kind::pointer && !type.reference_type && !type.rvalue_reference_type &&
                !type.atomic_scalar && (!type.address_class_known || type.address_class == 0u) && type.size_known &&
                (type.byte_size == 4u || type.byte_size == 8u) && type.byte_count == type.byte_size &&
                (width == 0u || type.byte_size == width);
        }

        [[nodiscard]] inline dwarf::member_record const* member(dwarf::type_record const& type,
            ::std::string_view name, ::std::span<dwarf::type_record const> types) noexcept
        {
            dwarf::member_record const* found{};
            for(auto const& field : type.members)
            {
                if(field.name != name) { continue; }
                if(found != nullptr || !field.offset_known || field.bit_field || field.inherited || field.type >= types.size()) { return nullptr; }
                auto const& value{types[field.type]};
                if(!value.size_known || field.byte_offset > type.byte_size || value.byte_size > type.byte_size-field.byte_offset) { return nullptr; }
                found = ::std::addressof(field);
            }
            return found;
        }

        [[nodiscard]] inline dwarf::object_node const* child(::std::vector<dwarf::object_node> const& nodes,
            ::std::size_t owner, dwarf::member_record const& field, ::std::span<dwarf::type_record const> types) noexcept
        {
            dwarf::object_node const* found{};
            auto const& parent{nodes[owner]};
            for(::std::size_t i{owner+1u}; i < nodes.size() && nodes[i].depth > parent.depth; ++i)
            {
                auto const& node{nodes[i]};
                if(node.parent != owner || node.name != field.name) { continue; }
                if(found != nullptr || node.type != field.type || node.kind != types[field.type].kind || node.bit_field || node.inherited ||
                   node.depth != parent.depth+1u || node.byte_offset < parent.byte_offset ||
                   node.byte_offset-parent.byte_offset != field.byte_offset || node.byte_size != types[field.type].byte_size)
                { return nullptr; }
                found = ::std::addressof(node);
            }
            return found;
        }

        [[nodiscard]] inline bool scalar_bits(dwarf::object_node const* node, unsigned width, bool is_pointer,
            ::std::uint64_t& bits) noexcept
        {
            if(node == nullptr || !node->value_available || node->reason != dwarf::object_unavailable_reason::none ||
               node->scalar_bytes != width || node->byte_size != width || node->omitted_children != 0u ||
               (is_pointer ? (node->kind != dwarf::type_kind::pointer || node->scalar_kind != dwarf::numeric_kind::unsigned_integer) :
                (node->kind != dwarf::type_kind::scalar || node->scalar_kind != dwarf::numeric_kind::unsigned_integer)) ||
               (width != 1u && width != 2u && width != 4u && width != 8u)) { return false; }
            auto const mask{dwarf::numeric_mask(static_cast<::std::uint8_t>(width))};
            if((node->bits & ~mask) != 0u) { return false; }
            bits = node->bits; return true;
        }

        inline void publish(dwarf::object_node& node, ::std::string summary, ::std::string_view printer)
        {
            node.pretty_summary = ::std::move(summary); node.pretty_printer = ::fast_io::concat_std(printer);
            node.pretty_available = true; node.pretty_truncated = false; node.pretty_omitted = 0u;
        }

        [[nodiscard]] inline bool handle_name(::std::string_view name) noexcept
        {
            if(name.size() > 4096u) { return false; }
            // Clang/GCC may emit an unqualified DW_AT_name even for a std
            // namespace DIE. No namespace identity or read authority is
            // inferred from that spelling; the exact one-pointer schema is
            // still required and its frame ABI remains explicitly unknown.
            bool const qualified{name.starts_with("std::")};
            if(qualified) { name.remove_prefix(5u); }
            if(qualified && name.starts_with("__"))
            {
                auto const end{name.find("::")};
                if(end == ::std::string_view::npos || end == 2u || end > 128u) { return false; }
                for(char c : name.substr(2u,end-2u))
                { if(!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) { return false; } }
                name.remove_prefix(end+2u);
            }
            return name.size() > 18u && name.starts_with("coroutine_handle<") && name.back() == '>';
        }

        [[nodiscard]] inline bool handle(::std::span<dwarf::type_record const> types,
            ::std::vector<dwarf::object_node>& nodes, ::std::size_t owner, unsigned wasm_address_bytes=0u)
        {
            auto& node{nodes[owner]}; auto const& type{types[node.type]};
            if(!handle_name(type.aggregate_name.empty() ? type.name : type.aggregate_name) || type.members.size() != 1u) { return false; }
            auto const& field{type.members[0u]};
            auto const* proved{member(type,field.name,types)};
            if(proved == nullptr || field.byte_offset != 0u || !pointer(types[field.type]) ||
               type.byte_size != types[field.type].byte_size) { return false; }
            auto const width{static_cast<unsigned>(types[field.type].byte_size)};
            if(wasm_address_bytes!=0u && width!=wasm_address_bytes) { return false; }
            ::std::uint64_t address{}; ::std::string text{}; ::fast_io::ostring_ref_std output{::std::addressof(text)};
            if(scalar_bits(child(nodes,owner,field,types),width,true,address))
            {
                ::fast_io::io::print(output,"coroutine_handle frame=guest:",::fast_io::mnp::hex0x(address),
                    ::std::string_view{address == 0u ? ", state=empty (frame ABI unavailable)" : ", state=unknown (frame ABI unavailable)"});
            }
            else { ::fast_io::io::print(output,"coroutine_handle state=unknown (frame pointer unavailable; frame ABI unavailable)"); }
            publish(node,::std::move(text),"cxx.coroutine_handle"); return true;
        }

        [[nodiscard]] inline bool gcc_function(::std::span<dwarf::type_record const> types,
            ::std::size_t function_pointer, ::std::size_t frame, unsigned width) noexcept
        {
            auto const target{types[function_pointer].referenced_type};
            if(target >= types.size()) { return false; }
            auto const& function{types[target]};
            if(function.kind != dwarf::type_kind::subroutine || !function.signature_complete || function.variadic ||
               function.referenced_type != dwarf::no_record || function.parameter_types.size() != 1u ||
               function.parameter_types[0u] >= types.size()) { return false; }
            auto const& parameter{types[function.parameter_types[0u]]};
            return pointer(parameter,width) && parameter.referenced_type == frame;
        }

        [[nodiscard]] inline bool frame(::std::span<dwarf::type_record const> types,
            ::std::vector<dwarf::object_node>& nodes, ::std::size_t owner, unsigned wasm_address_bytes=0u)
        {
            auto& node{nodes[owner]}; auto const& type{types[node.type]};
            ::std::string_view const spelling{type.aggregate_name.empty() ? type.name : type.aggregate_name};
            bool const clang{spelling.size() > 14u && spelling.ends_with(".coro_frame_ty")};
            bool const gcc{!clang && spelling.size() > 5u && spelling.ends_with("Frame")};
            if(!clang && !gcc) { return false; }
            auto const* resume{member(type,clang ? "__resume_fn" : "_Coro_resume_fn",types)};
            auto const* destroy{member(type,clang ? "__destroy_fn" : "_Coro_destroy_fn",types)};
            auto const* index{member(type,clang ? "__coro_index" : "_Coro_resume_index",types)};
            if(resume == nullptr || destroy == nullptr || index == nullptr || resume->byte_offset != 0u ||
               !pointer(types[resume->type])) { return false; }
            auto const width{static_cast<unsigned>(types[resume->type].byte_size)};
            if(wasm_address_bytes!=0u && width!=wasm_address_bytes) { return false; }
            if(!pointer(types[destroy->type],width) || destroy->byte_offset != width || index->byte_offset < width*2u) { return false; }
            auto const& it{types[index->type]};
            if(!cpp(it) || it.kind != dwarf::type_kind::scalar || !it.size_known || it.atomic_scalar ||
               (it.encoding != 7u && it.encoding != 8u) || it.byte_count != it.byte_size ||
               (clang ? (it.byte_size != 1u && it.byte_size != 2u && it.byte_size != 4u && it.byte_size != 8u) : it.byte_size != 2u)) { return false; }
            // LLVM emits erased void pointers for these two fields. GCC emits
            // void (*)(Frame*); verify both exact signatures rather than names.
            if(clang)
            {
                if(types[resume->type].referenced_type != dwarf::no_record || types[destroy->type].referenced_type != dwarf::no_record) { return false; }
            }
            else if(!gcc_function(types,resume->type,node.type,width) || !gcc_function(types,destroy->type,node.type,width)) { return false; }
            ::std::uint64_t resume_bits{}, destroy_bits{}, index_bits{};
            bool const resume_known{scalar_bits(child(nodes,owner,*resume,types),width,true,resume_bits)};
            bool const destroy_known{scalar_bits(child(nodes,owner,*destroy,types),width,true,destroy_bits)};
            bool const index_known{scalar_bits(child(nodes,owner,*index,types),static_cast<unsigned>(it.byte_size),false,index_bits)};
            ::std::string text{}; ::fast_io::ostring_ref_std output{::std::addressof(text)};
            ::fast_io::io::print(output,::std::string_view{clang ? "Clang coroutine frame, state=" : "GCC coroutine frame, state="});
            if(gcc && index_known && (index_bits & 1u) != 0u)
            { ::fast_io::io::print(output,"unknown (destroy path; destruction not proven)"); }
            else if(resume_known && destroy_known && resume_bits == 0u && destroy_bits != 0u)
            { ::fast_io::io::print(output,"done (final-suspend marker; lifetime not proven)"); }
            else if(resume_known && destroy_known && resume_bits != 0u && destroy_bits != 0u)
            { ::fast_io::io::print(output,"unknown (resumable; suspended/running not proven)"); }
            else { ::fast_io::io::print(output,"unknown (destroyed/uninitialized/unavailable not distinguished)"); }
            if(index_known) { ::fast_io::io::print(output,", suspension-index=",::fast_io::mnp::dec(index_bits)); }
            else { ::fast_io::io::print(output,", suspension-index=unavailable"); }
            ::fast_io::io::print(output,", function values are Wasm table indices");
            auto const* promise{member(type,clang ? "__promise" : "_Coro_promise",types)};
            if(promise != nullptr)
            {
                auto const* copied{child(nodes,owner,*promise,types)};
                ::fast_io::io::print(output,::std::string_view{copied != nullptr && copied->reason == dwarf::object_unavailable_reason::none ?
                    ", promise=raw copied child (field availability retained)" : ", promise=unavailable"});
            }
            else { ::fast_io::io::print(output,", promise=not described by frame DWARF"); }
            if(clang && type.clang18_wasm32_coro_dwarf && type.original_coro_destroy_offset==8u && width==4u)
            { ::fast_io::io::print(output,", compiler ABI correction: Clang18 Wasm32 destroy-offset=4 (original DWARF=8)"); }
            publish(node,::std::move(text),clang ? "cxx.coroutine_frame.clang" : "cxx.coroutine_frame.gcc"); return true;
        }
    }


    // Versioned ABI enrichment uses only this session's bounded guest reader.
    // Function words remain table indices, never callable host/guest pointers.
    template<typename Session>
    [[nodiscard]] bool enrich_handle(Session& work,::std::size_t root,
        ::std::span<::std::byte const> bytes,::std::span<::std::byte const> known,
        ::std::span<::std::byte const> eligible,::std::vector<dwarf::object_node>& tree,::std::size_t depth)
    {
        if(root>=work.types.size() || tree.empty() || tree[0].type!=root ||
           tree[0].pretty_printer!="cxx.coroutine_handle" || work.width!=4u) {return false;}
        auto const& type{work.types[root]};
        if(type.clang18_wasi32_coro_handle==type.clang23_wasi32_coro_handle || !details::cpp(type) || type.atomic_scalar ||
           !type.size_known || type.byte_size!=4u || bytes.size()!=4u || type.members.size()!=1u ||
           type.declaration_identity==dwarf::die_key{} || type.identity.unit!=type.declaration_identity.unit ||
           !type.qualified_type_name.starts_with("std::__2::coroutine_handle<") ||
           type.qualified_type_name.back()!='>') {return false;}
        auto const* field{details::member(type,"__handle_",work.types)};
        if(!field || field->byte_offset!=0u || !details::pointer(work.types[field->type],4u) ||
           work.types[field->type].referenced_type!=dwarf::no_record ||
           (!known.empty() && known.size()!=bytes.size()) ||
           (!eligible.empty() && eligible.size()!=bytes.size())) {return false;}
        ::std::uint64_t address{};
        for(unsigned i{};i!=4u;++i)
        {
            if((!known.empty() && known[i]!=::std::byte{255u}) ||
               (!eligible.empty() && eligible[i]!=::std::byte{255u})) {return false;}
            address|=static_cast<::std::uint64_t>(static_cast<unsigned char>(bytes[i]))<<(i*8u);
        }
        ::std::string_view const abi{type.clang18_wasi32_coro_handle ? "LLVM18.1.2/libc++18.1/Wasm32" : "LLVM23.1.0/libc++23.1/Wasm32"};
        if(address==0u)
        {
            details::publish(tree[0],::fast_io::concat_std("coroutine_handle frame=guest:0x0, state=empty, ABI=",abi),"cxx.coroutine_handle");
            return true;
        }
        ::std::vector<::std::byte> header{};
        if(!work.read(address,8u,header)) {return false;}
        ::std::uint64_t resume{},destroy{};
        for(unsigned i{};i!=4u;++i)
        {
            resume|=static_cast<::std::uint64_t>(static_cast<unsigned char>(header[i]))<<(i*8u);
            destroy|=static_cast<::std::uint64_t>(static_cast<unsigned char>(header[4u+i]))<<(i*8u);
        }
        ::std::string text{};::fast_io::ostring_ref_std out{::std::addressof(text)};
        ::fast_io::io::print(out,"coroutine_handle frame=guest:",::fast_io::mnp::hex0x(address),
            ", ABI=",abi,", state=",
            ::std::string_view{resume==0u && destroy!=0u ? "done (final-suspend marker; lifetime not proven)" :
                resume!=0u && destroy!=0u ? "unknown (resumable; suspended/running not proven)" :
                "unknown (destroyed/uninitialized/unavailable not distinguished)"},
            ", resume-table-index=",::fast_io::mnp::dec(resume),", destroy-table-index=",::fast_io::mnp::dec(destroy));
        ::std::string_view promise_status{"unavailable (no exact owned promise layout)"};
        if(type.template_types.size()==1u && type.template_types[0]==dwarf::no_record) {promise_status="erased";}
        else if(type.template_types.size()==1u && type.template_types[0]<work.types.size())
        {
            auto const promise_type{type.template_types[0]};auto const& promise{work.types[promise_type]};
            ::std::uint64_t offset{};bool found{},ambiguous{};
            if(details::cpp(promise) && !promise.atomic_scalar && promise.size_known && promise.byte_size &&
               promise.byte_size<=work.cap.max_bytes && promise.declaration_identity!=dwarf::die_key{} &&
               promise.declaration_identity.unit==type.declaration_identity.unit)
            {
                for(auto const& frame:work.types)
                {
                    if((type.clang18_wasi32_coro_handle ?
                        (!frame.clang18_wasm32_coro_dwarf || frame.clang23_wasm32_coro_dwarf || frame.original_coro_destroy_offset!=8u) :
                        (!frame.clang23_wasm32_coro_dwarf || frame.clang18_wasm32_coro_dwarf || frame.original_coro_destroy_offset!=0u)) ||
                       !details::cpp(frame) || frame.atomic_scalar || !frame.size_known ||
                       (frame.kind!=dwarf::type_kind::structure && frame.kind!=dwarf::type_kind::class_type) ||
                       frame.declaration_identity==dwarf::die_key{} ||
                       frame.declaration_identity.unit!=type.declaration_identity.unit) {continue;}
                    auto const* r{details::member(frame,"__resume_fn",work.types)};
                    auto const* d{details::member(frame,"__destroy_fn",work.types)};
                    auto const* p{details::member(frame,"__promise",work.types)};
                    if(!r || !d || !p || r->byte_offset!=0u || d->byte_offset!=4u || p->byte_offset!=8u ||
                       r->type!=d->type || !details::pointer(work.types[r->type],4u) ||
                       work.types[r->type].referenced_type!=dwarf::no_record ||
                       !dwarf::same_cpp_value_type(work.types,p->type,promise_type)) {continue;}
                    if(found && offset!=p->byte_offset) {ambiguous=true;break;}
                    offset=p->byte_offset;found=true;
                }
                if(found && !ambiguous && offset>=8u && offset<=0xffffffffu-address)
                {
                    if(work.elements>=work.cap.max_elements || depth>=work.cap.max_depth)
                    {work.limited=true;promise_status="unavailable (element/depth budget)";}
                    else
                    {
                        ::std::vector<::std::byte> value{};
                        if(work.read(address+offset,static_cast<::std::size_t>(promise.byte_size),value))
                        {
                            auto detached{tree};++work.elements;
                            if(work.append(detached,promise_type,value,"[promise]",depth))
                            {tree=::std::move(detached);promise_status="raw copied child (lifetime not proven)";}
                        }
                    }
                }
            }
        }
        ::fast_io::io::print(out,", promise=",promise_status);
        details::publish(tree[0],::std::move(text),"cxx.coroutine_handle");
        if(work.limited) {tree[0].pretty_truncated=true;}
        return true;
    }

    // Returns whether any owned node acquired a summary. No reads, function
    // calls, frame devirtualization, lifecycle claims or source-location guesses
    // are performed. Unknown compilers/layouts retain their raw object view.
    [[nodiscard]] inline bool summarize(::std::span<dwarf::type_record const> types,
        ::std::vector<dwarf::object_node>& nodes, unsigned wasm_address_bytes=0u)
    {
        if(types.size() > 65536u || nodes.size() > 1024u) { return false; }
        for(::std::size_t i{}; i != nodes.size(); ++i)
        {
            auto const& node{nodes[i]};
            if(node.depth > 32u || (node.parent == dwarf::no_record ? node.depth != 0u :
                (node.parent >= i || nodes[node.parent].depth+1u != node.depth))) { return false; }
        }
        bool changed{};
        for(::std::size_t i{}; i != nodes.size(); ++i)
        {
            auto const& node{nodes[i]};
            if(node.type >= types.size() || node.reason != dwarf::object_unavailable_reason::none || node.bit_field ||
               node.variant_part || node.variant_case || node.omitted_children != 0u) { continue; }
            auto const& type{types[node.type]};
            if(!details::cpp(type) || !type.size_known || type.byte_size != node.byte_size || type.name.size() > 4096u || type.aggregate_name.size() > 4096u ||
               type.members.size() > 1024u || type.atomic_scalar ||
               (type.kind != dwarf::type_kind::structure && type.kind != dwarf::type_kind::class_type) || type.kind != node.kind) { continue; }
            changed = details::handle(types,nodes,i,wasm_address_bytes) || details::frame(types,nodes,i,wasm_address_bytes) || changed;
        }
        return changed;
    }
}
#endif // UWVM_DEBUG_LANGUAGE_CPP
