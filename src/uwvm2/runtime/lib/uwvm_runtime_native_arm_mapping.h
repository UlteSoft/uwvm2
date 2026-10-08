// Actual ARM ELF mapping-symbol DATA. This never creates a code owner or
// widens a function endpoint. Literal-pool bytes remain private data even when
// a synthetic DWARF line happens to cover them.
#pragma once
#ifndef UWVM_MODULE
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#if defined(UWVM_RUNTIME_LLVM_JIT)
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/ObjectFile.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/elf_headers.h>
#endif
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
namespace uwvm2::runtime::lib::details::native_arm_mapping
{
    struct data_range { ::std::uint64_t section{},begin{},end{},section_size{}; };
    [[nodiscard]] inline char kind(::llvm::object::ObjectFile const& object,
        ::llvm::object::SymbolRef const& symbol,::llvm::StringRef name) noexcept
    {
        if((object.getArch()!=::llvm::Triple::arm && object.getArch()!=::llvm::Triple::armeb) ||
           !::llvm::isa<::llvm::object::ELFObjectFileBase>(object) || symbol.getObject()!=::std::addressof(object) ||
           name.size()<2u || name[0]!='$' || (name[1]!='a' && name[1]!='d' && name[1]!='t')) { return 0; }
        if(name.size()!=2u)
        {
            if(name.size()<4u || name[2]!='.') { return 0; }
            for(::std::size_t i{3u};i<name.size();++i) { if(name[i]<'0' || name[i]>'9') { return 0; } }
        }
        auto flags{symbol.getFlags()};if(!flags) { ::llvm::consumeError(flags.takeError());return 0; }
        if(*flags!=::llvm::object::SymbolRef::SF_FormatSpecific) { return 0; }
        auto const actual{::llvm::object::ELFSymbolRef{symbol}};
        namespace elf=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants;
        if(actual.getBinding()!=elf::binding_local || actual.getELFType()!=elf::symbol_notype ||
           actual.getSize()!=0u || actual.getOther()!=0u) { return 0; }
        return name[1];
    }
    [[nodiscard]] inline bool collect(::llvm::object::ObjectFile const& object,::std::vector<data_range>& output)
    {
        output.clear();
        if(object.getArch()!=::llvm::Triple::arm && object.getArch()!=::llvm::Triple::armeb) { return true; }
        if(!object.isRelocatableObject() || !::llvm::isa<::llvm::object::ELFObjectFileBase>(object) ||
           object.getData().size()>64u*1024u*1024u) { return false; }
        struct marker { ::std::uint64_t section{},offset{},size{};char state{}; };
        ::std::vector<marker> markers{};::std::size_t count{};
        for(auto const& symbol:object.symbols())
        {
            if(++count>1'048'576u) { return false; }
            auto name{symbol.getName()};if(!name) { ::llvm::consumeError(name.takeError());return false; }
            if(name->size()<2u || (*name)[0]!='$' || ((*name)[1]!='a' && (*name)[1]!='d' && (*name)[1]!='t')) { continue; }
            auto const state{kind(object,symbol,*name)};
            if(state==0 || state=='t') { return false; } // This adapter executes ARM state only.
            auto selected{symbol.getSection()};if(!selected) { ::llvm::consumeError(selected.takeError());return false; }
            if(*selected==object.section_end() || !(**selected).isText()) { return false; }
            auto address{symbol.getAddress()};if(!address) { ::llvm::consumeError(address.takeError());return false; }
            auto const& section{**selected};auto const begin{section.getAddress()},size{section.getSize()};
            if(size==0u || size>64u*1024u*1024u || *address<begin || *address-begin>=size ||
               (*address-begin)%4u!=0u) { return false; }
            markers.push_back({section.getIndex(),*address-begin,size,state});
        }
        ::std::sort(markers.begin(),markers.end(),[](auto const& a,auto const& b)
        { return a.section!=b.section ? a.section<b.section:a.offset<b.offset; });
        for(::std::size_t i{};i<markers.size();++i)
        {
            auto const& here{markers[i]};
            if(i!=0u && markers[i-1u].section==here.section &&
               (markers[i-1u].offset>=here.offset || markers[i-1u].size!=here.size)) { return false; }
            if(here.state!='d') { continue; }
            if(i==0u || markers[i-1u].section!=here.section || markers[i-1u].state!='a') { return false; }
            auto end{here.size};
            if(i+1u<markers.size() && markers[i+1u].section==here.section)
            {
                if(markers[i+1u].state!='a' || markers[i+1u].size!=here.size) { return false; }
                end=markers[i+1u].offset;
            }
            if(end<=here.offset || end>here.size) { return false; }
            output.push_back({here.section,here.offset,end,here.size});
        }
        return true;
    }
}
#endif
