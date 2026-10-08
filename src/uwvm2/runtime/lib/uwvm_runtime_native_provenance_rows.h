/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "native_dwarf_register_location.h"
#include "uwvm_runtime_native_arm_mapping.h"
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <fast_io.h>
# include <fast_io_dsal/string.h>
# include <llvm/BinaryFormat/Dwarf.h>
# include <llvm/DebugInfo/DIContext.h>
# include <llvm/DebugInfo/DWARF/DWARFContext.h>
# include <llvm/DebugInfo/DWARF/DWARFDebugLine.h>
# include <llvm/DebugInfo/DWARF/DWARFDie.h>
# include <llvm/DebugInfo/DWARF/DWARFFormValue.h>
# include <llvm/DebugInfo/DWARF/DWARFUnit.h>
# include <llvm/Object/ObjectFile.h>
# include <llvm/Object/ELFObjectFile.h>
# include "uwvm_runtime_ppc64_elfv1_loaded_function_range.h"
# include <llvm/Support/Error.h>

namespace uwvm2::runtime::lib::details::native_loaded_provenance
{
    // This is owned DATA, never read authority. Runtime queries must separately
    // pin/authenticate the actual stopped participant, trap, publication, code
    // range, module/function, function generation and execution epoch.
    enum class status : unsigned char { unavailable, unknown, exact, ambiguous };
    struct position
    {
        status state{status::unavailable};
        ::std::uintptr_t begin{}, end{};
        ::std::uint_least32_t wasm_offset{};
    };
    class image
    {
        struct text_range
        {
            ::std::uintptr_t begin{}, end{};
            ::std::uint64_t object{}, section{};
        };
        struct row
        {
            ::std::uintptr_t begin{}, end{};
            ::std::uint64_t object{};
            ::std::uint_least32_t line{}, file{}, column{};
        };
        struct file_key { ::std::uint64_t dwarf{}; ::std::uint_least32_t owned{}; };
        ::std::vector<text_range> text_{};
        ::std::vector<text_range> literal_data_{};
        ::std::vector<row> rows_{};
        ::std::vector<::fast_io::string> files_{};
        ::std::vector<::std::uint64_t> objects_{};
        ::std::uint_least64_t runtime_epoch_{};
        ::std::size_t decoded_rows_{};
        bool valid_{true};

        static constexpr ::std::size_t max_object_bytes{64u * 1024u * 1024u};
        static constexpr ::std::size_t max_objects{4096u};
        static constexpr ::std::size_t max_sections{4096u};
        static constexpr ::std::size_t max_rows{262144u};
        static constexpr ::std::size_t max_files{4096u};
        static constexpr ::std::size_t max_units{4096u};
        static constexpr ::std::size_t max_filename_bytes{192u};


        struct numeric_range
        {
            ::std::uintptr_t begin{}, end{}, owner_begin{}, owner_end{};
            ::std::uint64_t object{};
            ::std::uint_least32_t file{};
            unsigned dwarf_register{}, bits{};
        };
        ::std::vector<numeric_range> numeric_{};
        struct numeric_owner { ::std::uintptr_t begin{},end{};::std::uint64_t object{};::std::uint_least32_t file{}; };
        ::std::vector<numeric_owner> numeric_owners_{};
        // Private object DATA parser. Only exact Register locations of our
        // compiler-owned numeric witnesses can be projected at that one PC.
        // Direct/Indirect/Constant locations and all native frame fields are
        // ignored; no signal SP or native pointer is ever dereferenced here.
        [[nodiscard]] bool collect_numeric_stackmaps(::llvm::object::ObjectFile const& object,
            ::llvm::LoadedObjectInfo const& loaded, ::std::uint64_t object_key)
        {
            if(object.getArch()!=::llvm::Triple::ppc64 && object.getArch()!=::llvm::Triple::ppc64le) { return true; }
            auto const* elf{::llvm::dyn_cast<::llvm::object::ELFObjectFileBase>(::std::addressof(object))};
            if(elf==nullptr || !elf->is64Bit() || elf->getEType()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::type_relocatable) { return true; }
            auto chosen{object.section_end()};
            for(auto const& section:object.sections())
            {
                auto name{section.getName()};if(!name) { ::llvm::consumeError(name.takeError());return false; }
                if(*name==".llvm_stackmaps") { if(chosen!=object.section_end()) { return false; } chosen=::llvm::object::section_iterator{section}; }
            }
            if(chosen==object.section_end()) { return true; }
            auto content{chosen->getContents()};if(!content) { ::llvm::consumeError(content.takeError());return false; }
            auto const bytes{*content};bool okay{true};
            auto const read{[&](::std::size_t offset,unsigned width) -> ::std::uint64_t
            {
                if(width>8u || offset>bytes.size() || width>bytes.size()-offset) { okay=false;return 0u; }
                ::std::uint64_t value{};
                for(unsigned i{};i!=width;++i)
                { value |= static_cast<::std::uint64_t>(static_cast<unsigned char>(bytes[offset+i])) <<
                    (8u*(object.isLittleEndian() ? i : width-i-1u)); }
                return value;
            }};
            if(bytes.size()<16u || bytes.size()>max_object_bytes || read(0u,1u)!=3u || read(1u,3u)!=0u) { return false; }
            auto const functions{read(4u,4u)},constants{read(8u,4u)},records{read(12u,4u)};
            if(functions>max_sections || constants>max_rows || records>max_rows ||
               functions>(bytes.size()-16u)/24u || constants>(bytes.size()-16u-functions*24u)/8u) { return false; }
            struct function { ::std::uintptr_t begin{},end{};::std::uint_least32_t file{};::std::uint64_t count{};bool proved{}; };
            ::std::vector<function> owners;owners.reserve(functions);
            ::uwvm2::runtime::lib::details::ppc64_elfv1_relocation_index const descriptors{object};
            // Index actual-object relocations once, without rescanning every
            // relocation for each function. Only function address fields may
            // carry a relocation in this version-three numeric witness table.
            ::std::vector<::std::optional<::llvm::object::RelocationRef>> addresses(functions);
            for(auto const& section:object.sections())
            {
                auto target{section.getRelocatedSection()};
                if(!target) { ::llvm::consumeError(target.takeError());return false; }
                if(*target==object.section_end() || **target!=*chosen) { continue; }
                for(auto const& relocation:section.relocations())
                {
                    auto const offset{relocation.getOffset()};
                    if(offset<16u || (offset-16u)%24u!=0u || (offset-16u)/24u>=functions) { return false; }
                    auto& slot{addresses[(offset-16u)/24u]};if(slot) { return false; }slot=relocation;
                }
            }
            ::std::vector<numeric_owner> qualified;
            for(auto const& owner:numeric_owners_) { if(owner.object==object_key) { qualified.push_back(owner); } }
            auto const owner_less{[](numeric_owner const& a,numeric_owner const& b) noexcept
                { return a.begin<b.begin || (a.begin==b.begin && a.end<b.end); }};
            ::std::sort(qualified.begin(),qualified.end(),owner_less);
            ::std::vector<row> indexed_rows;
            for(auto const& item:rows_)
            { if(item.object==object_key && item.column==1u && item.line!=0u) { indexed_rows.push_back(item); } }
            auto const row_less{[](row const& a,row const& b) noexcept
                { return a.file<b.file || (a.file==b.file && (a.line<b.line || (a.line==b.line && a.begin<b.begin))); }};
            ::std::sort(indexed_rows.begin(),indexed_rows.end(),row_less);
            // Union only equal-file/equal-line metadata intervals. This is a
            // join for a separate exact-PC register witness, never code display
            // authority: code_permissions retains every conflicting source row.
            ::std::size_t kept{};
            for(auto const item:indexed_rows)
            {
                if(kept!=0u && indexed_rows[kept-1u].file==item.file && indexed_rows[kept-1u].line==item.line &&
                   item.begin<=indexed_rows[kept-1u].end)
                { indexed_rows[kept-1u].end=(::std::max)(indexed_rows[kept-1u].end,item.end); }
                else { indexed_rows[kept++]=item; }
            }
            indexed_rows.resize(kept);
            ::std::uint64_t count{};
            for(::std::size_t i{};i!=functions;++i)
            {
                auto const offset{16u+i*24u};auto const record_count{read(offset+16u,8u)};
                if(record_count>records-count) { return false; } count+=record_count;
                // Stack size is intentionally never interpreted or retained.
                function fn{};fn.count=record_count;
                if(!addresses[i]) { return false; }
                auto const& relocation{*addresses[i]};
                if(relocation.getType()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::ppc64_addr64) { return false; }
                auto addend{::llvm::object::ELFRelocationRef{relocation}.getAddend()};
                if(!addend) { ::llvm::consumeError(addend.takeError());return false; }
                if(*addend!=0) { return false; }
                auto symbol{relocation.getSymbol()};if(symbol==object.symbol_end()) { return false; }
                auto const elf_symbol{::llvm::object::ELFSymbolRef{*symbol}};
                auto const size{elf_symbol.getSize()};
                if(elf_symbol.getELFType()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::symbol_function || size==0u) { return false; }
                auto section_result{symbol->getSection()};if(!section_result) { ::llvm::consumeError(section_result.takeError());return false; }
                if(*section_result==object.section_end()) { return false; }
                auto const& code_section{**section_result};
                if(code_section.isText())
                {
                    auto address{symbol->getAddress()};if(!address) { ::llvm::consumeError(address.takeError());return false; }
                    auto const base{code_section.getAddress()},length{code_section.getSize()},load{loaded.getSectionLoadAddress(code_section)};
                    if(*address<base || *address-base>=length || size>length-(*address-base) ||
                       load==0u || load>UINTPTR_MAX || *address-base>UINTPTR_MAX-load ||
                       size>UINTPTR_MAX-load-(*address-base)) { return false; }
                    fn.begin=static_cast<::std::uintptr_t>(load+*address-base);fn.end=fn.begin+size;
                }
                else
                {
                    auto const body{::uwvm2::runtime::lib::details::get_ppc64_elfv1_loaded_function_range(object,*symbol,loaded,size,descriptors)};
                    if(body.begin!=0u && body.size!=0u && body.size<=UINTPTR_MAX-body.begin)
                    { fn.begin=body.begin;fn.end=fn.begin+body.size; }
                }
                numeric_owner const key{fn.begin,fn.end};
                auto found{::std::lower_bound(qualified.begin(),qualified.end(),key,owner_less)};
                if(found!=qualified.end() && found->begin==fn.begin && found->end==fn.end)
                {
                    auto next{found};++next;
                    if(next!=qualified.end() && next->begin==fn.begin && next->end==fn.end) { return false; }
                    fn.file=found->file;fn.proved=true;
                }
                owners.push_back(fn);
            }
            if(count!=records) { return false; }
            ::std::size_t cursor{static_cast<::std::size_t>(16u+functions*24u+constants*8u)};
            for(auto const& fn:owners)
            {
                for(::std::uint64_t record{};record!=fn.count;++record)
                {
                    if(cursor>bytes.size() || bytes.size()-cursor<16u) { return false; }
                    auto const id{read(cursor,8u)},pc_offset{read(cursor+8u,4u)},locations{read(cursor+14u,2u)};
                    if(read(cursor+12u,2u)!=0u || locations>128u || locations>(bytes.size()-cursor-16u)/12u) { return false; }
                    auto const where{cursor+16u};cursor=where+locations*12u;
                    auto const padding{(8u-cursor%8u)%8u};if(padding>bytes.size()-cursor) { return false; }
                    for(unsigned i{};i!=padding;++i) { if(read(cursor+i,1u)!=0u) { return false; } } cursor+=padding;
                    if(bytes.size()-cursor<4u || read(cursor,2u)!=0u) { return false; }
                    auto const liveouts{read(cursor+2u,2u)};cursor+=4u;
                    if(liveouts>(bytes.size()-cursor)/4u) { return false; } cursor+=liveouts*4u;
                    auto const end_padding{(8u-cursor%8u)%8u};if(end_padding>bytes.size()-cursor) { return false; }
                    for(unsigned i{};i!=end_padding;++i) { if(read(cursor+i,1u)!=0u) { return false; } } cursor+=end_padding;
                    if(!fn.proved || (id>>48u)!=0x5557u || locations!=1u || (id & 0xffffffffu)==0u ||
                       ((id>>32u)&0xffu)>=128u || pc_offset>=fn.end-fn.begin || 4u>fn.end-fn.begin-pc_offset) { continue; }
                    auto const kind{(id>>40u)&0xffu};unsigned bits{};
                    if(kind==0x7eu) { bits=64u; } else if(kind==0x7bu) { bits=128u; } else { continue; }
                    auto const width{read(where+2u,2u)},reg{read(where+4u,2u)};
                    if(read(where,1u)!=1u || read(where+1u,1u)!=0u || read(where+6u,2u)!=0u || read(where+8u,4u)!=0u ||
                       width*8u!=bits) { continue; } // Register only; NO address, spill or constant.
                    if((kind==0x7eu && reg>=32u) || (kind==0x7bu && (reg<77u || reg>=109u))) { continue; }
                    // The preceding owner bound proves this offset fits the
                    // host address width, including a 32-bit consumer parsing
                    // a foreign ELF64 object. Never narrow before that check.
                    auto const pc{fn.begin+static_cast<::std::uintptr_t>(pc_offset)};
                    row const key{pc,0u,object_key,static_cast<::std::uint_least32_t>(id),fn.file,1u};
                    auto found{::std::upper_bound(indexed_rows.begin(),indexed_rows.end(),key,row_less)};
                    if(found==indexed_rows.begin()) { continue; }--found;
                    if(found->file!=key.file || found->line!=key.line || pc>=found->end) { continue; }
                    if(numeric_.size()==max_rows) { return false; }
                    numeric_.push_back({pc,pc+1u,fn.begin,fn.end,object_key,fn.file,static_cast<unsigned>(reg),bits});
                }
            }
            return okay && cursor==bytes.size();
        }

        [[nodiscard]] bool collect_numeric(::llvm::DWARFDie const root, ::llvm::DWARFDebugLine::LineTable const& table,
            ::std::uint64_t object_key)
        {
            // No pointer/frame-relative/dereference/expression evaluator exists
            // here. Only register-only locations inside explicit lexical ranges
            // of the same actual emitted subprogram can qualify numeric bits.
            struct scope_entry { ::llvm::DWARFDie die; ::llvm::DWARFAddressRange scope{}, owner{}; ::std::uint_least32_t file{}; bool scoped{}; };
            ::std::vector<scope_entry> pending{}; pending.push_back({root});
            ::std::size_t visited{}, decoded{};
            while(!pending.empty())
            {
                auto entry{pending.back()}; pending.pop_back();
                if(++visited > max_rows || pending.size() > max_rows) { return false; }
                auto const tag{entry.die.getTag()};
                if(tag == ::llvm::dwarf::DW_TAG_subprogram || tag == ::llvm::dwarf::DW_TAG_lexical_block)
                {
                    auto ranges{entry.die.getAddressRanges()};
                    if(!ranges) { ::llvm::consumeError(ranges.takeError()); return false; }
                    if(tag == ::llvm::dwarf::DW_TAG_subprogram)
                    {
                        auto const file_index{::llvm::dwarf::toUnsigned(entry.die.find(::llvm::dwarf::DW_AT_decl_file))};
                        ::std::string file{}; // bounded LLVM output parameter only
                        if(file_index)
                        {
                            if(!table.hasFileAtIndex(*file_index) || !table.getFileNameByIndex(*file_index, "uwvm-native-provenance-v1",
                                ::llvm::DILineInfoSpecifier::FileLineInfoKind::RawValue, file)) { return false; }
                        }
                        else
                        {
                            // A synthetic zero-line subprogram may omit decl_file.
                            // Its private CU still owns the actual synthetic file.
                            auto const unit_file{::llvm::dwarf::toStringRef(root.find(::llvm::dwarf::DW_AT_name))};
                            if(unit_file.empty() || unit_file.size() > max_filename_bytes) { continue; }
                            file = ::fast_io::concat_std(::fast_io::mnp::strvw(::std::string_view{unit_file.data(), unit_file.size()}));
                        }
                        if(file.empty() || file.size() > max_filename_bytes) { return false; }
                        entry.file = static_cast<::std::uint_least32_t>(files_.size());
                        for(::std::size_t i{}; i != files_.size(); ++i) { if(::std::string_view{files_[i].data(), files_[i].size()} == file) { entry.file = static_cast<::std::uint_least32_t>(i); break; } }
                        if(entry.file == files_.size())
                        {
                            if(files_.size() == max_files) { return false; }
                            files_.push_back(::fast_io::concat_fast_io(::fast_io::mnp::strvw(file)));
                        }
                    }
                    if(ranges->empty() || ranges->size() > 4096u) { continue; }
                    for(auto const range: *ranges)
                    {
                        if(range.LowPC == 0u || range.HighPC <= range.LowPC || range.HighPC > UINTPTR_MAX) { continue; }
                        auto next{entry}; next.scope = range;
                        if(tag == ::llvm::dwarf::DW_TAG_subprogram)
                        {
                            next.owner = range; next.scoped = false;
                            if(numeric_owners_.size()==max_rows) { return false; }
                            numeric_owners_.push_back({static_cast<::std::uintptr_t>(range.LowPC),
                                static_cast<::std::uintptr_t>(range.HighPC),object_key,entry.file});
                        }
                        else
                        {
                            if(range.LowPC < entry.owner.LowPC || range.HighPC > entry.owner.HighPC ||
                               range.SectionIndex != entry.owner.SectionIndex) { continue; }
                            next.scoped = true;
                        }
                        for(auto const child: entry.die.children()) { next.die = child; pending.push_back(next); }
                    }
                    continue;
                }
                if(tag == ::llvm::dwarf::DW_TAG_variable && entry.scoped)
                {
                    auto const name{::llvm::dwarf::toStringRef(entry.die.find(::llvm::dwarf::DW_AT_name))};
                    if(!name.starts_with("uwvm.native.numeric.")) { continue; }
                    auto const type{entry.die.getAttributeValueAsReferencedDie(::llvm::dwarf::DW_AT_type)};
                    if(!type || type.getTag() != ::llvm::dwarf::DW_TAG_base_type) { continue; }
                    auto const type_name{::llvm::dwarf::toStringRef(type.find(::llvm::dwarf::DW_AT_name))};
                    auto const bytes{::llvm::dwarf::toUnsigned(type.find(::llvm::dwarf::DW_AT_byte_size))};
                    auto const encoding{::llvm::dwarf::toUnsigned(type.find(::llvm::dwarf::DW_AT_encoding))};
                    unsigned bits{};
                    if(bytes && encoding && *encoding == ::llvm::dwarf::DW_ATE_unsigned)
                    {
                        if(type_name == "uwvm.numeric.i32" && *bytes == 4u) { bits = 32u; }
                        if(type_name == "uwvm.numeric.i64" && *bytes == 8u) { bits = 64u; }
                        if(type_name == "uwvm.numeric.v128" && *bytes == 16u) { bits = 128u; }
                    }
                    if(bytes && encoding && *encoding == ::llvm::dwarf::DW_ATE_float)
                    {
                        if(type_name == "uwvm.numeric.f32" && *bytes == 4u) { bits = 32u; }
                        if(type_name == "uwvm.numeric.f64" && *bytes == 8u) { bits = 64u; }
                    }
                    if(bits == 0u || !entry.die.find(::llvm::dwarf::DW_AT_location)) { continue; }
                    auto locations{entry.die.getLocations(::llvm::dwarf::DW_AT_location)};
                    if(!locations) { ::llvm::consumeError(locations.takeError()); return false; }
                    if(locations->size() > max_rows - decoded) { return false; } decoded += locations->size();
                    // Default entries in a mixed location list need complement
                    // range arithmetic. Reject them; never overextend liveness.
                    bool has_ranges{}; for(auto const& location: *locations) { has_ranges |= location.Range.has_value(); }
                    for(auto const& location: *locations)
                    {
                        auto const register_value{exact_register_location(location.Expr)};
                        if(!register_value.available || (!location.Range && has_ranges)) { continue; }
                        auto const reg{register_value.number};
                        auto begin{entry.scope.LowPC}, end{entry.scope.HighPC};
                        if(location.Range)
                        {
                            if(location.Range->SectionIndex != entry.scope.SectionIndex) { continue; }
                            begin = (::std::max)(begin, location.Range->LowPC); end = (::std::min)(end, location.Range->HighPC);
                        }
                        if(begin >= end || numeric_.size() == max_rows) { continue; }
                        numeric_.push_back({static_cast<::std::uintptr_t>(begin), static_cast<::std::uintptr_t>(end),
                            static_cast<::std::uintptr_t>(entry.owner.LowPC), static_cast<::std::uintptr_t>(entry.owner.HighPC),
                            object_key, entry.file, reg, bits});
                    }
                }
                for(auto const child: entry.die.children()) { auto next{entry}; next.die = child; pending.push_back(next); }
            }
            return true;
        }

        // All LLVM borrows end synchronously with observe(). getObjectForDebug
        // is deliberately unused: RuntimeDyld COFF/Mach-O return empty there.
        // Process relocations using the REAL loaded section addresses directly;
        // there is no global slide and no guest custom-section address map.
        [[nodiscard]] bool collect(::std::uint64_t object_key, ::llvm::object::ObjectFile const& object,
            ::llvm::LoadedObjectInfo const& loaded)
        {
            if(!valid_ || object_key == 0u || object.getData().size() > max_object_bytes ||
               objects_.size() == max_objects) { return false; }
            for(auto const key: objects_) { if(key == object_key) { return false; } }
            auto const first_text{text_.size()};
            constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
            for(auto const& section: object.sections())
            {
                if(!section.isText() || section.getSize() == 0u) { continue; }
                auto const begin{loaded.getSectionLoadAddress(section)}, size{section.getSize()};
                if(begin == 0u || begin > limit || size > limit - begin || text_.size() == max_sections) { return false; }
                text_.push_back({static_cast<::std::uintptr_t>(begin), static_cast<::std::uintptr_t>(begin + size),
                    object_key, section.getIndex()});
            }
            ::std::vector<native_arm_mapping::data_range> arm_data{};
            if(!native_arm_mapping::collect(object,arm_data)) { return false; }
            for(auto const& data:arm_data)
            {
                text_range const* section{};
                for(::std::size_t i{first_text};i<text_.size();++i)
                { if(text_[i].section==data.section) { if(section!=nullptr) { return false; }section=::std::addressof(text_[i]); } }
                if(section==nullptr || data.section_size!=section->end-section->begin ||
                   data.end>data.section_size || data.begin>=data.end || literal_data_.size()==max_rows) { return false; }
                literal_data_.push_back({section->begin+static_cast<::std::uintptr_t>(data.begin),
                    section->begin+static_cast<::std::uintptr_t>(data.end),object_key,data.section});
            }
            objects_.push_back(object_key);
            if(first_text == text_.size()) { return true; }
            bool malformed{};
            auto const error{[&](::llvm::Error failure)
            {
                malformed = true; ::llvm::consumeError(::std::move(failure));
            }};
            auto context{::llvm::DWARFContext::create(object,
                ::llvm::DWARFContext::ProcessDebugRelocations::Process, ::std::addressof(loaded), "", error, error)};
            if(context == nullptr || malformed) { return false; }
            ::std::size_t unit_count{};
            for(auto const& unit_owner: context->compile_units())
            {
                if(unit_count == max_units) { return false; }
                ++unit_count;
                auto* const unit{unit_owner.get()};
                if(unit == nullptr) { return false; }
                auto const die{unit->getUnitDIE(false)}; // numeric locations require the complete DIE tree
                if(::llvm::dwarf::toStringRef(die.find(::llvm::dwarf::DW_AT_producer)) !=
                       "uwvm debug-jit native Wasm provenance v1" ||
                   ::llvm::dwarf::toStringRef(die.find(::llvm::dwarf::DW_AT_comp_dir)) != "uwvm-native-provenance-v1")
                { if(malformed) { return false; } continue; }
                auto table_result{context->getLineTableForUnit(unit, error)};
                if(!table_result) { error(table_result.takeError()); return false; }
                auto const* const table{*table_result};
                if(table == nullptr || malformed || table->Rows.size() > max_rows - decoded_rows_) { return false; }
                decoded_rows_ += table->Rows.size();
                if(!collect_numeric(die, *table, object_key)) { return false; }
                ::std::vector<file_key> unit_files{};
                for(auto const& sequence: table->Sequences)
                {
                    if(!sequence.isValid() || sequence.FirstRowIndex >= sequence.LastRowIndex ||
                       sequence.LastRowIndex > table->Rows.size() || sequence.HighPC > limit) { return false; }
                    text_range const* selected{};
                    for(::std::size_t index{first_text}; index != text_.size(); ++index)
                    {
                        // [owned text table ... text_.size) end
                        // [safe                               ] index is checked;
                        //  ^^ real SectionIndex plus complete range, not one global slide.
                        auto const& section{text_[index]};
                        if(sequence.SectionIndex != ::llvm::object::SectionedAddress::UndefSection &&
                           sequence.SectionIndex != section.section) { continue; }
                        if(sequence.LowPC >= section.begin && sequence.HighPC <= section.end)
                        { if(selected != nullptr) { return false; } selected = ::std::addressof(section); }
                    }
                    if(selected == nullptr) { return false; }
                    ::std::size_t first{sequence.FirstRowIndex};
                    while(first < sequence.LastRowIndex)
                    {
                        // [LLVM-owned rows ... checked LastRowIndex) end
                        // [safe                                     ] first < end;
                        //  ^^ no LLVM row iterator/pointer survives this callback.
                        auto const& current{table->Rows[first]};
                        if(current.EndSequence)
                        {
                            if(first + 1u != sequence.LastRowIndex || current.Address.Address != sequence.HighPC) { return false; }
                            break;
                        }
                        auto const begin{current.Address.Address};
                        if(begin < sequence.LowPC || begin > sequence.HighPC) { return false; }
                        ::std::size_t last{first + 1u};
                        while(last < sequence.LastRowIndex && table->Rows[last].Address.Address == begin &&
                              !table->Rows[last].EndSequence) { ++last; }
                        // DWARF permits ordinary zero-width rows at HighPC,
                        // followed by end_sequence at that same address. They
                        // describe no native byte and must not revoke valid
                        // preceding rows or manufacture a readable interval.
                        if(begin == sequence.HighPC)
                        {
                            if(last + 1u != sequence.LastRowIndex || !table->Rows[last].EndSequence ||
                               table->Rows[last].Address.Address != sequence.HighPC) { return false; }
                            for(::std::size_t index{first}; index != last; ++index)
                            {
                                // [zero-width terminal group ... bounded last) end
                                // [safe                                      ] validate scalar
                                //  ^^ section identity only; retain no row/pointer.
                                auto const section{table->Rows[index].Address.SectionIndex};
                                if(section != ::llvm::object::SectionedAddress::UndefSection &&
                                   section != selected->section) { return false; }
                            }
                            // [validated terminal group][final end_sequence]
                            // [safe                    ] last remains < LastRowIndex;
                            //                       ^^ consume only this bounded group.
                            first = last; continue;
                        }
                        auto const end{last == sequence.LastRowIndex ? sequence.HighPC : table->Rows[last].Address.Address};
                        if(end <= begin || end > sequence.HighPC) { return false; }
                        for(::std::size_t index{first}; index != last; ++index)
                        {
                            // [same-address row group ... last) end
                            // [safe                           ] each row has a bounded
                            //  ^^ interval; equal-PC origins stay separate/possibly ambiguous.
                            auto const& origin{table->Rows[index]};
                            if(origin.Address.SectionIndex != ::llvm::object::SectionedAddress::UndefSection &&
                               origin.Address.SectionIndex != selected->section) { return false; }
                            if(rows_.size() == max_rows) { return false; }
                            ::std::uint_least32_t file{(::std::numeric_limits<::std::uint_least32_t>::max)()};
                            for(auto const& entry: unit_files) { if(entry.dwarf == origin.File) { file = entry.owned; break; } }
                            if(file == (::std::numeric_limits<::std::uint_least32_t>::max)())
                            {
                                if(!table->hasFileAtIndex(origin.File)) { return false; }
                                ::std::string filename{}; // LLVM's public output parameter.
                                if(!table->getFileNameByIndex(origin.File, "uwvm-native-provenance-v1",
                                    ::llvm::DILineInfoSpecifier::FileLineInfoKind::RawValue, filename) ||
                                   filename.empty() || filename.size() > max_filename_bytes) { return false; }
                                file = static_cast<::std::uint_least32_t>(files_.size());
                                for(::std::size_t i{}; i != files_.size(); ++i) { if(::std::string_view{files_[i].data(), files_[i].size()} == filename) { file = static_cast<::std::uint_least32_t>(i); break; } }
                                if(file == files_.size())
                                {
                                    if(files_.size() == max_files) { return false; }
                                    files_.push_back(::fast_io::concat_fast_io(::fast_io::mnp::strvw(filename)));
                                }
                                unit_files.push_back({origin.File, file});
                            }
                            rows_.push_back({static_cast<::std::uintptr_t>(begin), static_cast<::std::uintptr_t>(end),
                                object_key, origin.Line, file, origin.Column});
                        }
                        // [row group first ... last][LastRowIndex <= Rows.size]
                        // [safe                     ] last was bounded above;
                        //                      ^^ advance only after consuming the group.
                        first = last;
                    }
                }
            }
            return !malformed && collect_numeric_stackmaps(object,loaded,object_key);
        }
    public:
        image() = default;
        image(image const&) = delete;
        image& operator=(image const&) = delete;
        image(image&&) noexcept = default;
        image& operator=(image&&) noexcept = default;
        // The checkpoint/reset host must revoke, never rebind old native rows to
        // a restored/new execution generation. No opaque handles are retained.
        void invalidate_runtime_generation() noexcept
        {
            valid_ = false; runtime_epoch_ = 0u; decoded_rows_ = 0u;
            rows_.clear(); files_.clear(); text_.clear(); literal_data_.clear(); objects_.clear(); numeric_.clear(); numeric_owners_.clear();
        }
        void bind_actual_runtime_epoch(::std::uint_least64_t epoch) noexcept
        {
            if(!valid_ || runtime_epoch_ != 0u || epoch == 0u) { invalidate_runtime_generation(); return; }
            runtime_epoch_ = epoch;
        }
        [[nodiscard]] bool observe(::std::uint64_t key, ::llvm::object::ObjectFile const& object,
            ::llvm::LoadedObjectInfo const& loaded) noexcept
        {
            if(runtime_epoch_ != 0u) { invalidate_runtime_generation(); return false; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try { if(collect(key, object, loaded)) { return true; } }
            catch(...) {}
#else
            static_cast<void>(key); static_cast<void>(object); static_cast<void>(loaded);
#endif
            // Failure is unavailable metadata, never a partial native address map.
            invalidate_runtime_generation(); return false;
        }
        [[nodiscard]] bool valid() const noexcept { return valid_; }
        [[nodiscard]] ::std::size_t row_count() const noexcept { return rows_.size(); }

        // Layout DATA only. The runtime calls this under the same owner/trap
        // guard as position lookup; users cannot supply native addresses.
        template<typename Location>
        [[nodiscard]] ::std::size_t numeric_locations(::std::uintptr_t pc, ::std::uintptr_t owner_begin,
            ::std::uintptr_t owner_end, ::std::string_view identity, ::std::size_t expression_size,
            ::std::uint_least64_t epoch, Location* output, ::std::size_t capacity) const noexcept
        {
            if(output == nullptr || capacity > 64u) { return 0u; }
            auto const position{lookup(pc, owner_begin, owner_end, identity, expression_size, epoch)};
            if(position.state == status::unavailable) { return 0u; }
            // Register liveness is proved by the separate typed variable's
            // actual relocated owner/lexical/location ranges below, including
            // its exact private file identity. Allocator spills/merged scopes
            // can have zero or overlapping source lines while that real numeric
            // register is live. Source-line ambiguity grants NO code bits;
            // foreign numeric files/ranges and expired epochs still match none.
            text_range const* section{};
            for(auto const& item: text_) { if(owner_begin >= item.begin && owner_end <= item.end) { if(section != nullptr) { return 0u; } section = &item; } }
            if(section == nullptr) { return 0u; }
            ::std::size_t count{};
            for(auto const& item: numeric_)
            {
                // The sealed runtime endpoint is the actual Wasm body, which
                // can exclude the native prologue or target trailer present in
                // DWARF's complete subprogram extent. Join only by containment:
                // metadata may narrow this authenticated body, never enlarge it.
                // lookup already pins pc to that body and its actual epoch;
                // exact object/file/type/register/lexical checks remain below.
                if(item.object != section->object || item.owner_begin > owner_begin || item.owner_end < owner_end ||
                   pc < item.begin || pc >= item.end || item.file >= files_.size() || ::std::string_view{files_[item.file].data(), files_[item.file].size()} != identity) { continue; }
                if(count == capacity) { return 0u; }
                output[count++] = {item.dwarf_register, item.bits};
            }
            return count;
        }
        // Actual relocated ARM mapping DATA only. Literal islands may be
        // skipped in forward boundary discovery; their bytes are never decoded
        // or displayed. This is separate from numeric/code permission and
        // cannot extend an authenticated function, epoch or text section.
        [[nodiscard]] bool instruction_code(::std::uintptr_t begin, ::std::size_t size,
            ::std::uintptr_t owner_begin, ::std::uintptr_t owner_end, ::std::string_view identity,
            ::std::size_t expression_size, ::std::uint_least64_t epoch, ::std::uint8_t* output) const noexcept
        {
            if(output == nullptr || size == 0u || size > PTRDIFF_MAX || begin != owner_begin ||
               owner_begin == 0u || owner_end <= owner_begin || size != owner_end - owner_begin ||
               owner_begin % 4u != 0u || size % 4u != 0u ||
               lookup(owner_begin,owner_begin,owner_end,identity,expression_size,epoch).state == status::unavailable)
            { return false; }
            text_range const* section{};
            for(auto const& item:text_)
            { if(owner_begin>=item.begin && owner_end<=item.end) { if(section!=nullptr) { return false; } section=&item; } }
            if(section==nullptr) { return false; }
            for(::std::size_t i{};i<size;++i) { output[i]=1u; }
            for(auto const& data:literal_data_)
            {
                if(data.object!=section->object || data.section!=section->section ||
                   data.end<=begin || data.begin>=owner_end) { continue; }
                auto const low{(::std::max)(data.begin,begin)},high{(::std::min)(data.end,owner_end)};
                if((low-begin)%4u!=0u || (high-begin)%4u!=0u) { return false; }
                for(auto pc{low};pc<high;++pc) { output[pc-begin]=0u; }
            }
            return output[0u]==1u;
        }
        // Per-byte permission is distinct from the private full function image
        // used for instruction boundary and continuation proofs. Conflicting,
        // unknown, foreign and omitted rows never qualify display bytes.
        [[nodiscard]] bool code_permissions(::std::uintptr_t begin, ::std::size_t size,
            ::std::uintptr_t owner_begin, ::std::uintptr_t owner_end, ::std::string_view identity,
            ::std::size_t expression_size, ::std::uint_least64_t epoch, ::std::uint8_t* output) const noexcept
        {
            if(output == nullptr || size > PTRDIFF_MAX || owner_begin == 0u || owner_end <= owner_begin ||
               begin < owner_begin || begin >= owner_end || size > owner_end - begin ||
               !valid_ || runtime_epoch_ != epoch || epoch == 0u || expression_size == 0u) { return false; }
            text_range const* section{};
            for(auto const& item: text_) { if(owner_begin >= item.begin && owner_end <= item.end) { if(section != nullptr) { return false; } section = &item; } }
            if(section == nullptr) { return false; }
            for(::std::size_t i{}; i != size; ++i) { output[i] = 0u; }
            ::std::size_t charged{};
            for(auto const& item: rows_)
            {
                if(item.object != section->object || item.end <= begin || item.begin >= begin + size) { continue; }
                auto const low{(::std::max)(item.begin, begin)}, high{(::std::min)(item.end, begin + size)};
                if(high - low > 4194304u - charged) { return false; } charged += high - low;
                bool const exact{item.begin >= owner_begin && item.end <= owner_end && item.file < files_.size() &&
                    ::std::string_view{files_[item.file].data(), files_[item.file].size()} == identity && item.line != 0u && item.column == 1u && item.line - 1u < expression_size};
                for(auto pc{low}; pc != high; ++pc) { output[pc - begin] |= exact ? 1u : 2u; }
            }
            for(auto const& data:literal_data_)
            {
                if(data.object!=section->object || data.section!=section->section || data.end<=begin || data.begin>=begin+size) { continue; }
                auto const low{(::std::max)(data.begin,begin)},high{(::std::min)(data.end,begin+size)};
                if(high-low>4194304u-charged) { return false; }charged+=high-low;
                for(auto pc{low};pc!=high;++pc) { output[pc-begin]=2u; }
            }
            for(::std::size_t i{}; i != size; ++i) { output[i] = output[i] == 1u ? 1u : 0u; }
            return true;
        }

        [[nodiscard]] position lookup(::std::uintptr_t pc, ::std::uintptr_t owner_begin,
            ::std::uintptr_t owner_end, ::std::string_view identity, ::std::size_t expression_size,
            ::std::uint_least64_t actual_runtime_epoch) const noexcept
        {
            if(!valid_ || actual_runtime_epoch == 0u || actual_runtime_epoch != runtime_epoch_ || identity.empty() ||
               expression_size == 0u || owner_begin == 0u || owner_end <= owner_begin || pc < owner_begin || pc >= owner_end)
            { return {}; }
            text_range const* section{};
            for(auto const& item: text_)
            {
                if(owner_begin >= item.begin && owner_end <= item.end)
                { if(section != nullptr) { return {}; } section = ::std::addressof(item); }
            }
            if(section == nullptr) { return {}; }
            for(auto const& data:literal_data_)
            { if(data.object==section->object && data.section==section->section && pc>=data.begin && pc<data.end) { return {}; } }
            position result{status::unknown, 0u, 0u, 0u}; // Missing rows have no invented native interval.
            bool known{}, unknown{}, ambiguous{};
            for(auto const& item: rows_)
            {
                if(item.object != section->object || pc < item.begin || pc >= item.end ||
                   item.begin < owner_begin || item.end > owner_end) { continue; }
                if(item.file >= files_.size()) { return {}; }
                if(::std::string_view{files_[item.file].data(), files_[item.file].size()} != identity) { ambiguous = true; continue; } // e.g. optimized/inlined foreign origin.
                result.begin = item.begin; result.end = item.end;
                if(item.line == 0u) { unknown = true; continue; }
                auto const offset{item.line - 1u};
                if(offset >= expression_size) { return {}; }
                if(known && result.wasm_offset != offset) { ambiguous = true; }
                result.wasm_offset = offset; known = true;
            }
            if(ambiguous || (known && unknown)) { result.state = status::ambiguous; }
            else if(known) { result.state = status::exact; }
            return result;
        }
    };
}
#endif
