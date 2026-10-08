// Exact LoongArch signal-extension ABI DATA, not a Wasm owner/trap proof.
// Linux uses an 8-aligned 1040-byte LASX payload; QEMU rounds it to 1056.
// Both put register bytes immediately after the same 16-byte context header.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/debugger/native_linux_context.h>
#include <fast_io.h>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#if !defined(__linux__) || !defined(__loongarch64)
# error This ABI fixture requires actual LoongArch64 target execution.
#endif
namespace ctx=uwvm2::uwvm::debugger::native_linux_context;
namespace regs=uwvm2::uwvm::debugger::native_registers;
struct info { std::uint32_t magic,size;std::uint64_t padding; };
struct linux_lasx { std::uint64_t registers[128],fcc;std::uint32_t fcsr; };
struct alignas(32) qemu_lasx { std::uint64_t registers[128],fcc;std::uint32_t fcsr; };
static_assert(sizeof(info)==16u && sizeof(linux_lasx)==1040u && sizeof(qemu_lasx)==1056u);
struct alignas(32) frame
{
    std::uint64_t alignment[2]{};
    ctx::kernel_context context{};
    std::array<unsigned char,1216u> tail{};
};
static_assert(offsetof(frame,tail)==offsetof(frame,context)+sizeof(ctx::kernel_context));
static_assert(offsetof(ctx::kernel_context,uc_mcontext)+offsetof(::mcontext_t,__extcontext)==sizeof(ctx::kernel_context));
static_assert(offsetof(frame,tail)%32u==16u);
static void check(bool yes,char const* reason)
{
    if(!yes) { fast_io::io::perrln("FAIL LoongArch extension: ",fast_io::mnp::os_c_str(reason));fast_io::fast_terminate(); }
}
static bool zero(regs::fp_value const& value)
{
    if(value.width!=0u) { return false; }
    for(auto byte:value.bytes) { if(byte!=0u) { return false; } }
    return true;
}
static void header(unsigned char* bytes,std::uint32_t magic,std::uint32_t size)
{
    info const h{magic,size,0xfadefadefadefadeull};std::memcpy(bytes,&h,sizeof(h));
}
static void reset(frame& f)
{
    f={};f.context.uc_mcontext.__flags=1u;f.context.uc_mcontext.__pc=0x1000u;
    f.context.uc_mcontext.__gregs[4u]=42u;
    f.context.uc_mcontext.__gregs[3u]=0xfadefadefadefadeull;
    f.tail.fill(0xa5u);
}
static void absent(frame const& f)
{
    auto const before=f;auto const raw=ctx::capture(f.context);
    check(!raw.floating.available,"unknown/short/no-FP context remains unavailable");
    for(auto const& value:raw.floating.values) { check(zero(value),"rejected context leaves no register bytes"); }
    check(std::memcmp(&before,&f,sizeof(f))==0,"rejected capture is read-only");
}
int main()
{
    static_assert(std::endian::native==std::endian::little);
    // Independently reproduce Linux's down-growing allocator, with the two
    // legitimate incoming stack residues. Alignment slack is at the END.
    auto const linux_size=[](std::uintptr_t base)
    { auto const data=(base-sizeof(linux_lasx))&~std::uintptr_t{31u};return base-(data-sizeof(info)); };
    check(linux_size(0x10010u)==1056u && linux_size(0x10000u)==1072u,"Linux LASX allocation extents");
    frame f{};
    unsigned positive{};
    for(auto layout:std::array<std::array<unsigned,3u>,5u>{{
        {0x41535801u,1056u,32u},{0x41535801u,1072u,32u},{0x41535801u,1088u,32u},
        {0x53580001u,544u,16u},{0x46505501u,288u,8u}}})
    {
        for(bool lbt:{false,true})
        {
            reset(f);unsigned const start=lbt?64u:0u;
            if(lbt) { header(f.tail.data(),0x42540001u,64u); }
            header(f.tail.data()+start,layout[0u],layout[1u]);
            for(unsigned i{};i!=32u;++i)
            {
                auto* const value=f.tail.data()+start+16u+i*layout[2u];
                for(unsigned j{};j!=layout[2u];++j) { value[j]=static_cast<unsigned char>(i*3u+j); }
            }
            auto const before=f;auto const raw=ctx::capture(f.context);
            fast_io::io::perrln("context-size=",layout[1u]," lbt=",lbt," captured-width=",raw.floating.values[0u].width);
            check(raw.floating.available,"complete Linux/QEMU FP context must remain available");
            unsigned const width=layout[2u]==8u?8u:16u;
            for(unsigned i{};i!=32u;++i)
            {
                auto const& value=raw.floating.values[i];
                check(value.width==width,"complete saved scalar/vector register width");
                for(unsigned j{};j!=width;++j) { check(value.bytes[j]==static_cast<unsigned char>(i*3u+j),"exact register stride and lane order"); }
                for(unsigned j=width;j!=16u;++j) { check(value.bytes[j]==0u,"unsaved upper bytes remain zero"); }
            }
            regs::numeric_location const loc[]{{32u,64u},{33u,32u},{34u,128u},{3u,64u},{22u,64u},{1u,64u}};
            auto const display=regs::project(raw,loc,6u);
            check(display.floating.values[0u].width==8u && display.floating.values[1u].width==4u,"qualified f64/f32 widths");
            for(unsigned j{};j!=8u;++j) { check(display.floating.values[0u].bytes[j]==j,"f64 exact bits"); }
            for(unsigned j{};j!=4u;++j) { check(display.floating.values[1u].bytes[j]==3u+j,"f32 exact low bits"); }
            for(unsigned j{4u};j!=16u;++j) { check(display.floating.values[1u].bytes[j]==0u,"f32 residual bits hidden"); }
            check(width==16u ? display.floating.values[2u].width==16u : zero(display.floating.values[2u]),
                  "v128 requires a complete saved vector; LASX upper128 never grants extra display");
            if(width==16u)
            { for(unsigned j{};j!=16u;++j) { check(display.floating.values[2u].bytes[j]==6u+j,"v128 exact ordered low lanes"); } }
            for(unsigned i{3u};i!=display.floating.values.size();++i)
            { check(zero(display.floating.values[i]),"unproved register and padding bytes remain hidden"); }
            check(display.known_bits[3u]==0u && display.known_bits[22u]==0u && display.known_bits[1u]==0u,
                  "stack/frame/return-link stay unavailable");
            check(std::memcmp(&before,&f,sizeof(f))==0,"capture never changes saved frame or poisoned padding");
            for(std::uint32_t flags:{0u,1u<<30u,1u<<31u,3u<<30u})
            { f.context.uc_mcontext.__flags=flags;absent(f); }
            ++positive;
        }
    }
    unsigned negative{};
    for(std::uint32_t length:{0u,1u,16u,1040u,1048u,1055u,1057u,1064u,1080u,1096u,4096u,0xffffffffu})
    { reset(f);header(f.tail.data(),0x41535801u,length);absent(f);++negative; }
    for(auto bad:std::array<std::array<unsigned,2u>,6u>{{
        {0u,0u},{0xdeadbeefu,1056u},{0x42540001u,48u},{0x42540001u,80u},
        {0x46505501u,272u},{0x53580001u,528u}}})
    { reset(f);header(f.tail.data(),bad[0u],bad[1u]);absent(f);++negative; }
    reset(f);header(f.tail.data(),0x42540001u,64u);header(f.tail.data()+64u,0x42540001u,64u);
    header(f.tail.data()+128u,0x41535801u,1056u);absent(f);++negative;
    fast_io::io::println("PASS LoongArch FP/LSX/LASX ABI DATA positive=",positive," negative=",negative,
                        " no-FP-flags=40 padding/control/upper-LASX hidden");
}
