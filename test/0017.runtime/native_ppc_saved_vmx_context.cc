// PowerPC Linux saved-VMX validity DATA; not a Wasm owner/trap proof.
// HWCAP only describes the CPU. MSR_VEC describes whether Linux saved VRs.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/debugger/native_linux_context.h>
#include <fast_io.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#if !defined(__linux__) || !defined(__powerpc__)
# error This ABI fixture requires PowerPC Linux target compilation/execution.
#endif
namespace ctx=uwvm2::uwvm::debugger::native_linux_context;
namespace regs=uwvm2::uwvm::debugger::native_registers;
static void check(bool yes,char const* reason)
{
    if(!yes) { fast_io::io::perrln("FAIL PPC saved VMX: ",fast_io::mnp::os_c_str(reason));fast_io::fast_terminate(); }
}
static bool zero(regs::fp_value const& v)
{
    if(v.width!=0u) { return false; }
    for(auto b:v.bytes) { if(b!=0u) { return false; } }
    return true;
}
static mcontext_t& saved(ctx::kernel_context& c)
{
#if defined(__powerpc64__)
    return c.uc_mcontext;
#else
    return *c.uc_mcontext.uc_regs;
#endif
}
static void reset(ctx::kernel_context& c,unsigned long msr)
{
    std::memset(&c,0,sizeof(c));
#if !defined(__powerpc64__)
    auto const base=reinterpret_cast<std::uintptr_t>(c.uc_reg_space);
    auto const address=(base+15u)&~std::uintptr_t{15u};
    check(address-base<=sizeof(c.uc_reg_space)-sizeof(mcontext_t),"complete inline PPC32 register storage");
    c.uc_mcontext.uc_regs=std::construct_at(reinterpret_cast<mcontext_t*>(address));
#endif
    auto& s=saved(c);
#if defined(__powerpc64__)
    auto& gpr=s.gp_regs;
    auto const base=reinterpret_cast<std::uintptr_t>(s.vmx_reserve);
    auto const address=(base+15u)&~std::uintptr_t{15u};
    check(address-base<=sizeof(s.vmx_reserve)-sizeof(*s.v_regs),"complete inline PPC64 vector storage");
    std::memset(s.vmx_reserve,0xc7u,sizeof(s.vmx_reserve));
    s.v_regs=reinterpret_cast<vrregset_t*>(address);
    auto& vectors=s.v_regs->vrregs;
    auto* scalar=&s.fp_regs[1u];
#else
    auto& gpr=s.gregs;
    auto& vectors=s.vrregs.vrregs;
    std::memset(&s.vrregs,0xc7u,sizeof(s.vrregs));
    auto* scalar=&s.fpregs.fpregs[1u];
#endif
    gpr[3u]=42u;gpr[32u]=0x1000u;gpr[33u]=msr;
    gpr[1u]=0xfadefadeul;gpr[31u]=0xbad0bad0ul;gpr[2u]=0x12345678ul;gpr[13u]=0x76543210ul;
    std::uint64_t const value=0x4045000000000000ull;std::memcpy(scalar,&value,sizeof(value));
    for(unsigned r{};r!=32u;++r)
    {
        auto* bytes=reinterpret_cast<unsigned char*>(&vectors[r]);
        for(unsigned b{};b!=16u;++b) { bytes[b]=static_cast<unsigned char>(r*5u+b); }
    }
}
int main()
{
    constexpr unsigned long vec=0x02000000ul;
    constexpr std::array<unsigned long,8u> not_vec{
        0ul,0x2000ul,0x4000ul,0x80000000ul,0x80002000ul,
        0x10000000ul,0x01000000ul,~0ul&~vec};
    ctx::kernel_context c{};
    unsigned cases{},valid{},refused{};
    for(bool hardware:{true,false})
    {
        ctx::kernel_has_altivec=hardware;
        for(bool present:{false,true})
        {
            for(auto flags:not_vec)
            {
                reset(c,flags|(present?vec:0ul));
                std::array<unsigned char,sizeof(c)> before{};std::memcpy(before.data(),&c,sizeof(c));
                auto const raw=ctx::capture(c);bool const expected=hardware&&present;
                fast_io::io::perrln("MSR_VEC=",unsigned(present)," HWCAP=",unsigned(hardware),
                                    " captured-v0-width=",raw.floating.values[32u].width);
                check(raw.machine==regs::architecture::powerpc && raw.pc()==0x1000u && raw.values[3u]==42u,
                      "integer/PC capture remains intact");
                check(raw.floating.values[1u].width==8u,"saved scalar FP remains available independently of VMX");
                for(unsigned r{};r!=32u;++r)
                {
                    auto const& v=raw.floating.values[32u+r];
                    check(expected ? v.width==16u : zero(v),"vector availability requires saved MSR_VEC and CPU capability");
                    if(expected)
                    { for(unsigned b{};b!=16u;++b) { check(v.bytes[b]==static_cast<unsigned char>(r*5u+b),"ordered complete vector lanes"); } }
                }
                for(unsigned dwarf:{77u,108u,1124u,1155u})
                {
                    unsigned const index=dwarf<109u ? 32u+dwarf-77u : 32u+dwarf-1124u;
                    for(unsigned bits:{32u,64u,128u})
                    {
                        regs::numeric_location const location{dwarf,bits};
                        auto const view=regs::project(raw,&location,1u);auto const& value=view.floating.values[index];
                        check(expected ? value.width==bits/8u : zero(value),
                              "LLVM/GNU vector aliases and narrow widths obey saved-state validity");
                        if(expected)
                        {
                            for(unsigned b{};b!=bits/8u;++b)
                            { check(value.bytes[b]==raw.floating.values[index].bytes[b],"proved vector bits preserved"); }
                            for(unsigned b=bits/8u;b!=16u;++b)
                            { check(value.bytes[b]==0u,"unproved vector residual bits remain hidden"); }
                        }
                        for(unsigned other{};other!=64u;++other)
                        { if(other!=index) { check(zero(view.floating.values[other]),"unqualified registers/control/padding remain hidden"); } }
                    }
                }
                regs::numeric_location const numeric[]{{3u,32u},{33u,64u},{1u,32u},{31u,32u},{2u,32u},{13u,32u}};
                auto const view=regs::project(raw,numeric,6u);
                check(view.values[3u]==42u && view.known_bits[3u]==32u,"proved integer bits preserved");
                check(view.floating.values[1u].width==8u,"proved scalar FP bits preserved");
                for(unsigned b{};b!=8u;++b)
                { check(view.floating.values[1u].bytes[b]==static_cast<unsigned char>(0x4045000000000000ull>>(8u*b)),"FP64 exact bits"); }
                for(unsigned index:{1u,31u,2u,13u})
                { check(view.values[index]==0u && view.known_bits[index]==0u,"SP/FP/TOC/TLS stay hidden"); }
                check(std::memcmp(before.data(),&c,sizeof(c))==0,"capture/project leave saved context unchanged");
                ++cases;if(expected) { ++valid; } else { ++refused; }
            }
        }
    }
    ctx::kernel_has_altivec=true;
    unsigned pointer_refusals{};
    for(unsigned which{};which!=4u;++which)
    {
        reset(c,vec);
#if defined(__powerpc64__)
        auto& s=saved(c);auto const base=reinterpret_cast<std::uintptr_t>(s.vmx_reserve);
        std::uintptr_t const bad[]{0u,1u,base-1u,base+sizeof(s.vmx_reserve)-sizeof(*s.v_regs)+1u};
        s.v_regs=reinterpret_cast<vrregset_t*>(bad[which]);
#else
        auto const base=reinterpret_cast<std::uintptr_t>(c.uc_reg_space);
        std::uintptr_t const bad[]{0u,1u,base-1u,base+sizeof(c.uc_reg_space)-sizeof(mcontext_t)+1u};
        c.uc_mcontext.uc_regs=reinterpret_cast<mcontext_t*>(bad[which]);
#endif
        auto const raw=ctx::capture(c);
        for(unsigned r{32u};r!=64u;++r) { check(zero(raw.floating.values[r]),"out-of-storage pointer never grants vector reads"); }
#if defined(__powerpc64__)
        check(raw.values[3u]==42u && raw.floating.values[1u].width==8u,"bad vector pointer does not suppress scalar state");
#else
        check(raw.machine==regs::architecture::unavailable,"bad PPC32 complete-context pointer refuses capture");
#endif
        ++pointer_refusals;
    }
    fast_io::io::println("PASS PPC saved VMX ABI DATA flags=",cases," valid=",valid," refused=",refused,
                        " pointer-refusals=",pointer_refusals," aliases/padding/control hidden pointer_bits=",sizeof(void*)*8u);
}
