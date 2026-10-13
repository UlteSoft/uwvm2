// Actual patched-LLVM object claim-ledger probe. Pure owned object DATA only;
// no fabricated section load, native pointer, canonical VM capture or execution.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <uwvm2/runtime/lib/uwvm_runtime_native_owner_function_claims.h>
#include <llvm/Support/MemoryBufferRef.h>
#include <fast_io.h>
#include <string_view>
namespace ledger=::uwvm2::runtime::lib::details::native_owner_function_claims;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("actual owner claims FAIL line=",__LINE__);return 1; } } while(false)
int main(int argc,char** argv)
{
    CHECK(argc==2 || (argc==3 && ::std::string_view{argv[2]}=="expect-decline"));
    ::fast_io::native_file file{argv[1],::fast_io::open_mode::in};
    auto const size{::fast_io::file_size(file)};
    CHECK(size!=0u && size<=64u*1024u*1024u && size<=PTRDIFF_MAX);
    ::fast_io::native_file_loader mapped{::fast_io::at(file)};CHECK(mapped.size()==size);
    // [same handle mapped actual file ... complete checked size]
    // [safe] native RAII mapping/file outlive ObjectFile and all section borrows.
    auto object{::llvm::object::ObjectFile::createObjectFile(::llvm::MemoryBufferRef{
        ::llvm::StringRef{mapped.data(),mapped.size()},"actual-native-owner-claims"})};
    if(!object) { ::llvm::consumeError(object.takeError());return 2; }
    ledger::image observed{};CHECK(ledger::collect(**object,observed) && observed.complete && observed.bodies.size()==4u);
    if(argc==3)
    {
        unsigned actual_extra{};
        for(auto const& claim:observed.claims)
        {
            if(!claim.defined || claim.kind!=::llvm::object::SymbolRef::ST_Function) { continue; }
            bool matched{};
            for(auto const& body:observed.bodies)
            { if(claim.name==body.entry_object_name || (!body.local_entry_object_name.empty() && claim.name==body.local_entry_object_name)) { matched=true;break; } }
            if(!matched) { ++actual_extra; }
        }
        CHECK(actual_extra!=0u && !observed.endpoints_unambiguous() &&
            (observed.blockers & static_cast<unsigned>(ledger::blocker::unexpected_alias))!=0u);
    }
    else { CHECK(observed.endpoints_unambiguous()); }
    unsigned original{},resume{},raw{},other{};
    for(auto const& body:observed.bodies)
    {
        CHECK(body.endpoints_proved && body.shape==1u && body.begin_offset<body.end_offset && body.end_offset<=body.section_size);
        switch(body.role) { case 1u:++original;break;case 2u:++resume;break;case 3u:++raw;break;case 0u:++other;break;default:CHECK(false); }
    }
    CHECK(original==1u && resume==1u && raw==1u && other==1u);
    ::fast_io::io::println("PASS actual object complete symbol ledger mode=",argc==3 ? "extra-alias-decline":"four-actual-definitions",
        " symbols=",::fast_io::mnp::dec(observed.claims.size())," blockers=",::fast_io::mnp::hex0x(observed.blockers),
        " loaded-ownership-qualified=false native-execution=false");
}
#undef CHECK
