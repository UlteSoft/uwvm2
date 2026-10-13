/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_variants.h"
# include "source_dwarf_constants.h"
#endif

// Cold embedded-only metadata parser. Native runtime owns source/stop authority;
// run/controller may install this immutable metadata without executing locations.
// Merely including the aggregate does not reference DWARF library symbols.
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
# ifndef UWVM_MODULE
#  include <algorithm>
#  include <array>
#  include <cstddef>
#  include <cstdint>
#  include <limits>
#  include <memory>
#  include <map>
#  include <new>
#  include <optional>
#  include <set>
#  include <span>
#  include <string>
#  include <string_view>
#  include <type_traits>
#  include <utility>
#  include <vector>
#  include <llvm/ADT/StringMap.h>
#  include <llvm/BinaryFormat/Dwarf.h>
#  include <llvm/DebugInfo/DWARF/DWARFContext.h>
#  include <llvm/DebugInfo/DWARF/DWARFDie.h>
#  include <llvm/DebugInfo/DWARF/DWARFDebugLine.h>
#  include <llvm/DebugInfo/DWARF/DWARFFormValue.h>
#  include <llvm/DebugInfo/DWARF/DWARFUnit.h>
#  include <llvm/Support/Error.h>
#  include <llvm/Support/MemoryBuffer.h>
# endif
# ifndef UWVM_MODULE_EXPORT
#  define UWVM_MODULE_EXPORT
# endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_dwarf
{
    namespace details { struct index_builder; }
    class index final
    {
        friend struct details::index_builder;
        // LLVM's StringMap create overload BORROWS every buffer. This owner is
        // never moved; reverse member destruction retires context first. LLVM
        // handles never escape through the public metadata accessors.
        // Diagnostic storage also precedes context: a destruction-time LLVM
        // callback may still record a kind while this same owner is alive.
        ::std::array<::std::uint8_t, 32u> diagnostics_{};
        ::std::size_t diagnostic_count_{};
        bool diagnostics_truncated_{};
        ::std::uint8_t address_bytes_{}; // validated immutable producer ABI; no memory read grant.
        ::llvm::StringMap<::std::unique_ptr<::llvm::MemoryBuffer>> buffers_{};
        ::std::unique_ptr<::llvm::DWARFContext> context_{};
        ::std::vector<line_directory_record> line_directories_{};
        ::std::vector<scope_record> scopes_{};
        ::std::vector<type_record> types_{};
        ::std::vector<variable_record> variables_{};
        explicit index() = default;
        void consume_diagnostic(::llvm::Error issue, ::std::uint8_t kind) noexcept
        {
            // No LLVM default stderr handler/string formatting/loader is used.
            // A bounded private kind list records failure; guest strings do not
            // become output, paths, commands or an external-source capability.
            if(diagnostic_count_ < diagnostics_.size()) { diagnostics_[diagnostic_count_++] = kind; }
            else { diagnostics_truncated_ = true; }
            ::llvm::consumeError(::std::move(issue));
        }
    public:
        index(index const&) = delete;
        index& operator=(index const&) = delete;
        index(index&&) = delete;
        index& operator=(index&&) = delete;
        ~index() = default;
        // All returned spans borrow this nonmoving owner until destruction.
        // Their strings/ranges describe untrusted metadata, not runtime values.
        [[nodiscard]] ::std::span<line_directory_record const> line_directories() const noexcept { return line_directories_; }
        [[nodiscard]] ::std::uint8_t address_bytes() const noexcept { return address_bytes_; }
        [[nodiscard]] ::std::span<scope_record const> scopes() const noexcept { return scopes_; }
        [[nodiscard]] ::std::span<type_record const> types() const noexcept { return types_; }
        [[nodiscard]] ::std::span<variable_record const> variables() const noexcept { return variables_; }
        [[nodiscard]] static error parse(image_input input, ::std::unique_ptr<index>& out, limits const& cap = {}) noexcept;
    };
    static_assert(!::std::is_move_constructible_v<index> && !::std::is_copy_constructible_v<index>);

    namespace details
    {
        struct index_builder
        {
            index& result;
            image_input input;
            limits const& cap;
            budget used{};
            ::std::map<::std::uint64_t,producer_language> producer_units{};
            error failure{};
            ::std::vector<::llvm::DWARFUnit*> units{};
            ::std::set< ::std::pair<::std::uint64_t, ::std::uint64_t>> visited{};
            // Attribute proof belongs to this immutable context only. Charge every
            // attribute once even when semantic helpers revisit a referenced DIE.
            ::std::set< ::std::pair<::std::uint64_t, ::std::uint64_t>> inspected_attributes{};
            ::std::set<::std::uint64_t> counted_line_tables{};
            ::std::map<::std::pair<::std::uint64_t, ::std::uint64_t>, ::std::size_t> type_indices{};

            [[nodiscard]] bool fail(error why) noexcept { if(failure == error::none) { failure = why; } return false; }
            [[nodiscard]] bool charge(::std::size_t amount, ::std::size_t limit, ::std::size_t& counter) noexcept
            { return budget::charge(amount, limit, counter) || fail(error::limit_exceeded); }
            [[nodiscard]] static die_key key(::llvm::DWARFDie die) noexcept
            {
                // [safe] only a checked live DIE from result.context_ enters;
                // both borrows last solely within this synchronous build.
                return {die.getDwarfUnit()->getOffset(), die.getOffset()};
            }
            [[nodiscard]] static bool external_form(::llvm::dwarf::Form form) noexcept
            {
                using namespace ::llvm::dwarf;
                return form == DW_FORM_ref_sup4 || form == DW_FORM_ref_sup8 || form == DW_FORM_strp_sup ||
                    form == DW_FORM_GNU_ref_alt || form == DW_FORM_GNU_strp_alt;
            }
            [[nodiscard]] bool prepare_sections()
            {
                if(input.address_bytes != 4u && input.address_bytes != 8u) { return fail(error::unsupported_dwarf); }
                if(input.sections.size() > cap.max_sections) { return fail(error::limit_exceeded); }
                ::std::vector<::std::string_view> seen{};
                for(auto const& entry : input.sections)
                {
                    if(entry.name.size() > 64u || entry.payload.size() > cap.max_section_bytes ||
                       !charge(entry.payload.size(), cap.max_total_bytes, used.bytes)) { return fail(error::limit_exceeded); }
                    if(::std::find(seen.begin(), seen.end(), entry.name) != seen.end()) { return fail(error::malformed); }
                    seen.push_back(entry.name); // borrowed only until parse returns; input owns this name.
                    if(entry.name == "external_debug_info" || entry.name == ".gnu_debugaltlink" || entry.name == ".debug_sup" ||
                       entry.name == ".debug_cu_index" || entry.name == ".debug_tu_index" || entry.name == ".debug_types" ||
                       entry.name.ends_with(".dwo") || entry.name == ".debug_gdb_scripts")
                    { return fail(error::unsupported_external); }
                    bool const selected{entry.name == ".debug_info" || entry.name == ".debug_abbrev" || entry.name == ".debug_str" ||
                        entry.name == ".debug_line" || entry.name == ".debug_line_str" || entry.name == ".debug_loc" ||
                        entry.name == ".debug_loclists" || entry.name == ".debug_ranges" || entry.name == ".debug_rnglists" ||
                        entry.name == ".debug_addr" || entry.name == ".debug_str_offsets"};
                    if(!selected)
                    {
                        // Known passive tables are unnecessary for this bounded
                        // DIE walk; none are passed to LLVM or interpreted.
                        if(entry.name.starts_with(".debug_") && entry.name != ".debug_pubnames" && entry.name != ".debug_pubtypes" &&
                           entry.name != ".debug_names" && entry.name != ".debug_aranges" && entry.name != ".debug_macinfo" &&
                           entry.name != ".debug_macro" && entry.name != ".debug_frame")
                        { return fail(error::unsupported_dwarf); }
                        continue;
                    }
                    // [caller-owned bounded custom payload] payload_end
                    // [safe                             ] unsafe (one-past)
                    //  ^^ no byte/pointer advances; getMemBufferCopy makes an
                    // independent owner before any caller storage can disappear.
                    auto const bytes{::llvm::StringRef{reinterpret_cast<char const*>(entry.payload.data()), entry.payload.size()}};
                    // [safe] selected is an exact nonempty .debug_* name; this
                    // name borrow drops only its first dot and stays in input.
                    auto const name{::llvm::StringRef{entry.name.data() + 1u, entry.name.size() - 1u}};
                    result.buffers_[name] = ::llvm::MemoryBuffer::getMemBufferCopy(bytes, name);
                }
                auto const info{result.buffers_.find("debug_info")}, abbrev{result.buffers_.find("debug_abbrev")};
                if(info == result.buffers_.end() || abbrev == result.buffers_.end() ||
                   info->second->getBufferSize() == 0u || abbrev->second->getBufferSize() == 0u)
                { return fail(error::missing_sections); }
                return true;
            }
            [[nodiscard]] bool preflight_units()
            {
                auto const info{result.buffers_.find("debug_info")->second->getBuffer()};
                // [owned debug_info bytes] end
                // [safe                 ] unsafe (one-past)
                //  ^^ the reader borrows the immutable buffer for this build.
                reader data{{reinterpret_cast<::std::byte const*>(info.data()), info.size()}};
                while(data.cursor != data.bytes.size())
                {
                    auto const begin{data.cursor};
                    ::std::uint64_t length{};
                    if(!data.fixed(4u, length)) { return fail(error::malformed); }
                    ::std::size_t offset_bytes{4u};
                    if(length == 0xffffffffu)
                    { offset_bytes = 8u; if(!data.fixed(8u, length)) { return fail(error::malformed); } }
                    else if(length >= 0xfffffff0u) { return fail(error::malformed); }
                    if(length > data.bytes.size() - data.cursor) { return fail(error::malformed); }
                    if(length > cap.max_unit_bytes || !charge(1u, cap.max_units, used.units)) { return fail(error::limit_exceeded); }
                    // [safe] checked length converts to size_t and forms a unit
                    // subspan within owned info; LLVM has not parsed any DIE yet.
                    reader header{data.bytes.subspan(data.cursor, static_cast<::std::size_t>(length))};
                    ::std::uint64_t version{}, abbreviation{};
                    ::std::uint8_t address{}, kind{1u};
                    if(!header.fixed(2u, version)) { return fail(error::malformed); }
                    if(version != 4u && version != 5u) { return fail(error::unsupported_dwarf); }
                    if(version == 5u)
                    {
                        if(!header.byte(kind) || !header.byte(address) || !header.fixed(offset_bytes, abbreviation)) { return fail(error::malformed); }
                        if(kind != 1u /* DW_UT_compile */) { return fail(error::unsupported_external); }
                    }
                    else if(!header.fixed(offset_bytes, abbreviation) || !header.byte(address)) { return fail(error::malformed); }
                    if(address != input.address_bytes || (address != 4u && address != 8u)) { return fail(error::unsupported_dwarf); }
                    if(abbreviation >= result.buffers_.find("debug_abbrev")->second->getBufferSize() || header.cursor == header.bytes.size())
                    { return fail(error::malformed); }
                    // [safe] all unit bytes were prechecked; advance to its exact
                    // end, never beyond debug_info. begin is only a scalar offset.
                    data.cursor += static_cast<::std::size_t>(length);
                    if(data.cursor <= begin) { return fail(error::malformed); }
                }
                return true;
            }
            template<typename String>
            [[nodiscard]] bool copy_string(::llvm::DWARFFormValue const& attribute, String& destination)
            {
                if(external_form(attribute.getForm())) { return fail(error::unsupported_external); }
                auto text{attribute.getAsCString()};
                if(!text) { ::llvm::consumeError(text.takeError()); return fail(error::malformed); }
                // [safe] LLVM's string is accepted only if it falls within an
                // owned immutable buffer. An arbitrary C string is never scanned.
                auto const pointer{*text};
                auto const address{reinterpret_cast<::std::uintptr_t>(pointer)};
                if(pointer == nullptr) { return fail(error::malformed); }
                for(auto const& buffer : result.buffers_)
                {
                    auto const bytes{buffer.second->getBuffer()};
                    auto const begin{reinterpret_cast<::std::uintptr_t>(bytes.data())};
                    if(address < begin || address - begin >= bytes.size()) { continue; }
                    auto const start{static_cast<::std::size_t>(address - begin)};
                    ::std::size_t length{};
                    while(length < bytes.size() - start)
                    {
                        // [safe] start+length < size; borrow only this owned byte.
                        auto const value{static_cast<unsigned char>(bytes[start + length])};
                        if(value == 0u)
                        {
                            if(!charge(length, cap.max_total_string_bytes, used.strings)) { return false; }
                            // [safe] start..start+length is checked and NUL-terminated
                            // in this owner; copy the span before the local borrow ends.
                            if constexpr(::std::is_same_v<String, ::fast_io::string>)
                            { destination = ::fast_io::concat_fast_io(::std::string_view{bytes.data() + start, length}); }
                            else { destination = ::fast_io::concat_std(::std::string_view{bytes.data() + start, length}); }
                            return true;
                        }
                        if(length == cap.max_string_bytes) { return fail(error::limit_exceeded); }
                        if(value < 0x20u || value == 0x7fu) { return fail(error::malformed); }
                        ++length; // scalar position only, within the owned string.
                    }
                    return fail(error::malformed);
                }
                return fail(error::malformed);
            }
            [[nodiscard]] bool referenced(::llvm::DWARFFormValue const& value, ::llvm::DWARFDie& destination)
            {
                using namespace ::llvm::dwarf;
                if(external_form(value.getForm()) || value.getForm() == DW_FORM_ref_sig8) { return fail(error::unsupported_external); }
                ::std::uint64_t offset{};
                if(auto relative = value.getAsRelativeReference())
                {
                    auto const* unit{value.getUnit()}; // borrow only this immutable context-owned CU.
                    if(unit == nullptr || *relative > (::std::numeric_limits<::std::uint64_t>::max)() - unit->getOffset())
                    { return fail(error::malformed); }
                    offset = unit->getOffset() + *relative;
                    // Relative references must remain in their originating CU.
                    if(offset < unit->getOffset() || offset >= unit->getNextUnitOffset()) { return fail(error::malformed); }
                    destination = const_cast<::llvm::DWARFUnit*>(unit)->getDIEForOffset(offset);
                }
                else if(auto absolute = value.getAsDebugInfoReference())
                {
                    offset = *absolute;
                    for(auto* unit : units)
                    {
                        // [safe] each CU is context-owned and preflighted; no
                        // reference opens another context or supplementary file.
                        if(offset >= unit->getOffset() && offset < unit->getNextUnitOffset())
                        { destination = unit->getDIEForOffset(offset); break; }
                    }
                }
                else { return fail(error::malformed); }
                return (destination && !destination.isNULL() && destination.getOffset() == offset) || fail(error::malformed);
            }
            [[nodiscard]] bool inherited(::llvm::DWARFDie start, ::llvm::dwarf::Attribute name,
                ::std::optional<::llvm::DWARFFormValue>& destination)
            {
                struct item { ::llvm::DWARFDie die; ::std::vector<die_key> ancestors; };
                ::std::vector<item> pending{{start, {}}};
                ::std::size_t hops{};
                destination.reset();
                while(!pending.empty())
                {
                    if(hops++ >= cap.max_reference_hops) { return fail(error::limit_exceeded); }
                    auto current{::std::move(pending.back())}; pending.pop_back();
                    auto const identity{key(current.die)};
                    if(::std::find(current.ancestors.begin(), current.ancestors.end(), identity) != current.ancestors.end())
                    { return fail(error::malformed); }
                    if(auto attribute = current.die.find(name)) { destination = *attribute; return true; }
                    current.ancestors.push_back(identity);
                    // Only name/type/declaration metadata calls this helper.
                    // Ranges, locations and call-sites always use concrete DIEs.
                    for(auto link : {::llvm::dwarf::DW_AT_specification, ::llvm::dwarf::DW_AT_abstract_origin})
                    {
                        if(auto reference = current.die.find(link))
                        {
                            ::llvm::DWARFDie target{};
                            if(!referenced(*reference, target)) { return false; }
                            pending.push_back({target, current.ancestors});
                        }
                    }
                }
                return true;
            }
            template<typename Record>
            [[nodiscard]] bool cu_producer(::llvm::DWARFUnit* unit, Record& record)
            {
                if(record.language != 0x0cu) { return true; }
                auto const key{unit->getOffset()}; auto const found{producer_units.find(key)};
                producer_language producer{};
                if(found != producer_units.end()) { producer = found->second; }
                else
                {
                    if(producer_units.size() >= cap.max_units) { return fail(error::limit_exceeded); }
                    if(auto attribute = unit->getUnitDIE(false).find(::llvm::dwarf::DW_AT_producer))
                    {
                        ::fast_io::string text{}; if(!copy_string(*attribute,text)) { return false; }
                        producer = copied_producer_language(record.language, {text.data(),text.size()});
                    }
                    producer_units.emplace(key,producer);
                }
                record.tinygo_producer = producer == producer_language::tinygo;
                record.zig_producer = producer == producer_language::zig;
                return true;
            }
            [[nodiscard]] bool scope_language(::llvm::DWARFDie die, scope_record& record)
            {
                auto* const unit{die.getDwarfUnit()};
                if(auto attribute = unit->getUnitDIE(false).find(::llvm::dwarf::DW_AT_language))
                {
                    if(!attribute->isFormClass(::llvm::DWARFFormValue::FC_Constant)) { return fail(error::malformed); }
                    auto const language{attribute->getAsUnsignedConstant()};
                    if(!language) { return fail(error::malformed); }
                    record.language = *language;
                }
                if(!cu_producer(unit,record)) { return false; }
                ::std::vector<::llvm::DWARFDie> pending{die}; ::std::vector<die_key> visited{};
                while(!pending.empty())
                {
                    auto const current{pending.back()}; pending.pop_back(); auto const identity{key(current)};
                    if(::std::find(visited.begin(),visited.end(),identity) != visited.end()) { continue; }
                    if(visited.size() >= cap.max_reference_hops) { return fail(error::limit_exceeded); }
                    visited.push_back(identity);
                    if(!inspect_attributes(current)) { return false; }
                    for(auto link : {::llvm::dwarf::DW_AT_specification,::llvm::dwarf::DW_AT_abstract_origin})
                    {
                        if(auto reference = current.find(link))
                        {
                            ::llvm::DWARFDie target{}; if(!referenced(*reference,target)) { return false; }
                            if(target.getDwarfUnit() != unit)
                            { record.language = 0u; record.tinygo_producer = false; record.zig_producer = false; return true; }
                            pending.push_back(target);
                        }
                    }
                }
                return true;
            }
            [[nodiscard]] bool name(::llvm::DWARFDie die, ::std::string& destination)
            {
                ::std::optional<::llvm::DWARFFormValue> attribute{};
                if(!inherited(die, ::llvm::dwarf::DW_AT_name, attribute)) { return false; }
                return !attribute || copy_string(*attribute, destination);
            }
            [[nodiscard]] bool file(::llvm::DWARFFormValue const& attribute, ::std::string& destination)
            {
                auto const file_index{attribute.getAsUnsignedConstant()};
                auto* const unit{const_cast<::llvm::DWARFUnit*>(attribute.getUnit())};
                // [safe] use the attribute's own CU, including abstract-origin
                // declarations. Its file index is never a global merged index.
                if(!file_index || unit == nullptr) { return fail(error::malformed); }
                auto const stmt{unit->getUnitDIE().find(::llvm::dwarf::DW_AT_stmt_list)};
                if(!stmt) { return true; } // declaration file unavailable, no file opened.
                auto table{result.context_->getLineTableForUnit(unit, [owner = ::std::addressof(result)](::llvm::Error issue)
                    { owner->consume_diagnostic(::std::move(issue), 1u); })};
                if(!table) { ::llvm::consumeError(table.takeError()); return fail(error::malformed); }
                if(*table == nullptr || result.diagnostic_count_ != 0u) { return fail(error::malformed); }
                auto const offset{stmt->getAsSectionOffset()};
                if(!offset) { return fail(error::malformed); }
                if(counted_line_tables.insert(*offset).second && !charge((*table)->Rows.size(), cap.max_line_rows, used.line_rows)) { return false; }
                if(!(*table)->hasFileAtIndex(*file_index))
                {
                    // DWARF4 file index zero explicitly denotes an unspecified
                    // file; DWARF5 zero is a real CU-local file-table entry.
                    if(*file_index == 0u && (*table)->Prologue.getVersion() < 5u) { return true; }
                    return fail(error::malformed);
                }
                // The checked CU-local entry borrows an owned line/str buffer.
                // Copy its raw filename, without filesystem/path resolution.
                return copy_string((*table)->Prologue.getFileNameEntry(*file_index).Name, destination);
            }
            [[nodiscard]] bool ranges(::llvm::DWARFDie die, ::std::vector<code_range>& destination)
            {
                auto source{die.getAddressRanges()};
                if(!source) { ::llvm::consumeError(source.takeError()); return fail(error::malformed); }
                if(!charge(source->size(), cap.max_ranges, used.ranges)) { return false; }
                for(auto const& entry : *source)
                {
                    // Legacy Wasm32 relocation can retain a dead end at 2^32
                    // or add a CU base to that marker (TinyGo 0.42). Such an
                    // unrepresentable executable interval grants NO coverage.
                    // Quarantine this WHOLE DIE, never clamp or inherit it.
                    if(input.address_bytes == 4u && entry.LowPC < input.code_section_content_size && entry.LowPC <= entry.HighPC && entry.HighPC >= 0x100000000ull &&
                       entry.SectionIndex == ::llvm::object::SectionedAddress::UndefSection && die.getTag() != ::llvm::dwarf::DW_TAG_compile_unit)
                    { destination.clear(); return true; }
                    // TinyGo can retain an inverted, in-Code inline interval
                    // after optimization. This is NOT executable coverage.
                    // Retire the WHOLE inline DIE, keeping unrelated physical
                    // functions; its explicit empty range excludes all children.
                    // Physical/CU/lexical or out-of-Code corruption remains fatal.
                    if(die.getTag() == ::llvm::dwarf::DW_TAG_inlined_subroutine && entry.LowPC > entry.HighPC &&
                       entry.LowPC < input.code_section_content_size && entry.HighPC < input.code_section_content_size &&
                       entry.SectionIndex == ::llvm::object::SectionedAddress::UndefSection)
                    { destination.clear(); return true; }
                    if(entry.LowPC > entry.HighPC || entry.HighPC > input.code_section_content_size ||
                       entry.SectionIndex != ::llvm::object::SectionedAddress::UndefSection)
                    { return fail(error::malformed); }
                    if(entry.LowPC != entry.HighPC) { destination.push_back({entry.LowPC, entry.HighPC}); }
                }
                ::std::sort(destination.begin(), destination.end(), [](auto const& a, auto const& b) { return a.begin < b.begin; });
                // A single DIE can describe its coverage with overlapping or
                // duplicate intervals (notably TinyGo). Preserve exactly their
                // union; no out-of-Code address or child-parent range is added.
                ::std::size_t count{};
                for(auto const entry : destination)
                {
                    if(count != 0u && entry.begin <= destination[count-1u].end)
                    { destination[count-1u].end = (::std::max)(destination[count-1u].end,entry.end); }
                    else { destination[count++] = entry; }
                }
                destination.resize(count); return true;
            }
            [[nodiscard]] bool locations(::llvm::DWARFDie die, ::llvm::dwarf::Attribute attribute,
                ::std::vector<location_record>& destination, position_role role = position_role::variable)
            {
                if(!die.find(attribute)) { return true; }
                auto source{die.getLocations(attribute)};
                if(!source) { ::llvm::consumeError(source.takeError()); return fail(error::malformed); }
                if(!charge(source->size(), cap.max_locations, used.locations)) { return false; }
                bool default_seen{}, quarantined{};
                ::std::vector<code_range> selected{};
                for(auto const& entry : *source)
                {
                    location_record record{};
                    if(entry.Range)
                    {
                        bool const unrepresentable{input.address_bytes == 4u && entry.Range->LowPC < input.code_section_content_size && entry.Range->LowPC <= entry.Range->HighPC &&
                            entry.Range->HighPC >= 0x100000000ull && entry.Range->SectionIndex == ::llvm::object::SectionedAddress::UndefSection};
                        if(!unrepresentable && (entry.Range->LowPC > entry.Range->HighPC || entry.Range->HighPC > input.code_section_content_size ||
                           entry.Range->SectionIndex != ::llvm::object::SectionedAddress::UndefSection))
                        { return fail(error::malformed); }
                        quarantined |= unrepresentable;
                        record.range = code_range{entry.Range->LowPC, entry.Range->HighPC};
                        if(entry.Range->LowPC != entry.Range->HighPC) { selected.push_back(*record.range); }
                    }
                    else { if(default_seen) { return fail(error::malformed); } default_seen = true; }
                    if(entry.Expr.size() > cap.max_expression_bytes || !charge(entry.Expr.size(), cap.max_total_expression_bytes, used.expressions))
                    { return fail(error::limit_exceeded); }
                    // [LLVM-owned location expression] expression_end
                    // [safe                         ] unsafe (one-past)
                    //  ^^ the finite decoder borrows only while source lives;
                    // the result contains scalar metadata, never this address.
                    record.plan = decode_location_plan({reinterpret_cast<::std::byte const*>(entry.Expr.data()), entry.Expr.size()},
                        die.getDwarfUnit()->getAddressByteSize(), cap, role);
                    if(role == position_role::variable && !entry.Expr.empty() && entry.Expr.front() == 0xa1u /* DW_OP_addrx */)
                    {
                        reader indexed{{reinterpret_cast<::std::byte const*>(entry.Expr.data()), entry.Expr.size()}};
                        ::std::uint8_t opcode{}; ::std::uint64_t address_index{};
                        if(!indexed.byte(opcode) || !indexed.leb(address_index)) { return fail(error::malformed); }
                        if(indexed.cursor == entry.Expr.size())
                        { if(!indexed_guest_address(die.getDwarfUnit(), address_index, record.plan)) { return false; } }
                        // Extra operators (TLS, reloc-base arithmetic, deref,
                        // calls) retain the explicit unsupported-expression plan.
                    }
                    if(record.plan.kind == plan_kind::wasm_local_value && record.plan.integer_transform_count != 0u &&
                       !resolve_integer_transforms(die.getDwarfUnit(),record.plan)) { return false; }
                    if(record.plan.kind == plan_kind::composite_value)
                    {
                        for(auto& piece : record.plan.pieces)
                        {
                            if(piece.atom.kind == plan_kind::wasm_local_value && piece.atom.integer_transform_count != 0u &&
                               !resolve_integer_transforms(die.getDwarfUnit(), piece.atom)) { return false; }
                            // A valid unsupported base DIE quarantines only this
                            // atom. Other copied pieces keep their own known bits.
                        }
                    }
                    if(record.plan.reason == unavailable_reason::malformed_expression) { return fail(error::malformed); }
                    if(record.plan.reason == unavailable_reason::allocation_failure) { return fail(error::allocation_failure); }
                    destination.push_back(::std::move(record));
                }
                ::std::sort(selected.begin(), selected.end(), [](auto const& a, auto const& b) { return a.begin < b.begin; });
                for(::std::size_t i{1u}; i < selected.size(); ++i)
                {
                    if(selected[i - 1u].end <= selected[i].begin) { continue; }
                    // Conflicting location descriptions do not invalidate every
                    // unrelated source function. Quarantine THIS location list;
                    // never choose its first range or grant a memory read from it.
                    quarantined = true; break;
                }
                if(quarantined)
                {
                    destination.clear(); location_record unavailable{};
                    unavailable.plan.address_bytes = input.address_bytes;
                    unavailable.plan.reason = unavailable_reason::unsupported_expression;
                    destination.push_back(::std::move(unavailable));
                }
                return true;
            }
            [[nodiscard]] bool resolve_integer_transforms(::llvm::DWARFUnit* unit, location_plan& plan)
            {
                for(::std::size_t i{}; i != plan.integer_transform_count; ++i)
                {
                    auto& step{plan.integer_transforms[i]};
                    if(step.kind != integer_transform_kind::unsigned_convert) { continue; }
                    if(step.operand > (::std::numeric_limits<::std::uint64_t>::max)() - unit->getOffset())
                    { return fail(error::malformed); }
                    auto const offset{unit->getOffset() + step.operand};
                    if(offset < unit->getOffset() || offset >= unit->getNextUnitOffset()) { return fail(error::malformed); }
                    auto const target{unit->getDIEForOffset(offset)};
                    if(!target || target.isNULL() || target.getOffset() != offset) { return fail(error::malformed); }
                    // Exact CU-owned base DIE, not a name, typedef, external
                    // reference or native register. Type references consume the
                    // same budget as the other metadata graph edges.
                    if(!charge(1u,cap.max_type_edges,used.type_edges)) { return false; }
                    auto reject{[&]() { plan.kind=plan_kind::unavailable; plan.reason=unavailable_reason::unsupported_expression; }};
                    if(target.getTag() != ::llvm::dwarf::DW_TAG_base_type) { reject(); return true; }
                    auto const encoding{target.find(::llvm::dwarf::DW_AT_encoding)};
                    auto const size{target.find(::llvm::dwarf::DW_AT_byte_size)};
                    auto const enc{encoding ? encoding->getAsUnsignedConstant() : ::std::nullopt};
                    auto const bytes{size ? size->getAsUnsignedConstant() : ::std::nullopt};
                    if(!enc || !bytes || *bytes == 0u || *bytes > 8u || (*enc != 0x07u && *enc != 0x08u))
                    { reject(); return true; }
                    auto width{*bytes * 8u};
                    if(auto const bits = target.find(::llvm::dwarf::DW_AT_bit_size))
                    {
                        auto const value{bits->getAsUnsignedConstant()};
                        if(!value || *value == 0u || *value > width) { reject(); return true; }
                        width = *value;
                    }
                    for(auto const attribute : {::llvm::dwarf::DW_AT_bit_offset,::llvm::dwarf::DW_AT_data_bit_offset})
                    {
                        if(auto const position = target.find(attribute))
                        {
                            auto const value{position->getAsUnsignedConstant()};
                            if(!value || *value != 0u) { reject(); return true; }
                        }
                    }
                    step.type_identity = key(target); step.bit_width = static_cast<::std::uint8_t>(width); step.resolved = true;
                }
                return true;
            }
            [[nodiscard]] bool indexed_guest_address(::llvm::DWARFUnit* unit, ::std::uint64_t index, location_plan& plan)
            {
                auto const section{result.buffers_.find("debug_addr")};
                auto const base{unit->getAddrOffsetSectionBase()};
                if(section == result.buffers_.end() || !base || index > (::std::numeric_limits<::std::uint32_t>::max)()) { return fail(error::malformed); }
                auto const owned{section->second->getBuffer()};
                // [owned immutable .debug_addr bytes] section_end
                // [safe                            ] entire owner is live;
                //  ^^ finite table headers/ranges are checked BEFORE LLVM's
                // indexed API, which otherwise adds base+index*width unchecked.
                reader tables{{reinterpret_cast<::std::byte const*>(owned.data()), owned.size()}};
                ::std::size_t contributions{};
                while(tables.cursor != tables.bytes.size())
                {
                    if(contributions++ >= cap.max_units) { return fail(error::limit_exceeded); }
                    ::std::uint64_t length{}; if(!tables.fixed(4u, length)) { return fail(error::malformed); }
                    if(length == 0xffffffffu) { if(!tables.fixed(8u, length)) { return fail(error::malformed); } }
                    else if(length >= 0xfffffff0u) { return fail(error::malformed); }
                    if(length > tables.bytes.size() - tables.cursor || length < 4u) { return fail(error::malformed); }
                    auto const end{tables.cursor + static_cast<::std::size_t>(length)}; // checked scalar advance, <= section_end.
                    ::std::uint64_t version{}; ::std::uint8_t width{}, segment{};
                    if(!tables.fixed(2u, version) || !tables.byte(width) || !tables.byte(segment)) { return fail(error::malformed); }
                    if(version != 5u || width != input.address_bytes || segment != 0u) { return fail(error::unsupported_dwarf); }
                    if((end - tables.cursor) % width != 0u) { return fail(error::malformed); }
                    if(*base == tables.cursor)
                    {
                        if(index >= (end - tables.cursor) / width) { return fail(error::malformed); }
                        auto const value{unit->getAddrOffsetSectionItem(static_cast<::std::uint32_t>(index))};
                        if(!value || value->SectionIndex != ::llvm::object::SectionedAddress::UndefSection ||
                           (width == 4u && value->Address > (::std::numeric_limits<::std::uint32_t>::max)())) { return fail(error::malformed); }
                        plan = {}; plan.kind = plan_kind::absolute_guest_offset; plan.reason = unavailable_reason::none;
                        plan.constant_bits = value->Address; plan.address_bytes = width; return true;
                    }
                    tables.cursor = end; // [safe] contribution was bounded before scalar cursor replacement.
                }
                return fail(error::malformed); // base must identify one table's first entry, not arbitrary bytes.
            }
            [[nodiscard]] bool type_reference(::llvm::DWARFDie die, ::std::size_t& destination, ::std::size_t depth)
            {
                ::std::optional<::llvm::DWARFFormValue> reference{};
                if(!inherited(die, ::llvm::dwarf::DW_AT_type, reference)) { return false; }
                if(!reference) { return true; }
                ::llvm::DWARFDie target{};
                if(!referenced(*reference, target)) { return false; }
                return intern_type(target, destination, depth);
            }
            [[nodiscard]] bool member_offset(::llvm::DWARFDie die, member_record& record, bool is_union)
            {
                auto const location{die.find(::llvm::dwarf::DW_AT_data_member_location)};
                if(!location) { record.offset_known = is_union; return true; }
                if(auto constant = location->getAsUnsignedConstant())
                { record.byte_offset = *constant; record.offset_known = true; return true; }
                if(auto constant = location->getAsSignedConstant(); constant && *constant >= 0)
                { record.byte_offset = static_cast<::std::uint64_t>(*constant); record.offset_known = true; return true; }
                if(auto expression = location->getAsBlock())
                {
                    if(expression->size() > cap.max_expression_bytes ||
                       !charge(expression->size(), cap.max_total_expression_bytes, used.expressions))
                    { return fail(error::limit_exceeded); }
                    // [owned LLVM member expression] expression_end
                    // [safe                        ] no guest evaluation occurs.
                    //  ^^ bounded local decoder accepts only plus_uconst N.
                    reader input_expression{{reinterpret_cast<::std::byte const*>(expression->data()), expression->size()}};
                    ::std::uint8_t op{}; ::std::uint64_t offset{};
                    if(input_expression.byte(op) && op == 0x23u /* DW_OP_plus_uconst */ &&
                       input_expression.leb(offset) && input_expression.cursor == expression->size())
                    { record.byte_offset = offset; record.offset_known = true; }
                    return true; // virtual bases/other expressions remain unavailable.
                }
                return true; // Dynamic member offsets never grant a memory read.
            }
            [[nodiscard]] bool dimension(::llvm::DWARFDie die, dimension_record& record)
            {
                if(auto lower = die.find(::llvm::dwarf::DW_AT_lower_bound))
                {
                    if((lower->getForm() == ::llvm::dwarf::DW_FORM_sdata || lower->getForm() == ::llvm::dwarf::DW_FORM_implicit_const) && lower->getAsSignedConstant())
                    { record.lower_bound = *lower->getAsSignedConstant(); record.lower_bound_known = true; }
                    else if(auto value = lower->getAsUnsignedConstant(); value && *value <= static_cast<::std::uint64_t>((::std::numeric_limits<::std::int64_t>::max)()))
                    { record.lower_bound = static_cast<::std::int64_t>(*value); record.lower_bound_known = true; }
                }
                else
                {
                    auto const language{die.getDwarfUnit()->getUnitDIE(false).find(::llvm::dwarf::DW_AT_language)};
                    if(language)
                    {
                        auto const value{language->getAsUnsignedConstant()};
                        if(value)
                        {
                            // DWARF5 Table 7.17 defaults only for the explicitly
                            // supported C/Objective-C/C++/Objective-C++/Go/Rust families. Unknown
                            // languages do not silently acquire C's zero bound.
                            switch(*value)
                            {
                                case 0x0001u: case 0x0002u: case 0x0004u: case 0x000cu: case 0x0016u:
                                case 0x0010u: case 0x0011u: // Objective-C and Objective-C++ use zero.
                                case 0x0019u: case 0x001au: case 0x001cu: case 0x001du:
                                case 0x0021u: case 0x002au: case 0x002bu: case 0x002cu:
                                    record.lower_bound_known = true; break;
                                default: break;
                            }
                        }
                    }
                }
                if(auto count = die.find(::llvm::dwarf::DW_AT_count))
                {
                    if(auto value = count->getAsUnsignedConstant()) { record.count = *value; record.count_known = true; }
                    else if(auto value = count->getAsSignedConstant(); value && *value >= 0)
                    { record.count = static_cast<::std::uint64_t>(*value); record.count_known = true; }
                    return true;
                }
                if(auto upper = die.find(::llvm::dwarf::DW_AT_upper_bound); upper && record.lower_bound_known)
                {
                    auto const signed_upper{upper->getForm() == ::llvm::dwarf::DW_FORM_sdata || upper->getForm() == ::llvm::dwarf::DW_FORM_implicit_const ?
                        upper->getAsSignedConstant() : ::std::optional<::std::int64_t>{}};
                    if(signed_upper && *signed_upper >= record.lower_bound)
                    {
                        // Signed endpoints may span INT64_MIN..INT64_MAX. The
                        // unsigned subtraction is exact modulo 2^64; reject its
                        // UINT64_MAX extent before adding the inclusive element.
                        auto const extent{static_cast<::std::uint64_t>(*signed_upper) - static_cast<::std::uint64_t>(record.lower_bound)};
                        if(extent != (::std::numeric_limits<::std::uint64_t>::max)())
                        { record.count = extent + 1u; record.count_known = true; }
                    }
                    else if(auto value = upper->getAsUnsignedConstant(); value && record.lower_bound >= 0 && *value >= static_cast<::std::uint64_t>(record.lower_bound))
                    {
                        auto const extent{*value - static_cast<::std::uint64_t>(record.lower_bound)};
                        if(extent != (::std::numeric_limits<::std::uint64_t>::max)())
                        { record.count = extent + 1u; record.count_known = true; }
                    }
                }
                return true;
            }
            [[nodiscard]] bool member(::llvm::DWARFDie die, member_record& record, bool is_union, ::std::size_t depth)
            {
                record.inherited = die.getTag() == ::llvm::dwarf::DW_TAG_inheritance;
                if(!name(die, record.name) || !type_reference(die, record.type, depth + 1u) || !member_offset(die, record, is_union)) { return false; }
                if(auto bits = die.find(::llvm::dwarf::DW_AT_bit_size))
                {
                    record.bit_field = true;
                    if(auto value = bits->getAsUnsignedConstant()) { record.bit_size = *value; }
                    if(auto offset = die.find(::llvm::dwarf::DW_AT_data_bit_offset))
                    { if(auto value = offset->getAsUnsignedConstant()) { record.data_bit_offset = *value; record.offset_known = true; } }
                    else { record.offset_known = false; } // Legacy bit_offset has target-dependent semantics.
                }
                return true;
            }
            [[nodiscard]] bool variant_part(::llvm::DWARFDie die, ::llvm::DWARFDie owner, variant_part_record& record, ::std::size_t depth)
            {
                if(depth > cap.max_depth) { return fail(error::limit_exceeded); }
                if(auto attribute = die.find(::llvm::dwarf::DW_AT_discr))
                {
                    record.has_discriminant = true; ::llvm::DWARFDie discriminant{};
                    if(!referenced(*attribute, discriminant)) { return false; }
                    // DWARF5 places this member in the variant part. LLVM also
                    // accepts a sibling in the same containing aggregate (the
                    // DWARF6 clarification), never an unrelated CU/object field.
                    auto const parent{discriminant.getParent()};
                    if(discriminant.getTag() == ::llvm::dwarf::DW_TAG_member && parent &&
                       (key(parent) == key(die) || key(parent) == key(owner)))
                    {
                        if(!charge(1u, cap.max_type_edges, used.type_edges) || !member(discriminant, record.discriminant, false, depth)) { return false; }
                        if(record.discriminant.type < result.types_.size())
                        {
                            auto const& type{result.types_[record.discriminant.type]};
                            bool const integer{type.encoding == 0x02u || type.encoding == 0x05u || type.encoding == 0x06u || type.encoding == 0x07u || type.encoding == 0x08u};
                            record.discriminant_bytes = type.byte_count;
                            record.discriminant_signed = type.encoding == 0x05u || type.encoding == 0x06u;
                            record.discriminant_supported = integer && variant_integer_width(type.byte_count) &&
                                (type.kind == type_kind::scalar || type.kind == type_kind::enumeration) && record.discriminant.offset_known;
                        }
                    }
                }
                for(auto child : die.children())
                {
                    if(child.getTag() != ::llvm::dwarf::DW_TAG_variant) { continue; }
                    if(!charge(1u, cap.max_type_edges, used.type_edges)) { return false; }
                    variant_record variant{}; if(!name(child, variant.name)) { return false; }
                    auto const value{child.find(::llvm::dwarf::DW_AT_discr_value)};
                    auto const list{child.find(::llvm::dwarf::DW_AT_discr_list)};
                    if(value && list) { return fail(error::malformed); }
                    if(!value && !list) { variant.selector_kind = variant_selector_kind::default_case; }
                    else if(record.discriminant_supported)
                    {
                        if(value)
                        {
                            ::std::optional<::std::uint64_t> bits{};
                            if(record.discriminant_signed)
                            { if(auto number = value->getAsSignedConstant()) { bits = static_cast<::std::uint64_t>(*number); } }
                            else { bits = value->getAsUnsignedConstant(); }
                            if(bits && canonical_variant_bits(*bits, record.discriminant_bytes, record.discriminant_signed) == *bits)
                            {
                                if(!charge(1u, cap.max_type_edges, used.type_edges)) { return false; }
                                variant.selectors.push_back({*bits, *bits}); variant.selector_kind = variant_selector_kind::intervals;
                            }
                        }
                        else if(auto block = list->getAsBlock())
                        {
                            if(block->size() > cap.max_expression_bytes || !charge(block->size(), cap.max_total_expression_bytes, used.expressions))
                            { return fail(error::limit_exceeded); }
                            // [immutable LLVM discriminator-list block] block_end
                            // [safe                                   ] no native or
                            //  ^^ guest address is derived; finite bounded reader.
                            auto const status{decode_variant_selectors({reinterpret_cast<::std::byte const*>(block->data()), block->size()},
                                record.discriminant_bytes, record.discriminant_signed, variant.selectors, cap.max_type_edges, cap.max_expression_bytes)};
                            if(status == variant_query_error::allocation_failure) { return fail(error::allocation_failure); }
                            if(status == variant_query_error::limit_exceeded) { return fail(error::limit_exceeded); }
                            if(status != variant_query_error::none) { return fail(error::malformed); }
                            if(!charge(variant.selectors.size(), cap.max_type_edges, used.type_edges)) { return false; }
                            variant.selector_kind = variant_selector_kind::intervals;
                        }
                    }
                    for(auto payload : child.children())
                    {
                        if(payload.getTag() == ::llvm::dwarf::DW_TAG_member || payload.getTag() == ::llvm::dwarf::DW_TAG_inheritance)
                        {
                            if(!charge(1u, cap.max_type_edges, used.type_edges)) { return false; }
                            member_record item{}; if(!member(payload, item, false, depth + 1u)) { return false; }
                            if(variant.name.empty() && !item.name.empty()) { variant.name = ::fast_io::concat_std(::std::string_view{item.name}); }
                            variant.members.push_back(::std::move(item));
                        }
                        else if(payload.getTag() == ::llvm::dwarf::DW_TAG_variant_part)
                        { variant.layout_supported = false; } // Preserve known members; nested selector evaluation is unavailable.
                    }
                    record.variants.push_back(::std::move(variant));
                }
                return true;
            }
            [[nodiscard]] bool intern_type(::llvm::DWARFDie die, ::std::size_t& destination, ::std::size_t depth)
            {
                if(depth > cap.max_depth) { return fail(error::limit_exceeded); }
                auto const original{key(die)};
                auto const lookup{type_indices.find({original.unit, original.offset})};
                if(lookup != type_indices.end()) { destination = lookup->second; return true; }
                if(result.types_.size() >= cap.max_types) { return fail(error::limit_exceeded); }
                destination = result.types_.size();
                type_indices.emplace(::std::pair{original.unit, original.offset}, destination);
                // Reserve an INDEX before following children. A struct may
                // legally point back to itself. Never retain a type_record
                // reference across recursive vector growth or expose a partial
                // graph: index::parse publishes only after the complete build.
                result.types_.emplace_back();
                type_record record{}; record.identity = original;
                if(auto const language = die.getDwarfUnit()->getUnitDIE(false).find(::llvm::dwarf::DW_AT_language))
                {
                    auto const value{language->getAsUnsignedConstant()};
                    if(!value) { return fail(error::malformed); }
                    record.language = *value;
                }
                if(!cu_producer(die.getDwarfUnit(),record)) { return false; }
                ::std::vector<die_key> visited_types{}; die_key chosen_name{};
                for(::std::size_t hop{}; hop < cap.max_reference_hops; ++hop)
                {
                    auto const identity{key(die)};
                    if(::std::find(visited_types.begin(), visited_types.end(), identity) != visited_types.end()) { return fail(error::malformed); }
                    visited_types.push_back(identity);
                    if(record.name.empty())
                    { if(!name(die,record.name)) { return false; } if(!record.name.empty()) { chosen_name = identity; } }
                    auto const tag{die.getTag()};
                    if(tag == ::llvm::dwarf::DW_TAG_atomic_type) { record.atomic_scalar = true; }
                    record.declaration_identity=identity;
                    if(tag == ::llvm::dwarf::DW_TAG_typedef && !record.name.empty()) { record.named_type_alias = true; }
                    if(!record.named_type_alias)
                    {
                        if(tag == ::llvm::dwarf::DW_TAG_const_type) { record.display_qualifiers |= 1u; }
                        else if(tag == ::llvm::dwarf::DW_TAG_volatile_type) { record.display_qualifiers |= 2u; }
                        else if(tag == ::llvm::dwarf::DW_TAG_restrict_type) { record.display_qualifiers |= 4u; }
                        else if(tag == ::llvm::dwarf::DW_TAG_atomic_type) { record.display_qualifiers |= 8u; }
                    }
                    bool const pointer_like{tag == ::llvm::dwarf::DW_TAG_pointer_type || tag == ::llvm::dwarf::DW_TAG_reference_type ||
                                            tag == ::llvm::dwarf::DW_TAG_rvalue_reference_type};
                    if(tag == ::llvm::dwarf::DW_TAG_base_type || pointer_like)
                    {
                        auto const size{die.find(::llvm::dwarf::DW_AT_byte_size)};
                        auto const width{size ? size->getAsUnsignedConstant() :
                            (pointer_like ? ::std::optional<::std::uint64_t>{input.address_bytes} : ::std::nullopt)};
                        if(width && *width != 0u && *width <= 16u)
                        {
                            record.byte_count = static_cast<::std::uint8_t>(*width);
                            record.byte_size = *width; record.size_known = true;
                            record.kind = pointer_like ? type_kind::pointer : type_kind::scalar;
                            record.reference_type = tag == ::llvm::dwarf::DW_TAG_reference_type || tag == ::llvm::dwarf::DW_TAG_rvalue_reference_type;
                            record.rvalue_reference_type = tag == ::llvm::dwarf::DW_TAG_rvalue_reference_type;
                            if(auto encoding = die.find(::llvm::dwarf::DW_AT_encoding))
                            { if(auto value = encoding->getAsUnsignedConstant()) { record.encoding = *value; } else { return fail(error::malformed); } }
                            if(!pointer_like && !record.atomic_scalar && original.unit == identity.unit &&
                               !record.tinygo_producer && !record.zig_producer && (cxx_language(record.language) ||
                                (c_integer_language(record.language) && (record.byte_count == 4u || record.byte_count == 8u))))
                            {
                                if(auto const attribute = die.find(::llvm::dwarf::DW_AT_name))
                                {
                                    // Reuse the already bounded direct base name;
                                    // a typedef/wrapper name is never substituted.
                                    ::std::string base_name{};
                                    if(chosen_name != identity && !copy_string(*attribute,base_name)) { return false; }
                                    auto const actual_name{chosen_name == identity ? ::std::string_view{record.name} : ::std::string_view{base_name}};
                                    record.narrow_builtin = cxx_narrow_from_base(record.language,false,actual_name,record.encoding,record.byte_count);
                                    record.wide_builtin = c_wide_from_base(record.language,false,actual_name,record.encoding,record.byte_count,input.address_bytes);
                                }
                            }
                        }
                        if(pointer_like)
                        {
                            if(auto const address_class = die.find(::llvm::dwarf::DW_AT_address_class))
                            {
                                // DWARF5 5.3/7.5.4: preserve its constant class;
                                // this bounded profile accepts uint64 values.
                                // LLVM's unsigned accessor also accepts
                                // flags and treats data16 as a block LENGTH, so
                                // neither may silently become a dereference class.
                                if(!address_class->isFormClass(::llvm::DWARFFormValue::FC_Constant)) { return fail(error::malformed); }
                                auto const form{address_class->getForm()};
                                ::std::uint64_t value{};
                                if(form == ::llvm::dwarf::DW_FORM_sdata || form == ::llvm::dwarf::DW_FORM_implicit_const)
                                {
                                    auto const number{address_class->getAsSignedConstant()};
                                    if(!number || *number < 0) { return fail(error::malformed); }
                                    value = static_cast<::std::uint64_t>(*number);
                                }
                                else if(form == ::llvm::dwarf::DW_FORM_data16)
                                {
                                    auto const block{address_class->getAsBlock()};
                                    if(!block || block->size() != 16u) { return fail(error::malformed); }
                                    // [immutable context-owned 16-byte constant] end
                                    // [safe                                   ] unsafe (one-past)
                                    //  ^^ exact block extent checked BEFORE char borrow/end derivation.
                                    auto const first{reinterpret_cast<char const*>(block->data())};
                                    auto const middle{first + 8u}; auto const last{first + 16u};
                                    ::std::uint64_t high{};
                                    auto const low_scan{::fast_io::parse_by_scan(first, middle, ::fast_io::mnp::le_get<64>(value))};
                                    auto const high_scan{::fast_io::parse_by_scan(middle, last, ::fast_io::mnp::le_get<64>(high))};
                                    if(low_scan.code != ::fast_io::parse_code::ok || low_scan.iter != middle ||
                                       high_scan.code != ::fast_io::parse_code::ok || high_scan.iter != last) { return fail(error::malformed); }
                                    // This bounded metadata model carries uint64
                                    // classes. An actual wider class is unsupported,
                                    // never truncated to a conventional guest class.
                                    if(high != 0u) { return fail(error::unsupported_dwarf); }
                                }
                                else
                                {
                                    auto const number{address_class->getAsUnsignedConstant()};
                                    if(!number) { return fail(error::malformed); }
                                    value = *number;
                                }
                                record.address_class = value; record.address_class_known = true;
                            }
                        }
                        if(pointer_like &&
                           !type_reference(die, record.referenced_type, depth + 1u)) { return false; }
                        break;
                    }
                    if(tag == ::llvm::dwarf::DW_TAG_subroutine_type || tag == ::llvm::dwarf::DW_TAG_ptr_to_member_type)
                    {
                        record.kind = tag == ::llvm::dwarf::DW_TAG_subroutine_type ? type_kind::subroutine : type_kind::member_pointer;
                        if(!type_reference(die,record.referenced_type,depth+1u)) { return false; }
                        if(record.kind == type_kind::member_pointer)
                        {
                            if(auto const containing = die.find(::llvm::dwarf::DW_AT_containing_type))
                            {
                                ::llvm::DWARFDie owner{};
                                if(!referenced(*containing,owner) || !charge(1u,cap.max_type_edges,used.type_edges) ||
                                   !intern_type(owner,record.containing_type,depth+1u)) { return false; }
                            }
                            if(auto const size = die.find(::llvm::dwarf::DW_AT_byte_size))
                            {
                                auto const value{size->getAsUnsignedConstant()};
                                if(!value) { return fail(error::malformed); }
                                record.byte_size=*value;record.size_known=true;
                            }
                        }
                        else
                        {
                            record.signature_complete=true;
                            die_key object_pointer{};bool object_pointer_known{};
                            if(auto const object = die.find(::llvm::dwarf::DW_AT_object_pointer))
                            {
                                ::llvm::DWARFDie parameter{};
                                if(!referenced(*object,parameter)) { return false; }
                                object_pointer=key(parameter);object_pointer_known=true;
                            }
                            if(auto const convention = die.find(::llvm::dwarf::DW_AT_calling_convention))
                            {
                                auto const value{convention->getAsUnsignedConstant()};
                                if(!value) { return fail(error::malformed); }
                                record.calling_convention=*value;record.calling_convention_known=true;
                            }
                            for(auto const child : die.children())
                            {
                                if(child.getTag()==::llvm::dwarf::DW_TAG_unspecified_parameters) { record.variadic=true;continue; }
                                if(child.getTag()!=::llvm::dwarf::DW_TAG_formal_parameter) { continue; }
                                if(!charge(1u,cap.max_type_edges,used.type_edges)) { return false; }
                                ::std::size_t parameter_type{no_record};
                                if(!type_reference(child,parameter_type,depth+1u)) { return false; }
                                if(object_pointer_known && key(child)==object_pointer)
                                {
                                    if(parameter_type==no_record || result.types_[parameter_type].kind!=type_kind::pointer ||
                                       result.types_[parameter_type].referenced_type==no_record) { record.signature_complete=false; }
                                    else
                                    {
                                        auto const pointee{result.types_[parameter_type].referenced_type};
                                        record.method_qualifiers=result.types_[pointee].display_qualifiers&3u;
                                    }
                                    continue;
                                }
                                if(parameter_type==no_record) { record.signature_complete=false; }
                                if(record.parameter_types.empty())
                                {
                                    if(auto const artificial=child.find(::llvm::dwarf::DW_AT_artificial))
                                    {
                                        auto const flag{artificial->getAsUnsignedConstant()};
                                        if(!flag) { return fail(error::malformed); }
                                        record.first_parameter_artificial=*flag!=0u;
                                    }
                                }
                                record.parameter_types.push_back(parameter_type);
                            }
                            for(auto const attribute : {::llvm::dwarf::DW_AT_reference,::llvm::dwarf::DW_AT_rvalue_reference})
                            {
                                if(auto const reference = die.find(attribute))
                                {
                                    auto const value{reference->getAsUnsignedConstant()};
                                    if(!value) { return fail(error::malformed); }
                                    if(attribute==::llvm::dwarf::DW_AT_reference) { record.method_lvalue_reference=*value!=0u; }
                                    else { record.method_rvalue_reference=*value!=0u; }
                                }
                            }
                        }
                        break;
                    }
                    if(tag == ::llvm::dwarf::DW_TAG_structure_type || tag == ::llvm::dwarf::DW_TAG_class_type ||
                       tag == ::llvm::dwarf::DW_TAG_union_type || tag == ::llvm::dwarf::DW_TAG_array_type ||
                       tag == ::llvm::dwarf::DW_TAG_enumeration_type)
                    {
                        record.kind = tag == ::llvm::dwarf::DW_TAG_structure_type ? type_kind::structure :
                            tag == ::llvm::dwarf::DW_TAG_class_type ? type_kind::class_type :
                            tag == ::llvm::dwarf::DW_TAG_union_type ? type_kind::union_type :
                            tag == ::llvm::dwarf::DW_TAG_array_type ? type_kind::array : type_kind::enumeration;
                        if(auto size = die.find(::llvm::dwarf::DW_AT_byte_size))
                        { if(auto value = size->getAsUnsignedConstant()) { record.byte_size = *value; record.size_known = true; } }
                        if(record.kind == type_kind::array || record.kind == type_kind::enumeration)
                        { if(!type_reference(die, record.referenced_type, depth + 1u)) { return false; } }
                        if(record.kind == type_kind::array)
                        {
                            record.contiguous_array = !die.find(::llvm::dwarf::DW_AT_byte_stride) && !die.find(::llvm::dwarf::DW_AT_bit_stride);
                            if(auto language = die.getDwarfUnit()->getUnitDIE(false).find(::llvm::dwarf::DW_AT_language))
                            {
                                if(auto value = language->getAsUnsignedConstant())
                                {
                                    // Fortran's absent ordering attribute has a
                                    // column-major default (DWARF5 5.5).
                                    switch(*value)
                                    { case 0x0007u: case 0x0008u: case 0x000eu: case 0x0022u: case 0x0023u:
                                        record.row_major_array = false; break; default: break; }
                                }
                            }
                            if(auto ordering = die.find(::llvm::dwarf::DW_AT_ordering))
                            { auto const value{ordering->getAsUnsignedConstant()}; record.row_major_array = value && *value == 0u; }
                        }
                        if(record.kind == type_kind::enumeration)
                        {
                            if(auto encoding = die.find(::llvm::dwarf::DW_AT_encoding))
                            { if(auto value = encoding->getAsUnsignedConstant()) { record.encoding = *value; } }
                            if(record.referenced_type != no_record && record.referenced_type < result.types_.size())
                            {
                                auto const& base{result.types_[record.referenced_type]};
                                if(record.encoding == 0u) { record.encoding = base.encoding; }
                                if(!record.size_known && base.size_known) { record.byte_size = base.byte_size; record.size_known = true; }
                            }
                            if(record.size_known && record.byte_size <= 16u) { record.byte_count = static_cast<::std::uint8_t>(record.byte_size); }
                        }
                        // [context-owned bounded CU children] end
                        // [safe                             ] each handle is
                        // inspected without native/guest pointer arithmetic.
                        for(auto child : die.children())
                        {
                            auto const child_tag{child.getTag()};
                            if(child_tag == ::llvm::dwarf::DW_TAG_member || child_tag == ::llvm::dwarf::DW_TAG_inheritance)
                            {
                                if(!charge(1u, cap.max_type_edges, used.type_edges)) { return false; }
                                member_record item{};
                                if(!member(child, item, record.kind == type_kind::union_type, depth)) { return false; }
                                record.members.push_back(::std::move(item));
                            }
                            else if(child_tag == ::llvm::dwarf::DW_TAG_variant_part)
                            {
                                if(!charge(1u, cap.max_type_edges, used.type_edges)) { return false; }
                                variant_part_record part{}; if(!variant_part(child, die, part, depth + 1u)) { return false; }
                                record.variant_parts.push_back(::std::move(part));
                            }
                            else if(record.kind == type_kind::array && child_tag == ::llvm::dwarf::DW_TAG_subrange_type)
                            {
                                if(record.dimensions.size() >= cap.max_array_dimensions || !charge(1u, cap.max_type_edges, used.type_edges))
                                { return fail(error::limit_exceeded); }
                                dimension_record bound{}; if(!dimension(child, bound)) { return false; }
                                if(child.find(::llvm::dwarf::DW_AT_byte_stride) || child.find(::llvm::dwarf::DW_AT_bit_stride)) { record.contiguous_array = false; }
                                record.dimensions.push_back(bound);
                            }
                            else if(record.kind == type_kind::enumeration && child_tag == ::llvm::dwarf::DW_TAG_enumerator)
                            {
                                if(!charge(1u, cap.max_type_edges, used.type_edges)) { return false; }
                                enumerator_record value{}; if(!name(child, value.name)) { return false; }
                                auto const constant{child.find(::llvm::dwarf::DW_AT_const_value)};
                                if(!constant) { continue; }
                                if(constant->getForm() == ::llvm::dwarf::DW_FORM_sdata || constant->getForm() == ::llvm::dwarf::DW_FORM_implicit_const)
                                { if(auto number = constant->getAsSignedConstant()) { value.bits = static_cast<::std::uint64_t>(*number); value.signed_value = true; } else { continue; } }
                                else if(auto number = constant->getAsUnsignedConstant()) { value.bits = *number; }
                                else { continue; }
                                record.enumerators.push_back(::std::move(value));
                            }
                        }
                        if(record.kind == type_kind::array && !record.size_known && record.referenced_type < result.types_.size() &&
                           !record.dimensions.empty() && record.contiguous_array)
                        {
                            auto const& element{result.types_[record.referenced_type]};
                            auto size{element.byte_size}; bool known{element.size_known};
                            for(auto const& bound : record.dimensions)
                            {
                                if(!bound.count_known || (size != 0u && bound.count > (::std::numeric_limits<::std::uint64_t>::max)() / size)) { known = false; break; }
                                size *= bound.count;
                            }
                            if(known) { record.byte_size = size; record.size_known = true; }
                        }
                        break;
                    }
                    if(tag != ::llvm::dwarf::DW_TAG_typedef && tag != ::llvm::dwarf::DW_TAG_const_type &&
                       tag != ::llvm::dwarf::DW_TAG_volatile_type && tag != ::llvm::dwarf::DW_TAG_restrict_type &&
                       tag != ::llvm::dwarf::DW_TAG_atomic_type)
                    { break; } // Unsupported types remain explicit metadata-unavailable.
                    auto next{die.find(::llvm::dwarf::DW_AT_type)};
                    if(!next) { break; }
                    if(hop + 1u == cap.max_reference_hops) { return fail(error::limit_exceeded); }
                    ::llvm::DWARFDie target{};
                    if(!referenced(*next, target)) { return false; }
                    die = target; // only an owned-context handle changes; no guest pointer dereference.
                }
                // [owned type records ... destination ... end]
                // [safe                                       ] destination is
                // the installed index; recursion may have reallocated storage.
                result.types_[destination] = ::std::move(record);
                return true;
            }
            [[nodiscard]] bool type(::llvm::DWARFDie variable, ::std::size_t& destination)
            { return type_reference(variable, destination, 0u); }
            [[nodiscard]] bool qualified_name(::llvm::DWARFDie die, ::std::string_view leaf, ::std::string& destination)
            {
                // A C++ out-of-class static definition inherits its lexical
                // name from the specification DIE. Only metadata naming follows
                // that reference; actual scope/location authority stays on die.
                ::std::vector<die_key> seen{};
                for(::std::size_t hop{}; ; ++hop)
                {
                    if(hop >= cap.max_reference_hops) { return fail(error::limit_exceeded); }
                    auto const identity{key(die)};
                    if(::std::find(seen.begin(), seen.end(), identity) != seen.end()) { return fail(error::malformed); }
                    seen.push_back(identity);
                    auto const specification{die.find(::llvm::dwarf::DW_AT_specification)};
                    if(!specification) { break; }
                    ::llvm::DWARFDie declaration{}; if(!referenced(*specification, declaration)) { return false; }
                    die = declaration; // owned immutable metadata handle; not value/location inheritance.
                }
                ::std::vector<::std::string> parts{};
                auto parent{die.getParent()};
                for(::std::size_t depth{}; parent && parent.getTag() != ::llvm::dwarf::DW_TAG_compile_unit; ++depth)
                {
                    if(depth >= cap.max_depth) { return fail(error::limit_exceeded); }
                    auto const tag{parent.getTag()};
                    if(tag == ::llvm::dwarf::DW_TAG_namespace || tag == ::llvm::dwarf::DW_TAG_class_type ||
                       tag == ::llvm::dwarf::DW_TAG_structure_type || tag == ::llvm::dwarf::DW_TAG_union_type || tag == ::llvm::dwarf::DW_TAG_subprogram)
                    { ::std::string part{}; if(!name(parent, part)) { return false; } if(!part.empty()) { parts.push_back(::std::move(part)); } }
                    parent = parent.getParent(); // bounded immutable DIE handle, not a runtime/source pointer.
                }
                ::std::string pending{};
                for(auto i{parts.size()}; i != 0u; --i)
                {
                    // [owned namespace parts ... i-1 ... end]
                    // [safe                              ] i > 0 && i <= size.
                    auto const& part{parts[i - 1u]};
                    if(part.size() > cap.max_string_bytes || pending.size() > cap.max_string_bytes - part.size()) { return fail(error::limit_exceeded); }
                    auto const length{pending.size() + part.size()};
                    if(cap.max_string_bytes < 2u || length > cap.max_string_bytes - 2u) { return fail(error::limit_exceeded); }
                    pending = ::fast_io::concat_std(::std::string_view{pending}, ::std::string_view{part}, "::");
                }
                if(leaf.size() > cap.max_string_bytes || pending.size() > cap.max_string_bytes - leaf.size() ||
                   !charge(pending.size() + leaf.size(), cap.max_total_string_bytes, used.strings)) { return fail(error::limit_exceeded); }
                destination = ::fast_io::concat_std(::std::string_view{pending}, leaf); return true;
            }
            [[nodiscard]] bool inspect_attributes(::llvm::DWARFDie die)
            {
                if(!die || die.isNULL()) { return fail(error::malformed); }
                auto const identity{key(die)};
                if(inspected_attributes.contains({identity.unit, identity.offset})) { return true; }
                ::std::set<::llvm::dwarf::Attribute> names{};
                for(auto const& attribute : die.attributes())
                {
                    if(!charge(1u, cap.max_attributes, used.attributes)) { return false; }
                    if(!attribute || attribute.Value.getForm() == ::llvm::dwarf::Form{0} ||
                       !names.insert(attribute.Attr).second) { return fail(error::malformed); }
                    // DWARF5 2.2: each attribute name occurs at most once in a
                    // DIE. Reject duplicate names BEFORE any semantic find()
                    // can select one of two conflicting values.
                    using namespace ::llvm::dwarf;
                    auto const form{attribute.Value.getForm()};
                    if((form == DW_FORM_strx || form == DW_FORM_GNU_str_index || form == DW_FORM_addrx ||
                        form == DW_FORM_GNU_addr_index || form == DW_FORM_loclistx || form == DW_FORM_rnglistx) &&
                       attribute.Value.getRawUValue() > (::std::numeric_limits<::std::uint32_t>::max)())
                    { return fail(error::malformed); } // do not allow LLVM's uint32 index APIs to truncate.
                    ::llvm::StringRef base_section{};
                    if(attribute.Attr == DW_AT_str_offsets_base) { base_section = "debug_str_offsets"; }
                    else if(attribute.Attr == DW_AT_addr_base || attribute.Attr == DW_AT_GNU_addr_base) { base_section = "debug_addr"; }
                    else if(attribute.Attr == DW_AT_rnglists_base) { base_section = "debug_rnglists"; }
                    else if(attribute.Attr == DW_AT_loclists_base) { base_section = "debug_loclists"; }
                    if(!base_section.empty())
                    {
                        auto const offset{attribute.Value.getAsSectionOffset()};
                        auto const owned{result.buffers_.find(base_section)};
                        if(!offset || owned == result.buffers_.end() || *offset > owned->second->getBufferSize())
                        { return fail(error::malformed); }
                    }
                    if(external_form(attribute.Value.getForm()) || attribute.Value.getForm() == DW_FORM_ref_sig8 ||
                       attribute.Attr == DW_AT_dwo_name || attribute.Attr == DW_AT_GNU_dwo_name || attribute.Attr == DW_AT_GNU_dwo_id)
                    { return fail(error::unsupported_external); }
                }
                inspected_attributes.emplace(identity.unit, identity.offset);
                return true;
            }
            [[nodiscard]] bool preflight_attributes()
            {
                struct item { ::llvm::DWARFDie die; ::std::size_t depth; };
                ::std::set< ::std::pair<::std::uint64_t, ::std::uint64_t>> seen{};
                for(auto* unit : units)
                {
                    // [immutable context-owned CU] DIE_end
                    // [safe                     ] handles only; no guest pointer,
                    //  ^^ parse the entire bounded CU before following references.
                    ::std::vector<item> pending{{unit->getUnitDIE(false), 0u}};
                    while(!pending.empty())
                    {
                        auto const current{pending.back()}; pending.pop_back();
                        if(!current.die || current.die.isNULL()) { continue; }
                        if(current.depth > cap.max_depth || seen.size() >= used.dies)
                        { return fail(error::limit_exceeded); }
                        auto const identity{key(current.die)};
                        if(identity.unit != unit->getOffset() || identity.offset < unit->getOffset() ||
                           identity.offset >= unit->getNextUnitOffset() || !seen.emplace(identity.unit, identity.offset).second)
                        { return fail(error::malformed); }
                        if(!inspect_attributes(current.die)) { return false; }
                        auto const sibling{current.die.getSibling()};
                        if(sibling) { pending.push_back({sibling, current.depth}); }
                        auto const child{current.die.getFirstChild()};
                        if(child)
                        {
                            if(current.depth >= cap.max_depth) { return fail(error::limit_exceeded); }
                            pending.push_back({child, current.depth + 1u});
                        }
                    }
                }
                // All CU/type/abstract-origin/parent attributes are now checked
                // before ANY semantic .find(). A later walk reuses the proof,
                // including forward references and other compilation units.
                return result.diagnostic_count_ == 0u || fail(error::malformed);
            }
            [[nodiscard]] bool walk(::llvm::DWARFUnit* unit)
            {
                struct item { ::llvm::DWARFDie die; ::std::size_t depth, scope; bool physical, inside_function, discarded; };
                // [safe] unit is retained by the same nonmoving context; only
                // getUnitDIE is used, never getNonSkeletonUnitDIE/parseDWO.
                ::std::vector<item> pending{{unit->getUnitDIE(false), 0u, no_record, false, false, false}};
                while(!pending.empty())
                {
                    auto current{pending.back()}; pending.pop_back();
                    if(!current.die || current.die.isNULL()) { continue; }
                    if(current.depth > cap.max_depth) { return fail(error::limit_exceeded); }
                    auto const identity{key(current.die)};
                    if(!visited.emplace(identity.unit, identity.offset).second || identity.offset < unit->getOffset() ||
                       identity.offset >= unit->getNextUnitOffset()) { return fail(error::malformed); }
                    if(!inspect_attributes(current.die)) { return false; }
                    auto child_scope{current.scope}; auto child_physical{current.physical}; auto child_inside_function{current.inside_function};
                    auto const tag{current.die.getTag()};
                    // DWARF5 3.8 (also DWARF4 3.7): try/catch are executable
                    // lexical scopes. Keep their OWN ranges (including explicit
                    // empty ranges) before selecting child inline/variable DIEs.
                    // No language/type/name or abstract origin grants a range.
                    bool const is_scope{tag == ::llvm::dwarf::DW_TAG_compile_unit || tag == ::llvm::dwarf::DW_TAG_subprogram ||
                        tag == ::llvm::dwarf::DW_TAG_inlined_subroutine || tag == ::llvm::dwarf::DW_TAG_lexical_block ||
                        tag == ::llvm::dwarf::DW_TAG_try_block || tag == ::llvm::dwarf::DW_TAG_catch_block};
                    // LLVM LLD marks a discarded concrete function in .debug_info
                    // with the CU address-width maximum. Its nested offset lists
                    // can still be present (including max+offset from that dead
                    // base), so they must not invalidate unrelated live functions
                    // or become live ranges after integer wraparound. ONLY a
                    // direct executable-scope low_pc can establish this marker;
                    // name/type/abstract_origin and merely out-of-Code ranges
                    // cannot. A CU base alone does not discard its live children.
                    bool discarded{current.discarded};
                    if(is_scope && tag != ::llvm::dwarf::DW_TAG_compile_unit)
                    {
                        if(auto low = current.die.find(::llvm::dwarf::DW_AT_low_pc))
                        {
                            auto const address{low->getAsAddress()};
                            if(!address) { return fail(error::malformed); }
                            discarded = discarded || *address == ::llvm::dwarf::computeTombstoneAddress(input.address_bytes);
                        }
                    }
                    if(is_scope)
                    {
                        scope_record record{}; record.identity = identity; record.parent = current.scope;
                        if(!scope_language(current.die,record)) { return false; }
                        record.kind = tag == ::llvm::dwarf::DW_TAG_compile_unit ? scope_kind::compile_unit :
                            tag == ::llvm::dwarf::DW_TAG_subprogram ? scope_kind::subprogram :
                            tag == ::llvm::dwarf::DW_TAG_inlined_subroutine ? scope_kind::inline_subprogram : scope_kind::lexical_block;
                        // Attributes of EVERY DIE were already structurally
                        // preflighted. Keep declaration/name/type references, but
                        // never decode/publish execution locations below a real
                        // discarded concrete scope. Empty own ranges remain an
                        // exclusion; an inline child cannot resurrect this parent.
                        if(!name(current.die, record.name) || (!discarded &&
                           (!ranges(current.die, record.ranges) || !locations(current.die, ::llvm::dwarf::DW_AT_frame_base,
                               record.frame_base, position_role::frame_base)))) { return false; }
                        record.concrete = !record.ranges.empty();
                        // Only direct attributes of this actual DIE are borrowed
                        // from the owned CU. Do not use inherited()/abstract origin
                        // for scope range authority, including empty ranges.
                        record.own_ranges_declared = static_cast<bool>(current.die.find(::llvm::dwarf::DW_AT_ranges)) ||
                            static_cast<bool>(current.die.find(::llvm::dwarf::DW_AT_low_pc)) ||
                            static_cast<bool>(current.die.find(::llvm::dwarf::DW_AT_high_pc));
                        if(tag == ::llvm::dwarf::DW_TAG_subprogram)
                        {
                            if(auto object = current.die.find(::llvm::dwarf::DW_AT_object_pointer))
                            {
                                ::llvm::DWARFDie parameter{};
                                if(!referenced(*object,parameter)) { return false; }
                                if(parameter.getTag() != ::llvm::dwarf::DW_TAG_formal_parameter)
                                { return fail(error::malformed); }
                                record.object_pointer = key(parameter); record.object_pointer_known = true;
                            }
                            child_physical = record.concrete; child_inside_function = true;
                            // An explicitly empty physical function has NO
                            // executable descendants. Structurally preflight
                            // their attributes, but ignore retired ranges and
                            // storage instead of rejecting unrelated functions.
                            if(record.own_ranges_declared && record.ranges.empty())
                            { discarded = true; record.frame_base.clear(); }
                        }
                        if(tag == ::llvm::dwarf::DW_TAG_inlined_subroutine)
                        {
                            if(auto attribute = current.die.find(::llvm::dwarf::DW_AT_call_file)) { if(!file(*attribute, record.call_file)) { return false; } }
                            if(auto attribute = current.die.find(::llvm::dwarf::DW_AT_call_line))
                            { auto value{attribute->getAsUnsignedConstant()}; if(!value) { return fail(error::malformed); } record.call_line = *value; }
                            if(auto attribute = current.die.find(::llvm::dwarf::DW_AT_call_column))
                            { auto value{attribute->getAsUnsignedConstant()}; if(!value) { return fail(error::malformed); } record.call_column = *value; }
                        }
                        child_scope = result.scopes_.size(); result.scopes_.push_back(::std::move(record));
                    }
                    else if(!discarded && ((current.physical && (tag == ::llvm::dwarf::DW_TAG_variable || tag == ::llvm::dwarf::DW_TAG_formal_parameter)) ||
                            (!current.inside_function && tag == ::llvm::dwarf::DW_TAG_variable)))
                    {
                        variable_record record{}; record.identity = identity; record.scope = current.scope;
                        record.parameter = tag == ::llvm::dwarf::DW_TAG_formal_parameter;
                        record.global = !current.inside_function;
                        if(!name(current.die, record.name) || !type(current.die, record.type) ||
                           !locations(current.die, ::llvm::dwarf::DW_AT_location, record.locations) ||
                           !qualified_name(current.die, record.name, record.qualified_name)) { return false; }
                        if(record.parameter && record.name == "this" && record.type < result.types_.size() &&
                           record.scope < result.scopes_.size())
                        {
                            auto const& owned_type{result.types_[record.type]};
                            auto const& owner_scope{result.scopes_[record.scope]};
                            bool cpp{};
                            switch(owned_type.language)
                            { case 0x04u: case 0x11u: case 0x19u: case 0x1au: case 0x21u:
                              case 0x2au: case 0x2bu: case 0x3au: cpp=true; break; default: break; }
                            if(cpp && !owned_type.tinygo_producer && !owned_type.zig_producer && owned_type.kind == type_kind::pointer &&
                               !owned_type.reference_type && owner_scope.object_pointer_known &&
                               owner_scope.object_pointer == record.identity)
                            {
                                if(auto artificial = current.die.find(::llvm::dwarf::DW_AT_artificial))
                                {
                                    auto const flag{artificial->getAsUnsignedConstant()};
                                    if(!flag) { return fail(error::malformed); }
                                    record.immutable_cpp_object_pointer = *flag != 0u;
                                }
                            }
                        }
                        // DIRECT constant metadata only: abstract origins cannot supply
                        // a value at an unrelated concrete activation/lexical scope.
                        if(auto constant = current.die.find(::llvm::dwarf::DW_AT_const_value))
                        {
                            location_plan plan{};
                            // [owned immutable type records ... checked type ... end]
                            // [safe                                                ] the condition
                            //  ^^ proves type < size BEFORE the selected arm borrows it.
                            auto const decoded{record.type < result.types_.size() ?
                                ::uwvm2::uwvm::debugger::source_dwarf_constants::decode(*constant, result.types_[record.type], input.address_bytes, plan) :
                                ::uwvm2::uwvm::debugger::source_dwarf_constants::error::unavailable};
                            if(decoded == ::uwvm2::uwvm::debugger::source_dwarf_constants::error::malformed) { return fail(error::malformed); }
                            if(current.die.find(::llvm::dwarf::DW_AT_location))
                            {
                                // Attribute PRESENCE matters: an explicit empty
                                // location list is unavailable storage, not permission
                                // to substitute a constant. DWARF 2.2 forbids mixing
                                // value/storage descriptions on this direct DIE.
                                // Conflicting value/storage representations cannot become
                                // a guessed guest address. Keep only explicit unavailable plans.
                                for(auto& location : record.locations)
                                { location.plan = {}; location.plan.reason = unavailable_reason::unsupported_expression; }
                            }
                            else
                            {
                                if(!charge(1u, cap.max_locations, used.locations)) { return false; }
                                if(decoded != ::uwvm2::uwvm::debugger::source_dwarf_constants::error::none)
                                { plan = {}; plan.reason = unavailable_reason::unsupported_expression; }
                                record.locations.push_back({{}, ::std::move(plan)});
                            }
                        }
                        if(auto declaration = current.die.find(::llvm::dwarf::DW_AT_declaration))
                        { auto const value{declaration->getAsUnsignedConstant()}; if(!value) { return fail(error::malformed); } record.declaration = *value != 0u; }
                        ::std::optional<::llvm::DWARFFormValue> attribute{};
                        if(!inherited(current.die, ::llvm::dwarf::DW_AT_external, attribute)) { return false; }
                        if(attribute) { auto const value{attribute->getAsUnsignedConstant()}; if(!value) { return fail(error::malformed); } record.external = *value != 0u; }
                        record.static_storage = record.global;
                        for(auto const& location : record.locations) { if(location.plan.kind == plan_kind::absolute_guest_offset) { record.static_storage = true; } }
                        if(!inherited(current.die, ::llvm::dwarf::DW_AT_decl_file, attribute) || (attribute && !file(*attribute, record.declaration_file))) { return false; }
                        if(!inherited(current.die, ::llvm::dwarf::DW_AT_decl_line, attribute)) { return false; }
                        if(attribute) { auto value{attribute->getAsUnsignedConstant()}; if(!value) { return fail(error::malformed); } record.declaration_line = *value; }
                        result.variables_.push_back(::std::move(record));
                    }
                    // [safe] child/sibling are handles into this parsed CU; the
                    // explicit worklist enforces depth and rejects repeated DIEs.
                    // Abstract origin never contributes ranges/locations here.
                    if(auto sibling = current.die.getSibling()) { pending.push_back({sibling, current.depth, current.scope, current.physical, current.inside_function, current.discarded}); }
                    if(auto child = current.die.getFirstChild())
                    {
                        if(current.depth == cap.max_depth) { return fail(error::limit_exceeded); }
                        pending.push_back({child, current.depth + 1u, child_scope, child_physical, child_inside_function, discarded});
                    }
                }
                return result.diagnostic_count_ == 0u || fail(error::malformed);
            }
            [[nodiscard]] bool build()
            {
                if(!prepare_sections() || !preflight_units()) { return false; }
                // [owned buffers at their final heap owner] buffer_end
                // [safe                                 ] context borrows them
                // until destruction. Only the in-memory overload is reachable.
                result.context_ = ::llvm::DWARFContext::create(result.buffers_, input.address_bytes, true,
                    [owner = ::std::addressof(result)](::llvm::Error issue) { owner->consume_diagnostic(::std::move(issue), 1u); },
                    [owner = ::std::addressof(result)](::llvm::Error issue) { owner->consume_diagnostic(::std::move(issue), 2u); });
                if(!result.context_) { return fail(error::allocation_failure); }
                auto const expected_units{used.units};
                for(auto const& owned_unit : result.context_->compile_units())
                {
                    auto* const unit{owned_unit.get()}; // borrow belongs solely to result.context_.
                    if(units.size() >= expected_units || unit->isDWOUnit() || unit->getUnitType() != ::llvm::dwarf::DW_UT_compile ||
                       (unit->getVersion() != 4u && unit->getVersion() != 5u) || unit->getAddressByteSize() != input.address_bytes)
                    { return fail(error::unsupported_dwarf); }
                    auto const root{unit->getUnitDIE()};
                    if(!root || root.getTag() != ::llvm::dwarf::DW_TAG_compile_unit)
                    { return fail(error::malformed); }
                    // This is an ACTUAL parsed count, not a preallocation heap
                    // limit: LLVM parses the bounded CU before returning it.
                    // The external controlled test/build process still supplies
                    // the hard memory limit for LLVM and allocator internals.
                    if(!charge(unit->getNumDIEs(), cap.max_dies, used.dies)) { return false; }
                    units.push_back(unit);
                }
                if(units.size() != expected_units || result.diagnostic_count_ != 0u) { return fail(error::malformed); }
                if(!preflight_attributes()) { return false; }
                // Resolve each line table against its own embedded CU. All
                // attributes were preflighted before these semantic lookups.
                for(auto* unit : units)
                {
                    auto const root{unit->getUnitDIE()};
                    auto const stmt{root.find(::llvm::dwarf::DW_AT_stmt_list)};
                    if(!stmt) { continue; }
                    auto const offset{stmt->getAsSectionOffset()};
                    if(!offset) { return fail(error::malformed); }
                    line_directory_record record{*offset, {}};
                    if(auto directory = root.find(::llvm::dwarf::DW_AT_comp_dir))
                    { if(!copy_string(*directory, record.directory)) { return false; } }
                    result.line_directories_.push_back(::std::move(record));
                }
                ::std::sort(result.line_directories_.begin(), result.line_directories_.end(),
                    [](auto const& a, auto const& b) { return a.line_unit_offset < b.line_unit_offset; });
                for(::std::size_t i{1u}; i < result.line_directories_.size(); ++i)
                {
                    auto const& previous{result.line_directories_[i - 1u]};
                    auto const& current{result.line_directories_[i]};
                    if(previous.line_unit_offset == current.line_unit_offset && previous.directory != current.directory)
                    { return fail(error::malformed); }
                }
                auto const end{::std::unique(result.line_directories_.begin(), result.line_directories_.end(),
                    [](auto const& a, auto const& b) { return a.line_unit_offset == b.line_unit_offset; })};
                result.line_directories_.erase(end, result.line_directories_.end());
                for(auto* unit : units) { if(!walk(unit)) { return false; } }
                return failure == error::none;
            }
        };
    }

    inline error index::parse(image_input input, ::std::unique_ptr<index>& out, limits const& cap) noexcept
    {
        out.reset(); // old metadata is never kept after a failed new parse.
#if defined(__cpp_exceptions)
        try
        {
#endif
            ::std::unique_ptr<index> candidate{new (::std::nothrow) index{}};
            if(!candidate) { return error::allocation_failure; }
            details::index_builder builder{*candidate, input, cap};
            if(!builder.build()) { return builder.failure == error::none ? error::malformed : builder.failure; }
            // The only publication is this move AFTER a complete successful
            // parse. Failure destroys every partial record/context/buffer owner.
            candidate->address_bytes_ = input.address_bytes;
            out = ::std::move(candidate);
            return error::none;
#if defined(__cpp_exceptions)
        }
        catch(::std::bad_alloc const&) { out.reset(); return error::allocation_failure; }
        catch(...) { out.reset(); return error::malformed; }
#endif
    }
}
#endif
