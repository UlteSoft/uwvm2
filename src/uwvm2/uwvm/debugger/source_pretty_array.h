/*************************************************************
 * UWVM2 owned standard-array printer. APL-2.0.             *
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
    template<class Session> [[nodiscard]] outcome extended_array(Session& work,::std::size_t root,
        ::std::span<::std::byte const> bytes,::std::span<::std::byte const> known,
        ::std::span<::std::byte const> eligible,::std::vector<dwarf::object_node>& tree,::std::size_t depth)
    {
        if(root>=work.types.size()||!family(work.types[root],"array")) {return outcome::not_matched;}
        auto const& array{work.types[root]};::std::uint64_t count{};
        if(!array.size_known||array.byte_size!=bytes.size()||array.template_types.size()!=1u||
           array.template_types[0u]>=work.types.size()||
           !sequence_details::template_count(work.types,root,1u,work.width,count)) {return outcome::unavailable;}
        auto const element{array.template_types[0u]};auto const& value{work.types[element]};field storage{};
        // Empty arrays have no element object. Prove T/N, then ignore the
        // implementation's dummy/zero-sized storage without reading it.
        if(count)
        {
            if(array.members.size()!=1u||!value.size_known||!value.byte_size||value.byte_size>bytes.size()||
               count>bytes.size()/value.byte_size) {return outcome::unavailable;}
            auto const& member{array.members[0u]};
            if((member.name!="_M_elems"&&member.name!="__elems_")||member.inherited||member.bit_field||
               !member.offset_known||member.type>=work.types.size()) {return outcome::unavailable;}
            storage={member.type,member.byte_offset};auto const& raw{work.types[storage.type]};
            if(raw.kind!=dwarf::type_kind::array||!raw.size_known||!raw.contiguous_array||!raw.row_major_array||
               raw.referenced_type!=element||raw.dimensions.size()!=1u||!raw.dimensions[0u].count_known||
               !raw.dimensions[0u].lower_bound_known||raw.dimensions[0u].lower_bound!=0||raw.dimensions[0u].count!=count||
               raw.byte_size!=count*value.byte_size||storage.offset>bytes.size()||raw.byte_size>bytes.size()-storage.offset)
            {return outcome::unavailable;}
        }
        tree.resize(1u);::std::string text{"array size="};number(text,count);text+=" [";::std::uint64_t shown{};
        for(;shown<count;)
        {
            if(work.elements>=work.cap.max_elements||work.nodes>=work.cap.max_nodes) {break;}
            auto const first{static_cast<::std::size_t>(storage.offset+shown*value.byte_size)};
            auto const size{static_cast<::std::size_t>(value.byte_size)};::std::string label{"["};number(label,shown);label+=']';
            auto const before{tree.size()};++work.elements;
            if(!work.append(tree,element,bytes.subspan(first,size),::std::move(label),depth,
                known.empty()?known:known.subspan(first,size),eligible.empty()?eligible:eligible.subspan(first,size)))
            {if(!work.limited) {return outcome::unavailable;}break;}
            if(shown<8u&&text.size()<3000u) {if(shown) {text+=", ";}scalar_text(tree[before],text);}
            ++shown;
        }
        if(shown<count||shown>8u) {if(shown) {text+=", ";}text+="...";}text+=']';
        summary(tree[0u],"C++ std::array",::std::move(text),count-shown);return outcome::formatted;
    }
}
#endif // UWVM_DEBUG_LANGUAGE_CPP
