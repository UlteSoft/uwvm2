// Cross-format relocated DWARF COMPONENT. Read ELF/COFF/Mach-O objects emitted
// from native_provenance_sections_metadata.cc by actual llc. Synthetic section loads are
// scalar probe data, never proof of runtime executable/native-trap ownership.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <uwvm2/runtime/lib/uwvm_runtime_native_provenance_rows.h>
#include <llvm/Object/SymbolSize.h>
#include <llvm/Support/MemoryBuffer.h>
#include <algorithm>
#include <array>
#include <memory>
#include <vector>
#include <fast_io.h>

namespace provenance = ::uwvm2::runtime::lib::details::native_loaded_provenance;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("loaded native provenance object failure line=", __LINE__); return 1; } } while(false)

class probe_loaded final : public ::llvm::LoadedObjectInfo
{
    struct section { ::std::uint64_t index{}, base{}; };
    ::std::vector<section> sections{};
public:
    explicit probe_loaded(::llvm::object::ObjectFile const& object)
    {
        ::std::uint64_t slot{};
        for(auto const& section: object.sections())
        {
            if(!section.isText() || section.getSize() == 0u) { continue; }
            if(slot == 128u || section.getSize() >= 0x100000u) { ::fast_io::fast_terminate(); }
            // Each real object section has a distinct bounded synthetic load.
            // No single image slide can accidentally make this probe PASS.
            sections.push_back({section.getIndex(), 0x100000u + slot * 0x100000u});
            ++slot;
        }
    }
    ::std::uint64_t getSectionLoadAddress(::llvm::object::SectionRef const& section) const override
    {
        for(auto const item: sections) { if(item.index == section.getIndex()) { return item.base; } }
        return 0u;
    }
    ::std::unique_ptr<::llvm::LoadedObjectInfo> clone() const override
    { return ::std::make_unique<probe_loaded>(*this); }
};

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    provenance::image image{};
    ::std::uintptr_t begin{}, end{};
    provenance::position retained{};
    ::std::string_view retained_identity{}; ::std::size_t retained_extent{};
    ::std::array<bool, 2u> found{};
    ::std::array<::std::uint64_t, 2u> section_indices{};
    {
        ::fast_io::native_file file{argv[1], ::fast_io::open_mode::in};
        auto const length{::fast_io::file_size(file)};
        CHECK(length != 0u && length <= 64u * 1024u * 1024u && length <= PTRDIFF_MAX);
        ::std::vector<::std::byte> bytes(static_cast<::std::size_t>(length));
        // [owned bytes.data ... bytes.size) end
        // [safe                              ] bounded length precedes allocation;
        //  ^^ one-past read endpoint belongs to this live owned vector.
        ::fast_io::operations::read_all_bytes(file, bytes.data(), bytes.data() + bytes.size());
        ::std::byte extra{};
        // [one owned byte ... one-past) end
        // [safe                       ] probe only this fixed bounded byte.
        CHECK(::fast_io::operations::read_some_bytes(file, &extra, &extra + 1u) == &extra);
        auto buffer{::llvm::MemoryBuffer::getMemBufferCopy(
            ::llvm::StringRef{reinterpret_cast<char const*>(bytes.data()), bytes.size()}, "native-provenance-object")};
        auto object_result{::llvm::object::ObjectFile::createObjectFile(buffer->getMemBufferRef())};
        if(!object_result) { ::llvm::consumeError(object_result.takeError()); return 2; }
        auto const& object{**object_result};
        probe_loaded loaded{object};
        CHECK(image.observe(1u, object, loaded) && image.valid() && image.row_count() != 0u);
        image.bind_actual_runtime_epoch(7u);
        for(auto const& symbol_size: ::llvm::object::computeSymbolSizes(object))
        {
            auto name{symbol_size.first.getName()};
            if(!name) { ::llvm::consumeError(name.takeError()); continue; }
            ::std::size_t selected{};
            if(name->ends_with("native_provenance_first")) { selected = 0u; }
            else if(name->ends_with("native_provenance_second")) { selected = 1u; }
            else { continue; }
            CHECK(!found[selected] && symbol_size.second != 0u);
            auto const identity{selected == 0u ? "uwvm-m2-f3-g4.wasm-native-v1" : "uwvm-m5-f7-g9.wasm-native-v1"};
            auto const extent{selected == 0u ? 3u : 5u};
            auto section_result{symbol_size.first.getSection()};
            if(!section_result) { ::llvm::consumeError(section_result.takeError()); continue; }
            auto const section{*section_result};
            if(section == object.section_end() || !section->isText()) { continue; }
            auto address{symbol_size.first.getAddress()};
            if(!address) { ::llvm::consumeError(address.takeError()); continue; }
            auto const source{section->getAddress()}, size{section->getSize()}, base{loaded.getSectionLoadAddress(*section)};
            if(*address < source) { continue; }
            auto const offset{*address - source};
            CHECK(offset != 0u); // Fixture padding makes section-relative addends observable.
            if(offset >= size || symbol_size.second > size - offset || base == 0u ||
               base > UINTPTR_MAX || offset > UINTPTR_MAX - base || symbol_size.second > UINTPTR_MAX - base - offset) { continue; }
            begin = static_cast<::std::uintptr_t>(base + offset); end = begin + symbol_size.second;
            CHECK(end > begin && end - begin <= 65536u);
            for(auto pc{begin}; pc < end; ++pc)
            {
                auto const position{image.lookup(pc, begin, end, identity, extent, 7u)};
                CHECK(position.state != provenance::status::unavailable);
                if(position.state == provenance::status::exact)
                {
                    CHECK((selected == 0u ? (position.wasm_offset == 0u || position.wasm_offset == 2u) :
                                             (position.wasm_offset == 1u || position.wasm_offset == 4u)) &&
                          position.begin <= pc && pc < position.end && position.begin >= begin && position.end <= end);
                    retained = position; retained_identity = identity; retained_extent = extent; found[selected] = true; section_indices[selected] = section->getIndex(); break;
                }
            }
            CHECK(found[selected]);
        }
        CHECK(found[0u] && found[1u] && section_indices[0u] != section_indices[1u]);
        CHECK(image.lookup(retained.begin, begin, end, retained_identity, retained_extent, 8u).state == provenance::status::unavailable);
        CHECK(image.lookup(end, begin, end, retained_identity, retained_extent, 7u).state == provenance::status::unavailable);
    } // Actual object, LLVM context and loaded-section mock are all retired.
    auto const still_owned{image.lookup(retained.begin, begin, end, retained_identity, retained_extent, 7u)};
    CHECK(still_owned.state == provenance::status::exact && still_owned.wasm_offset == retained.wasm_offset);
    image.invalidate_runtime_generation();
    CHECK(image.lookup(retained.begin, begin, end, retained_identity, retained_extent, 7u).state == provenance::status::unavailable);
    image.bind_actual_runtime_epoch(8u); // Old rows cannot be revived after restore/reset.
    CHECK(!image.valid() && retained.state == provenance::status::exact);
    ::fast_io::io::println("PASS relocated object native provenance component, independent section loads, LLVM borrow retirement, bounds/epoch invalidation; native execution ownership is separate");
}
