// Actual target LLVM MC memory-order semantics; no execution authority.
#include <uwvm2/uwvm/debugger/native_owned_instruction_semantics.h>
#include <llvm/Support/TargetSelect.h>
#include <array>
#include <string>
#include <fast_io.h>
namespace mc = ::uwvm2::uwvm::debugger::native_owned_instruction_semantics;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native memory-order semantics failure line=",__LINE__); return 1; } } while(false)
int main()
{
#if defined(UWVM_USE_LLVM_JIT) && (defined(__aarch64__) || defined(__riscv) || defined(__i386__))
    CHECK(!::llvm::InitializeNativeTarget());
    CHECK(!::llvm::InitializeNativeTargetDisassembler());
    struct description
    {
        ::std::array<char,64u> triple{};
        ::std::array<char,64u> cpu{},features{};
        ::std::size_t triple_size{},cpu_size{},features_size{};
        unsigned description_version{1u},pointer_bits{64u},maximum_instruction_bytes{},minimum_instruction_alignment{};
        bool available{true},little_endian{true};
    } target{};
#if defined(__aarch64__)
    char const* const triple{"aarch64-linux-gnu"};
#elif defined(__i386__)
    char const* const triple{"i686-linux-gnu"}; target.pointer_bits=32u;
#else
    char const* const triple{"riscv64-linux-gnu"};
    constexpr char feature[]{"+m,+a,+f,+d,+c,+zicsr"};
    for(char c:feature) { if(c!='\0') { target.features[target.features_size++]=c; } }
#endif
    for(;triple[target.triple_size]!='\0';++target.triple_size) { target.triple[target.triple_size]=triple[target.triple_size]; }
    ::llvm::Triple actual{triple}; ::std::string error{};
    auto* const backend{mc::details::lookup_target(actual,error)}; CHECK(backend);
    auto registers{mc::details::make_registers(*backend,actual)}; CHECK(registers);
    ::llvm::MCTargetOptions options{};
    auto assembly{mc::details::make_assembly(*backend,*registers,actual,options)}; CHECK(assembly);
    auto subtarget{mc::details::make_subtarget(*backend,actual,{}, {target.features.data(),target.features_size})}; CHECK(subtarget);
    target.maximum_instruction_bytes=mc::details::maximum_instruction_length(*assembly,*subtarget,actual);
    target.minimum_instruction_alignment=assembly->getMinInstAlignment();
    mc::decoder decoder{target}; CHECK(decoder);
    auto decode{[&](::std::uint32_t bits)
    {
        ::std::array<::std::uint8_t,4u> owned{};
        for(unsigned i{};i!=4u;++i) { owned[i]=static_cast<::std::uint8_t>(bits>>(8u*i)); }
        return decoder.decode(0x1000u,owned);
    }};
#if defined(__aarch64__)
    for(auto bits:{0x08dffec8u,0x48dffec8u,0x88dffec8u,0xc8dffec8u})
    {
        auto const ordered{decode(bits)};
        CHECK(ordered && ordered.safe_for_single_instruction() && !ordered.safe_for_public_display());
    }
    if(!decode(0x0b1502f5u).safe_for_public_display())
    {
        auto instructions{::std::unique_ptr<::llvm::MCInstrInfo>{backend->createMCInstrInfo()}};
        auto context{mc::details::make_context(actual,*assembly,*registers,*subtarget)};
        auto raw{::std::unique_ptr<::llvm::MCDisassembler>{backend->createMCDisassembler(*subtarget,*context)}};
        ::llvm::MCInst instruction{}; ::std::uint64_t size{};
        constexpr ::std::array<::std::uint8_t,4u> bytes{0xf5u,0x02u,0x15u,0x0bu};
        CHECK(raw->getInstruction(instruction,size,bytes,0x1000u,::llvm::nulls()) == ::llvm::MCDisassembler::Success);
        auto const& descriptor{instructions->get(instruction.getOpcode())};
        ::fast_io::io::perrln("ADD display diagnostic exact-opcode=",instructions->getName(instruction.getOpcode())=="ADDWrs",
            " actual-operands=",instruction.getNumOperands()," descriptor-operands=",descriptor.getNumOperands(),
            " load=",descriptor.mayLoad()," store=",descriptor.mayStore()," effects=",descriptor.hasUnmodeledSideEffects(),
            " flags=",instruction.getFlags());
        for(unsigned i{};i!=instruction.getNumOperands();++i)
        {
            auto const& operand{instruction.getOperand(i)}; auto const& info{*(descriptor.operands().begin()+i)};
            ::fast_io::io::perrln("operand=",i," type=",unsigned(info.OperandType)," class=",info.RegClass,
                " reg=",operand.isReg()," imm=",operand.isImm()," scalar=",operand.isImm()?operand.getImm():0,
                " in-fixed-class=",operand.isReg() && info.RegClass>=0 && unsigned(info.RegClass)<registers->getNumRegClasses() && registers->getRegClass(unsigned(info.RegClass)).contains(operand.getReg()));
        }
    }
    CHECK(decode(0x0b1502f5u).safe_for_public_display()); // add w21,w23,w21: packed shifted-register Operand
    CHECK(decode(0x4a150315u).safe_for_public_display()); // eor w21,w24,w21
    CHECK(!decode(0x910043ffu).safe_for_public_display()); // add sp,sp,#16
    CHECK(!decode(0x085ffec8u).safe_for_single_instruction()); // ldaxrb: exclusive monitor
    CHECK(!decode(0xd4000001u).safe_for_single_instruction()); // svc
    CHECK(!decode(0xd69f03e0u).safe_for_single_instruction()); // eret
#elif defined(__i386__)
    CHECK(decode(0x0000d801u).safe_for_public_display()); // add eax,ebx
    CHECK(decode(0x0000d831u).safe_for_public_display()); // xor eax,ebx
    CHECK(!decode(0x0000038du).safe_for_public_display()); // lea eax,[ebx]
    CHECK(!decode(0x0000340fu).safe_for_single_instruction()); // sysenter
#else
    auto const ordered{decode(0x0230000fu)}; // fence r,rw emitted by acquire byte load
    CHECK(ordered && ordered.safe_for_single_instruction() && !ordered.safe_for_public_display());
    CHECK(!decode(0x0000100fu).safe_for_single_instruction()); // fence.i changes instruction view
    CHECK(!decode(0x00000073u).safe_for_single_instruction()); // ecall
    CHECK(!decode(0x30051073u).safe_for_single_instruction()); // csrrw mstatus
#endif
    ::fast_io::io::println("PASS actual target MC: bounded acquire/fence execution semantics, hidden memory/order rows, exclusive/system operations refused");
    return 0;
#else
    return 77;
#endif
}
