// Linux MIPS n64 signal-context DATA; not a Wasm runtime owner/trap proof.
// Poisoned FP slots model storage that USED_FP=0 leaves unwritten.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/debugger/native_linux_context.h>
#include <fast_io.h>
#include <array>
#include <cstdint>
#include <cstring>
#if !defined(__linux__) || !defined(__mips__) || __SIZEOF_POINTER__ != 8
# error This ABI fixture requires MIPS n64 target compilation/execution.
#endif
namespace ctx=uwvm2::uwvm::debugger::native_linux_context;
namespace regs=uwvm2::uwvm::debugger::native_registers;
static void check(bool yes,char const* reason)
{
    if(!yes) { fast_io::io::perrln("FAIL MIPS USED_FP: ",fast_io::mnp::os_c_str(reason));fast_io::fast_terminate(); }
}
static bool zero(regs::fp_value const& value)
{
    if(value.width!=0u) { return false; }
    for(auto byte:value.bytes) { if(byte!=0u) { return false; } }
    return true;
}
static std::uint64_t bits(regs::fp_value const& value)
{
    std::uint64_t result{};
    for(unsigned i{};i!=value.width && i!=8u;++i) { result|=std::uint64_t{value.bytes[i]}<<(8u*i); }
    return result;
}
int main()
{
    ctx::kernel_context saved{};
    saved.uc_mcontext.pc=0x1000u;saved.uc_mcontext.gregs[2u]=42u;
    saved.uc_mcontext.gregs[29u]=0xfadefadefadefadeull;
    saved.uc_mcontext.mdhi=0xbad0bad0bad0bad0ull;saved.uc_mcontext.mdlo=0xbeefbeefbeefbeefull;
    for(unsigned i{};i!=32u;++i)
    {
        std::uint64_t const poison=0xa5b6c7d810203040ull+i;
        std::memcpy(&saved.uc_mcontext.fpregs.fp_r.fp_dregs[i],&poison,8u);
    }
    std::uint64_t const single=0xa5b6c7d83fc00000ull,double_value=0x7ff8123456789abcull;
    std::memcpy(&saved.uc_mcontext.fpregs.fp_r.fp_dregs[1u],&single,8u);
    std::memcpy(&saved.uc_mcontext.fpregs.fp_r.fp_dregs[2u],&double_value,8u);
    constexpr std::array<std::uint32_t,20u> flags{
        0u,1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,14u,15u,
        0x80000000u,0x80000001u,0x80000002u,0x80000003u};
    for(auto used:flags)
    {
        saved.uc_mcontext.used_math=used;auto const before=saved;
        auto const raw=ctx::capture(saved);
        bool const fp=(used&1u)!=0u;
        fast_io::io::perrln("USED_MATH=",used," captured-f1-width=",raw.floating.values[1u].width);
        check(raw.floating.available==fp,"availability must require saved scalar FP context");
        for(unsigned i{};i!=32u;++i)
        {
            check(fp ? raw.floating.values[i].width==8u : zero(raw.floating.values[i]),
                  "unwritten FP slots must remain zero/unavailable");
        }
        regs::numeric_location const locations[]{{2u,32u},{33u,32u},{34u,64u},{29u,64u},{30u,64u},{31u,64u}};
        auto const display=regs::project(raw,locations,6u);
        check(display.known_bits[2u]==32u && display.values[2u]==42u,"integer numeric control remains available");
        check(display.known_bits[29u]==0u && display.known_bits[30u]==0u && display.known_bits[31u]==0u,
              "stack/frame/return-link remain hidden");
        if(fp)
        {
            check(display.floating.values[1u].width==4u && bits(display.floating.values[1u])==0x3fc00000u,
                  "saved odd FPR preserves exact Wasm f32 bits");
            for(unsigned i{4u};i!=16u;++i) { check(display.floating.values[1u].bytes[i]==0u,"f32 residual bits remain hidden"); }
            check(display.floating.values[2u].width==8u && bits(display.floating.values[2u])==double_value,
                  "saved f64 NaN payload survives normalization");
        }
        else
        {
            check(!display.floating.available,"FP location DATA does not grant unsaved state");
            for(auto const& value:display.floating.values) { check(zero(value),"unsaved projection remains empty"); }
        }
        regs::numeric_location const too_wide{33u,128u};
        auto const vector=regs::project(raw,&too_wide,1u);
        check(zero(vector.floating.values[1u]),"scalar frame never grants v128 upper bits");
        check(std::memcmp(&before,&saved,sizeof(saved))==0,"capture never changes private kernel frame");
    }
    fast_io::io::println("PASS MIPS USED_FP flags/poisoned slots/f32/f64/read-only ABI DATA");
}
