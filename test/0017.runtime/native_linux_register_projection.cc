// Target ABI DATA projection checks, not a kernel-stop or Wasm owner proof.
#include <uwvm2/uwvm/debugger/native_registers.h>
#include <fast_io.h>
#include <bit>
#if defined(__linux__) && defined(__arm__)
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/debugger/native_linux_context.h>
#include <uwvm2/uwvm/debugger/native_linux_platform.h>
#endif
#include <uwvm2/runtime/lib/native_dwarf_register_location.h>
namespace regs = uwvm2::uwvm::debugger::native_registers;
#define CHECK(x) do { if(!(x)) { fast_io::io::perrln("FAIL Linux register projection line=",__LINE__); fast_io::fast_terminate(); } } while(false)
int main()
{
#if defined(__linux__) && defined(__arm__)
    static_assert(UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE == 1);
    namespace kernel=uwvm2::uwvm::debugger::native_linux_context;
    kernel::kernel_context context{};
    context.uc_mcontext.arm_pc=0x1000u;context.uc_mcontext.arm_sp=0x2000u;
    context.uc_mcontext.arm_r0=0xabcdef01u;context.uc_mcontext.arm_cpsr=0x10u;
    auto saved=kernel::capture(context);
    CHECK(saved.machine==regs::architecture::arm && saved.pc()==0x1000u &&
          saved.sp()==0x2000u && saved.values[0u]==0xabcdef01u);
    kernel::set_pc(context,0x1004u);CHECK(context.uc_mcontext.arm_pc==0x1004u);
    kernel::set_pc(context,0x1003u);CHECK(context.uc_mcontext.arm_pc==0x1004u);
    context.uc_mcontext.arm_cpsr|=1u<<5u;
    CHECK(kernel::capture(context).size()==0u);
    kernel::set_pc(context,0x1008u);CHECK(context.uc_mcontext.arm_pc==0x1004u);
    context.uc_mcontext.arm_cpsr=0x10u;context.uc_mcontext.arm_pc=0x1001u;
    CHECK(kernel::capture(context).size()==0u);
    fast_io::io::println("PASS ARM-state context under actual host ISA; Thumb/unaligned saved state and PC mutation refused");
#endif
    regs::snapshot raw{}; raw.machine=regs::architecture::s390x; raw.floating.available=true;
    auto& f0=raw.floating.values[0u];f0.width=8u;
    // Low host residual bits are recognizable; F32 lives in high 32 bits.
    f0.bytes={0xa5u,0xb6u,0xc7u,0xd8u,0u,0u,0xc0u,0x3fu};
    regs::numeric_location const f32{16u,32u};auto view=regs::project(raw,&f32,1u);
    CHECK(view.floating.values[0u].width==4u);
    CHECK(view.floating.values[0u].bytes[0u]==0u && view.floating.values[0u].bytes[1u]==0u &&
          view.floating.values[0u].bytes[2u]==0xc0u && view.floating.values[0u].bytes[3u]==0x3fu);
    for(unsigned i=4u;i!=16u;++i) { CHECK(view.floating.values[0u].bytes[i]==0u); }
    regs::numeric_location const mixed[]{ {16u,32u},{16u,64u} };
    auto intersection=regs::project(raw,mixed,2u);
    CHECK(intersection.floating.values[0u].width==4u && intersection.floating.values[0u].bytes[2u]==0xc0u && intersection.floating.values[0u].bytes[3u]==0x3fu);
    regs::numeric_location const reversed[]{ {16u,64u},{16u,32u} };
    CHECK(regs::project(raw,reversed,2u).floating.values[0u]==intersection.floating.values[0u]);
    raw.machine=regs::architecture::powerpc;regs::numeric_location const ppc32{32u,32u};auto ppc=regs::project(raw,&ppc32,1u);
    CHECK(ppc.floating.values[0u].width==0u);
    for(auto byte:ppc.floating.values[0u].bytes) { CHECK(byte==0u); }
    // Independent IEEE widening round trip, including zero/subnormals/limits.
    auto& ppc_source=raw.floating.values[0u];
    for(::std::uint32_t input : {0u,0x80000000u,1u,0x007fffffu,0x00800000u,0x3fc00000u,0x42280000u,0x7f7fffffu,0x7f800000u,0xff800000u})
    {
        auto const widened{::std::bit_cast<::std::uint64_t>(static_cast<double>(::std::bit_cast<float>(input)))};
        for(unsigned i{}; i != 8u; ++i) { ppc_source.bytes[i]=static_cast<unsigned char>(widened >> (8u*i)); }
        ::std::uint32_t actual{};CHECK(regs::ppc_single_value(ppc_source,actual) && actual==input);
        auto const qualified=regs::project(raw,&ppc32,1u);CHECK(qualified.floating.values[0u].width==4u);
        for(unsigned i{}; i != 4u; ++i) { CHECK(qualified.floating.values[0u].bytes[i]==static_cast<unsigned char>(input >> (8u*i))); }
    }
    // A NaN's unqualified low physical payload must never escape.
    ::std::uint64_t const nan{0x7ff8000020000123ull};
    for(unsigned i{}; i != 8u; ++i) { ppc_source.bytes[i]=static_cast<unsigned char>(nan >> (8u*i)); }
    ::std::uint32_t payload{};CHECK(regs::ppc_single_value(ppc_source,payload) && payload==0x7fc00001u);
    raw.machine=regs::architecture::sparc64;raw.floating.values[32u].width=4u;
    raw.floating.values[32u].bytes={0u,0u,0xc0u,0x3fu};regs::numeric_location const sparc32{32u,32u};
    auto sparc=regs::project(raw,&sparc32,1u);CHECK(sparc.floating.values[32u].width==4u);
    CHECK(regs::fp_index_of(raw.machine,u8"f0")==32u && regs::fp_index_of(raw.machine,u8"d0")==0u);
    namespace dwarf = uwvm2::runtime::lib::details::native_loaded_provenance;
    constexpr ::std::array<unsigned char,3u> arm_d0{0x90u,0x80u,0x02u}, ppc_v0{0x90u,0xe4u,0x08u};
    static_assert(dwarf::exact_register_location(arm_d0).available && dwarf::exact_register_location(arm_d0).number==256u);
    static_assert(dwarf::exact_register_location(ppc_v0).available && dwarf::exact_register_location(ppc_v0).number==1124u);
    constexpr ::std::array<unsigned char,2u> unterminated{0x90u,0x80u}, dereference{0x50u,0x06u};
    constexpr ::std::array<unsigned char,3u> overlong{0x90u,0x80u,0x00u};
    constexpr ::std::array<unsigned char,6u> overflow{0x90u,0xffu,0xffu,0xffu,0xffu,0x10u}, pieces{0x90u,0x80u,0x02u,0x9du,0x20u,0x20u};
    static_assert(!dwarf::exact_register_location(unterminated).available && !dwarf::exact_register_location(dereference).available &&
                  !dwarf::exact_register_location(overlong).available && !dwarf::exact_register_location(overflow).available &&
                  !dwarf::exact_register_location(pieces).available);
    raw.machine=regs::architecture::arm;raw.floating.values[0u].width=8u;
    regs::numeric_location const arm64{dwarf::exact_register_location(arm_d0).number,64u};
    CHECK(regs::project(raw,&arm64,1u).floating.values[0u].width==8u);
    raw.machine=regs::architecture::powerpc;raw.floating.values[32u].width=16u;
    regs::numeric_location const ppc128{dwarf::exact_register_location(ppc_v0).number,128u};
    CHECK(regs::project(raw,&ppc128,1u).floating.values[32u].width==16u);
    raw.floating.values[32u].bytes.fill(0x5au);
    raw.floating.values[63u].width=16u;raw.floating.values[63u].bytes.fill(0xa6u);
    for(auto location: {regs::numeric_location{77u,128u},regs::numeric_location{108u,128u}})
    {
        auto const index{32u+location.dwarf_register-77u};auto const qualified=regs::project(raw,&location,1u);
        CHECK(qualified.floating.values[index]==raw.floating.values[index]);
        for(unsigned other{};other!=64u;++other) { if(other!=index) { CHECK(qualified.floating.values[other].width==0u); } }
    }
    for(unsigned reg: {76u,109u,1123u,1156u})
    {
        regs::numeric_location const invalid{reg,128u};auto const rejected=regs::project(raw,&invalid,1u);
        for(auto const& value:rejected.floating.values) { CHECK(value.width==0u); }
    }
    fast_io::io::println("PASS SystemZ high-half f32, PPC widened f32, SPARC aliases, ARM/PPC multi-byte DWARF registers; addresses and bit-pieces refused");
}
