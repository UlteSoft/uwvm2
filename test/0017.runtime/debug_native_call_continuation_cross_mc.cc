// Actual LLVM MC DATA qualification. This does not mint runtime authority.
#define UWVM_USE_LLVM_JIT 1
#include <uwvm2/uwvm/debugger/native_wasm_call_continuation.h>
#include <llvm/Support/TargetSelect.h>
#include <fast_io.h>
#include <fast_io_dsal/string_view.h>
#include <array>
#include <cstring>
#include <span>
namespace dbg=::uwvm2::uwvm::debugger;
namespace mc=dbg::native_owned_instruction_semantics;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("cross call MC line=",__LINE__);return 1; } } while(false)
struct target_description
{
    ::std::array<char,64u> triple{},cpu{},features{};
    ::std::size_t triple_size{},cpu_size{},features_size{};
    unsigned description_version{1u},pointer_bits{},maximum_instruction_bytes{},minimum_instruction_alignment{};
    bool available{},little_endian{};
};
int main()
{
    ::llvm::InitializeAllTargetInfos();::llvm::InitializeAllTargets();
    ::llvm::InitializeAllTargetMCs();::llvm::InitializeAllDisassemblers();
    struct row { char const* triple; ::std::uint32_t direct,indirect,tail,nop; unsigned bits; };
    constexpr row cases[]{
        {"aarch64-linux-gnu",0x94000000u,0xd63f0200u,0xd61f0200u,0xd503201fu,64u},
        {"arm-linux-gnueabihf",0xeb000000u,0xe12fff33u,0xe12fff13u,0xe1a00000u,32u},
        {"armeb-linux-gnueabi",0xeb000000u,0xe12fff33u,0xe12fff13u,0xe1a00000u,32u},
        {"powerpc-linux-gnu",0x48000005u,0x4e800421u,0x4e800420u,0x60000000u,32u},
        {"powerpc64-linux-gnu",0x48000005u,0x4e800421u,0x4e800420u,0x60000000u,64u},
        {"powerpc64le-linux-gnu",0x48000005u,0x4e800421u,0x4e800420u,0x60000000u,64u},
        {"mips64-linux-gnuabi64",0x0c000004u,0x0320f809u,0x03200008u,0u,64u},
        {"mips64el-linux-gnuabi64",0x0c000004u,0x0320f809u,0x03200008u,0u,64u},
        {"riscv64-linux-gnu",0x000000efu,0x000280e7u,0x00028067u,0x00000013u,64u},
        {"loongarch64-linux-gnu",0x54000000u,0x4c000181u,0x4c000180u,0x03400000u,64u},
        {"sparc64-linux-gnu",0x40000000u,0x9fc04000u,0x81c04000u,0x01000000u,64u},
        {"i686-linux-gnu",0u,0u,0u,0u,32u},
        {"s390x-linux-gnu",0u,0u,0u,0u,64u},
    };
    unsigned tested{},memory_order_tested{},vector_configuration_tested{};
    for(auto const& row:cases)
    {
        target_description description{};
        description.triple_size=::std::strlen(row.triple);CHECK(description.triple_size<description.triple.size());
        ::std::memcpy(description.triple.data(),row.triple,description.triple_size);
        ::llvm::Triple const triple{row.triple};::std::string error{};
        auto const* target{mc::details::lookup_target(triple,error)};CHECK(target);
        auto registers{mc::details::make_registers(*target,triple)};CHECK(registers);
        ::llvm::MCTargetOptions options{};
        auto assembly{mc::details::make_assembly(*target,*registers,triple,options)};
        if(triple.isARM())
        {
            constexpr char cpu[]{"cortex-a7"};description.cpu_size=sizeof(cpu)-1u;
            ::std::memcpy(description.cpu.data(),cpu,description.cpu_size);
        }
        if(triple.getArch()==::llvm::Triple::riscv64)
        {
            constexpr char features[]{"+v"};description.features_size=sizeof(features)-1u;
            ::std::memcpy(description.features.data(),features,description.features_size);
        }
        auto subtarget{mc::details::make_subtarget(*target,triple,
            {description.cpu.data(),description.cpu_size},{description.features.data(),description.features_size})};CHECK(assembly && subtarget);
        description.available=true;description.pointer_bits=row.bits;description.little_endian=assembly->isLittleEndian();
        description.maximum_instruction_bytes=mc::details::maximum_instruction_length(*assembly,*subtarget,triple);
        description.minimum_instruction_alignment=assembly->getMinInstAlignment();
        dbg::native_disassembly::decoder display{description};mc::decoder semantic{description};CHECK(display && semantic);
        for(unsigned form{};form!=3u;++form)
        {
            ::std::array<::std::uint8_t,32u> storage{};
            unsigned size{16u};
            if(triple.getArch()==::llvm::Triple::x86)
            {
                unsigned instruction_size{form==0u?5u:2u};
                storage[0u]=form==0u?0xe8u:0xffu;
                if(form!=0u) { storage[1u]=form==1u?0xd0u:0xe0u; }
                size=instruction_size+3u;
                for(unsigned i{instruction_size};i!=size;++i) { storage[i]=0x90u; }
            }
            else if(triple.getArch()==::llvm::Triple::systemz)
            {
                unsigned instruction_size{form==0u?6u:2u};
                storage[0u]=form==0u?0xc0u:form==1u?0x0du:0x07u;
                storage[1u]=form==0u?0xe5u:form==1u?0xe1u:0xf1u;
                size=instruction_size+6u;
                for(unsigned i{instruction_size};i!=size;i+=2u) { storage[i]=0x18u;storage[i+1u]=0u; }
            }
            else
            {
                ::std::uint32_t const words[]{form==0u?row.direct:form==1u?row.indirect:row.tail,row.nop,row.nop,row.nop};
                for(unsigned w{};w!=4u;++w) for(unsigned b{};b!=4u;++b)
                { storage[w*4u+b]=static_cast<::std::uint8_t>(words[w]>>(8u*((description.little_endian||triple.isARM())?b:3u-b))); }
            }
            ::std::span<::std::uint8_t const> const bytes{storage.data(),size};
            auto const decoded{semantic.decode(0x1000u,bytes)};
            ::fast_io::io::println(::fast_io::mnp::os_c_str(row.triple)," form=",form," decoded=",bool(decoded),
                " kind=",static_cast<unsigned>(decoded.semantics().kind)," returning=",decoded.safe_for_call_continuation());
            if(!decoded || decoded.safe_for_call_continuation()!=(form!=2u))
            {
                auto context{mc::details::make_context(triple,*assembly,*registers,*subtarget)};
                auto raw{::std::unique_ptr<::llvm::MCDisassembler>{target->createMCDisassembler(*subtarget,*context)}};
                auto info{::std::unique_ptr<::llvm::MCInstrInfo>{target->createMCInstrInfo()}};
                ::llvm::MCInst actual{};::std::uint64_t size{};
                auto const status{raw->getInstruction(actual,size,bytes,0x1000u,::llvm::nulls())};
                ::fast_io::io::perrln("actual MC status=",static_cast<unsigned>(status)," size=",size," flags=",actual.getFlags());
                if(status==::llvm::MCDisassembler::Success && actual.getOpcode()<info->getNumOpcodes())
                {
                    auto const name{info->getName(actual.getOpcode())};
                    auto const& descriptor{info->get(actual.getOpcode())};
                    ::fast_io::io::perrln("descriptor operands=",descriptor.getNumOperands()," defs=",descriptor.getNumDefs(),
                        " actual operands=",actual.getNumOperands()," analysis=",bool(::std::unique_ptr<::llvm::MCInstrAnalysis>{target->createMCInstrAnalysis(info.get())}));
                    ::fast_io::io::perrln("actual opcode=",::fast_io::string_view{name.data(),name.size()});
                    for(unsigned i{};i!=actual.getNumOperands();++i)
                    {
                        auto const& operand{actual.getOperand(i)};
                        if(operand.isReg() && operand.getReg()!=0u && operand.getReg()<registers->getNumRegs())
                        { ::fast_io::io::perrln("operand=",i," register=",::fast_io::mnp::os_c_str(registers->getName(operand.getReg()))); }
                    }
                }
            }
            CHECK(decoded && decoded.safe_for_call_continuation()==(form!=2u));
            auto const selected{dbg::native_wasm_call_continuation::prepare(display,semantic,bytes,0x1000u,0x1000u+size,0x1000u)};
            CHECK(bool(selected)==(form!=2u));
            if(selected)
            {
                CHECK(selected.continuation==0x1000u+decoded.semantics().size+decoded.call_delay_bytes());
                CHECK(!dbg::native_wasm_call_continuation::prepare(display,semantic,bytes,0x1000u,0x1000u+size-1u,0x1000u));
                CHECK(!dbg::native_wasm_call_continuation::prepare(display,semantic,bytes,0x1000u,0x1000u+size,0x1001u));
                if(decoded.call_delay_bytes())
                {
                    auto unsafe=storage;for(unsigned b{};b!=4u;++b) { unsafe[4u+b]=bytes[b]; }
                    CHECK(!dbg::native_wasm_call_continuation::prepare(display,semantic,::std::span<::std::uint8_t const>{unsafe.data(),size},0x1000u,0x1000u+size,0x1000u));
                }
            }
            ++tested;
        }
        auto const check_step{[&](::std::uint32_t word,bool expected)
        {
            ::std::array<::std::uint8_t,12u> bytes{};
            ::std::uint32_t const words[]{word,row.nop,row.nop};
            for(unsigned w{};w!=3u;++w) for(unsigned b{};b!=4u;++b)
            { bytes[w*4u+b]=static_cast<::std::uint8_t>(words[w]>>(8u*(description.little_endian?b:3u-b))); }
            auto const decoded{semantic.decode(0x1000u,bytes)};
            CHECK(!expected || (decoded && decoded.safe_for_single_instruction() && !decoded.safe_for_public_display()));
            CHECK(expected || !decoded || (!decoded.safe_for_single_instruction() && !decoded.safe_for_public_display()));
            for(bool ni:{false,true})
            {
                auto const selected{dbg::native_wasm_step_boundary::prepare(display,semantic,bytes,0x1000u,0x100cu,0x1000u,ni)};
                CHECK(bool(selected)==expected);
                if(expected) { CHECK(selected.first_successor==0x1004u && selected.second_successor==0u); }
                CHECK(!dbg::native_wasm_step_boundary::prepare(display,semantic,
                    ::std::span<::std::uint8_t const>{bytes.data(),4u},0x1000u,0x1004u,0x1000u,ni));
            }
            CHECK(!semantic.decode(0x1000u,::std::span<::std::uint8_t const>{bytes.data(),3u}));
            return 0;
        }};
        auto const check_order{[&](::std::uint32_t word,bool expected)
        { ++memory_order_tested;return check_step(word,expected); }};
        if(triple.isAArch64())
        {
            for(auto word:{0x08dffec8u,0x48dffec8u,0x88dffec8u,0xc8dffec8u}) { CHECK(check_order(word,true)==0); }
            for(auto word:{0x085ffec8u,0xd4000001u,0xd69f03e0u}) { CHECK(check_order(word,false)==0); }
        }
        else if(triple.isRISCV())
        {
            auto const check_vector{[&](::std::uint32_t word,bool expected)
            { ++vector_configuration_tested;return check_step(word,expected); }};
            // Actual MC bytes, including every encoded immediate VTYPE. High
            // reserved bits, reserved LMUL and future SEW stay refused. This
            // proves execution DATA only; every admitted configuration row is
            // still hidden, and a successor outside the owner is rejected.
            for(unsigned type{};type!=1024u;++type)
            {
                bool const valid{type<=255u && (type & 7u)!=4u && ((type>>3u) & 7u)<=3u};
                CHECK(check_vector(0xc0007057u | (type<<20u) | (31u<<15u) | (5u<<7u),valid)==0);
            }
            for(unsigned type{};type!=2048u;++type)
            {
                bool const valid{type<=255u && (type & 7u)!=4u && ((type>>3u) & 7u)<=3u};
                CHECK(check_vector(0x00007057u | (type<<20u) | (10u<<15u) | (5u<<7u),valid)==0);
            }
            for(unsigned avl{};avl!=32u;++avl)
            { CHECK(check_vector(0xc0007057u | (0xd8u<<20u) | (avl<<15u),true)==0); }
            for(unsigned reg{};reg!=32u;++reg)
            {
                CHECK(check_vector(0xc0007057u | (0xd8u<<20u) | (4u<<15u) | (reg<<7u),true)==0);
                CHECK(check_vector(0x00007057u | (0xd8u<<20u) | (reg<<15u) | (5u<<7u),true)==0);
                CHECK(check_vector(0x80007057u | (reg<<20u) | (10u<<15u) | (5u<<7u),true)==0);
            }
            for(auto word:{0xc2002573u,0xc2102573u,0x00851073u,0x00f51073u,0x30051073u})
            { CHECK(check_vector(word,false)==0); }
            CHECK(check_order(0x0230000fu,true)==0);
            for(auto word:{0x0000100fu,0x00000073u,0x30051073u}) { CHECK(check_order(word,false)==0); }
        }
        else if(triple.getArch()==::llvm::Triple::sparcv9)
        {
            for(unsigned mask{};mask!=16u;++mask) { CHECK(check_order(0x8143e000u|mask,true)==0); }
            for(unsigned mask:{16u,31u,32u,64u,127u,128u,255u}) { CHECK(check_order(0x8143e000u|mask,false)==0); }
        }
        if(triple.isARM())
        {
            ::std::array<::std::uint8_t,24u> bytes{},mask{};
            ::std::uint32_t const words[]{row.nop,0xffffffffu,row.direct,row.nop,row.nop,row.nop};
            for(unsigned w{};w!=6u;++w) for(unsigned i{};i!=4u;++i)
            { bytes[w*4u+i]=static_cast<::std::uint8_t>(words[w]>>(8u*i));mask[w*4u+i]=w==1u?0u:1u; }
            auto const mapped{dbg::native_wasm_call_continuation::prepare(display,semantic,bytes,0x1000u,0x1018u,0x1008u,mask)};
            CHECK(mapped && mapped.continuation==0x100cu);
            CHECK(!dbg::native_wasm_call_continuation::prepare(display,semantic,bytes,0x1000u,0x1018u,0x1008u));
            for(unsigned i{12u};i!=16u;++i) { mask[i]=0u; }
            CHECK(!dbg::native_wasm_call_continuation::prepare(display,semantic,bytes,0x1000u,0x1018u,0x1008u,mask));
            ++tested;
        }
    }
    ::fast_io::io::println("PASS cross-call actual MC cases=",tested," memory-order-cases=",memory_order_tested," vector-configuration-cases=",vector_configuration_tested," runtime-qualified=false");
}
