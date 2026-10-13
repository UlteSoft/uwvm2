// Actual ELF DATA relocation oracle. Remaps both sides of a PPC64 TOC
// displacement; it neither executes these object bytes nor exposes VM memory.
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Endian.h>
#include <fast_io.h>
#include <cstdint>
#include <map>
#include <string>
namespace L=::llvm;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("PPC64 TOC relocation failure line=",__LINE__); return 1; } } while(false)
class memory final:public L::SectionMemoryManager
{
public:
    ::std::map<::std::string,::std::uint8_t*> sections;
    ::std::uint8_t* allocateCodeSection(::std::uintptr_t n,unsigned a,unsigned id,L::StringRef name) override
    { auto* p=SectionMemoryManager::allocateCodeSection(n,a,id,name);sections[name.str()]=p;return p; }
    ::std::uint8_t* allocateDataSection(::std::uintptr_t n,unsigned a,unsigned id,L::StringRef name,bool ro) override
    { auto* p=SectionMemoryManager::allocateDataSection(n,a,id,name,ro);sections[name.str()]=p;return p; }
};
class resolver final:public L::LegacyJITSymbolResolver
{
    L::JITSymbol findSymbolInLogicalDylib(::std::string const&) override { return nullptr; }
    L::JITSymbol findSymbol(::std::string const&) override { return nullptr; }
};
int main(int argc,char** argv)
{
    CHECK(argc==2);
    ::fast_io::native_file file{::fast_io::mnp::os_c_str(argv[1]),::fast_io::open_mode::in};
    ::fast_io::native_file_loader bytes{::fast_io::at(file)};CHECK(bytes.size() && bytes.size()<=65536u);
    auto object=L::object::ObjectFile::createObjectFile(L::MemoryBufferRef{
        L::StringRef{bytes.data(),bytes.size()},"owned-ppc64-toc-fixture"});CHECK(bool(object));
    auto const arch{(**object).getArch()};bool const little{arch==L::Triple::ppc64le};CHECK(little || arch==L::Triple::ppc64);
    for(unsigned negative: {0u,1u,2u})
    for(unsigned phase{};phase!=3u;++phase)
    {
        memory mem;resolver symbols;L::RuntimeDyld loader{mem,symbols};
        auto loaded{loader.loadObject(**object)};CHECK(loaded && !loader.hasError());
        ::std::uint64_t toc{0x10010000u},data{0x10218000u};
        {
            if(phase>=1u) { toc+=0x20000u; }
            if(phase>=2u) { data+=0x14000u; }
            if(negative==1u) { data=toc+(UINT64_C(1)<<40); }
            if(negative==2u) { data|=2u; }
            for(auto const& [name,address]:mem.sections)
            { loader.mapSectionAddress(address,name==".toc"?0x10010000u:name==".rodata"?0x10218000u:0x10000000u); }
            // Change the bases independently before final resolution, which is
            // the RuntimeDyld mapping contract. No remapping after finalization.
            loader.mapSectionAddress(mem.sections.at(".toc"),toc);
            loader.mapSectionAddress(mem.sections.at(".rodata"),data);
            loader.resolveRelocations();
            if(negative) { CHECK(loader.hasError());break; }
            CHECK(!loader.hasError());
            auto const offset{data-toc-0x8000u};
            auto const immediate{[&](char const* name)->unsigned
            {
                auto* p=static_cast<::std::uint8_t*>(loader.getSymbolLocalAddress(name));
                if(!p) { return 0x10000u; }
                return (little?L::support::endian::read32le(p):L::support::endian::read32be(p))&0xffffu;
            }};
            CHECK(immediate("toc_pool_ha")==((offset+0x8000u)>>16u&0xffffu));
            CHECK(immediate("toc_pool_lo")==(offset&0xffffu));
            CHECK(immediate("toc_pool_lo_ds")==(offset&0xfffcu));
            CHECK(immediate("toc_near16")==0x8000u);
            CHECK(immediate("toc_near_ds")==0x8000u);
        }
    }
    ::fast_io::io::println("PASS actual PPC64 TOC cross-section offsets, carry, independent remapping, overflow and DS-alignment rejection; DATA only");
}
