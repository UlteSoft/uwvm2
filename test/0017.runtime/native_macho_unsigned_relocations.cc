// Source-only qualification input. The sole remote keeper generates the
// paired .s with actual llvm-mc for x86_64/arm64 Apple and rebuilds this file
// against the actual candidate LLVM closure. No local compile/run is allowed.
// argv[1]: actual Mach-O .o; optional argv[2]: fresh output-prefix for mutated
// object copies. The real ObjectFile, DWARFContext, DWARFDataExtractor, ranges
// and formal DWARF5 addr parsers must perform the tested relocation pipeline.
// Synthetic section loads are component data, never native PC/stop ownership.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <uwvm2/runtime/lib/uwvm_runtime_native_provenance_rows.h>
#include <llvm/BinaryFormat/MachO.h>
#include <llvm/DebugInfo/DWARF/DWARFDataExtractor.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugAddr.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugRangeList.h>
#include <llvm/Object/MachO.h>
#include <llvm/Object/RelocationResolver.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/TargetParser/Triple.h>
#include <fast_io.h>
#include <fast_io_dsal/string_view.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace provenance = ::uwvm2::runtime::lib::details::native_loaded_provenance;
namespace
{
#define REQUIRE(condition, message) do { if(!(condition)) { ::fast_io::io::perrln("FAIL actual Mach-O relocation component line=", ::fast_io::mnp::dec(__LINE__), " ", message); return false; } } while(false)
    struct diagnostics
    {
        unsigned errors{}, warnings{};
        void record(::llvm::Error failure, bool warning)
        {
            if(!failure) { return; }
            if(warning) { ++warnings; } else { ++errors; }
            auto const text{::llvm::toString(::std::move(failure))};
            ::fast_io::io::perrln("LLVM Mach-O ", warning ? "warning: " : "error: ",
                ::fast_io::mnp::strvw(text));
        }
        [[nodiscard]] bool clean() const noexcept { return errors == 0u && warnings == 0u; }
    };
    [[nodiscard]] bool error_free(::llvm::Error failure, char const* operation)
    {
        if(!failure) { return true; }
        auto const text{::llvm::toString(::std::move(failure))};
        ::fast_io::io::perrln("FAIL actual LLVM ", ::fast_io::mnp::os_c_str(operation), ": ",
            ::fast_io::mnp::strvw(text));
        return false;
    }
    struct sections
    {
        ::llvm::object::SectionRef first{}, second{}, ranges{}, addr{}, width32{};
        ::std::uint64_t first_symbol{}, second_symbol{};
    };
    [[nodiscard]] bool inspect_sections(::llvm::object::MachOObjectFile const& object, sections& out)
    {
        ::std::array<bool, 5u> found{};
        for(auto const& section: object.sections())
        {
            auto name{section.getName()};
            if(!name) { return error_free(name.takeError(), "section name"); }
            ::std::size_t slot{};
            ::llvm::object::SectionRef* selected{};
            if(*name == "__prov_first") { selected = &out.first; slot = 0u; }
            else if(*name == "__prov_second") { selected = &out.second; slot = 1u; }
            else if(*name == "__debug_ranges") { selected = &out.ranges; slot = 2u; }
            else if(*name == "__debug_addr") { selected = &out.addr; slot = 3u; }
            else if(*name == "__prov_w32") { selected = &out.width32; slot = 4u; }
            else { continue; }
            REQUIRE(!found[slot], "five unique actual named sections");
            *selected = section; found[slot] = true;
        }
        for(auto const present: found) { REQUIRE(present, "all five actual sections present"); }
        REQUIRE(out.first.isText() && out.second.isText() && out.first.getIndex() != out.second.getIndex(),
                "two distinct actual pure-instruction text sections");
        REQUIRE(out.first.getSize() == 48u && out.second.getSize() == 56u && out.second.getAddress() > 1u,
                "nonzero known function offsets and second source base support P<O");
        REQUIRE(out.ranges.getSize() == 48u && out.addr.getSize() == 32u && out.width32.getSize() == 4u,
                "actual ranges, formal DWARF5 addr and non-DWARF width32 extents");
        REQUIRE((object.getSection64(out.ranges.getRawDataRefImpl()).flags & ::llvm::MachO::S_ATTR_DEBUG) != 0u &&
                (object.getSection64(out.addr.getRawDataRefImpl()).flags & ::llvm::MachO::S_ATTR_DEBUG) == 0u,
                "ranges local-forcing DEBUG attribute and addr external-preserving attribute");
        ::std::array<bool, 2u> symbols{};
        for(auto const& symbol: object.symbols())
        {
            auto name{symbol.getName()};
            if(!name) { return error_free(name.takeError(), "symbol name"); }
            if(*name != "first" && *name != "second") { continue; }
            auto const slot{*name == "first" ? 0u : 1u};
            REQUIRE(!symbols[slot], "unique actual first/second symbol");
            auto section{symbol.getSection()};
            if(!section) { return error_free(section.takeError(), "symbol section"); }
            REQUIRE(*section != object.section_end(), "external atom is section-defined");
            auto address{symbol.getAddress()};
            if(!address) { return error_free(address.takeError(), "symbol address"); }
            auto const& expected_section{slot == 0u ? out.first : out.second};
            REQUIRE((*section)->getIndex() == expected_section.getIndex() &&
                    *address >= expected_section.getAddress() &&
                    *address - expected_section.getAddress() == (slot == 0u ? 16u : 24u),
                    "actual exported atoms use specified section-relative padding");
            if(slot == 0u) { out.first_symbol = *address; } else { out.second_symbol = *address; }
            symbols[slot] = true;
        }
        REQUIRE(symbols[0u] && symbols[1u], "both actual exported atom symbols found");
        return true;
    }
    class section_loads final : public ::llvm::LoadedObjectInfo
    {
        ::std::uint64_t first_index{}, second_index{}, first_base{}, second_base{};
    public:
        section_loads(sections const& source, ::std::uint64_t first, ::std::uint64_t second)
            : first_index{source.first.getIndex()}, second_index{source.second.getIndex()},
              first_base{first}, second_base{second} {}
        ::std::uint64_t getSectionLoadAddress(::llvm::object::SectionRef const& section) const override
        {
            if(section.getIndex() == first_index) { return first_base; }
            if(section.getIndex() == second_index) { return second_base; }
            return 0u;
        }
        ::std::unique_ptr<::llvm::LoadedObjectInfo> clone() const override
        { return ::std::make_unique<section_loads>(*this); }
    };
    [[nodiscard]] ::std::unique_ptr<::llvm::DWARFContext> context_for(
        ::llvm::object::ObjectFile const& object, ::llvm::LoadedObjectInfo const* loaded, diagnostics& report)
    {
        return ::llvm::DWARFContext::create(object, ::llvm::DWARFContext::ProcessDebugRelocations::Process,
            loaded, "", [&](::llvm::Error failure) { report.record(::std::move(failure), false); },
            [&](::llvm::Error failure) { report.record(::std::move(failure), true); });
    }
    [[nodiscard]] bool read_value(::llvm::DWARFDataExtractor const& extractor, ::std::uint32_t width,
        ::std::uint64_t offset, ::std::uint64_t expected, ::std::uint64_t section,
        bool relocated = true)
    {
        REQUIRE(extractor.isValidOffsetForDataOfSize(offset, width), "bounded actual DWARF extraction");
        auto cursor{offset};
        ::std::uint64_t section_index{::llvm::object::SectionedAddress::UndefSection};
        ::llvm::Error error{::llvm::Error::success()};
        auto const value{extractor.getRelocatedValue(width, &cursor, &section_index, &error)};
        REQUIRE(error_free(::std::move(error), "DWARF getRelocatedValue"), "actual extractor error-free");
        REQUIRE(cursor == offset + width && value == expected, "actual relocated value and cursor");
        REQUIRE(section_index == (relocated ? section : ::llvm::object::SectionedAddress::UndefSection),
                "actual relocation map identifies the intended text section");
        return true;
    }
    [[nodiscard]] bool inspect_relocations(::llvm::object::MachOObjectFile const& object, sections const& source)
    {
        unsigned count{};
        ::std::array<bool, 4u> local_offsets{};
        for(auto const& relocation: source.ranges.relocations())
        {
            auto const raw{object.getRelocation(relocation.getRawDataRefImpl())};
            REQUIRE(!object.isRelocationScattered(raw) && !object.getPlainRelocationExternal(raw) &&
                    object.getAnyRelocationPCRel(raw) == 0u && object.getAnyRelocationLength(raw) == 3u &&
                    object.getAnyRelocationType(raw) == 0u,
                    "actual local absolute unsigned 8-byte ranges relocation flags");
            auto const offset{relocation.getOffset()};
            REQUIRE(offset < 32u && offset % 8u == 0u && !local_offsets[offset / 8u],
                    "four distinct actual local relocation positions");
            auto const actual_section{object.getRelocationSection(relocation.getRawDataRefImpl())};
            auto const expected_index{offset < 16u ? source.first.getIndex() : source.second.getIndex()};
            REQUIRE(actual_section != object.section_end() && actual_section->getIndex() == expected_index &&
                    relocation.getSymbol() == object.symbol_end(), "actual local section ordinal rather than invented symbol");
            local_offsets[offset / 8u] = true; ++count;
        }
        REQUIRE(count == 4u, "exactly four actual local range relocations");
        count = 0u;
        ::std::array<bool, 3u> external_offsets{};
        for(auto const& relocation: source.addr.relocations())
        {
            auto const raw{object.getRelocation(relocation.getRawDataRefImpl())};
            REQUIRE(!object.isRelocationScattered(raw) && object.getPlainRelocationExternal(raw) &&
                    object.getAnyRelocationPCRel(raw) == 0u && object.getAnyRelocationLength(raw) == 3u &&
                    object.getAnyRelocationType(raw) == 0u,
                    "actual external absolute unsigned 8-byte addr relocation flags");
            auto const offset{relocation.getOffset()};
            REQUIRE(offset >= 8u && offset < 32u && offset % 8u == 0u &&
                    !external_offsets[(offset - 8u) / 8u], "three distinct external address relocations");
            auto const symbol{relocation.getSymbol()};
            REQUIRE(symbol != object.symbol_end(), "actual external symbol table entry");
            auto name{symbol->getName()};
            if(!name) { return error_free(name.takeError(), "external relocation symbol name"); }
            REQUIRE(*name == (offset == 8u ? "first" : "second"), "actual external atom identity");
            external_offsets[(offset - 8u) / 8u] = true; ++count;
        }
        REQUIRE(count == 3u, "exactly three actual external address relocations");
        count = 0u;
        for(auto const& relocation: source.width32.relocations())
        {
            auto const raw{object.getRelocation(relocation.getRawDataRefImpl())};
            REQUIRE(!object.isRelocationScattered(raw) && object.getPlainRelocationExternal(raw) &&
                    object.getAnyRelocationPCRel(raw) == 0u && object.getAnyRelocationLength(raw) == 2u &&
                    object.getAnyRelocationType(raw) == 0u && relocation.getOffset() == 0u,
                    "actual external absolute unsigned 4-byte non-DWARF relocation flags");
            auto const symbol{relocation.getSymbol()};
            REQUIRE(symbol != object.symbol_end(), "width32 actual external symbol");
            auto name{symbol->getName()};
            if(!name) { return error_free(name.takeError(), "width32 symbol name"); }
            REQUIRE(*name == "second", "width32 targets real second atom");
            ++count;
        }
        REQUIRE(count == 1u, "one actual width32 relocation");
        return true;
    }
    [[nodiscard]] bool loaded_values(::llvm::object::MachOObjectFile const& object, sections const& source,
        ::std::uint64_t first_base, ::std::uint64_t second_base, char const* scenario)
    {
        section_loads loaded{source, first_base, second_base};
        diagnostics report{};
        auto context{context_for(object, &loaded, report)};
        REQUIRE(context && report.clean(), "actual loaded DWARF context has no diagnostics");
        auto const& dwarf_object{context->getDWARFObj()};
        ::llvm::DWARFDataExtractor ranges{dwarf_object, dwarf_object.getRangesSection(), true, 8u};
        ::llvm::DWARFDataExtractor addresses{dwarf_object, dwarf_object.getAddrSection(), true, 8u};
        // A zero load address explicitly preserves original object addresses.
        // These expected constants come from the independent .s padding and
        // addends, not from a reimplementation of a relocation resolver.
        auto const first{first_base == 0u ? source.first.getAddress() : first_base};
        auto const second{second_base == 0u ? source.second.getAddress() : second_base};
        REQUIRE(read_value(ranges, 8u, 0u, first + 19u, source.first.getIndex()) &&
                read_value(ranges, 8u, 8u, first + 25u, source.first.getIndex()) &&
                read_value(ranges, 8u, 16u, second + 29u, source.second.getIndex()) &&
                read_value(ranges, 8u, 24u, second + 37u, source.second.getIndex()),
                "actual local per-section delta preserves nonzero function offsets and addends");
        REQUIRE(read_value(ranges, 8u, 32u, 0u, 0u, false) && read_value(ranges, 8u, 40u, 0u, 0u, false),
                "range terminator is not fabricated as a relocation");
        REQUIRE(read_value(addresses, 8u, 8u, first + 23u, source.first.getIndex()) &&
                read_value(addresses, 8u, 16u, second + 35u, source.second.getIndex()) &&
                read_value(addresses, 8u, 24u, second + 16u, source.second.getIndex()),
                "actual external loaded-symbol plus positive and negative 64-bit in-place addends");
        ::llvm::DWARFDebugRangeList range_list{};
        ::std::uint64_t range_offset{};
        REQUIRE(error_free(range_list.extract(ranges, &range_offset), "actual range-list parser") &&
                range_offset == 48u && range_list.getEntries().size() == 2u,
                "actual formal range-list parse including terminator");
        auto const& entries{range_list.getEntries()};
        REQUIRE(entries[0u].StartAddress == first + 19u && entries[0u].EndAddress == first + 25u &&
                entries[0u].SectionIndex == source.first.getIndex() &&
                entries[1u].StartAddress == second + 29u && entries[1u].EndAddress == second + 37u &&
                entries[1u].SectionIndex == source.second.getIndex(),
                "actual range parser retains exact target-section identities");
        ::llvm::DWARFDebugAddrTable address_table{};
        ::std::uint64_t addr_offset{};
        REQUIRE(error_free(address_table.extractV5(addresses, &addr_offset, 8u,
            [&](::llvm::Error failure) { report.record(::std::move(failure), true); }), "actual DWARF5 address table") &&
            report.clean() && addr_offset == 32u && address_table.getLength() == 28u &&
            address_table.getVersion() == 5u && address_table.getAddressSize() == 8u &&
            address_table.getSegmentSelectorSize() == 0u && address_table.getAddressEntries().size() == 3u,
            "formal actual DWARF5 header and bounded address count");
        auto const actual_addresses{address_table.getAddressEntries()};
        REQUIRE(actual_addresses[0u] == first + 23u && actual_addresses[1u] == second + 35u &&
                actual_addresses[2u] == second + 16u, "actual DWARF5 parser applies three real symbol relocations");
        auto resolver{::llvm::object::getRelocationResolver(object)};
        REQUIRE(resolver.first && resolver.second, "actual target Mach-O resolver exists");
        auto contents{source.width32.getContents()};
        if(!contents) { return error_free(contents.takeError(), "width32 actual section contents"); }
        ::llvm::DWARFDataExtractor raw_width32{*contents, true, 4u};
        ::std::uint64_t raw_offset{};
        auto const raw{raw_width32.getUnsigned(&raw_offset, 4u)};
        REQUIRE(raw_offset == 4u && raw == 0xfffffffcu, "actual negative four-byte in-place value");
        auto const relocation{*source.width32.relocations().begin()};
        REQUIRE(resolver.first(relocation.getType()) &&
                ::llvm::object::resolveRelocation(resolver.second, relocation, second + 24u, raw) == second + 20u,
                "real relocation reference applies negative 32-bit addend with width truncation");
        ::fast_io::io::println("actual Mach-O loaded relocation component: ", ::fast_io::mnp::os_c_str(scenario));
        return true;
    }
    [[nodiscard]] bool no_loaded_preserves_raw(::llvm::object::MachOObjectFile const& object, sections const& source)
    {
        diagnostics report{};
        auto context{context_for(object, nullptr, report)};
        REQUIRE(context && report.clean(), "NoL retains legacy Mach-O relocation skip");
        auto const& dwarf_object{context->getDWARFObj()};
        ::llvm::DWARFDataExtractor ranges{dwarf_object, dwarf_object.getRangesSection(), true, 8u};
        REQUIRE(read_value(ranges, 8u, 0u, source.first.getAddress() + 19u, 0u, false) &&
                read_value(ranges, 8u, 8u, source.first.getAddress() + 25u, 0u, false) &&
                read_value(ranges, 8u, 16u, source.second.getAddress() + 29u, 0u, false) &&
                read_value(ranges, 8u, 24u, source.second.getAddress() + 37u, 0u, false),
                "NoL raw local words retain assembler-factored original addresses");
        ::llvm::DWARFDataExtractor addresses{dwarf_object, dwarf_object.getAddrSection(), true, 8u};
        REQUIRE(read_value(addresses, 8u, 8u, 7u, 0u, false) && read_value(addresses, 8u, 16u, 11u, 0u, false) &&
                read_value(addresses, 8u, 24u, (::std::numeric_limits<::std::uint64_t>::max)() - 7u, 0u, false),
                "NoL external data is explicitly skipped raw in-place data, never a resolved symbol address");
        ::fast_io::io::println("actual Mach-O NoL: local raw addresses preserved; external relocations explicitly skipped");
        return true;
    }
    [[nodiscard]] bool overwrite32(::std::vector<::std::byte>& bytes, ::std::size_t offset, ::std::uint32_t word)
    {
        REQUIRE(offset <= bytes.size() && 4u <= bytes.size() - offset, "mutation bounded before deriving byte pointers");
        // [owned bytes ... offset][exact four-byte relocation word] ... end
        // [safe                                                          ] one-past
        //                         ^^ derive this exact mutation only after proof.
        auto* first{reinterpret_cast<unsigned char*>(bytes.data()) + offset};
        ::fast_io::basic_obuffer_view<unsigned char> output{first, first + 4u};
        ::fast_io::io::print(output, ::fast_io::mnp::le_put<32>(word));
        REQUIRE(output.curr_ptr == first + 4u, "fast_io wrote exactly one little-endian relocation word");
        return true;
    }
    [[nodiscard]] bool mutation_position(::llvm::object::MachOObjectFile const& object,
        ::llvm::object::SectionRef const& section, ::llvm::object::RelocationRef const& relocation,
        ::std::size_t bytes, ::std::size_t& offset, bool address_word = false)
    {
        auto const header{object.getSection64(section.getRawDataRefImpl())};
        auto const index{relocation.getRawDataRefImpl()};
        REQUIRE(index.d.a == section.getRawDataRefImpl().d.a && index.d.b < header.nreloc &&
                header.reloff <= bytes && header.nreloc <= (bytes - header.reloff) / 8u,
                "actual SDK relocation table/index bounds precede file offset arithmetic");
        offset = static_cast<::std::size_t>(header.reloff) + static_cast<::std::size_t>(index.d.b) * 8u +
            (address_word ? 0u : 4u);
        REQUIRE(offset <= bytes && 4u <= bytes - offset, "actual SDK selected relocation word lies inside copied object");
        return true;
    }
    [[nodiscard]] bool absolute_local_preserves_raw(::std::vector<::std::byte> const& original,
        ::llvm::object::MachOObjectFile const& object, sections const& source,
        ::llvm::object::RelocationRef const& relocation)
    {
        ::std::size_t offset{};
        REQUIRE(mutation_position(object, source.ranges, relocation, original.size(), offset),
                "bounded actual local-to-R_ABS mutation");
        auto bytes{original};
        auto const word{object.getRelocation(relocation.getRawDataRefImpl()).r_word1};
        REQUIRE(overwrite32(bytes, offset, word & 0xff000000u), "actual local target changed only to R_ABS");
        auto buffer{::llvm::MemoryBuffer::getMemBufferCopy(
            ::llvm::StringRef{reinterpret_cast<char const*>(bytes.data()), bytes.size()}, "actual-macho-r-abs-object")};
        auto result{::llvm::object::ObjectFile::createObjectFile(buffer->getMemBufferRef())};
        if(!result) { return error_free(result.takeError(), "R_ABS actual object parse"); }
        auto* const macho{::llvm::dyn_cast<::llvm::object::MachOObjectFile>(result->get())};
        REQUIRE(macho != nullptr, "R_ABS clone remains actual Mach-O");
        sections actual{};
        REQUIRE(inspect_sections(*macho, actual), "R_ABS actual section/symbol identities unchanged");
        bool found{};
        for(auto const& candidate: actual.ranges.relocations())
        {
            if(candidate.getOffset() != relocation.getOffset()) { continue; }
            auto const raw{macho->getRelocation(candidate.getRawDataRefImpl())};
            REQUIRE(!macho->isRelocationScattered(raw) && !macho->getPlainRelocationExternal(raw) &&
                    macho->getPlainRelocationSymbolNum(raw) == ::llvm::MachO::R_ABS &&
                    macho->getRelocationSection(candidate.getRawDataRefImpl()) == macho->section_end() &&
                    candidate.getSymbol() == macho->symbol_end(),
                    "actual absolute local fixup legitimately has no target section or symbol");
            found = true;
        }
        REQUIRE(found, "actual R_ABS relocation remains in the copied table");
        auto original_contents{source.ranges.getContents()};
        if(!original_contents) { return error_free(original_contents.takeError(), "R_ABS original in-place section"); }
        ::llvm::DWARFDataExtractor raw_ranges{*original_contents, true, 8u};
        auto raw_offset{relocation.getOffset()};
        REQUIRE(raw_ranges.isValidOffsetForDataOfSize(raw_offset, 8u), "bounded original absolute field");
        auto const raw_value{raw_ranges.getU64(&raw_offset)};
        section_loads loaded{actual, 0x100000u, 0x300000u};
        diagnostics report{};
        auto context{context_for(*macho, &loaded, report)};
        REQUIRE(context && report.clean(), "legitimate R_ABS has no LLVM relocation diagnostics");
        auto const& dwarf_object{context->getDWARFObj()};
        ::llvm::DWARFDataExtractor ranges{dwarf_object, dwarf_object.getRangesSection(), true, 8u};
        REQUIRE(read_value(ranges, 8u, relocation.getOffset(), raw_value,
            ::llvm::object::SectionedAddress::UndefSection),
            "R_ABS preserves original raw value without inventing a loaded section identity");
        ::fast_io::io::println("actual Mach-O R_ABS: raw absolute value preserved, section identity undefined");
        return true;
    }
    [[nodiscard]] bool malformed_object(::std::vector<::std::byte> const& original,
        ::llvm::object::MachOObjectFile const& object, sections const& source,
        ::llvm::object::SectionRef const& section, ::llvm::object::RelocationRef const& relocation,
        ::std::uint32_t word, char const* name, char const* output_prefix, bool address_word = false)
    {
        ::std::size_t offset{};
        REQUIRE(mutation_position(object, section, relocation, original.size(), offset, address_word), "actual relocation mutation position");
        auto bytes{original};
        REQUIRE(overwrite32(bytes, offset, word), "bounded fast_io relocation-word mutation");
        if(output_prefix != nullptr)
        {
            auto path{::fast_io::concat_std(::fast_io::mnp::os_c_str(output_prefix), ".",
                object.getArch() == ::llvm::Triple::x86_64 ? "x64" : "arm64", ".",
                ::fast_io::mnp::os_c_str(name), ".o")};
            ::fast_io::native_file output{path, ::fast_io::open_mode::out};
            // [owned cloned bytes ... bytes.size) end
            // [safe                              ] complete bounded clone only.
            ::fast_io::operations::write_all_bytes(output, bytes.data(), bytes.data() + bytes.size());
        }
        auto buffer{::llvm::MemoryBuffer::getMemBufferCopy(
            ::llvm::StringRef{reinterpret_cast<char const*>(bytes.data()), bytes.size()}, "mutated-real-macho-object")};
        auto result{::llvm::object::ObjectFile::createObjectFile(buffer->getMemBufferRef())};
        // These fields must remain a structurally accepted ObjectFile so the
        // actual DWARFContext pipeline, rather than this fixture, rejects them.
        if(!result) { return error_free(result.takeError(), "mutated object must reach actual DWARF validation"); }
        auto* const macho{::llvm::dyn_cast<::llvm::object::MachOObjectFile>(result->get())};
        REQUIRE(macho != nullptr, "mutated object remains actual Mach-O");
        section_loads loaded{source, 0x100000u, 0x300000u};
        diagnostics report{};
        auto context{context_for(*macho, &loaded, report)};
        REQUIRE(context && !report.clean(), "malformed real object raises recoverable LLVM diagnostics");
        provenance::image image{};
        REQUIRE(!image.observe(1u, *macho, loaded) && !image.valid(),
                "actual native provenance consumer revokes entire malformed object image");
        image.bind_actual_runtime_epoch(7u);
        REQUIRE(image.lookup(0x100013u, 0x100010u, 0x100030u, "unavailable", 3u, 7u).state == provenance::status::unavailable,
                "malformed object cannot grant an exact native provenance position");
        ::fast_io::io::println("actual Mach-O malformed relocation rejected: ", ::fast_io::mnp::os_c_str(name),
            " errors=", ::fast_io::mnp::dec(report.errors), " warnings=", ::fast_io::mnp::dec(report.warnings));
        return true;
    }
    [[nodiscard]] bool run(char const* path, char const* output_prefix)
    {
        ::fast_io::native_file file{::fast_io::mnp::os_c_str(path), ::fast_io::open_mode::in};
        auto const size{::fast_io::file_size(file)};
        REQUIRE(size != 0u && size <= 64u * 1024u * 1024u && size <= PTRDIFF_MAX,
                "bounded actual object file before allocation");
        ::std::vector<::std::byte> bytes(static_cast<::std::size_t>(size));
        // [owned object bytes ... size) end
        // [safe                       ] cap and PTRDIFF_MAX checked before endpoint.
        ::fast_io::operations::read_all_bytes(file, bytes.data(), bytes.data() + bytes.size());
        ::std::byte extra{};
        // [one owned extra byte] end; exact one-past endpoint is valid.
        REQUIRE(::fast_io::operations::read_some_bytes(file, &extra, &extra + 1u) == &extra,
                "file did not grow beyond bounded object copy");
        auto buffer{::llvm::MemoryBuffer::getMemBufferCopy(
            ::llvm::StringRef{reinterpret_cast<char const*>(bytes.data()), bytes.size()}, "actual-llvm-mc-macho-object")};
        auto result{::llvm::object::ObjectFile::createObjectFile(buffer->getMemBufferRef())};
        if(!result) { return error_free(result.takeError(), "actual Mach-O object parse"); }
        auto* const object{::llvm::dyn_cast<::llvm::object::MachOObjectFile>(result->get())};
        REQUIRE(object && object->is64Bit() && object->isLittleEndian() && object->isRelocatableObject() &&
                (object->getArch() == ::llvm::Triple::x86_64 || object->getArch() == ::llvm::Triple::aarch64),
                "actual little-endian x86_64/arm64 Apple relocatable object");
        sections source{};
        REQUIRE(inspect_sections(*object, source) && inspect_relocations(*object, source),
                "actual assembly layout and relocation flags checked before using resolver");
        REQUIRE(loaded_values(*object, source, 0x100000u, 0x300000u, "independent section loads") &&
                loaded_values(*object, source, 0x100000u, 1u, "P<O second section") &&
                loaded_values(*object, source, source.first.getAddress(), source.second.getAddress(), "original addresses") &&
                loaded_values(*object, source, 0u, 0u, "zero-load fallback") &&
                no_loaded_preserves_raw(*object, source), "actual Context/extractor/parser scenarios");
        auto const local{*source.ranges.relocations().begin()};
        auto const external{*source.addr.relocations().begin()};
        auto const local_word{object->getRelocation(local.getRawDataRefImpl()).r_word1};
        auto const external_word{object->getRelocation(external.getRawDataRefImpl()).r_word1};
        REQUIRE(absolute_local_preserves_raw(bytes, *object, source, local),
                "legitimate R_ABS is distinct from an invalid nonzero section ordinal");
        // SDK-decoded host bitfields specify malformed flags; fast_io writes
        // their little-endian representation. No object-endian parsing occurs
        // here, and no malformed record is ever passed directly to a resolver.
        REQUIRE(malformed_object(bytes, *object, source, source.ranges, local, local_word | (1u << 24u), "pcrel", output_prefix) &&
                malformed_object(bytes, *object, source, source.ranges, local, local_word & ~(3u << 25u), "bad-width", output_prefix) &&
                malformed_object(bytes, *object, source, source.ranges, local, (local_word & 0xff000000u) | 0x00ffffffu, "bad-local-ordinal", output_prefix) &&
                malformed_object(bytes, *object, source, source.addr, external, (external_word & 0xff000000u) | 0x00ffffffu, "bad-external-symbol", output_prefix),
                "actual malformed absolute relocation cases");
        auto const subtractor{object->getArch() == ::llvm::Triple::x86_64 ? ::llvm::MachO::X86_64_RELOC_SUBTRACTOR : ::llvm::MachO::ARM64_RELOC_SUBTRACTOR};
        REQUIRE(malformed_object(bytes, *object, source, source.ranges, local,
            (local_word & 0x0fffffffu) | (static_cast<::std::uint32_t>(subtractor) << 28u), "subtractor", output_prefix),
            "unsupported actual subtractor does not silently become an unsigned address");
        if(object->getArch() == ::llvm::Triple::aarch64)
        {
            REQUIRE(malformed_object(bytes, *object, source, source.ranges, local,
                (local_word & 0x0fffffffu) | (static_cast<::std::uint32_t>(::llvm::MachO::ARM64_RELOC_ADDEND) << 28u),
                "addend", output_prefix), "unsupported actual AArch64 ADDEND revokes whole map");
            REQUIRE(local.getOffset() <= 0x00ffffffu, "actual address fits scattered relocation field");
            auto const scattered_word{::llvm::MachO::R_SCATTERED | (3u << 28u) |
                static_cast<::std::uint32_t>(local.getOffset())};
            REQUIRE(malformed_object(bytes, *object, source, source.ranges, local, scattered_word,
                "scattered", output_prefix, true), "actual AArch64 scattered record revokes whole map");
        }
        ::fast_io::io::println("PASS actual Mach-O ", object->getArch() == ::llvm::Triple::x86_64 ? "x86_64" : "arm64",
            " relocation COMPONENT: independent text sections, real local/external flags, 64/32-bit negative addends, P<O, NoL raw fallback, actual DWARF ranges/addr parsers and malformed-image revocation; no native execution ownership claimed");
        return true;
    }
#undef REQUIRE
}
int main(int argc, char** argv)
{
    if(argc != 2 && argc != 3)
    {
        ::fast_io::io::perrln("usage: native_macho_unsigned_relocations REAL_LLVM_MC_OBJECT [FRESH_MUTATION_OUTPUT_PREFIX]");
        return 2;
    }
    return run(argv[1], argc == 3 ? argv[2] : nullptr) ? 0 : 1;
}
