/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)            *
 * Licensed under the APL-2.0 License (see LICENSE file).      *
 *************************************************************/
#pragma once
#include "native_debug_cfi_types.h"
#include "native_debug_cfi_arm.h"

// Internal metadata, not a debugger memory/register permission. Only the
// owning memory manager may observe its actual relocated EH registration.
// No expression, personality, LSDA, native pointer or LLVM borrowed row escapes.
#ifndef UWVM_MODULE
# include <array>
# include <bit>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <span>
# include <vector>
# include <llvm/DebugInfo/DWARF/DWARFDataExtractor.h>
# include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>
# include <llvm/Object/ObjectFile.h>
# include <llvm/Object/ELFObjectFile.h>
# include <llvm/BinaryFormat/ELF.h>
# include <llvm/ExecutionEngine/RuntimeDyld.h>
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::llvm_jit::details
{
    class runtime_llvm_jit_section_memory_manager;

    class native_debug_registered_cfi final
    {
        friend class runtime_llvm_jit_section_memory_manager;
        struct function
        {
            ::std::uintptr_t begin{}, end{};
            ::std::vector<native_debug_cfi_row> rows{};
            ::std::uintptr_t arm_retired_frame_begin{};
        };
        ::std::vector<function> functions_{};
        ::llvm::Triple::ArchType arch_{::llvm::Triple::UnknownArch};
        unsigned address_size_{};
        ::std::size_t row_count_{}, observed_bytes_{};
        bool little_endian_{}, valid_{true};

        // Conservative A32 integer DATA veto. Destination fields include
        // multiply/media/coprocessor pairs, loads and base writeback, not just
        // LDM lists. False positives are allowed; this is neither a complete
        // ISA decoder nor permission to execute/read any native instruction.
        [[nodiscard]] static bool arm_may_replace_frame_register(::std::uint32_t word) noexcept
        {
            if((word >> 28u) == 15u) { return true; } // Unconditional extension space stays unqualified.
            unsigned const group{(word >> 25u) & 7u}, rd{(word >> 12u) & 15u}, rn{(word >> 16u) & 15u};
            bool const load{(word & (1u << 20u)) != 0u};
            auto const memory_writeback{[&]() noexcept
            { return rn == 11u && ((word & (1u << 24u)) == 0u || (word & (1u << 21u)) != 0u); }};
            switch(group)
            {
                case 0u: // LDRD/LDREXD may define the implicit second register r11 from r10.
                    return rd == 11u || ((rd == 10u || rn == 11u) && (word & 0x90u) == 0x90u);
                case 1u: return rd == 11u;
                case 2u: return (load && rd == 11u) || memory_writeback();
                case 3u: // Media/multiply fields differ from ordinary register-offset memory.
                    return (word & 0x10u) != 0u ? rd == 11u || rn == 11u : (load && rd == 11u) || memory_writeback();
                case 4u: return (load && (word & (1u << 11u)) != 0u) ||
                    (rn == 11u && (word & (1u << 21u)) != 0u);
                case 5u: return false; // B/BL change PC/LR, never the frame register.
                case 6u: return rd == 11u || rn == 11u; // Includes two-core-register transfers.
                default: return rd == 11u || (word & 0x0f000000u) == 0x0f000000u; // SVC cannot preserve a frame proof.
            }
        }
        void retire() noexcept { functions_.clear(); valid_ = false; }
        void observe_object(::llvm::Triple::ArchType arch, unsigned size, bool little) noexcept
        {
            if(!valid_) { return; }
            if(arch_ != ::llvm::Triple::UnknownArch &&
               (arch != arch_ || size != address_size_ || little != little_endian_)) { retire(); return; }
            arch_ = arch; address_size_ = size; little_endian_ = little;
        }
        // Default ARM EHABI is unchanged. Debug-only FullDebug functions also
        // carry .debug_frame. Copy its original bytes and apply only the exact
        // same-object ELF32 REL relocation graph; never read an arbitrary loaded
        // debug address, personality, external symbol or native stack here.
        void observe_arm_debug_frame(::llvm::object::ObjectFile const& object,
            ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) noexcept
        {
            if(!valid_ || object.getArch() != ::llvm::Triple::arm) { return; }
#if defined(UWVM_CPP_EXCEPTIONS)
            try
            {
                if(!object.isELF() || !object.isLittleEndian() || object.getBytesInAddress() != 4u)
                { retire(); return; }
                observe_object(object.getArch(), 4u, true);
                auto selected{object.section_end()};
                for(auto section{object.section_begin()}; section != object.section_end(); ++section)
                {
                    auto name{section->getName()};
                    if(!name) { ::llvm::consumeError(name.takeError()); retire(); return; }
                    if(*name != ".debug_frame") { continue; }
                    if(selected != object.section_end()) { retire(); return; }
                    selected = section;
                }
                if(selected == object.section_end()) { return; }
                auto contents{selected->getContents()};
                if(!contents) { ::llvm::consumeError(contents.takeError()); retire(); return; }
                if(contents->empty() || contents->size() > 1024uz * 1024uz ||
                    contents->size() != selected->getSize()) { retire(); return; }
                ::std::vector<::std::uint8_t> bytes(contents->begin(), contents->end());
                auto const read32{[&](::std::size_t offset) noexcept
                {
                    return static_cast<::std::uint32_t>(bytes[offset]) |
                        (static_cast<::std::uint32_t>(bytes[offset+1uz]) << 8u) |
                        (static_cast<::std::uint32_t>(bytes[offset+2uz]) << 16u) |
                        (static_cast<::std::uint32_t>(bytes[offset+3uz]) << 24u);
                }};
                struct record
                { ::std::size_t offset{}; bool cie{}, cie_relocated{}, text_relocated{}; ::std::uintptr_t retired_frame_begin{}; };
                ::std::vector<record> records{};
                for(::std::size_t offset{}; offset != bytes.size();)
                {
                    if(bytes.size()-offset < 8uz || records.size() == 8192uz) { retire(); return; }
                    auto const length{read32(offset)};
                    // DWARF64 and zero-length terminators are not emitted by
                    // this qualified ELF32 graph; do not infer their fields.
                    if(length < 4u || length == UINT32_MAX || length > bytes.size()-offset-4uz)
                    { retire(); return; }
                    bool const cie{read32(offset+4uz) == UINT32_MAX};
                    if(!cie && length < 12u) { retire(); return; }
                    records.push_back({offset, cie, false, false});
                    offset += 4uz + length;
                }
                // Linux elf.h may define these ABI names after LLVM's ELF
                // header. Capture either provider without undefining host macros.
#if defined(SHT_REL)
                constexpr auto elf_rel_section_type{static_cast<::std::uint32_t>(SHT_REL)};
#else
                constexpr auto elf_rel_section_type{static_cast<::std::uint32_t>(::llvm::ELF::SHT_REL)};
#endif
#if defined(R_ARM_ABS32)
                constexpr auto arm_abs32_relocation_type{static_cast<::std::uint32_t>(R_ARM_ABS32)};
#else
                constexpr auto arm_abs32_relocation_type{static_cast<::std::uint32_t>(::llvm::ELF::R_ARM_ABS32)};
#endif
                // A32 instruction words require the original ELF mapping
                // symbols too. Object Arch=arm alone also admits Thumb/data.
                struct mapping { ::std::uint64_t section{}, offset{}; char kind{}; };
                ::std::vector<mapping> mappings{};
                unsigned symbols{};
                for(auto const& symbol: object.symbols())
                {
                    if(++symbols > 65536u) { retire(); return; }
                    auto name{symbol.getName()};
                    if(!name) { ::llvm::consumeError(name.takeError()); retire(); return; }
                    if(name->size() < 2uz || (*name)[0u] != '$' ||
                        ((*name)[1u] != 'a' && (*name)[1u] != 't' && (*name)[1u] != 'd') ||
                        (name->size() != 2uz && (*name)[2u] != '.')) { continue; }
                    auto section{symbol.getSection()}; auto address{symbol.getAddress()};
                    if(!section || !address)
                    {
                        if(!section) { ::llvm::consumeError(section.takeError()); }
                        if(!address) { ::llvm::consumeError(address.takeError()); }
                        retire(); return;
                    }
                    if(*section == object.section_end() || !(*section)->isText()) { continue; }
                    if(*address < (*section)->getAddress() ||
                        *address-(*section)->getAddress() > (*section)->getSize() || mappings.size() == 8192uz)
                    { retire(); return; }
                    mappings.push_back({(*section)->getIndex(), *address-(*section)->getAddress(), (*name)[1u]});
                }
                unsigned relocation_sections{}, relocation_count{};
                ::std::size_t scanned_code_bytes{};
                for(auto const& section: object.sections())
                {
                    auto target{section.getRelocatedSection()};
                    if(!target) { ::llvm::consumeError(target.takeError()); retire(); return; }
                    if(*target == object.section_end() || (*target)->getIndex() != selected->getIndex()) { continue; }
                    if(++relocation_sections != 1u ||
                        ::llvm::object::ELFSectionRef(section).getType() != elf_rel_section_type)
                    { retire(); return; }
                    for(auto const& relocation: section.relocations())
                    {
                        if(++relocation_count > 8192u || relocation.getType() != arm_abs32_relocation_type)
                        { retire(); return; }
                        auto const offset{relocation.getOffset()};
                        record* matched{}; bool cie_field{};
                        for(auto& candidate: records)
                        {
                            if(candidate.cie) { continue; }
                            if(offset == candidate.offset+4uz) { matched = &candidate; cie_field = true; break; }
                            if(offset == candidate.offset+8uz) { matched = &candidate; break; }
                        }
                        if(matched == nullptr || (cie_field ? matched->cie_relocated : matched->text_relocated))
                        { retire(); return; }
                        auto symbol{relocation.getSymbol()};
                        if(symbol == object.symbol_end()) { retire(); return; }
                        auto address{symbol->getAddress()}; auto symbol_section{symbol->getSection()};
                        if(!address || !symbol_section)
                        {
                            if(!address) { ::llvm::consumeError(address.takeError()); }
                            if(!symbol_section) { ::llvm::consumeError(symbol_section.takeError()); }
                            retire(); return;
                        }
                        if(*symbol_section == object.section_end() || *address < (*symbol_section)->getAddress())
                        { retire(); return; }
                        auto const symbol_offset{*address-(*symbol_section)->getAddress()};
                        auto const addend{read32(static_cast<::std::size_t>(offset))};
                        if(symbol_offset > UINT32_MAX || addend > UINT32_MAX-symbol_offset) { retire(); return; }
                        auto const position{symbol_offset+addend};
                        ::std::uint64_t value{};
                        if(cie_field)
                        {
                            if((*symbol_section)->getIndex() != selected->getIndex()) { retire(); return; }
                            bool found{};
                            for(auto const& candidate: records)
                            { if(candidate.cie && candidate.offset == position) { found = true; break; } }
                            if(!found) { retire(); return; }
                            value = position; matched->cie_relocated = true;
                        }
                        else
                        {
                            auto const size{(*symbol_section)->getSize()};
                            auto const base{loaded.getSectionLoadAddress(**symbol_section)};
                            auto const range{read32(matched->offset+12uz)};
                            if(!(*symbol_section)->isText() || base == 0u || base > UINT32_MAX ||
                                size > UINT32_MAX-base || position >= size || range == 0u || range > size-position ||
                                range > 16uz * 1024uz * 1024uz-scanned_code_bytes)
                            { retire(); return; }
                            value = base+position; matched->text_relocated = true;
                            scanned_code_bytes += range;
                            auto code{(*symbol_section)->getContents()};
                            if(!code) { ::llvm::consumeError(code.takeError()); retire(); return; }
                            if(code->size() != size || (position & 3u) != 0u || (range & 3u) != 0u)
                            { retire(); return; }
                            mapping const* initial{};
                            ::std::uint64_t code_limit{range};
                            for(auto const& marker: mappings)
                            {
                                if(marker.section != (*symbol_section)->getIndex()) { continue; }
                                if(marker.offset <= position && (initial == nullptr || marker.offset >= initial->offset))
                                {
                                    if(initial != nullptr && marker.offset == initial->offset) { retire(); return; }
                                    initial = ::std::addressof(marker);
                                }
                                if(marker.offset > position && marker.offset < position+code_limit && marker.kind != 'a')
                                { code_limit = marker.offset-position; }
                            }
                            if(initial == nullptr || initial->kind != 'a' || (code_limit & 3u) != 0u)
                            { retire(); return; }
                            if(code_limit != range)
                            { matched->retired_frame_begin = static_cast<::std::uintptr_t>(value+code_limit); }
                            // Only the first unconditional MOV/ADD r11,sp in
                            // a straight-line prologue may install the frame.
                            // A later identical instruction, a conditional
                            // restore or any other possible r11 write retires
                            // the lexical suffix. Rows/branch layout cannot
                            // revive that proof. Literal/Thumb regions stop it
                            // before any bytes there are decoded as A32.
                            bool installed{}, straight_prologue{true};
                            for(::std::uint64_t n{}; n != code_limit; n += 4u)
                            {
                                auto const* word{reinterpret_cast<unsigned char const*>(code->data())+position+n};
                                auto const instruction{static_cast<::std::uint32_t>(word[0u]) |
                                    (static_cast<::std::uint32_t>(word[1u]) << 8u) |
                                    (static_cast<::std::uint32_t>(word[2u]) << 16u) |
                                    (static_cast<::std::uint32_t>(word[3u]) << 24u)};
                                if(arm_may_replace_frame_register(instruction))
                                {
                                    bool const install{instruction == 0xe1a0b00du ||
                                        (instruction & 0xfffff000u) == 0xe28db000u};
                                    if(!installed && straight_prologue && install) { installed = true; }
                                    else { matched->retired_frame_begin = static_cast<::std::uintptr_t>(value+n+4u); break; }
                                }
                                if(((instruction >> 25u) & 7u) == 5u ||
                                    (instruction & 0x0ffffff0u) == 0x012fff10u ||
                                    (instruction & 0x0ffffff0u) == 0x012fff30u ||
                                    ((instruction & (1u << 20u)) != 0u &&
                                        ((((instruction >> 25u) & 7u) == 4u && (instruction & (1u << 15u)) != 0u) ||
                                         ((((instruction >> 26u) & 3u) == 1u) && ((instruction >> 12u) & 15u) == 15u))))
                                { straight_prologue = false; }
                            }
                        }
                        for(unsigned n{}; n != 4u; ++n)
                        { bytes[static_cast<::std::size_t>(offset)+n] = static_cast<::std::uint8_t>(value >> (8u*n)); }
                    }
                }
                for(auto const& candidate: records)
                { if(!candidate.cie && (!candidate.cie_relocated || !candidate.text_relocated)) { retire(); return; } }
                // Only owned, bounded relocated bytes enter LLVM's synchronous
                // parser. .debug_frame offsets are section-relative, not its
                // loaded address. No borrowed object or LLVM row survives.
                auto const first_function{functions_.size()};
                observe(bytes.data(), 0u, bytes.size(), false);
                if(!valid_) { return; }
                for(auto const& candidate: records)
                {
                    if(candidate.cie || candidate.retired_frame_begin == 0u) { continue; }
                    auto const begin{read32(candidate.offset+8uz)}, range{read32(candidate.offset+12uz)};
                    bool matched{};
                    for(auto index{first_function}; index != functions_.size(); ++index)
                    {
                        auto& function{functions_[index]};
                        if(function.begin == begin && function.end == static_cast<::std::uint64_t>(begin)+range)
                        { function.arm_retired_frame_begin = candidate.retired_frame_begin; matched = true; break; }
                    }
                    if(!matched) { retire(); return; }
                }
            }
            catch(...) { retire(); } // Metadata failure cannot change EHABI registration.
#else
            static_cast<void>(loaded); retire();
#endif
        }
        static native_debug_cfi_rule normalize(::llvm::dwarf::UnwindLocation const& location, unsigned register_limit) noexcept
        {
            using L = ::llvm::dwarf::UnwindLocation;
            using K = native_debug_cfi_rule_kind;
            // Address spaces and arbitrary DWARF expressions are deliberately
            // unavailable; an expression must never introduce a native read.
            if(location.hasAddressSpace()) { return {}; }
            switch(location.getLocation())
            {
                case L::Same: return {K::same, 0u, 0};
                case L::CFAPlusOffset:
                    return {location.getDereference() ? K::cfa_memory : K::cfa_value, 0u, location.getOffset()};
                case L::RegPlusOffset:
                    if(location.getRegister() >= register_limit) { return {}; }
                    return {location.getDereference() ? K::register_memory : K::register_value,
                        location.getRegister(), location.getOffset()};
                default: return {};
            }
        }
        void observe(::std::uint8_t const* bytes, ::std::uint64_t load_address, ::std::size_t size, bool eh_frame = true) noexcept
        {
            if(!valid_) { return; }
#if defined(UWVM_CPP_EXCEPTIONS)
            try
            {
                // Precisely bounded target-endian GPR/RA metadata. i386 has
                // four-byte addresses even when observed by a 64-bit host.
                // A parsed row alone grants no native caller, read or finish.
                // Expressions and other architectures remain unavailable.
                bool const arm{arch_ == ::llvm::Triple::arm && !eh_frame};
                bool const narrow{arm || arch_ == ::llvm::Triple::x86};
                bool const i386{arch_ == ::llvm::Triple::x86};
                bool const mips64{arch_ == ::llvm::Triple::mips64 || arch_ == ::llvm::Triple::mips64el};
                auto const limit{narrow ? static_cast<::std::uint64_t>(UINT32_MAX) : static_cast<::std::uint64_t>(UINTPTR_MAX)};
                auto const ra_register{arm ? 14u : i386 ? 8u : mips64 ? 31u : (arch_ == ::llvm::Triple::riscv64 || arch_ == ::llvm::Triple::loongarch64) ? 1u : arch_ == ::llvm::Triple::aarch64 ? 30u : 16u};
                auto const cfa_register_limit{arm ? 15u : i386 ? 8u : arch_ == ::llvm::Triple::x86_64 ? 16u : 32u};
                if((!arm && !i386 && !mips64 && arch_ != ::llvm::Triple::x86_64 && arch_ != ::llvm::Triple::riscv64 && arch_ != ::llvm::Triple::aarch64 && arch_ != ::llvm::Triple::loongarch64) || address_size_ != (narrow ? 4u : 8u) || little_endian_ != (arch_ != ::llvm::Triple::mips64) ||
                   bytes == nullptr || (eh_frame && load_address == 0u) || (!eh_frame && !arm) || size == 0uz || size > 1024uz * 1024uz || size > 16uz * 1024uz * 1024uz - observed_bytes_ ||
                   load_address > limit || size > limit - load_address) { retire(); return; }
                observed_bytes_ += size;
                ::llvm::DWARFDebugFrame frame{arch_, eh_frame, load_address};
                // [actual relocated manager-owned EH section: size bytes] end
                // [safe] parse synchronously while the original allocation lives.
                auto error{frame.parse(::llvm::DWARFDataExtractor{
                    ::llvm::StringRef{reinterpret_cast<char const*>(bytes), size}, little_endian_, static_cast<::std::uint8_t>(address_size_)})};
                if(error) { ::llvm::consumeError(::std::move(error)); retire(); return; }
                for(auto const& entry: frame.entries())
                {
                    auto const fde{::llvm::dyn_cast<::llvm::dwarf::FDE>(::std::addressof(entry))};
                    if(fde == nullptr) { continue; }
                    auto const cie{fde->getLinkedCIE()};
                    auto const begin{fde->getInitialLocation()}, size{fde->getAddressRange()};
                    if(cie == nullptr || cie->getReturnAddressRegister() != ra_register ||
                       begin == 0u || begin > limit || size == 0u || size > limit - begin || functions_.size() == 4096uz)
                    { retire(); return; }
                    auto table{::llvm::dwarf::createUnwindTable(fde)};
                    if(!table) { ::llvm::consumeError(table.takeError()); retire(); return; }
                    if(table->size() > 65536uz - row_count_) { retire(); return; }
                    function candidate{static_cast<::std::uintptr_t>(begin), static_cast<::std::uintptr_t>(begin + size), {}};
                    for(auto const& row: *table)
                    {
                        if(!row.hasAddress() || row.getAddress() < begin || row.getAddress() > begin + size ||
                           (!candidate.rows.empty() && row.getAddress() < candidate.rows.back().begin)) { retire(); return; }
                        if(row.getAddress() == begin + size) { continue; } // No executable interval.
                        native_debug_cfi_row value{};
                        value.begin = static_cast<::std::uintptr_t>(row.getAddress()); value.end = candidate.end;
                        auto const& cfa{row.getCFAValue()};
                        value.usable = cfa.getLocation() == ::llvm::dwarf::UnwindLocation::RegPlusOffset &&
                            !cfa.getDereference() && !cfa.hasAddressSpace() && cfa.getRegister() < cfa_register_limit &&
                            (!arm || cfa.getRegister() == 11u || cfa.getRegister() == 13u);
                        value.cfa_usable = value.usable;
                        if(value.usable)
                        {
                            value.cfa_register = cfa.getRegister(); value.cfa_offset = cfa.getOffset();
                            for(::std::uint32_t reg{}; reg != value.registers.size(); ++reg)
                            {
                                auto rule{row.getRegisterLocations().getRegisterLocation(reg)};
                                if(rule) { value.registers[reg] = normalize(*rule, arm ? 15u : i386 ? 8u : 32u); }
                            }
                            value.usable = value.registers[ra_register].kind != native_debug_cfi_rule_kind::unavailable;
                            if(arch_ == ::llvm::Triple::aarch64)
                            {
                                // DWARF column 34 is private PAuth RA state.
                                // Never strip a signature or turn signed LR
                                // bits into caller authority. CFA-only metadata
                                // remains usable without a return-address read.
                                auto state{row.getRegisterLocations().getRegisterLocation(34u)};
                                if(state && (state->getLocation() != ::llvm::dwarf::UnwindLocation::Constant ||
                                    state->getConstant() != 0 || state->getDereference() || state->hasAddressSpace()))
                                { value.usable = false; }
                            }
                        }
                        // CIE and FDE programs may update the same PC. The last
                        // state wins; zero-width earlier states must not match.
                        if(!candidate.rows.empty() && candidate.rows.back().begin == value.begin)
                        { candidate.rows.back() = value; }
                        else { candidate.rows.push_back(value); }
                    }
                    for(::std::size_t i{1uz}; i < candidate.rows.size(); ++i)
                    { candidate.rows[i - 1uz].end = candidate.rows[i].begin; }
                    row_count_ += candidate.rows.size();
                    functions_.push_back(::std::move(candidate));
                }
            }
            catch(...) { retire(); } // Debug metadata failure cannot alter EH registration.
#else
            static_cast<void>(bytes); static_cast<void>(load_address); static_cast<void>(size); static_cast<void>(eh_frame); retire();
#endif
        }
        bool copy_row(::std::uintptr_t begin, ::std::uintptr_t end, ::std::uintptr_t pc,
            native_debug_cfi_row& out, bool require_return_address = true) const noexcept
        {
            out = {};
            if(!valid_ || begin == 0u || end <= begin || pc < begin || pc >= end) { return false; }
            function const* selected{};
            for(auto const& function: functions_)
            {
                if(pc < function.begin || pc >= function.end) { continue; }
                // Any overlap is ambiguous, including a mismatching enclosing
                // FDE. An interior PC or widened extent cannot pick an owner.
                if(selected != nullptr || function.begin != begin || function.end != end) { return false; }
                selected = ::std::addressof(function);
            }
            if(selected == nullptr) { return false; }
            if(arch_ == ::llvm::Triple::arm && selected->arm_retired_frame_begin != 0u &&
                pc >= selected->arm_retired_frame_begin) { return false; }
            for(auto const& row: selected->rows)
            {
                if(pc >= row.begin && pc < row.end && (require_return_address ? row.usable : row.cfa_usable))
                { out = row; return true; }
            }
            return false;
        }
    };

    struct native_debug_cfi_caller
    {
        ::std::array<::std::uint64_t, 17u> registers{};
        ::std::uint32_t known{};
        ::std::uintptr_t cfa{}, return_pc{};
    };
    template<typename Reader>
    [[nodiscard]] inline bool evaluate_native_debug_cfi_x64_bounded(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 17u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t copy_begin, ::std::uintptr_t copy_end, Reader const& read_word,
        native_debug_cfi_caller& out) noexcept
    {
        out = {};
        constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
        if(!row.usable || row.cfa_register >= 16u || !(known & (1u << row.cfa_register)) ||
           !(known & (1u << 7u)) || copy_begin == 0u || copy_end <= copy_begin) { return false; }
        auto const add{[&](::std::uint64_t base, ::std::int32_t offset, ::std::uintptr_t& result) noexcept
        {
            if(base > limit) { return false; }
            if(offset >= 0)
            {
                if(static_cast<::std::uint32_t>(offset) > limit - base) { return false; }
                result = static_cast<::std::uintptr_t>(base + static_cast<::std::uint32_t>(offset));
            }
            else
            {
                auto const magnitude{static_cast<::std::uint64_t>(-static_cast<::std::int64_t>(offset))};
                if(magnitude > base) { return false; }
                result = static_cast<::std::uintptr_t>(base - magnitude);
            }
            return true;
        }};
        ::std::uintptr_t cfa{};
        if(!add(registers[row.cfa_register], row.cfa_offset, cfa) || registers[7u] < copy_begin ||
           registers[7u] >= copy_end || cfa <= registers[7u] || cfa > copy_end) { return false; }
        native_debug_cfi_caller candidate{};
        for(::std::uint32_t reg{}; reg != candidate.registers.size(); ++reg)
        {
            // Recover callee-preserved GPRs and RA only. Volatile registers,
            // omitted rules, FP/vector state and infrastructure are not inferred.
            if(reg != 3u && reg != 6u && reg != 12u && reg != 13u && reg != 14u && reg != 15u && reg != 16u) { continue; }
            auto const& rule{row.registers[reg]};
            using K = native_debug_cfi_rule_kind;
            ::std::uintptr_t value{};
            bool available{};
            if(rule.kind == K::same && (known & (1u << reg))) { value = registers[reg]; available = true; }
            else if(rule.kind == K::cfa_value || rule.kind == K::cfa_memory) { available = add(cfa, rule.offset, value); }
            else if((rule.kind == K::register_value || rule.kind == K::register_memory) && rule.reg < 16u && (known & (1u << rule.reg)))
            { available = add(registers[rule.reg], rule.offset, value); }
            if(available && (rule.kind == K::cfa_memory || rule.kind == K::register_memory))
            {
                ::std::uint64_t loaded{}; available = value >= copy_begin && value < copy_end && copy_end - value >= 8u && read_word(value, loaded); value = static_cast<::std::uintptr_t>(loaded);
            }
            if(available) { candidate.registers[reg] = value; candidate.known |= 1u << reg; }
        }
        if(!(candidate.known & (1u << 16u)) || candidate.registers[16u] == 0u) { return false; }
        candidate.cfa = cfa; candidate.return_pc = candidate.registers[16u];
        candidate.registers[7u] = cfa; candidate.known |= 1u << 7u;
        out = candidate; return true;
    }
    // Pure readers consume ONLY owned DATA. Integer labels below carry no live
    // stack permission. The runtime must authenticate each copied CFI word.
    struct native_debug_cfi_owned_word
    {
        ::std::uintptr_t address{};
        ::std::uint64_t value{};
    };
    [[nodiscard]] inline bool evaluate_native_debug_cfi_x64(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 17u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t copy_begin, ::std::span<::std::byte const> copy, native_debug_cfi_caller& out) noexcept
    {
        out = {};
        if(copy.empty() || copy.size() > UINTPTR_MAX - copy_begin) { return false; }
        auto const read{[&](::std::uintptr_t address, ::std::uint64_t& value) noexcept
        {
            ::std::memcpy(::std::addressof(value), copy.data() + (address - copy_begin), 8u);
            if constexpr(::std::endian::native != ::std::endian::little) { value = ::std::byteswap(value); }
            return true;
        }};
        return evaluate_native_debug_cfi_x64_bounded(row, registers, known, copy_begin,
            copy_begin + copy.size(), read, out);
    }
    [[nodiscard]] inline bool evaluate_native_debug_cfi_x64_sparse(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 17u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t frame_begin, ::std::uintptr_t frame_end,
        ::std::span<native_debug_cfi_owned_word const> words, native_debug_cfi_caller& out) noexcept
    {
        out = {};
        if(words.size() > 7u || frame_end <= frame_begin) { return false; }
        for(::std::size_t n{}; n != words.size(); ++n)
        {
            auto const address{words[n].address};
            if(address < frame_begin || address >= frame_end || frame_end - address < 8u) { return false; }
            for(::std::size_t prior{}; prior != n; ++prior)
            { if(words[prior].address == address) { return false; } }
        }
        auto const read{[&](::std::uintptr_t address, ::std::uint64_t& value) noexcept
        {
            for(auto const& word: words)
            { if(word.address == address) { value = word.value; return true; } }
            return false; // A missing slot is unknown, never an invented zero.
        }};
        return evaluate_native_debug_cfi_x64_bounded(row, registers, known,
            frame_begin, frame_end, read, out);
    }

    // i386 slots have their own type: no eight-byte reader may consume an
    // adjacent word (which may already belong to the parent or VM frame).
    struct native_debug_cfi_i386_owned_word
    {
        ::std::uintptr_t address{};
        ::std::uint32_t value{};
    };
    struct native_debug_cfi_i386_caller
    {
        ::std::array<::std::uint32_t, 9u> registers{};
        ::std::uint32_t known{};
        ::std::uintptr_t cfa{}, return_pc{};
    };
    template<typename Reader>
    [[nodiscard]] inline bool evaluate_native_debug_cfi_i386_bounded(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 9u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t frame_begin, ::std::uintptr_t frame_end, Reader const& read_word,
        native_debug_cfi_i386_caller& out) noexcept
    {
        out = {};
        using K = native_debug_cfi_rule_kind;
        if(!row.usable || row.cfa_register >= 8u || !(known & (1u << row.cfa_register)) ||
           !(known & (1u << 4u)) || frame_begin == 0u || frame_end <= frame_begin || frame_end > UINT32_MAX ||
           row.registers[8u].kind != K::cfa_memory || row.registers[8u].offset != -4) { return false; }
        auto const add{[](::std::uint64_t base, ::std::int32_t delta, ::std::uintptr_t& result) noexcept
        {
            if(base > UINT32_MAX) { return false; }
            auto const sum{static_cast<::std::int64_t>(base) + static_cast<::std::int64_t>(delta)};
            if(sum < 0 || sum > UINT32_MAX) { return false; }
            result = static_cast<::std::uintptr_t>(sum); return true;
        }};
        ::std::uintptr_t cfa{};
        if(!add(registers[row.cfa_register], row.cfa_offset, cfa) ||
           registers[4u] != frame_begin || cfa <= frame_begin || cfa > frame_end) { return false; }
        native_debug_cfi_i386_caller candidate{};
        for(unsigned reg : {3u, 5u, 6u, 7u, 8u})
        {
            auto const& rule{row.registers[reg]};
            ::std::uintptr_t value{}; bool available{};
            if(rule.kind == K::same && (known & (1u << reg)))
            { available = add(registers[reg], 0, value); }
            else if(rule.kind == K::cfa_value || rule.kind == K::cfa_memory)
            { available = add(cfa, rule.offset, value); }
            else if((rule.kind == K::register_value || rule.kind == K::register_memory) &&
                    rule.reg < 8u && (known & (1u << rule.reg)))
            { available = add(registers[rule.reg], rule.offset, value); }
            if(available && (rule.kind == K::cfa_memory || rule.kind == K::register_memory))
            {
                ::std::uint32_t loaded{};
                available = value >= frame_begin && value < cfa && cfa - value >= 4u && read_word(value, loaded);
                value = loaded;
            }
            if(available) { candidate.registers[reg] = static_cast<::std::uint32_t>(value); candidate.known |= 1u << reg; }
        }
        if(!(candidate.known & (1u << 8u)) || candidate.registers[8u] == 0u) { return false; }
        candidate.cfa = cfa; candidate.return_pc = candidate.registers[8u];
        candidate.registers[4u] = static_cast<::std::uint32_t>(cfa); candidate.known |= 1u << 4u;
        out = candidate; return true;
    }
    [[nodiscard]] inline bool evaluate_native_debug_cfi_i386_sparse(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 9u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t frame_begin, ::std::uintptr_t frame_end,
        ::std::span<native_debug_cfi_i386_owned_word const> words, native_debug_cfi_i386_caller& out) noexcept
    {
        out = {};
        if(words.size() > 5u || frame_end <= frame_begin || frame_end > UINT32_MAX) { return false; }
        for(::std::size_t n{}; n != words.size(); ++n)
        {
            auto const address{words[n].address};
            if(address < frame_begin || address >= frame_end || frame_end - address < 4u) { return false; }
            for(::std::size_t prior{}; prior != n; ++prior)
            {
                auto const other{words[prior].address};
                if(address > other ? address - other < 4u : other - address < 4u) { return false; }
            }
        }
        auto const read{[&](::std::uintptr_t address, ::std::uint32_t& value) noexcept
        {
            for(auto const& word: words)
            { if(word.address == address) { value = word.value; return true; } }
            return false;
        }};
        return evaluate_native_debug_cfi_i386_bounded(row, registers, known, frame_begin, frame_end, read, out);
    }

    struct native_debug_cfi_riscv64_caller
    {
        ::std::array<::std::uint64_t, 32u> registers{};
        ::std::uint32_t known{};
        ::std::uintptr_t cfa{}, return_pc{};
    };
    template<typename Reader>
    [[nodiscard]] inline bool evaluate_native_debug_cfi_riscv64_bounded(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 32u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t copy_begin, ::std::uintptr_t copy_end, Reader const& read_word,
        native_debug_cfi_riscv64_caller& out) noexcept
    {
        out = {};
        constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
        if(!row.usable || row.cfa_register >= 32u || !(known & (1u << row.cfa_register)) ||
           !(known & (1u << 2u)) || copy_begin == 0u || copy_end <= copy_begin) { return false; }
        auto const add{[&](::std::uint64_t base, ::std::int32_t offset, ::std::uintptr_t& result) noexcept
        {
            if(base > limit) { return false; }
            if(offset >= 0)
            {
                if(static_cast<::std::uint32_t>(offset) > limit - base) { return false; }
                result = static_cast<::std::uintptr_t>(base + static_cast<::std::uint32_t>(offset));
            }
            else
            {
                auto const magnitude{static_cast<::std::uint64_t>(-static_cast<::std::int64_t>(offset))};
                if(magnitude > base) { return false; }
                result = static_cast<::std::uintptr_t>(base - magnitude);
            }
            return true;
        }};
        ::std::uintptr_t cfa{};
        if(!add(registers[row.cfa_register], row.cfa_offset, cfa) || registers[2u] < copy_begin ||
           registers[2u] >= copy_end || cfa <= registers[2u] || cfa > copy_end) { return false; }
        native_debug_cfi_riscv64_caller candidate{};
        for(::std::uint32_t reg{}; reg != candidate.registers.size(); ++reg)
        {
            // Recover callee-preserved GPRs and RA only. Volatile registers,
            // omitted rules, FP/vector state and infrastructure are not inferred.
            if(reg != 1u && reg != 8u && reg != 9u && (reg < 18u || reg > 27u)) { continue; }
            auto const& rule{row.registers[reg]};
            using K = native_debug_cfi_rule_kind;
            ::std::uintptr_t value{};
            bool available{};
            if(rule.kind == K::same && (known & (1u << reg))) { value = registers[reg]; available = true; }
            else if(rule.kind == K::cfa_value || rule.kind == K::cfa_memory) { available = add(cfa, rule.offset, value); }
            else if((rule.kind == K::register_value || rule.kind == K::register_memory) && rule.reg < 32u && (known & (1u << rule.reg)))
            { available = add(registers[rule.reg], rule.offset, value); }
            if(available && (rule.kind == K::cfa_memory || rule.kind == K::register_memory))
            {
                ::std::uint64_t loaded{}; available = value >= copy_begin && value < copy_end && copy_end - value >= 8u && read_word(value, loaded); value = static_cast<::std::uintptr_t>(loaded);
            }
            if(available) { candidate.registers[reg] = value; candidate.known |= 1u << reg; }
        }
        if(!(candidate.known & (1u << 1u)) || candidate.registers[1u] == 0u) { return false; }
        candidate.cfa = cfa; candidate.return_pc = candidate.registers[1u];
        candidate.registers[2u] = cfa; candidate.known |= 1u << 2u;
        out = candidate; return true;
    }
    [[nodiscard]] inline bool evaluate_native_debug_cfi_riscv64_sparse(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 32u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t frame_begin, ::std::uintptr_t frame_end,
        ::std::span<native_debug_cfi_owned_word const> words, native_debug_cfi_riscv64_caller& out) noexcept
    {
        out = {};
        if(words.size() > 13u || frame_end <= frame_begin) { return false; }
        for(::std::size_t n{}; n != words.size(); ++n)
        {
            auto const address{words[n].address};
            if(address < frame_begin || address >= frame_end || frame_end - address < 8u) { return false; }
            for(::std::size_t prior{}; prior != n; ++prior)
            { if(words[prior].address == address) { return false; } }
        }
        auto const read{[&](::std::uintptr_t address, ::std::uint64_t& value) noexcept
        {
            for(auto const& word: words)
            { if(word.address == address) { value = word.value; return true; } }
            return false; // A missing slot is unknown, never an invented zero.
        }};
        return evaluate_native_debug_cfi_riscv64_bounded(row, registers, known,
            frame_begin, frame_end, read, out);
    }

    // LoongArch LP64: recover only the explicit RA rule and preserved
    // R22..R31. SP/R3 is derived from CFA. TP/R2, reserved R21, volatile
    // GPRs, FP/LSX/LASX and absent rules remain unknown.
    struct native_debug_cfi_loongarch64_caller
    {
        ::std::array<::std::uint64_t, 32u> registers{};
        ::std::uint32_t known{};
        ::std::uintptr_t cfa{}, return_pc{};
    };
    template<typename Reader>
    [[nodiscard]] inline bool evaluate_native_debug_cfi_loongarch64_bounded(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 32u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t frame_begin, ::std::uintptr_t frame_end, Reader const& read_word,
        native_debug_cfi_loongarch64_caller& out) noexcept
    {
        out = {};
        constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
        if(!row.usable || row.cfa_register >= 32u || !(known & (1u << row.cfa_register)) ||
           !(known & (1u << 3u)) || frame_begin == 0u || frame_end < frame_begin ||
           registers[3u] != frame_begin || (frame_begin & 15u) != 0u) { return false; }
        auto const add{[&](::std::uint64_t base, ::std::int32_t offset, ::std::uintptr_t& value) noexcept
        {
            if(base > limit) { return false; }
            if(offset >= 0)
            {
                if(static_cast<::std::uint32_t>(offset) > limit - base) { return false; }
                value = static_cast<::std::uintptr_t>(base + static_cast<::std::uint32_t>(offset));
            }
            else
            {
                auto const magnitude{static_cast<::std::uint64_t>(-static_cast<::std::int64_t>(offset))};
                if(magnitude > base) { return false; }
                value = static_cast<::std::uintptr_t>(base - magnitude);
            }
            return true;
        }};
        ::std::uintptr_t cfa{};
        if(!add(registers[row.cfa_register], row.cfa_offset, cfa) ||
           cfa < frame_begin || cfa > frame_end || (cfa & 15u) != 0u) { return false; }
        native_debug_cfi_loongarch64_caller candidate{};
        for(unsigned reg{}; reg != 32u; ++reg)
        {
            if(reg != 1u && (reg < 22u || reg > 31u)) { continue; }
            auto const& rule{row.registers[reg]}; using K = native_debug_cfi_rule_kind;
            ::std::uintptr_t value{}; bool available{};
            if(rule.kind == K::same && (known & (1u << reg))) { available = add(registers[reg], 0, value); }
            else if(rule.kind == K::cfa_value || rule.kind == K::cfa_memory) { available = add(cfa, rule.offset, value); }
            else if((rule.kind == K::register_value || rule.kind == K::register_memory) &&
                    rule.reg < 32u && (known & (1u << rule.reg)))
            { available = add(registers[rule.reg], rule.offset, value); }
            if(available && (rule.kind == K::cfa_memory || rule.kind == K::register_memory))
            {
                ::std::uint64_t loaded{};
                available = value >= frame_begin && value < cfa && cfa - value >= 8u && read_word(value, loaded);
                value = static_cast<::std::uintptr_t>(loaded);
            }
            if(available) { candidate.registers[reg] = value; candidate.known |= 1u << reg; }
        }
        if(!(candidate.known & (1u << 1u)) || candidate.registers[1u] == 0u ||
           (candidate.registers[1u] & 3u) != 0u) { return false; }
        candidate.cfa = cfa; candidate.return_pc = candidate.registers[1u];
        candidate.registers[3u] = cfa; candidate.known |= 1u << 3u;
        out = candidate; return true;
    }
    [[nodiscard]] inline bool evaluate_native_debug_cfi_loongarch64_sparse(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 32u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t frame_begin, ::std::uintptr_t frame_end,
        ::std::span<native_debug_cfi_owned_word const> words, native_debug_cfi_loongarch64_caller& out) noexcept
    {
        out = {};
        if(words.size() > 11u || frame_end < frame_begin) { return false; }
        for(::std::size_t n{}; n != words.size(); ++n)
        {
            auto const address{words[n].address};
            if(address < frame_begin || address >= frame_end || frame_end - address < 8u) { return false; }
            for(::std::size_t prior{}; prior != n; ++prior)
            {
                auto const other{words[prior].address};
                if(address > other ? address - other < 8u : other - address < 8u) { return false; }
            }
        }
        auto const read{[&](::std::uintptr_t address, ::std::uint64_t& value) noexcept
        {
            for(auto const& word: words)
            { if(word.address == address) { value = word.value; return true; } }
            return false;
        }};
        return evaluate_native_debug_cfi_loongarch64_bounded(row, registers, known, frame_begin, frame_end, read, out);
    }

    // Linux MIPS64 N64 private data: s0..s7, gp, fp and ra only. SP/r29
    // is the proved CFA. Volatile, TLS/kernel, FP/MSA and absent rules stay
    // unknown. This evaluator grants no native read or execution authority.
    struct native_debug_cfi_mips64_caller
    {
        ::std::array<::std::uint64_t, 32u> registers{};
        ::std::uint32_t known{};
        ::std::uintptr_t cfa{}, return_pc{};
    };
    template<typename Reader>
    [[nodiscard]] inline bool evaluate_native_debug_cfi_mips64_bounded(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 32u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t frame_begin, ::std::uintptr_t frame_end, Reader const& read_word,
        native_debug_cfi_mips64_caller& out) noexcept
    {
        out = {};
        constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
        if(!row.usable || row.cfa_register >= 32u || !(known & (1u << row.cfa_register)) ||
           !(known & (1u << 29u)) || frame_begin == 0u || frame_end < frame_begin ||
           registers[29u] != frame_begin || (frame_begin & 15u) != 0u) { return false; }
        auto const add{[&](::std::uint64_t base, ::std::int32_t offset, ::std::uintptr_t& value) noexcept
        {
            if(base > limit) { return false; }
            if(offset >= 0)
            {
                if(static_cast<::std::uint32_t>(offset) > limit - base) { return false; }
                value = static_cast<::std::uintptr_t>(base + static_cast<::std::uint32_t>(offset));
            }
            else
            {
                auto const magnitude{static_cast<::std::uint64_t>(-static_cast<::std::int64_t>(offset))};
                if(magnitude > base) { return false; }
                value = static_cast<::std::uintptr_t>(base - magnitude);
            }
            return true;
        }};
        ::std::uintptr_t cfa{};
        if(!add(registers[row.cfa_register], row.cfa_offset, cfa) ||
           cfa < frame_begin || cfa > frame_end || (cfa & 15u) != 0u) { return false; }
        native_debug_cfi_mips64_caller candidate{};
        for(unsigned reg{}; reg != 32u; ++reg)
        {
            if((reg < 16u || reg > 23u) && reg != 28u && reg != 30u && reg != 31u) { continue; }
            auto const& rule{row.registers[reg]}; using K = native_debug_cfi_rule_kind;
            ::std::uintptr_t value{}; bool available{};
            if(rule.kind == K::same && (known & (1u << reg))) { available = add(registers[reg], 0, value); }
            else if(rule.kind == K::cfa_value || rule.kind == K::cfa_memory) { available = add(cfa, rule.offset, value); }
            else if((rule.kind == K::register_value || rule.kind == K::register_memory) &&
                    rule.reg < 32u && (known & (1u << rule.reg)))
            { available = add(registers[rule.reg], rule.offset, value); }
            if(available && (rule.kind == K::cfa_memory || rule.kind == K::register_memory))
            {
                ::std::uint64_t loaded{};
                available = (value & 7u) == 0u && value >= frame_begin && value < cfa && cfa - value >= 8u && read_word(value, loaded);
                available = available && loaded <= limit;
                value = static_cast<::std::uintptr_t>(loaded);
            }
            if(available) { candidate.registers[reg] = value; candidate.known |= 1u << reg; }
        }
        if(!(candidate.known & (1u << 31u)) || candidate.registers[31u] == 0u ||
           (candidate.registers[31u] & 3u) != 0u) { return false; }
        candidate.cfa = cfa; candidate.return_pc = candidate.registers[31u];
        candidate.registers[29u] = cfa; candidate.known |= 1u << 29u;
        out = candidate; return true;
    }
    [[nodiscard]] inline bool evaluate_native_debug_cfi_mips64_sparse(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 32u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t frame_begin, ::std::uintptr_t frame_end,
        ::std::span<native_debug_cfi_owned_word const> words, native_debug_cfi_mips64_caller& out) noexcept
    {
        out = {};
        if(words.size() > 11u || frame_end < frame_begin) { return false; }
        for(::std::size_t n{}; n != words.size(); ++n)
        {
            auto const address{words[n].address};
            if((address & 7u) != 0u || address < frame_begin || address >= frame_end || frame_end - address < 8u) { return false; }
            for(::std::size_t prior{}; prior != n; ++prior)
            {
                auto const other{words[prior].address};
                if(address > other ? address - other < 8u : other - address < 8u) { return false; }
            }
        }
        auto const read{[&](::std::uintptr_t address, ::std::uint64_t& value) noexcept
        {
            for(auto const& word: words)
            { if(word.address == address) { value = word.value; return true; } }
            return false;
        }};
        return evaluate_native_debug_cfi_mips64_bounded(row, registers, known, frame_begin, frame_end, read, out);
    }

    // AAPCS64: only X19..X29 and the explicit LR rule may survive a
    // recovered frame. SP is a private CFA value. X0..X18, PC, PSTATE,
    // FP/SIMD/SVE, signed-address state and omitted rules stay unknown.
    struct native_debug_cfi_aarch64_caller
    {
        ::std::array<::std::uint64_t, 32u> registers{};
        ::std::uint32_t known{};
        ::std::uintptr_t cfa{}, return_pc{};
    };
    template<typename Reader>
    [[nodiscard]] inline bool evaluate_native_debug_cfi_aarch64_bounded(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 32u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t copy_begin, ::std::uintptr_t copy_end, Reader const& read_word,
        native_debug_cfi_aarch64_caller& out) noexcept
    {
        out = {};
        constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
        if(!row.usable || row.cfa_register >= 32u || !(known & (1u << row.cfa_register)) ||
           !(known & (1u << 31u)) || copy_begin == 0u || copy_end < copy_begin) { return false; }
        auto const add{[&](::std::uint64_t base, ::std::int32_t offset, ::std::uintptr_t& result) noexcept
        {
            if(base > limit) { return false; }
            if(offset >= 0)
            {
                if(static_cast<::std::uint32_t>(offset) > limit - base) { return false; }
                result = static_cast<::std::uintptr_t>(base + static_cast<::std::uint32_t>(offset));
            }
            else
            {
                auto const magnitude{static_cast<::std::uint64_t>(-static_cast<::std::int64_t>(offset))};
                if(magnitude > base) { return false; }
                result = static_cast<::std::uintptr_t>(base - magnitude);
            }
            return true;
        }};
        ::std::uintptr_t cfa{};
        if(!add(registers[row.cfa_register], row.cfa_offset, cfa) ||
           registers[31u] < copy_begin || registers[31u] > copy_end ||
           cfa < registers[31u] || cfa > copy_end || (registers[31u] & 15u) != 0u || (cfa & 15u) != 0u) { return false; }
        native_debug_cfi_aarch64_caller candidate{};
        for(::std::uint32_t reg{19u}; reg != 31u; ++reg)
        {
            auto const& rule{row.registers[reg]};
            using K = native_debug_cfi_rule_kind;
            ::std::uintptr_t value{};
            bool available{};
            if(rule.kind == K::same && (known & (1u << reg))) { value = registers[reg]; available = true; }
            else if(rule.kind == K::cfa_value || rule.kind == K::cfa_memory) { available = add(cfa, rule.offset, value); }
            else if((rule.kind == K::register_value || rule.kind == K::register_memory) &&
                    rule.reg < 32u && (known & (1u << rule.reg)))
            { available = add(registers[rule.reg], rule.offset, value); }
            if(available && (rule.kind == K::cfa_memory || rule.kind == K::register_memory))
            {
                ::std::uint64_t loaded{};
                available = value >= copy_begin && value < copy_end && copy_end - value >= 8u && read_word(value, loaded);
                value = static_cast<::std::uintptr_t>(loaded);
            }
            if(available) { candidate.registers[reg] = value; candidate.known |= 1u << reg; }
        }
        if(!(candidate.known & (1u << 30u)) || candidate.registers[30u] == 0u ||
           (candidate.registers[30u] & 3u) != 0u) { return false; }
        candidate.cfa = cfa; candidate.return_pc = candidate.registers[30u];
        candidate.registers[31u] = cfa; candidate.known |= 1u << 31u;
        out = candidate; return true;
    }
    [[nodiscard]] inline bool evaluate_native_debug_cfi_aarch64_sparse(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 32u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t frame_begin, ::std::uintptr_t frame_end,
        ::std::span<native_debug_cfi_owned_word const> words, native_debug_cfi_aarch64_caller& out) noexcept
    {
        out = {};
        if(words.size() > 12u || frame_end < frame_begin) { return false; }
        for(::std::size_t n{}; n != words.size(); ++n)
        {
            auto const address{words[n].address};
            if(address < frame_begin || address >= frame_end || frame_end - address < 8u) { return false; }
            for(::std::size_t prior{}; prior != n; ++prior)
            { if(words[prior].address == address) { return false; } }
        }
        auto const read{[&](::std::uintptr_t address, ::std::uint64_t& value) noexcept
        {
            for(auto const& word: words)
            { if(word.address == address) { value = word.value; return true; } }
            return false;
        }};
        return evaluate_native_debug_cfi_aarch64_bounded(row, registers, known,
            frame_begin, frame_end, read, out);
    }

}
