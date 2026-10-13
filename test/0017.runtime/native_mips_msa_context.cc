// MIPS N64 kernel ABI DATA. Positive records are fixture data, not a
// genuine Linux MSA signal capture or a Wasm JIT stop qualification.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/debugger/native_linux_context.h>
#include <fast_io.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <new>
#include <sys/mman.h>
#include <unistd.h>
#if !defined(__linux__) || !defined(__mips__) || __SIZEOF_POINTER__ != 8
# error This fixture requires actual Linux MIPS N64 execution.
#endif
namespace ctx=uwvm2::uwvm::debugger::native_linux_context;
namespace regs=uwvm2::uwvm::debugger::native_registers;
static_assert(offsetof(ctx::kernel_context,uc_sigmask)==640u);
struct frame { ctx::kernel_context context{}; std::array<unsigned char,288u> remainder{}; };
static void check(bool yes,char const* reason)
{
    if(!yes) { fast_io::io::perrln("FAIL MIPS MSA: ",fast_io::mnp::os_c_str(reason));fast_io::fast_terminate(); }
}
static bool zero(regs::fp_value const& value)
{
    if(value.width!=0u) { return false; }
    for(auto byte:value.bytes) { if(byte!=0u) { return false; } }
    return true;
}
static unsigned char* extension(ctx::kernel_context& c)
{ return reinterpret_cast<unsigned char*>(&c)+656u; }
static void word(unsigned char* p,std::uint32_t value) { std::memcpy(p,&value,4u); }
static void prepare(frame& f)
{
    f={};f.context.uc_mcontext.used_math=11u;f.context.uc_mcontext.pc=0x1000u;
    f.context.uc_mcontext.gregs[29u]=0xfadefadefadefadeull;
    auto* tail=extension(f.context);std::memset(tail,0xa5u,276u);
    word(tail,0x784d5341u);word(tail+4u,272u);word(tail+272u,0x78454e44u);
    for(unsigned i{};i!=32u;++i)
    {
        std::uint64_t lower{},upper{};
        for(unsigned byte{};byte!=8u;++byte)
        { lower|=std::uint64_t{(i*5u+byte)&255u}<<(byte*8u);upper|=std::uint64_t{(i*5u+8u+byte)&255u}<<(byte*8u); }
        std::memcpy(&f.context.uc_mcontext.fpregs.fp_r.fp_dregs[i],&lower,8u);
        std::memcpy(tail+8u+i*8u,&upper,8u);
    }
}
static void rejected(frame const& f)
{
    auto const before=f;auto raw=ctx::capture(f.context);
    for(unsigned i{32u};i!=64u;++i) { check(zero(raw.floating.values[i]),"invalid extension never yields vector state"); }
    for(unsigned reg:{32u,63u})
    {
        regs::numeric_location loc{reg,128u};auto view=regs::project(raw,&loc,1u);
        for(auto const& value:view.floating.values) { check(zero(value),"v128 cannot use scalar-only saved halves"); }
    }
    check(std::memcmp(&before,&f,sizeof(f))==0,"rejected capture is read-only");
}
int main()
{
    frame f{};prepare(f);auto before=f;auto raw=ctx::capture(f.context);
    for(unsigned i{};i!=32u;++i)
    {
        auto const& vector=raw.floating.values[32u+i];check(vector.width==16u,"complete saved MSA slot");
        for(unsigned byte{};byte!=16u;++byte) { check(vector.bytes[byte]==((i*5u+byte)&255u),"ordered lanes from separate kernel u64 halves"); }
        regs::numeric_location const loc{32u+i,128u};auto view=regs::project(raw,&loc,1u);
        check(view.floating.values[32u+i]==vector,"DWARF scalar/vector aliases select full vector");
        for(unsigned other{};other!=64u;++other) { if(other!=32u+i) { check(zero(view.floating.values[other]),"unproved aliases stay hidden"); } }
        check(view.known_bits[29u]==0u,"native stack address hidden");
        regs::numeric_location const scalar{32u+i,32u};auto narrow=regs::project(raw,&scalar,1u);
        check(narrow.floating.values[i].width==4u,"scalar role grants exactly four bytes");
        for(unsigned byte{4u};byte!=16u;++byte) { check(narrow.floating.values[i].bytes[byte]==0u,"scalar residual bytes zero"); }
        for(unsigned alias{32u};alias!=64u;++alias) { check(zero(narrow.floating.values[alias]),"scalar role never releases W alias"); }
    }
    for(unsigned width:{32u,64u})
    {
        regs::numeric_location const aliases[]{{32u,128u},{32u,width}};
        regs::numeric_location const reverse[]{{32u,width},{32u,128u}};
        auto first=regs::project(raw,aliases,2u),second=regs::project(raw,reverse,2u);
        check(zero(first.floating.values[32u]) && zero(second.floating.values[32u]),"scalar/W aliases intersect independent of order");
        check(first.floating.values[0u].width==width/8u && first.floating.values==second.floating.values,"scalar authority survives overlapping vector role");
    }
    check(regs::fp_index_of(regs::architecture::mips64,u8"$w0")==32u &&
          regs::fp_index_of(regs::architecture::mips64,u8"w31")==63u &&
          regs::fp_index_of(regs::architecture::mips64,u8"w32")==64u,"bounded physical vector names");
    check(std::memcmp(&before,&f,sizeof(f))==0,"valid capture read-only");
    unsigned negatives{};
    for(unsigned flags:{0u,1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,12u,13u,14u,15u})
    { prepare(f);f.context.uc_mcontext.used_math=flags;rejected(f);++negatives; }
    for(unsigned length:{0u,8u,264u,268u,271u,273u,280u,4096u,0xffffffffu})
    { prepare(f);word(extension(f.context)+4u,length);rejected(f);++negatives; }
    for(unsigned magic:{0u,0x78454e44u,0xdeadbeefu})
    { prepare(f);word(extension(f.context),magic);rejected(f);++negatives; }
    prepare(f);word(extension(f.context)+272u,0u);rejected(f);++negatives;
    // A valid libc-sized context directly borders an unreadable page.
    // A scalar frame and every rejected header must not read the MSA tail
    // outside that context, even when an attacker supplies a huge length.
    auto const page=static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
    auto* map=static_cast<unsigned char*>(::mmap(nullptr,2u*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    check(map!=MAP_FAILED && ::mprotect(map+page,page,PROT_NONE)==0,"guard page setup");
    auto* saved=::new(map+page-sizeof(ctx::kernel_context)) ctx::kernel_context{};
    saved->uc_mcontext.used_math=1u;auto scalar=ctx::capture(*saved);
    check(scalar.floating.values[0u].width==8u && zero(scalar.floating.values[32u]),"real QEMU scalar contract never reads vector tail");
    for(unsigned length:{0u,8u,264u,268u,271u,273u,280u,4096u,0xffffffffu})
    {
        saved->uc_mcontext.used_math=11u;word(extension(*saved),0x784d5341u);word(extension(*saved)+4u,length);
        auto value=ctx::capture(*saved);check(zero(value.floating.values[32u]),"guarded invalid size rejected before tail read");
    }
    using ctx::kernel_context; saved->~kernel_context();check(::munmap(map,2u*page)==0,"guard page cleanup");
    fast_io::io::println("PASS MIPS MSA ABI DATA vectors=32 rejected=",negatives," guard-page=10 scalar/vector aliases isolated");
}
