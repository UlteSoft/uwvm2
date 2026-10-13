// Real object-only producer/record probe. NO synthetic loaded addresses,
// generated execution, descriptor dereference, VM capture or native authority.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <fast_io.h>
#include <uwvm2/runtime/lib/uwvm_runtime_native_owner_table_format.h>
#include <uwvm2/runtime/lib/uwvm_runtime_native_owner_table_object_graph.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBufferRef.h>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>
namespace wire = ::uwvm2::runtime::lib::details::native_owner_table_format;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("actual native owner object FAIL line=",__LINE__); return 1; } } while(false)
int main(int argc,char** argv)
{
    CHECK(argc==2);
    ::fast_io::native_file file{argv[1],::fast_io::open_mode::in};
    auto const size{::fast_io::file_size(file)};
    CHECK(size!=0u && size<=64u*1024u*1024u && size<=PTRDIFF_MAX);
    ::fast_io::native_file_loader mapped{::fast_io::at(file)};
    CHECK(mapped.size()==size);
    // [one live immutable test object mapping ... exact size] end
    // [safe] loader/file RAII outlive ObjectFile and every borrowed section.
    auto result{::llvm::object::ObjectFile::createObjectFile(::llvm::MemoryBufferRef{
        ::llvm::StringRef{mapped.data(),mapped.size()},"actual-native-owner-object"})};
    if(!result) { ::llvm::consumeError(result.takeError()); return 2; }
    auto const& object{**result};
    namespace graph = ::uwvm2::runtime::lib::details::native_owner_object_graph;
    ::std::vector<graph::row> actual{};
    CHECK(graph::collect(object,actual) && actual.size()==4u);
    ::std::array<bool,4u> actual_seen{};
    for(auto const& row:actual)
    {
        CHECK(row.endpoints_proved && row.shape==1u && row.pointer_bytes==object.getBytesInAddress() &&
            row.begin_offset<row.end_offset && row.end_offset<=row.section_size && !row.entry_object_name.empty() &&
            (row.local_entry_object_name.empty() || row.local_entry_object_name!=row.entry_object_name));
        ::std::size_t slot{4u};
        if(row.original_ir_name=="wasm_body") { slot=0u;CHECK(row.role==1u); }
        else if(row.original_ir_name=="wasm_body.checkpoint.resume.v2") { slot=1u;CHECK(row.role==2u); }
        else if(row.original_ir_name=="wasm_body.checkpoint.resume.v2.raw") { slot=2u;CHECK(row.role==3u); }
        else if(row.original_ir_name=="unqualified_helper") { slot=3u;CHECK(row.role==0u); }
        CHECK(slot<actual_seen.size() && !actual_seen[slot]);actual_seen[slot]=true;
        unsigned entry_matches{},local_matches{};
        for(auto const& symbol:object.symbols())
        {
            auto name{symbol.getName()};
            if(!name) { ::llvm::consumeError(name.takeError());return 5; }
            bool const entry{*name==row.entry_object_name},local{!row.local_entry_object_name.empty() && *name==row.local_entry_object_name};
            if(!entry && !local) { continue; }
            auto kind{symbol.getType()};
            if(!kind) { ::llvm::consumeError(kind.takeError());return 6; }
            CHECK(*kind==::llvm::object::SymbolRef::ST_Function);
            auto selected{symbol.getSection()};
            if(!selected) { ::llvm::consumeError(selected.takeError());return 7; }
            CHECK(*selected!=object.section_end());
            // ELFv1 actual entry can be .opd DATA. Merely observing its name
            // does NOT make that section executable, readable native memory or
            // prove the descriptor graph. Only table text relocations above are
            // endpoint DATA; later canonical publisher must prove self aliases.
            if(entry) { ++entry_matches; }
            if(local) { ++local_matches;CHECK((*selected)->isText() && (*selected)->getIndex()==row.section); }
        }
        CHECK(entry_matches==1u && local_matches==(row.local_entry_object_name.empty() ? 0u:1u));
    }
    for(::std::size_t i{};i<actual.size();++i)
    {
        for(::std::size_t j{};j<i;++j)
        {
            // Real object-relative spans only; this is not a loaded-address
            // model or a grant. The four fixture definitions must not overlap.
            CHECK(actual[i].section!=actual[j].section || actual[i].end_offset<=actual[j].begin_offset ||
                actual[j].end_offset<=actual[i].begin_offset);
        }
    }
    for(bool value:actual_seen) { CHECK(value); }
    ::std::array<bool,4u> seen{};
    ::std::size_t rows{}, sections{};
    struct field { ::std::uint64_t offset{}; ::std::size_t relocations{}; };
    for(auto const& section:object.sections())
    {
        auto name{section.getName()};
        if(!name) { ::llvm::consumeError(name.takeError()); return 3; }
        if(*name!=".uwvm.native.owners" && *name!=".uwvm$NO" && *name!="__uwvm_nowners") { continue; }
        CHECK(++sections==1u && !section.isText());
        auto bytes{section.getContents()};
        if(!bytes) { ::llvm::consumeError(bytes.takeError()); return 4; }
        CHECK(!bytes->empty() && bytes->size()<=64u*1024u*1024u && bytes->size()<=PTRDIFF_MAX);
        ::std::vector<field> fields{};
        ::std::size_t offset{};
        while(offset<bytes->size())
        {
            CHECK(rows<4u && offset<=bytes->size() && bytes->size()-offset>=wire::fixed_bytes);
            // [actual section bytes0..offset][remaining same section] end
            // [safe] remaining length proved BEFORE suffix pointer/span creation.
            auto const* data{reinterpret_cast<unsigned char const*>(bytes->data())+offset};
            ::std::span<unsigned char const> span{data,bytes->size()-offset};
            wire::record row{};
            CHECK(wire::decode(span,object.getBytesInAddress(),object.isLittleEndian(),row));
            CHECK(row.continuous_shape==1u && row.bytes!=0u && row.bytes<=span.size() && !row.entry_object_name.empty());
            // The names are actual AsmPrinter DATA, not a manual platform prefix.
            // The object probe still does not authenticate an owner/alias/PC.
            CHECK(row.local_entry_object_name.empty() || row.local_entry_object_name!=row.entry_object_name);
            ::std::size_t selected{4u};
            if(row.original_ir_name=="wasm_body") { selected=0u; CHECK(row.role==1u); }
            else if(row.original_ir_name=="wasm_body.checkpoint.resume.v2") { selected=1u; CHECK(row.role==2u); }
            else if(row.original_ir_name=="wasm_body.checkpoint.resume.v2.raw") { selected=2u; CHECK(row.role==3u); }
            else if(row.original_ir_name=="unqualified_helper") { selected=3u; CHECK(row.role==0u); }
            CHECK(selected<seen.size() && !seen[selected]);seen[selected]=true;
            CHECK(offset<=UINT64_MAX-row.end_relocation_offset);
            fields.push_back({offset+row.begin_relocation_offset,0u});
            fields.push_back({offset+row.end_relocation_offset,0u});
            // [consumed exact row][remaining same section] end
            // [safe] nonzero row<=remaining precedes offset advance; no byte pointer retained.
            offset+=row.bytes;++rows;
        }
        auto observe=[&](auto const& relocation_section)
        {
            for(auto const relocation:relocation_section.relocations())
            {
                auto const position{relocation.getOffset()};
                for(auto& target:fields)
                {
                    if(position==target.offset) { ++target.relocations; }
                }
            }
        };
        // Some formats expose relocations directly on data; ELF uses a linked
        // relocation section. Select the actual same-object target relationship.
        observe(section);
        for(auto const& relocation_section:object.sections())
        {
            if(relocation_section.getIndex()==section.getIndex()) { continue; }
            auto target{relocation_section.getRelocatedSection()};
            if(!target) { ::llvm::consumeError(target.takeError()); continue; }
            if(*target!=object.section_end() && (*target)->getIndex()==section.getIndex()) { observe(relocation_section); }
        }
        for(auto const& target:fields)
        {
            // DATA evidence only: ADDEND/pair forms may occupy the same slot.
            // A production owner needs a typed allowlisted actual relocation
            // graph, same loaded text section, addend/overflow bounds and the
            // canonical runtime publisher. This probe does NOT supply that.
            CHECK(target.relocations>=1u && target.relocations<=2u);
        }
    }
    CHECK(sections==1u && rows==4u);
    for(bool value:seen) { CHECK(value); }
    ::fast_io::io::println("PASS actual object owner records=",::fast_io::mnp::dec(rows),
        " pointer-bytes=",::fast_io::mnp::dec(object.getBytesInAddress()),
        " endian=",::fast_io::mnp::cond(object.isLittleEndian(),"little","big"),
        " reloc-fields-observed=true actual-same-object-text-offsets=true generated-execution=false loaded-ownership-qualified=false");
}
#undef CHECK
