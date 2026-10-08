// Cold real-object component: load an actual llvm-mc ELF object through
// RuntimeDyld and borrow its genuine LoadedObjectInfo. Never execute foreign
// code, read descriptor native pointers, publish ranges or issue VM authority.
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <fast_io.h>
#include <llvm/ExecutionEngine/RuntimeDyld.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/SymbolSize.h>
#include <llvm/Support/MemoryBuffer.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/lib/uwvm_runtime_loaded_function_ranges.h>
#if !defined(UWVM_RUNTIME_LLVM_JIT)
#error "Use the configured real LLVM backend; do not fake derived macros"
#endif
int main(int argc,char** argv)
{
    if(argc!=3) { return 1; }
    auto const expected{::std::string_view{argv[2]}};
    if(expected!="accept" && expected!="reject") { return 2; }
    ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(argv[1]),::fast_io::open_mode::in};
    if(input.size()==0u || input.size()>static_cast<::std::size_t>(PTRDIFF_MAX)) { return 3; }
    // [actual FastIO mapping first...size] end
    // [safe] native loader owns the complete nonempty bounded image; LLVM's
    // buffer copy pins its actual bytes before any object/section borrow.
    auto buffer{::llvm::MemoryBuffer::getMemBufferCopy(::llvm::StringRef{
        reinterpret_cast<char const*>(input.data()),input.size()},argv[1])};
    auto parsed{::llvm::object::ObjectFile::createObjectFile(buffer->getMemBufferRef())};
    if(!parsed) { ::llvm::consumeError(parsed.takeError()); return 4; }
    auto object{::std::move(*parsed)};
    auto const* elf{::llvm::dyn_cast<::llvm::object::ELFObjectFileBase>(object.get())};
    if(elf==nullptr || !elf->is64Bit() || elf->getEMachine()!=::llvm::ELF::EM_PPC64 || elf->getEType()!=::llvm::ELF::ET_REL) { return 5; }
    ::std::optional<::llvm::object::SymbolRef> descriptor{},body{};
    ::std::size_t text_function_count{};
    // [actual owned object's symbols begin...end]
    // [safe] range iteration advances only non-end actual object iterators.
    for(auto const& symbol:object->symbols())
    {
        auto name{symbol.getName()};if(!name) { ::llvm::consumeError(name.takeError()); return 6; }
        if(*name=="owned_ppc64_v1") { if(descriptor) { return 7; }descriptor=symbol; }
        if(*name=="owned_ppc64_body") { if(body) { return 8; }body=symbol; }
        auto type{symbol.getType()};if(!type) { ::llvm::consumeError(type.takeError()); return 9; }
        if(*type!=::llvm::object::SymbolRef::ST_Function) { continue; }
        auto section{symbol.getSection()};if(!section) { ::llvm::consumeError(section.takeError()); return 10; }
        if(*section!=object->section_end() && (*section)->isText()) { ++text_function_count; }
    }
    // Fixtures deliberately contain no STT_FUNC in text, reproducing the real
    // ELFv1 descriptor representation rather than masking it with a text alias.
    if(!descriptor || !body || text_function_count!=0u || ::llvm::object::ELFSymbolRef{*body}.getSize()!=8u) { return 11; }
    auto body_section{body->getSection()};if(!body_section) { ::llvm::consumeError(body_section.takeError()); return 12; }
    if(*body_section==object->section_end() || !(*body_section)->isText()) { return 13; }
    auto address{body->getAddress()};if(!address) { ::llvm::consumeError(address.takeError()); return 14; }
    auto const base{(*body_section)->getAddress()},extent{(*body_section)->getSize()};
    if(*address<base) { return 15; }auto const offset{*address-base};
    if(offset>extent || 8u>extent-offset) { return 16; }
    // Actual section allocation/relocation bookkeeping, no synthesized subclass
    // or manually provided section address. Foreign instructions are not called.
    ::llvm::SectionMemoryManager memory{};
    ::llvm::RuntimeDyld dyld{memory,memory};
    auto loaded{dyld.loadObject(*object)};
    if(!loaded || dyld.hasError()) { return 17; }
    auto const relocated{loaded->getSectionLoadAddress(**body_section)};
    constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
    if(relocated==0u || relocated>limit || offset>limit-relocated) { return 18; }
    auto const wanted{relocated+offset};if(8u>limit-wanted) { return 19; }
    auto const symbol_size{::llvm::object::ELFSymbolRef{*descriptor}.getSize()};
    ::uwvm2::runtime::lib::details::ppc64_elfv1_relocation_index index{*object};
    auto const observed{::uwvm2::runtime::lib::details::get_ppc64_elfv1_loaded_function_range(*object,*descriptor,*loaded,symbol_size,index)};
    // Genuine second object identity, not a fabricated object/address row.
    // Its index must not be borrowed with the first object's actual symbol.
    auto foreign{::llvm::object::ObjectFile::createObjectFile(buffer->getMemBufferRef())};
    if(!foreign) { ::llvm::consumeError(foreign.takeError()); return 22; }
    ::uwvm2::runtime::lib::details::ppc64_elfv1_relocation_index foreign_index{**foreign};
    auto const mismatch{::uwvm2::runtime::lib::details::get_ppc64_elfv1_loaded_function_range(*object,*descriptor,*loaded,symbol_size,foreign_index)};
    if(mismatch.begin!=0u || mismatch.size!=0u) { return 23; }
    ::std::size_t count{};::std::uintptr_t collected{},collected_size{};
    ::uwvm2::runtime::lib::details::for_each_llvm_jit_loaded_function_range(*object,*loaded,
        [&](::std::uintptr_t entry,::std::uintptr_t bytes) { ++count;collected=entry;collected_size=bytes; });
    if(expected=="accept")
    {
        if(observed.begin!=wanted || observed.size!=8u || count!=1u || collected!=wanted || collected_size!=8u) { return 20; }
    }
    else if(observed.begin!=0u || observed.size!=0u || count!=0u) { return 21; }
    ::fast_io::println("ppc64 ELF object range ",expected,"; endian=",elf->isLittleEndian()?"little":"big",
        "; observed-functions=",::fast_io::mnp::dec(count));
    return 0;
}
#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <uwvm2/utils/macro/pop_macros.h>
