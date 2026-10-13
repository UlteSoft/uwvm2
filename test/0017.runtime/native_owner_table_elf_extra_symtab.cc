// Real patched-LLVM ELF64LE fixture -> actual extra DYNSYM table mutation.
// File DATA only; no loaded/native address, generated execution or permission.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <uwvm2/runtime/lib/uwvm_runtime_native_owner_function_claims.h>
#include <llvm/Support/MemoryBufferRef.h>
#include <fast_io.h>
#include <cstddef>
#include <cstring>
#include <vector>
namespace ledger=::uwvm2::runtime::lib::details::native_owner_function_claims;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("actual extra ELF symtab FAIL line=",__LINE__);return 1; } } while(false)
template<unsigned Bits>
[[nodiscard]] static bool put(::std::vector<unsigned char>& bytes,::std::size_t offset,::std::uint64_t value)
{
    constexpr ::std::size_t width{Bits/8u};
    if(offset>bytes.size() || width>bytes.size()-offset) { return false; }
    // [owned mutated actual file ... offset][complete width field] end
    // [safe] complete width is within the owned vector before pointer formation.
    auto* first{bytes.data()+offset};auto* last{first+width};
    ::fast_io::basic_obuffer_view<unsigned char> output{first,last};
    ::fast_io::io::print(output,::fast_io::mnp::le_put<Bits>(value));
    return output.curr_ptr==last;
}
int main(int argc,char** argv)
{
    CHECK(argc==2);
    ::fast_io::native_file file{argv[1],::fast_io::open_mode::in};auto const size{::fast_io::file_size(file)};
    CHECK(size!=0u && size<=64u*1024u*1024u && size<=PTRDIFF_MAX);
    ::fast_io::native_file_loader mapped{::fast_io::at(file)};CHECK(mapped.size()==size);
    auto original{::llvm::object::ObjectFile::createObjectFile(::llvm::MemoryBufferRef{
        ::llvm::StringRef{mapped.data(),mapped.size()},"actual-owner-ELF64LE"})};
    if(!original) { ::llvm::consumeError(original.takeError());return 2; }
    auto const* object{::llvm::dyn_cast<::llvm::object::ELF64LEObjectFile>(original->get())};CHECK(object!=nullptr);
    ledger::image valid{};CHECK(ledger::collect(*object,valid) && valid.endpoints_unambiguous() && valid.bodies.size()==4u);
    auto const& elf{object->getELFFile()};auto sections{elf.sections()};
    if(!sections) { ::llvm::consumeError(sections.takeError());return 3; }
    using header=::llvm::object::ELF64LE::Ehdr;using section=::llvm::object::ELF64LE::Shdr;
    auto const& eh{elf.getHeader()};auto const count{sections->size()};
    CHECK(count>0u && count<4096u && eh.e_shnum==count && eh.e_shentsize==sizeof(section));
    auto const old_offset{static_cast<::std::size_t>(eh.e_shoff)};
    CHECK(old_offset<=mapped.size() && count<=(mapped.size()-old_offset)/sizeof(section));
    ::std::size_t symtab{count};unsigned ordinary{},dynamic{};
    for(::std::size_t i{};i<count;++i)
    {
        auto const kind{(*sections)[i].sh_type};
        if(kind==::llvm::ELF::SHT_SYMTAB) { symtab=i;++ordinary; }
        else if(kind==::llvm::ELF::SHT_DYNSYM) { ++dynamic; }
    }
    CHECK(ordinary==1u && dynamic==0u && symtab<count);
    auto const new_offset{(mapped.size()+7u)&~::std::size_t{7u}};
    auto const new_table_bytes{(count+1u)*sizeof(section)};
    CHECK(new_offset<=64u*1024u*1024u && new_table_bytes<=64u*1024u*1024u-new_offset);
    ::std::vector<unsigned char> bytes(new_offset+new_table_bytes);
    // [complete actual file map][exact sized owned copy]
    // [safe] both mappings/copies live until ObjectFile destruction; old/new
    // tables were fully bounded before deriving these file byte pointers.
    ::std::memcpy(bytes.data(),mapped.data(),mapped.size());
    ::std::memcpy(bytes.data()+new_offset,mapped.data()+old_offset,count*sizeof(section));
    ::std::memcpy(bytes.data()+new_offset+count*sizeof(section),
        mapped.data()+old_offset+symtab*sizeof(section),sizeof(section));
    CHECK(put<64u>(bytes,offsetof(header,e_shoff),new_offset));
    CHECK(put<16u>(bytes,offsetof(header,e_shnum),count+1u));
    CHECK(put<32u>(bytes,new_offset+count*sizeof(section)+offsetof(section,sh_type),::llvm::ELF::SHT_DYNSYM));
    auto malformed{::llvm::object::ObjectFile::createObjectFile(::llvm::MemoryBufferRef{
        ::llvm::StringRef{reinterpret_cast<char const*>(bytes.data()),bytes.size()},"actual-owner-ELF64LE-extra-dynsym"})};
    if(!malformed) { ::llvm::consumeError(malformed.takeError());return 4; }
    // Constructor accepts a real extra symbol-table section. ObjectFile's
    // symbols() still observes only ordinary first SYMTAB; ledger must refuse
    // claiming completeness rather than hide the extra table's actual claims.
    ledger::image declined{};CHECK(!ledger::collect(**malformed,declined) && !declined.complete && declined.bodies.empty() && declined.claims.empty());
    ::fast_io::io::println("PASS actual extra ELF DYNSYM declined before all-symbol complete; loaded-ownership-qualified=false native-execution=false");
}
#undef CHECK
