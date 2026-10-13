// Real ARM ELF DATA relocation oracle. Native execution/stack authority is
// never inferred from this fixture; every inspected byte is manager-owned DATA.
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Endian.h>
#include <fast_io.h>
#include <fast_io_dsal/string_view.h>
#include <cstdint>
#include <map>
#include <string>
namespace L=::llvm;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("ARM EHABI relocation failure line=",__LINE__); return 1; } } while(false)
struct extent { ::std::uint8_t* address{}; ::std::uintptr_t size{}; };
class memory final:public L::SectionMemoryManager
{
public:
    ::std::map<::std::string,extent> sections;
    ::std::uint8_t* allocateDataSection(::std::uintptr_t n,unsigned a,unsigned id,L::StringRef name,bool ro) override
    { auto* p=SectionMemoryManager::allocateDataSection(n,a,id,name,ro);sections[name.str()]={p,n};return p; }
};
class resolver final:public L::LegacyJITSymbolResolver
{
public:
    ::std::uint32_t target{};
    L::JITSymbol findSymbolInLogicalDylib(::std::string const&) override { return nullptr; }
    L::JITSymbol findSymbol(::std::string const& name) override
    { return name=="external_typeinfo" ? L::JITSymbol{target,L::JITSymbolFlags::Exported} : L::JITSymbol{nullptr}; }
};
int main(int argc,char** argv)
{
    CHECK(argc==2 || argc==3);
    bool const unsupported{argc==3 && ::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}=="--expect-unsupported"};
    CHECK(argc==2 || unsupported);
    ::fast_io::native_file file{::fast_io::mnp::os_c_str(argv[1]),::fast_io::open_mode::in};
    ::fast_io::native_file_loader bytes{::fast_io::at(file)};CHECK(bytes.size() && bytes.size()<=65536u);
    auto object=L::object::ObjectFile::createObjectFile(L::MemoryBufferRef{
        L::StringRef{bytes.data(),bytes.size()},"owned-arm-ehabi-data-fixture"});CHECK(bool(object));
    CHECK((**object).getArch()==L::Triple::arm && (**object).isLittleEndian());
    for(unsigned phase{};phase!=4u;++phase)
    {
        memory mem;resolver symbols;symbols.target=0x16008000u+(phase==3u?0x8000u:0u);
        L::RuntimeDyld loader{mem,symbols};auto loaded{loader.loadObject(**object)};CHECK(loaded && !loader.hasError());
        ::std::uint32_t data{0x14000000u+(phase>=1u?0x20000u:0u)},got{0x15000000u+(phase>=2u?0x14000u:0u)};
        for(auto const& [name,section]:mem.sections)
        { loader.mapSectionAddress(section.address,name==".rodata"?data:name==".got"?got:0x13000000u); }
        loader.resolveRelocations();
        if(unsupported) { CHECK(loader.hasError());break; }
        CHECK(!loader.hasError());auto const& pool{mem.sections.at(".got")};auto const& ro{mem.sections.at(".rodata")};
        // The actual object, rather than the loader's global-name map, owns
        // this local symbol. Check its exact section offset independently.
        bool found{}; ::std::uint64_t local_offset{};
        for(auto const& symbol:(**object).symbols())
        {
            auto name{symbol.getName()}; CHECK(bool(name));
            if(*name!="local_typeinfo") { continue; }
            auto value{symbol.getAddress()}; CHECK(bool(value)); local_offset=*value; found=true;
        }
        CHECK(found && local_offset==20u && ro.size>=24u);
        auto const local_target{data+static_cast<::std::uint32_t>(local_offset)};
        for(auto const& [name,addend]: {::std::pair{"target2_zero",0u}, {"target2_plus",4u}, {"target2_minus",::std::uint32_t(-4)}, {"target2_local",0u}, {"got_prel",0u}})
        {
            auto const* p=static_cast<::std::uint8_t const*>(loader.getSymbolLocalAddress(name));
            CHECK(p && p>=ro.address && static_cast<::std::uintptr_t>(p-ro.address)<=ro.size && ro.size-static_cast<::std::uintptr_t>(p-ro.address)>=4u);
            auto const place{data+static_cast<::std::uint32_t>(p-ro.address)};
            auto const slot{place+L::support::endian::read32le(p)-addend};
            CHECK(slot>=got && slot-got<=pool.size && pool.size-(slot-got)>=4u);
            CHECK(L::support::endian::read32le(pool.address+(slot-got))==(::std::string_view{name}=="target2_local"?local_target:symbols.target));
        }
    }
    if(unsupported) { ::fast_io::io::println("PASS actual unsupported ARM relocation reports loader error; no native execution"); }
    else { ::fast_io::io::println("PASS actual ARM TARGET2/GOT_PREL indirect slots, signed addends, local symbol offset and independent remapping; DATA only"); }
}
