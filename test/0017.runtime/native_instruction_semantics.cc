// Actual LLVM MC target descriptor test. It consumes owned bytes only and
// does not qualify a platform trap adapter or authorize native execution.
#define UWVM_USE_LLVM_JIT 1
#include <uwvm2/uwvm/debugger/native_instruction_semantics.h>
#include <uwvm2/uwvm/debugger/native_disassembly_abi.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/MCTargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Support/raw_ostream.h>
#include <array>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <fast_io.h>

namespace semantics = ::uwvm2::uwvm::debugger::native_instruction_semantics;
namespace abi = ::uwvm2::uwvm::debugger::native_disassembly_abi;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native instruction semantics failure line=", __LINE__); return 1; } } while(false)

// Probe the public API signatures rather than treating every development LLVM
// with the same major as equivalent. Ownership is unchanged on either path.
template<typename Target>
static auto make_registers(Target const& target, ::llvm::Triple const& triple)
{
    if constexpr(requires { target.createMCRegInfo(triple); })
    { return ::std::unique_ptr<::llvm::MCRegisterInfo>{target.createMCRegInfo(triple)}; }
    else { return ::std::unique_ptr<::llvm::MCRegisterInfo>{target.createMCRegInfo(triple.str())}; }
}
template<typename Target>
static auto make_assembly(Target const& target, ::llvm::MCRegisterInfo const& registers, ::llvm::Triple const& triple)
{
    if constexpr(requires { target.createMCAsmInfo(registers, triple, ::llvm::MCTargetOptions{}); })
    { return ::std::unique_ptr<::llvm::MCAsmInfo>{target.createMCAsmInfo(registers, triple, ::llvm::MCTargetOptions{})}; }
    else { return ::std::unique_ptr<::llvm::MCAsmInfo>{target.createMCAsmInfo(registers, triple.str(), ::llvm::MCTargetOptions{})}; }
}
template<typename Target>
static auto make_subtarget(Target const& target, ::llvm::Triple const& triple)
{
    if constexpr(requires { target.createMCSubtargetInfo(triple, "", ""); })
    { return ::std::unique_ptr<::llvm::MCSubtargetInfo>{target.createMCSubtargetInfo(triple, "", "")}; }
    else { return ::std::unique_ptr<::llvm::MCSubtargetInfo>{target.createMCSubtargetInfo(triple.str(), "", "")}; }
}
template<typename Context = ::llvm::MCContext>
static auto make_context(::llvm::Triple const& triple, ::llvm::MCAsmInfo const& assembly,
    ::llvm::MCRegisterInfo const& registers, ::llvm::MCSubtargetInfo const& subtarget)
{
    if constexpr(::std::is_constructible_v<Context, ::llvm::Triple const&, ::llvm::MCAsmInfo const&,
        ::llvm::MCRegisterInfo const&, ::llvm::MCSubtargetInfo const&>)
    { return ::std::make_unique<Context>(triple, assembly, registers, subtarget); }
    else { return ::std::make_unique<Context>(triple, ::std::addressof(assembly), ::std::addressof(registers), ::std::addressof(subtarget)); }
}

int main()
{
#if defined(__x86_64__) || defined(_M_X64)
    abi::uwvm_LLVMInitializeX86TargetInfo();
    abi::uwvm_LLVMInitializeX86Target();
    abi::uwvm_LLVMInitializeX86TargetMC();
    abi::uwvm_LLVMInitializeX86Disassembler();
    ::llvm::Triple triple{"x86_64-pc-linux-gnu"}; // Decoder bytes, independent of host OS.
#elif defined(__aarch64__) || defined(_M_ARM64)
    abi::uwvm_LLVMInitializeAArch64TargetInfo();
    abi::uwvm_LLVMInitializeAArch64Target();
    abi::uwvm_LLVMInitializeAArch64TargetMC();
    abi::uwvm_LLVMInitializeAArch64Disassembler();
    ::llvm::Triple triple{"aarch64-unknown-linux-gnu"};
#else
    ::fast_io::io::println("UNAVAILABLE native MC semantics test: this fixture covers x86_64 and AArch64 only");
    return 77;
#endif
#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64)
    ::std::string error{};
    auto const* target{::llvm::TargetRegistry::lookupTarget(triple.str(), error)};
    CHECK(target != nullptr);
    auto instructions{::std::unique_ptr<::llvm::MCInstrInfo>{target->createMCInstrInfo()}};
    auto registers{make_registers(*target, triple)};
    CHECK(instructions != nullptr && registers != nullptr);
    auto assembly{make_assembly(*target, *registers, triple)};
    auto subtarget{make_subtarget(*target, triple)};
    CHECK(assembly != nullptr && subtarget != nullptr);
    auto context{make_context(triple, *assembly, *registers, *subtarget)};
    auto decoder{::std::unique_ptr<::llvm::MCDisassembler>{target->createMCDisassembler(*subtarget, *context)}};
    CHECK(decoder != nullptr);
    ::llvm::MCInst ordinary{};
    ::std::uint64_t ordinary_size{};
    auto decode{[&](::std::span<::std::uint8_t const> owned)
    {
        ::llvm::MCInst instruction{};
        ::std::uint64_t size{};
        // [owned fixture bytes ... owned.size) end
        // [safe                                ] LLVM receives this bounded borrow;
        //  ^^ 0x1000 is display metadata, never dereferenced as a native PC.
        auto const status{decoder->getInstruction(instruction, size,
            ::llvm::ArrayRef<::std::uint8_t>{owned.data(), owned.size()}, 0x1000u, ::llvm::nulls())};
        auto const result{semantics::classify(instruction, *instructions, *registers, status, size, owned.size())};
        if(result.kind == semantics::flow::ordinary) { ordinary = instruction; ordinary_size = size; }
        return result;
    }};
#if defined(__x86_64__) || defined(_M_X64)
    auto const direct_call{decode(::std::array<::std::uint8_t,5>{0xe8u,0u,0u,0u,0u})};
    auto const indirect_call{decode(::std::array<::std::uint8_t,2>{0xffu,0xd0u})};
    auto const returned{decode(::std::array<::std::uint8_t,1>{0xc3u})};
    auto const conditional{decode(::std::array<::std::uint8_t,2>{0x75u,0x02u})};
    auto const indirect_branch{decode(::std::array<::std::uint8_t,2>{0xffu,0xe0u})};
    auto const trap{decode(::std::array<::std::uint8_t,2>{0x0fu,0x0bu})};
    auto const plain{decode(::std::array<::std::uint8_t,2>{0x31u,0xc0u})};
    CHECK(!decode(::std::array<::std::uint8_t,1>{0xe8u}));
#else
    auto const direct_call{decode(::std::array<::std::uint8_t,4>{0u,0u,0u,0x94u})};
    auto const indirect_call{decode(::std::array<::std::uint8_t,4>{0u,0u,0x3fu,0xd6u})};
    auto const returned{decode(::std::array<::std::uint8_t,4>{0xc0u,0x03u,0x5fu,0xd6u})};
    auto const conditional{decode(::std::array<::std::uint8_t,4>{0x40u,0u,0u,0x54u})};
    auto const indirect_branch{decode(::std::array<::std::uint8_t,4>{0u,0u,0x1fu,0xd6u})};
    auto const trap{decode(::std::array<::std::uint8_t,4>{0u,0u,0x20u,0xd4u})};
    auto const plain{decode(::std::array<::std::uint8_t,4>{0x20u,0u,0x80u,0x52u})};
    CHECK(!decode(::std::array<::std::uint8_t,3>{0u,0u,0u}));
#endif
    CHECK(direct_call.kind == semantics::flow::call && direct_call.may_change_pc);
    CHECK(indirect_call.kind == semantics::flow::call && indirect_call.may_change_pc);
    CHECK(returned.kind == semantics::flow::return_instruction && returned.may_change_pc && returned.barrier);
    CHECK(conditional.kind == semantics::flow::branch && conditional.conditional_branch && conditional.may_change_pc);
    CHECK(indirect_branch.kind == semantics::flow::branch && indirect_branch.indirect_branch && indirect_branch.may_change_pc);
    CHECK(trap.kind == semantics::flow::trap && trap.may_change_pc);
    CHECK(plain.kind == semantics::flow::ordinary && !plain.may_change_pc && ordinary_size != 0u);
    CHECK(!semantics::classify(ordinary, *instructions, *registers, ::llvm::MCDisassembler::SoftFail, ordinary_size, 15u));
    CHECK(!semantics::classify(ordinary, *instructions, *registers, ::llvm::MCDisassembler::Fail, ordinary_size, 15u));
    CHECK(!semantics::classify(ordinary, *instructions, *registers, ::llvm::MCDisassembler::Success, 0u, 15u));
    CHECK(!semantics::classify(ordinary, *instructions, *registers, ::llvm::MCDisassembler::Success, ordinary_size, 0u));
    ::llvm::MCInst forged{};
    forged.setOpcode(instructions->getNumOpcodes());
    CHECK(!semantics::classify(forged, *instructions, *registers, ::llvm::MCDisassembler::Success, 1u, 1u));
    forged.setOpcode(ordinary.getOpcode()); // Valid opcode with missing real decoded definitions.
    CHECK(!semantics::classify(forged, *instructions, *registers, ::llvm::MCDisassembler::Success, ordinary_size, 15u));
    CHECK(ordinary.getNumOperands() != 0u);
    ordinary.getOperand(0u) = ::llvm::MCOperand::createReg(registers->getNumRegs());
    CHECK(!semantics::classify(ordinary, *instructions, *registers, ::llvm::MCDisassembler::Success, ordinary_size, 15u));
    ::fast_io::io::println("PASS real LLVM native MC call/return/branch/trap descriptors, owned bytes, bounded opcode/operands/registers, failed/soft-failed decode rejection");
#endif
}
