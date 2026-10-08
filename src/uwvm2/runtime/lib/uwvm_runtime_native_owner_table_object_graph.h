// PRIVATE SOURCE stage: bounded actual-object relocation DATA only. This is
// neither a loaded native owner nor an execution/read/trap permission issuer.
#pragma once
#include "uwvm_runtime_native_owner_table_format.h"
#ifndef UWVM_MODULE
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/coff_headers.h>
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/elf_headers.h>
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/macho_headers.h>
# include <llvm/Object/COFF.h>
# include <llvm/Object/ELFObjectFile.h>
# include <llvm/Object/MachO.h>
# include <llvm/Object/ObjectFile.h>
# include <llvm/Support/Casting.h>
# include <llvm/Support/Error.h>
# include <llvm/TargetParser/Triple.h>
#endif
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
namespace uwvm2::runtime::lib::details::native_owner_object_graph
{
    struct row
    {
        ::std::string original_ir_name{}, entry_object_name{}, local_entry_object_name{};
        unsigned role{}, shape{}, pointer_bytes{};
        // Real same-object TEXT section/offsets, never loaded code addresses.
        ::std::uint64_t section{}, begin_offset{}, end_offset{}, section_size{};
        bool endpoints_proved{};
    };
    namespace detail
    {
        inline constexpr ::std::size_t max_object_bytes{64u*1024u*1024u}, max_sections{4096u}, max_rows{262144u};
        struct point { ::std::uint64_t section{}, offset{}, size{}; bool known{}; };
        [[nodiscard]] inline bool text_point(::llvm::object::ObjectFile const& object,
            ::llvm::object::SectionRef const& section, ::std::uint64_t offset, point& out) noexcept
        {
            // [actual same-object section value][possible section_end value]
            // [safe] sentinel/object identity before any section dereference/API.
            if(section.getObject()!=::std::addressof(object) || ::llvm::object::section_iterator{section}==object.section_end() || !section.isText()) { return false; }
            auto const size{section.getSize()};
            if(size==0u || size>max_object_bytes || offset>size) { return false; }
            out={section.getIndex(),offset,size,true};return true;
        }
        [[nodiscard]] inline bool symbol_point(::llvm::object::ObjectFile const& object,
            ::llvm::object::RelocationRef const& relocation, ::std::uint64_t addend, point& out) noexcept
        {
            if(relocation.getObject()!=::std::addressof(object)) { return false; }
            if(auto const macho{::llvm::dyn_cast<::llvm::object::MachOObjectFile>(::std::addressof(object))})
            {
                auto const info{macho->getRelocation(relocation.getRawDataRefImpl())};
                if(macho->isRelocationScattered(info) || !macho->getPlainRelocationExternal(info)) { return false; }
                auto const table{macho->getSymtabLoadCommand()};
                auto const index{macho->getPlainRelocationSymbolNum(info)};
                ::std::size_t const width{macho->is64Bit() ? sizeof(::llvm::MachO::nlist_64):sizeof(::llvm::MachO::nlist)};
                auto const bytes{object.getData().size()};
                // [actual file ... symoff][actual nsyms complete entries][file_end]
                // [safe] LLVM's getRelocationSymbol forms a pointer before checking
                // the external index. Prove actual index and whole table first;
                // never rely on a resulting iterator != symbol_end as bounds.
                if(index>=table.nsyms || table.symoff>bytes || table.nsyms>(bytes-table.symoff)/width)
                { return false; }
            }
            auto const symbol{relocation.getSymbol()};
            // [actual same-object relocation symbol iterator][symbol_end]
            // [safe] exact sentinel is checked before any symbol API call.
            if(symbol==object.symbol_end()) { return false; }
            auto selected{symbol->getSection()};
            if(!selected) { ::llvm::consumeError(selected.takeError());return false; }
            auto const section{*selected};
            if(section==object.section_end()) { return false; }
            auto address{symbol->getAddress()};
            if(!address) { ::llvm::consumeError(address.takeError());return false; }
            auto const base{section->getAddress()}, size{section->getSize()};
            if(!section->isText() || *address<base || *address-base>size || addend>size-(*address-base)) { return false; }
            // [actual symbol offset][nonnegative exact absolute addend][section_end]
            // [safe] offset<=size and addend<=remaining before scalar addition.
            return text_point(object,*section,*address-base+addend,out);
        }
        [[nodiscard]] inline bool elf_absolute(::llvm::Triple::ArchType arch,unsigned pointer,::std::uint64_t type) noexcept
        {
            namespace elf = ::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants;
            switch(arch)
            {
                case ::llvm::Triple::x86:return pointer==4u && type==elf::relocation_386_32;
                case ::llvm::Triple::x86_64:return pointer==8u && type==elf::relocation_x86_64_64;
                case ::llvm::Triple::aarch64:case ::llvm::Triple::aarch64_be:
                    return pointer==8u ? type==elf::relocation_aarch64_abs64 : pointer==4u && type==elf::relocation_aarch64_abs32;
                case ::llvm::Triple::arm:case ::llvm::Triple::armeb:case ::llvm::Triple::thumb:case ::llvm::Triple::thumbeb:
                    return pointer==4u && type==elf::relocation_arm_abs32;
                case ::llvm::Triple::ppc:return pointer==4u && type==elf::relocation_ppc_addr32;
                case ::llvm::Triple::ppc64:case ::llvm::Triple::ppc64le:return pointer==8u && type==elf::relocation_ppc64_addr64;
                case ::llvm::Triple::mips:case ::llvm::Triple::mipsel:return pointer==4u && type==elf::relocation_mips_32;
                case ::llvm::Triple::mips64:case ::llvm::Triple::mips64el:
                    return pointer==8u ? type==elf::relocation_mips_64 : pointer==4u && type==elf::relocation_mips_32;
                case ::llvm::Triple::riscv32:return pointer==4u && type==elf::relocation_riscv_32;
                case ::llvm::Triple::riscv64:return pointer==8u && type==elf::relocation_riscv_64;
                case ::llvm::Triple::loongarch32:return pointer==4u && type==elf::relocation_larch_32;
                case ::llvm::Triple::loongarch64:return pointer==8u && type==elf::relocation_larch_64;
                case ::llvm::Triple::systemz:return pointer==8u && type==elf::relocation_390_64;
                case ::llvm::Triple::sparc:case ::llvm::Triple::sparcel:
                    return pointer==4u && (type==elf::relocation_sparc_32 || type==elf::relocation_sparc_ua32);
                case ::llvm::Triple::sparcv9:return pointer==8u && (type==elf::relocation_sparc_64 || type==elf::relocation_sparc_ua64);
                default:return false;
            }
        }
        [[nodiscard]] inline bool coff_absolute(::llvm::Triple::ArchType arch,unsigned pointer,::std::uint64_t type) noexcept
        {
            namespace coff = ::uwvm2::runtime::compiler::llvm_jit::win64_coff_constants;
            switch(arch)
            {
                case ::llvm::Triple::x86:return pointer==4u && type==coff::relocation_i386_dir32;
                case ::llvm::Triple::x86_64:return pointer==8u && type==coff::relocation_amd64_addr64;
                case ::llvm::Triple::aarch64:return pointer==8u && type==coff::relocation_arm64_addr64;
                case ::llvm::Triple::arm:case ::llvm::Triple::thumb:return pointer==4u && type==coff::relocation_arm_addr32;
                default:return false;
            }
        }
        [[nodiscard]] inline bool macho_absolute(::llvm::Triple::ArchType arch,unsigned pointer,::std::uint64_t type) noexcept
        {
            namespace macho = ::llvm::MachO;
            switch(arch)
            {
                case ::llvm::Triple::x86_64:return pointer==8u && type==macho::X86_64_RELOC_UNSIGNED;
                case ::llvm::Triple::aarch64:return pointer==8u && type==macho::ARM64_RELOC_UNSIGNED;
                case ::llvm::Triple::x86:return pointer==4u && type==macho::GENERIC_RELOC_VANILLA;
                case ::llvm::Triple::arm:case ::llvm::Triple::thumb:return pointer==4u && type==macho::ARM_RELOC_VANILLA;
                case ::llvm::Triple::ppc:return pointer==4u && type==macho::PPC_RELOC_VANILLA;
                default:return false;
            }
        }
        [[nodiscard]] inline bool endpoint(::llvm::object::ObjectFile const& object,
            ::llvm::object::SectionRef const& relocation_section,::llvm::object::RelocationRef const& relocation,
            unsigned pointer,::std::uint64_t raw,point& out) noexcept
        {
            out={};
            if(relocation.getObject()!=::std::addressof(object) || relocation_section.getObject()!=::std::addressof(object)) { return false; }
            if(::llvm::isa<::llvm::object::ELFObjectFileBase>(object))
            {
                if(!elf_absolute(object.getArch(),pointer,relocation.getType())) { return false; }
                auto const kind{::llvm::object::ELFSectionRef{relocation_section}.getType()};
                ::std::uint64_t addend{};
                if(kind==::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::section_rel) { addend=raw; }
                else if(kind==::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::section_rela || kind==::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::section_crel)
                {
                    // LLVM getAddend() is called only on genuine explicit-addend
                    // sections. REL has no addend API and must use its raw field.
                    auto parsed{::llvm::object::ELFRelocationRef{relocation}.getAddend()};
                    if(!parsed) { ::llvm::consumeError(parsed.takeError());return false; }
                    // This v1 absolute-label producer uses nonnegative addends.
                    // Unproved negative/compound forms decline, never wrap.
                    if(*parsed<0 || raw!=0u) { return false; }
                    addend=static_cast<::std::uint64_t>(*parsed);
                }
                else { return false; }
                return symbol_point(object,relocation,addend,out);
            }
            if(::llvm::isa<::llvm::object::COFFObjectFile>(object))
            {
                if(!coff_absolute(object.getArch(),pointer,relocation.getType())) { return false; }
                return symbol_point(object,relocation,raw,out);
            }
            if(auto const macho{::llvm::dyn_cast<::llvm::object::MachOObjectFile>(::std::addressof(object))})
            {
                auto const info{macho->getRelocation(relocation.getRawDataRefImpl())};
                if(macho->isRelocationScattered(info) || macho->getAnyRelocationPCRel(info)!=0u ||
                   macho->getAnyRelocationLength(info)!=(pointer==8u ? 3u:2u) ||
                   !macho_absolute(object.getArch(),pointer,macho->getAnyRelocationType(info))) { return false; }
                if(macho->getPlainRelocationExternal(info)) { return symbol_point(object,relocation,raw,out); }
                auto const ordinal{macho->getPlainRelocationSymbolNum(info)};
                if(ordinal==0u || ordinal>max_sections) { return false; }
                for(auto const& section:object.sections())
                {
                    // Nonexternal Mach-O relocation ordinal is actual one-based
                    // section index. No symbol guess or image-wide slide applies.
                    if(section.getIndex()!=ordinal-1u) { continue; }
                    auto const base{section.getAddress()},size{section.getSize()};
                    if(!section.isText() || raw<base || raw-base>size) { return false; }
                    return text_point(object,section,raw-base,out);
                }
                return false;
            }
            return false;
        }
        struct wire_row
        {
            native_owner_table_format::record record{};
            ::std::size_t offset{};
            point endpoints[2u]{};
            unsigned counts[2u]{};
        };
    }
    // Failures discard all DATA rows. Object-relative endpoints require later
    // genuine notifyObjectLoaded+same engine/plan/source/generation publication.
    // No row here grants a code view, native address, lease, SI/NI or trap gate.
    [[nodiscard]] inline bool collect(::llvm::object::ObjectFile const& object,::std::vector<row>& output)
    {
        output.clear();
        if(!object.isRelocatableObject() || object.getData().size()>detail::max_object_bytes ||
           (object.getBytesInAddress()!=4u && object.getBytesInAddress()!=8u)) { return false; }
        ::std::vector<row> candidate{};
        ::std::size_t section_count{},table_count{};
        for(auto const& section:object.sections())
        {
            if(++section_count>detail::max_sections) { return false; }
            auto name{section.getName()};
            if(!name) { ::llvm::consumeError(name.takeError());return false; }
            bool const table{*name==".uwvm.native.owners" || *name==".uwvm$NO" || *name=="__uwvm_nowners"};
            if(!table) { continue; }
            if(++table_count!=1u || section.isText()) { return false; }
            auto bytes{section.getContents()};
            if(!bytes) { ::llvm::consumeError(bytes.takeError());return false; }
            if(bytes->empty() || bytes->size()!=section.getSize() || bytes->size()>detail::max_object_bytes || bytes->size()>PTRDIFF_MAX) { return false; }
            ::std::vector<detail::wire_row> records{};
            ::std::size_t offset{};
            while(offset<bytes->size())
            {
                if(records.size()==detail::max_rows || offset>bytes->size() ||
                   bytes->size()-offset<native_owner_table_format::fixed_bytes) { return false; }
                // [one actual object section ... offset][remaining same section]
                // [safe] exact remaining length is proved before suffix pointer.
                auto const* begin{reinterpret_cast<unsigned char const*>(bytes->data())+offset};
                native_owner_table_format::record record{};
                if(!native_owner_table_format::decode({begin,bytes->size()-offset},object.getBytesInAddress(),object.isLittleEndian(),record) ||
                   record.bytes==0u || record.bytes>bytes->size()-offset) { return false; }
                auto const width{record.bytes};
                records.push_back({::std::move(record),offset,{},{}});
                // [consumed one exact complete row][remaining same section]
                // [safe] nonzero width<=remaining before numeric cursor advance.
                offset+=width;
            }
            auto observe=[&](::llvm::object::SectionRef const& relocation_section) -> bool
            {
                ::std::size_t relocations{};
                for(auto const relocation:relocation_section.relocations())
                {
                    if(++relocations>2u*detail::max_rows) { return false; }
                    auto const where{relocation.getOffset()};
                    // Closed bounded row starts, not O(rows*relocations). This
                    // callback owns all records; no iterator survives observation.
                    auto found{::std::upper_bound(records.begin(),records.end(),where,
                        [](auto position,auto const& value) { return position<value.offset; })};
                    if(found==records.begin()) { return false; }
                    // [begin ... found] [end] with found strictly after begin
                    // [safe] the sentinel check precedes moving one owned row back.
                    --found;
                    if(where<found->offset || where-found->offset>=found->record.bytes) { return false; }
                    auto const relative{where-found->offset};
                    unsigned index{};
                    if(relative==found->record.begin_relocation_offset) { index=0u; }
                    else if(relative==found->record.end_relocation_offset) { index=1u; }
                    else { return false; } // No relocation may change role/name/header DATA.
                    if(++found->counts[index]!=1u) { return false; } // paired/compound forms unqualified v1
                    auto const raw{index==0u ? found->record.unrelocated_begin_bits:found->record.unrelocated_end_bits};
                    if(!detail::endpoint(object,relocation_section,relocation,found->record.pointer_bytes,raw,found->endpoints[index])) { return false; }
                }
                return true;
            };
            if(!observe(section)) { return false; }
            for(auto const& relocation_section:object.sections())
            {
                if(relocation_section.getIndex()==section.getIndex()) { continue; }
                auto target{relocation_section.getRelocatedSection()};
                if(!target) { ::llvm::consumeError(target.takeError());return false; }
                if(*target!=object.section_end() && (*target)->getIndex()==section.getIndex())
                { if(!observe(relocation_section)) { return false; } }
            }
            for(auto& record:records)
            {
                auto const& first{record.endpoints[0u]};auto const& last{record.endpoints[1u]};
                bool const proved{record.counts[0u]==1u && record.counts[1u]==1u && first.known && last.known &&
                    first.section==last.section && first.size==last.size && first.offset<last.offset && last.offset<=last.size};
                // Every unknown/zero/missing body remains a row, never removed.
                // A continuous-shape candidate with missing/foreign endpoints is
                // malformed and invalidates the whole candidate; shape0 remains
                // explicit DATA that a later canonical publisher must reject.
                if(record.record.continuous_shape==1u && !proved) { return false; }
                row value{};value.original_ir_name=::std::move(record.record.original_ir_name);
                value.entry_object_name=::std::move(record.record.entry_object_name);
                value.local_entry_object_name=::std::move(record.record.local_entry_object_name);
                value.role=record.record.role;value.shape=record.record.continuous_shape;value.pointer_bytes=record.record.pointer_bytes;
                value.endpoints_proved=proved;
                if(proved) { value.section=first.section;value.begin_offset=first.offset;value.end_offset=last.offset;value.section_size=first.size; }
                candidate.push_back(::std::move(value));
            }
        }
        if(table_count!=1u || candidate.empty()) { return false; }
        output=::std::move(candidate);return true;
    }
}
#endif
