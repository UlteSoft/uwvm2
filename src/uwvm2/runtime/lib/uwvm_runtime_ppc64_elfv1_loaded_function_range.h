/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)            *
 * Licensed under the APL-2.0 License (see LICENSE file).      *
 *************************************************************/
#pragma once
#include <cstdint>
#include <limits>
#include <memory>
#include <algorithm>
#include <optional>
#include <utility>
#include <vector>
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/elf_headers.h>
# include <llvm/ExecutionEngine/RuntimeDyld.h>
# include <llvm/Object/ELFObjectFile.h>
# include <llvm/Support/Error.h>

namespace uwvm2::runtime::lib::details
{
    struct ppc64_elfv1_loaded_function_range
    { ::std::uintptr_t begin{}, size{}; };


    // One actual-object index per notification, not one relocation scan per
    // descriptor. Borrowed RelocationRefs live only while the object is pinned
    // in its synchronous listener callback. Scratch may throw to Hot14's real
    // pending-listener boundary; no allocation/lookup is added to guest code.
    class ppc64_elfv1_relocation_index final
    {
        struct entry
        {
            ::std::uint64_t section{},offset{};
            ::llvm::object::RelocationRef relocation;
        };
        ::llvm::object::ObjectFile const* object_{};
        ::std::vector<entry> entries_{};
    public:
        explicit ppc64_elfv1_relocation_index(::llvm::object::ObjectFile const& object)
        {
            auto const* elf{::llvm::dyn_cast<::llvm::object::ELFObjectFileBase>(::std::addressof(object))};
            if(elf==nullptr || !elf->is64Bit() || elf->getEMachine()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::machine_ppc64 ||
               elf->getEType()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::type_relocatable || (elf->getPlatformFlags() & ::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::ppc64_abi)>1u) { return; }
            // [actual owned object sections begin...end]
            // [safe] range iteration advances only non-end actual iterators.
            for(auto const& section:object.sections())
            {
                auto relocated{section.getRelocatedSection()};
                if(!relocated) { ::llvm::consumeError(relocated.takeError()); return; }
                if(*relocated==object.section_end()) { continue; }
                auto const& target{**relocated};
                if(target.isText()) { continue; }
                auto name{target.getName()};if(!name) { ::llvm::consumeError(name.takeError()); return; }
                if(*name!=".opd") { continue; }
                auto const metadata{::llvm::object::ELFSectionRef{target}};
                if(metadata.getType()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::section_progbits || (metadata.getFlags() & ::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::flag_alloc)==0u) { continue; }
                auto const index{target.getIndex()};
                // [actual owned relocation begin...end]
                // [safe] each RelocationRef is copied before the non-end range
                // advance. No caller row/address can mint this object index.
                for(auto const& relocation:section.relocations())
                { entries_.push_back({index,relocation.getOffset(),relocation}); }
            }
            ::std::sort(entries_.begin(),entries_.end(),[](entry const& a,entry const& b) noexcept
                { return a.section<b.section || (a.section==b.section && a.offset<b.offset); });
            object_=::std::addressof(object); // Seal only the completely indexed actual object.
        }
        [[nodiscard]] bool matches(::llvm::object::ObjectFile const& actual) const noexcept
        { return object_==::std::addressof(actual); }
        [[nodiscard]] ::llvm::object::RelocationRef const* unique(::llvm::object::SectionRef const& section,
            ::std::uint64_t offset) const noexcept
        {
            if(object_==nullptr || section.getObject()!=object_) { return nullptr; }
            auto const section_index{section.getIndex()};
            auto const found{::std::lower_bound(entries_.begin(),entries_.end(),::std::pair{section_index,offset},
                [](entry const& a,auto const& b) noexcept { return a.section<b.first || (a.section==b.first && a.offset<b.second); })};
            // [owned sorted relocation table begin...found...end]
            // [safe] check the full end before either fields or one-past update.
            if(found==entries_.end() || found->section!=section_index || found->offset!=offset) { return nullptr; }
            auto next{found};++next; // Non-end found proves increment may reach the sentinel.
            if(next!=entries_.end() && next->section==section_index && next->offset==offset) { return nullptr; }
            return ::std::addressof(found->relocation);
        }
    };

    // Cold object-owned DATA only. ELFv1's STT_FUNC is its 24-byte .opd
    // descriptor; LLVM records the real body extent in that symbol's st_size
    // and the first descriptor relocation names its text entry. No caller
    // integer, native descriptor memory, name lookup or next-symbol estimate
    // can authenticate this range. Only real LoadedObjectInfo relocates it.
    [[nodiscard]] inline ppc64_elfv1_loaded_function_range
        get_ppc64_elfv1_loaded_function_range(::llvm::object::ObjectFile const& object,
            ::llvm::object::SymbolRef const& symbol,
            ::llvm::LoadedObjectInfo const& loaded,
            ::std::uint64_t body_size, ppc64_elfv1_relocation_index const& index)
    {
        if(!index.matches(object) || symbol.getObject() != ::std::addressof(object) || body_size == 0u) { return {}; }
        auto const* elf{::llvm::dyn_cast<::llvm::object::ELFObjectFileBase>(::std::addressof(object))};
        if(elf == nullptr || !elf->is64Bit() || elf->getEMachine() != ::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::machine_ppc64 ||
           elf->getEType() != ::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::type_relocatable ||
           (elf->getPlatformFlags() & ::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::ppc64_abi) > 1u) { return {}; }
        auto const elf_symbol{::llvm::object::ELFSymbolRef{symbol}};
        if(elf_symbol.getELFType() != ::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::symbol_function || elf_symbol.getSize() != body_size) { return {}; }
        auto descriptor_section_result{symbol.getSection()};
        if(!descriptor_section_result) { ::llvm::consumeError(descriptor_section_result.takeError()); return {}; }
        auto const descriptor_section{*descriptor_section_result};
        // [actual object section iterator][section_end]
        // [safe] Exact object symbol membership precedes section lookup; check
        // the sentinel before any section field or owned name borrow.
        if(descriptor_section == object.section_end() || descriptor_section->isText()) { return {}; }
        auto descriptor_name{descriptor_section->getName()};
        if(!descriptor_name) { ::llvm::consumeError(descriptor_name.takeError()); return {}; }
        auto const descriptor_metadata{::llvm::object::ELFSectionRef{*descriptor_section}};
        if(*descriptor_name != ".opd" || descriptor_metadata.getType() != ::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::section_progbits ||
           (descriptor_metadata.getFlags() & ::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::flag_alloc) == 0u) { return {}; }
        auto descriptor_address{symbol.getAddress()};
        if(!descriptor_address) { ::llvm::consumeError(descriptor_address.takeError()); return {}; }
        auto const descriptor_base{descriptor_section->getAddress()}, descriptor_size{descriptor_section->getSize()};
        if(*descriptor_address < descriptor_base) { return {}; }
        auto const descriptor_offset{*descriptor_address - descriptor_base};
        if((descriptor_offset & 7u) != 0u || descriptor_offset > descriptor_size ||
           24u > descriptor_size - descriptor_offset) { return {}; }
        auto const* observed{index.unique(*descriptor_section,descriptor_offset)};
        if(observed==nullptr || observed->getType()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::ppc64_addr64) { return {}; }
        auto const& relocation{*observed};
        auto const target_symbol{relocation.getSymbol()};
        if(target_symbol == object.symbol_end()) { return {}; }
        auto text_result{target_symbol->getSection()};
        if(!text_result) { ::llvm::consumeError(text_result.takeError()); return {}; }
        auto const text{*text_result};
        // [actual relocation target section][section_end]
        // [safe] Sentinel and executable-section checks precede every
        // address/size/relocation query; neither native bytes nor .opd
        // pointers are dereferenced or cast to a callable.
        if(text == object.section_end() || !text->isText()) { return {}; }
        auto target_address{target_symbol->getAddress()};
        if(!target_address) { ::llvm::consumeError(target_address.takeError()); return {}; }
        auto addend{::llvm::object::ELFRelocationRef{relocation}.getAddend()};
        if(!addend) { ::llvm::consumeError(addend.takeError()); return {}; }
        auto const text_base{text->getAddress()}, text_size{text->getSize()};
        if(*target_address < text_base) { return {}; }
        auto offset{*target_address - text_base};
        if(offset > text_size) { return {}; }
        if(*addend < 0)
        {
            // INT64_MIN is handled without negating it directly.
            auto const magnitude{static_cast<::std::uint64_t>(-(*addend + 1)) + 1u};
            if(magnitude > offset) { return {}; }
            offset -= magnitude; // Subtraction follows the complete lower-bound proof.
        }
        else
        {
            auto const magnitude{static_cast<::std::uint64_t>(*addend)};
            if(magnitude > text_size - offset) { return {}; }
            offset += magnitude; // Addition follows the remaining-section upper bound.
        }
        if(offset >= text_size || body_size > text_size - offset) { return {}; }
        auto const load_address{loaded.getSectionLoadAddress(*text)};
        constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
        if(load_address == 0u || load_address > limit || offset > limit - load_address) { return {}; }
        auto const begin{load_address + offset};
        if(body_size > limit - begin) { return {}; }
        // [real relocated text + checked entry offset ... exact body end]
        // [safe] All integer additions follow native-width/text bounds;
        // the descriptor's declared real LLVM extent fits loaded text.
        return {static_cast<::std::uintptr_t>(begin), static_cast<::std::uintptr_t>(body_size)};
    }
}
#endif
