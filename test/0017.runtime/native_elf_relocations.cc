// RuntimeDyld DATA oracle: real target ELF objects and remapped load addresses.
// No object bytes execute here; the separate QEMU MCJIT/Wasm witnesses do that.
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Endian.h>
#include <llvm/Support/MathExtras.h>
#include <fast_io.h>
#include <fast_io_dsal/string_view.h>
#include <cstdint>
#include <map>
#include <string>
namespace L=::llvm;
static void check(bool value) { if(!value) { ::fast_io::io::perrln("native ELF relocation DATA mismatch"); ::fast_io::fast_terminate(); } }
class memory final:public L::SectionMemoryManager
{
public:
    struct allocation { unsigned id; ::std::uint8_t* bytes; };
    ::std::map<::std::string,allocation> sections;
    ::std::uint8_t* allocateCodeSection(::std::uintptr_t size,unsigned alignment,unsigned id,L::StringRef name) override
    { auto* p=SectionMemoryManager::allocateCodeSection(size,alignment,id,name); sections[name.str()]={id,p};return p; }
    ::std::uint8_t* allocateDataSection(::std::uintptr_t size,unsigned alignment,unsigned id,L::StringRef name,bool readonly) override
    { auto* p=SectionMemoryManager::allocateDataSection(size,alignment,id,name,readonly); sections[name.str()]={id,p};return p; }
};
class resolver final:public L::LegacyJITSymbolResolver
{
public:
    ::std::uint64_t target;
    explicit resolver(::std::uint64_t value):target(value){}
    L::JITSymbol findSymbolInLogicalDylib(::std::string const&) override { return nullptr; }
    L::JITSymbol findSymbol(::std::string const& name) override
    { check(name=="external_target");return L::JITSymbol{target,L::JITSymbolFlags::Exported}; }
};
static ::std::uint64_t call(L::RuntimeDyld& loader,memory& mem,char const* name,::std::uint64_t wanted,::std::uint64_t base,bool ppc)
{
    auto* bytes=static_cast<::std::uint8_t*>(loader.getSymbolLocalAddress(name));check(bytes);
    auto pc=loader.getSymbol(name).getAddress();auto word=L::support::endian::read32be(bytes);
    if(ppc) { check((word&0xfc000003u)==0x48000001u); }
    else { check((word&0xc0000000u)==0x40000000u); }
    auto delta=ppc?L::SignExtend64<26>(word&0x03fffffc):L::SignExtend64<32>(::std::uint64_t(word&0x3fffffffu)<<2);
    auto destination=pc+delta;
    if(destination==wanted)return destination;
    check(destination>=base && destination-base<4096u && !(destination&3u));
    auto* stub=mem.sections.at(".text").bytes+(destination-base);
    if(ppc)
    {
        auto hi=L::support::endian::read32be(stub);auto lo=L::support::endian::read32be(stub+4u);
        check((hi&0xffff0000u)==0x3d800000u && (lo&0xffff0000u)==0x618c0000u);
        check(((::std::uint64_t(hi&0xffffu)<<16)|(lo&0xffffu))==wanted);
        check(L::support::endian::read32be(stub+8u)==0x7d8903a6u && L::support::endian::read32be(stub+12u)==0x4e800420u);
    }
    else
    {
        auto hh=L::support::endian::read32be(stub), hm=L::support::endian::read32be(stub+4u);
        auto lm=L::support::endian::read32be(stub+12u),lo=L::support::endian::read32be(stub+16u);
        check((hh&0xffc00000u)==0x03000000u && (hm&0xfffffc00u)==0x82106000u);
        check((lm&0xffc00000u)==0x0b000000u && (lo&0xfffffc00u)==0x8a116000u);
        check(((::std::uint64_t(hh&0x3fffffu)<<42)|(::std::uint64_t(hm&0x3ffu)<<32)|(::std::uint64_t(lm&0x3fffffu)<<10)|(lo&0x3ffu))==wanted);
        check(L::support::endian::read32be(stub+8u)==0x83287020u && L::support::endian::read32be(stub+20u)==0x82104005u);
        check(L::support::endian::read32be(stub+24u)==0x81c06000u && L::support::endian::read32be(stub+28u)==0x01000000u);
    }
    return destination;
}
int main(int argc,char** argv)
{
    check(argc==2 || argc==3);
    ::fast_io::native_file file{::fast_io::mnp::os_c_str(argv[1]),::fast_io::open_mode::in};
    auto size=::fast_io::file_size(file);check(size && size<=1048576u);
    ::fast_io::native_file_loader mapped{::fast_io::at(file)};check(mapped.size()==size);
    auto object=L::object::ObjectFile::createObjectFile(L::MemoryBufferRef{L::StringRef{mapped.data(),mapped.size()},"native-elf-relocation-oracle"});
    check(bool(object));bool ppc=(**object).getArch()==L::Triple::ppc;check(ppc || (**object).getArch()==L::Triple::sparcv9);
    // 'overflow' is a negative control, required to fail even under NDEBUG.
    bool bad=argc==3; if(bad)check(::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}=="overflow");
    for(::std::uint64_t base: {UINT64_C(0x10000000),UINT64_C(0x50000000)})
    {
        auto target=ppc?UINT64_C(0xf12389ac):UINT64_C(0x123456789abcdef0);
        if(bad)target=UINT64_C(0x10000000000);
        memory mem;resolver symbols{target};L::RuntimeDyld loader{mem,symbols};
        auto loaded=loader.loadObject(**object);check(loaded && !loader.hasError());
        auto other=ppc?base+0x10000000u:base+(UINT64_C(1)<<40);
        for(auto const& [name,section]:mem.sections)
        { loader.mapSectionAddress(section.bytes,name==".text"?base:name==".text.other"?other:base+0x10000u); }
        loader.resolveRelocations();check(!loader.hasError());
        if(bad) { ::fast_io::io::perrln("ERROR overflow accepted");return 1; }
        auto a=call(loader,mem,"call_external",target,base,ppc);
        check(call(loader,mem,"call_external_again",target,base,ppc)==a);
        call(loader,mem,"call_addend",target+16u,base,ppc);
        check(call(loader,mem,"call_local",loader.getSymbol("local_target").getAddress(),base,ppc)==loader.getSymbol("local_target").getAddress());
        call(loader,mem,"call_cross",other,base,ppc);
        auto* data=static_cast<::std::uint8_t*>(loader.getSymbolLocalAddress("address_data"));check(data);
        check((ppc?L::support::endian::read32be(data):L::support::endian::read64be(data))==target+16u);
        auto* relative=static_cast<::std::uint8_t*>(loader.getSymbolLocalAddress("relative_data"));check(relative);
        auto expected=loader.getSymbol("local_target").getAddress()-loader.getSymbol("relative_data").getAddress();
        check(ppc?L::support::endian::read32be(relative)==::std::uint32_t(expected):L::support::endian::read64be(relative)==expected);
    }
    ::fast_io::io::println("PASS actual ELF remapping, same-section calls, far cross-section/external stubs, stub reuse, single addend, data/PC-relative relocation; DATA only");
}
