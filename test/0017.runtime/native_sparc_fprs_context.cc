// Exact Linux SPARC64 RT-frame DATA fixture; not a runtime owner/trap proof.
// The unsaved half is poisoned to expose QEMU's all-register-save blind spot.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/debugger/native_linux_context.h>
#include <fast_io.h>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#if !defined(__linux__) || !defined(__sparc__) || !defined(__arch64__)
# error This ABI fixture requires real SPARC64 target compilation/execution.
#endif
namespace ctx=uwvm2::uwvm::debugger::native_linux_context;
namespace regs=uwvm2::uwvm::debugger::native_registers;
struct abi_fpu
{
    std::uint32_t registers[64];
    std::uint64_t fsr,gsr,fprs;
};
static_assert(sizeof(abi_fpu)==280u && offsetof(abi_fpu,fprs)==272u);
struct alignas(16) frame
{
    ctx::sparc_signal_window window{};
    ctx::kernel_context context{};
    abi_fpu fpu{};
};
static_assert(offsetof(frame,context)==192u && offsetof(frame,fpu)==528u);
static void check(bool yes,char const* reason)
{
    if(!yes) { fast_io::io::perrln("FAIL SPARC FPRS: ",fast_io::mnp::os_c_str(reason));fast_io::fast_terminate(); }
}
static bool zero(regs::fp_value const& value)
{
    if(value.width!=0u) { return false; }
    for(auto byte:value.bytes) { if(byte!=0u) { return false; } }
    return true;
}
int main()
{
    static_assert(std::endian::native==std::endian::big);
    frame saved{};
    saved.context.saved.pc=0x1000u;saved.context.saved.next_pc=0x1004u;
    saved.context.saved.registers[14u]=0x2000u;
    saved.context.fp_save=reinterpret_cast<std::uintptr_t>(&saved.fpu);
    for(auto& value:saved.fpu.registers) { value=0xa5b6c7d8u; }
    saved.fpu.registers[0u]=0x3fc00000u;saved.fpu.registers[1u]=0x10203040u;
    saved.fpu.registers[32u]=0x40230000u;
    saved.fpu.fsr=0xfacefacefacefaceull;saved.fpu.gsr=0xbad0bad0bad0bad0ull;
    for(std::uint64_t flags=0u;flags!=8u;++flags)
    {
        saved.fpu.fprs=flags;auto const before=saved;
        auto const raw=ctx::capture(saved.context);
        bool const lower=(flags&1u)!=0u,upper=(flags&2u)!=0u;
        fast_io::io::println("FPRS=",flags," lower=",lower," upper=",upper,
                            " captured-d0-width=",raw.floating.values[0u].width,
                            " captured-d16-width=",raw.floating.values[16u].width);
        check(raw.floating.available==(lower||upper),"availability must derive from saved banks");
        for(unsigned i=0u;i!=32u;++i)
        {
            auto const expected=i<16u?lower:upper;
            check(expected ? raw.floating.values[i].width==8u : zero(raw.floating.values[i]),
                  "unwritten double-register half must remain zero/unavailable");
            check(lower ? raw.floating.values[32u+i].width==4u : zero(raw.floating.values[32u+i]),
                  "all single aliases belong only to saved lower half");
        }
        regs::numeric_location const single{32u,32u},double_value{72u,64u};
        auto const f32=regs::project(raw,&single,1u),f64=regs::project(raw,&double_value,1u);
        if(lower)
        {
            check(f32.floating.values[32u].width==4u &&
                  f32.floating.values[32u].bytes[2u]==0xc0u &&
                  f32.floating.values[32u].bytes[3u]==0x3fu,"saved Wasm f32 preserves exact 1.5 bits");
            std::uint64_t bits{};
            for(unsigned i=0u;i!=8u;++i) { bits|=std::uint64_t{f64.floating.values[0u].bytes[i]}<<(8u*i); }
            check(f64.floating.values[0u].width==8u && bits==0x3fc0000010203040ull,
                  "saved full double preserves independent big-endian bit pattern");
        }
        else { check(zero(f32.floating.values[32u]) && zero(f64.floating.values[0u]),
                     "numeric location DATA cannot expose an unsaved bank"); }
        check(std::memcmp(&before,&saved,sizeof(saved))==0,"capture never changes private frame/control values");
    }
    auto* forbidden=::mmap(nullptr,4096u,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    check(forbidden!=MAP_FAILED,"guard page");
    saved.context.fp_save=reinterpret_cast<std::uintptr_t>(forbidden);saved.fpu.fprs=7u;
    auto const foreign=ctx::capture(saved.context);
    check(!foreign.floating.available,"non-inline kernel pointer remains refused without dereferencing");
    for(auto const& value:foreign.floating.values) { check(zero(value),"foreign FP state remains absent"); }
    check(::munmap(forbidden,4096u)==0,"guard retirement");
    fast_io::io::println("PASS SPARC FPRS banks/aliases/poisoned holes/guard pointer, read-only ABI DATA");
}
