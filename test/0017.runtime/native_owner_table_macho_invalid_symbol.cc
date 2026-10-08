// Actual patched-LLVM Mach-O object mutation. Only owned file bytes change;
// no loaded address, VM memory, generated execution or synthetic owner exists.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <uwvm2/runtime/lib/uwvm_runtime_native_owner_table_object_graph.h>
#include <fast_io.h>
#include <llvm/Support/MemoryBufferRef.h>
#include <cstring>
#include <vector>
namespace graph=::uwvm2::runtime::lib::details::native_owner_object_graph;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("actual Mach-O owner mutation FAIL line=",__LINE__);return 1; } } while(false)
int main(int argc,char** argv)
{
    CHECK(argc==2);
    ::fast_io::native_file file{argv[1],::fast_io::open_mode::in};
    auto const size{::fast_io::file_size(file)};
    CHECK(size!=0u && size<=64u*1024u*1024u && size<=PTRDIFF_MAX);
    ::fast_io::native_file_loader mapped{::fast_io::at(file)};CHECK(mapped.size()==size);
    // [live actual object mapping ... checked size][one owned copy]
    // [safe] mapping/file outlive ObjectFile; destination is exactly same size.
    ::std::vector<unsigned char> bytes(mapped.size());
    ::std::memcpy(bytes.data(),mapped.data(),mapped.size());
    auto original{::llvm::object::ObjectFile::createObjectFile(::llvm::MemoryBufferRef{
        ::llvm::StringRef{mapped.data(),mapped.size()},"actual-owner-Mach-O"})};
    if(!original) { ::llvm::consumeError(original.takeError());return 2; }
    auto const* macho{::llvm::dyn_cast<::llvm::object::MachOObjectFile>(original->get())};CHECK(macho!=nullptr);
    ::std::vector<graph::row> valid{};CHECK(graph::collect(*macho,valid) && valid.size()==4u);
    auto const table{macho->getSymtabLoadCommand()};CHECK(table.nsyms>0u && table.nsyms<0xFFFFFFu);
    ::std::size_t where{};::std::uint32_t word{};bool found{};
    for(auto const& section:macho->sections())
    {
        auto name{section.getName()};if(!name) { ::llvm::consumeError(name.takeError());return 3; }
        if(*name!="__uwvm_nowners") { continue; }
        ::std::uint32_t relocation_offset{},relocation_count{};
        if(macho->is64Bit())
        { auto const header{macho->getSection64(section.getRawDataRefImpl())};relocation_offset=header.reloff;relocation_count=header.nreloc; }
        else
        { auto const header{macho->getSection(section.getRawDataRefImpl())};relocation_offset=header.reloff;relocation_count=header.nreloc; }
        CHECK(relocation_offset<=bytes.size() && relocation_count<=(bytes.size()-relocation_offset)/8u);
        for(auto const& relocation:section.relocations())
        {
            auto const info{macho->getRelocation(relocation.getRawDataRefImpl())};
            if(macho->isRelocationScattered(info) || !macho->getPlainRelocationExternal(info)) { continue; }
            auto const index{relocation.getRawDataRefImpl()};
            CHECK(index.d.a==section.getRawDataRefImpl().d.a && index.d.b<relocation_count);
            // [actual file relocation table][checked actual index][8-byte row]
            // [safe] count<=remaining/8 and index<count before scalar arithmetic.
            where=static_cast<::std::size_t>(relocation_offset)+static_cast<::std::size_t>(index.d.b)*8u+4u;
            CHECK(where<=bytes.size() && 4u<=bytes.size()-where);
            word=macho->isLittleEndian() ? (info.r_word1&0xFF000000u)|table.nsyms :
                (info.r_word1&0xFFu)|(table.nsyms<<8u);
            found=true;break;
        }
    }
    CHECK(found);
    // [owned actual bytes ... where][exact four-byte relocation word][end]
    // [safe] complete width within owned file before either pointer forms.
    auto* first{bytes.data()+where};auto* end{first+4u};
    ::fast_io::basic_obuffer_view<unsigned char> output{first,end};
    if(macho->isLittleEndian()) { ::fast_io::io::print(output,::fast_io::mnp::le_put<32>(word)); }
    else { ::fast_io::io::print(output,::fast_io::mnp::be_put<32>(word)); }
    CHECK(output.curr_ptr==end);
    auto malformed{::llvm::object::ObjectFile::createObjectFile(::llvm::MemoryBufferRef{
        ::llvm::StringRef{reinterpret_cast<char const*>(bytes.data()),bytes.size()},"actual-owner-Mach-O-invalid-index"})};
    if(!malformed) { ::llvm::consumeError(malformed.takeError());return 4; }
    // Constructor acceptance is intentional: original table extent is intact,
    // only relocation's external symbol number is exactly one past actual nsyms.
    ::std::vector<graph::row> declined{};
    CHECK(!graph::collect(**malformed,declined) && declined.empty());
    ::fast_io::io::println("PASS actual Mach-O relocation index=nsyms declined before SDK pointer; loaded-ownership-qualified=false");
}
#undef CHECK
