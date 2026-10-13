// Actual LLVM MC DATA component only. It executes no decoded instruction,
// accesses no requested native PC and qualifies no trap/NI/finish backend.
#define UWVM_USE_LLVM_JIT 1
#if defined(_WIN32)
# define UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT 1
#endif
#if defined(__APPLE__) && defined(__x86_64__)
# define UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT 1
#endif
#include <uwvm2/uwvm/debugger/native_owned_instruction_semantics.h>
#include <array>
#include <limits>
#include <span>
#include <fast_io.h>

namespace owned_mc = ::uwvm2::uwvm::debugger::native_owned_instruction_semantics;
namespace branch = ::uwvm2::uwvm::debugger::native_branch_destination;
namespace semantics = ::uwvm2::uwvm::debugger::native_instruction_semantics;
#define CHECK(x) do { if(!(x)) { ::fast_io::print(::fast_io::err(), "direct branch DATA failure line=", ::fast_io::mnp::dec(__LINE__), "\n"); return 1; } } while(false)

constexpr bool checked_scalar_boundaries()
{
    ::std::uintptr_t value{17u};
    if(branch::add_displacement(0u, -1, value) || value != 17u) { return false; }
    if(branch::add_displacement((::std::numeric_limits<::std::uintptr_t>::max)(), 1, value) || value != 17u) { return false; }
    if(!branch::add_displacement(3u, -3, value) || value != 0u) { return false; }
    return !branch::add_displacement(0u, (::std::numeric_limits<::std::int64_t>::min)(), value) && value == 0u;
}
static_assert(checked_scalar_boundaries());
static_assert(!branch::result{});
static_assert(!owned_mc::decoded_instruction{}.destination());
static_assert(!owned_mc::decoded_instruction{}.safe_for_single_instruction());

int main()
{
    owned_mc::decoder decoder{};
#if (defined(__linux__) && defined(__x86_64__)) || \
    (defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))) || \
    (defined(__APPLE__) && (defined(__aarch64__) || defined(__x86_64__)) && defined(TARGET_OS_OSX) && TARGET_OS_OSX)
    CHECK(decoder);
    auto at{[&](::std::uintptr_t pc, auto const& bytes)
        { return decoder.decode(pc, ::std::span<::std::uint8_t const>{bytes}); }};
# if defined(__x86_64__) || defined(_M_X64)
    constexpr ::std::array<::std::uint8_t, 5u> call_bytes{0xe8u, 0u, 0u, 0u, 0u};
    auto const direct_call{at(0x1000u, call_bytes)};
    auto const back_call{at(0x1000u, ::std::array<::std::uint8_t, 5u>{0xe8u, 0xfbu, 0xffu, 0xffu, 0xffu})};
    auto const branch_forward{at(0x1000u, ::std::array<::std::uint8_t, 2u>{0x75u, 0x02u})};
    auto const branch_backward{at(0x1000u, ::std::array<::std::uint8_t, 2u>{0xebu, 0xfeu})};
    auto const indirect_call{at(0x1000u, ::std::array<::std::uint8_t, 2u>{0xffu, 0xd0u})};
    auto const rip_memory_call{at(0x1000u, ::std::array<::std::uint8_t, 6u>{0xffu, 0x15u, 0u, 0u, 0u, 0u})};
    auto const returned{at(0x1000u, ::std::array<::std::uint8_t, 1u>{0xc3u})};
    auto const ordinary{at(0x1000u, ::std::array<::std::uint8_t, 2u>{0x31u, 0xc0u})};
    CHECK(direct_call.destination() && direct_call.destination().display_pc == 0x1005u && !direct_call.destination().conditional);
    CHECK(back_call.destination() && back_call.destination().display_pc == 0x1000u);
    CHECK(branch_forward.destination() && branch_forward.destination().display_pc == 0x1004u && branch_forward.destination().conditional);
    CHECK(branch_backward.destination() && branch_backward.destination().display_pc == 0x1000u && !branch_backward.destination().conditional);
    CHECK(!indirect_call.destination() && !rip_memory_call.destination() && !returned.destination() && !ordinary.destination());
    CHECK(ordinary.safe_for_single_instruction() && !direct_call.safe_for_single_instruction());
    CHECK(!at(0x1000u, ::std::array<::std::uint8_t, 1u>{0xe8u}).destination());
    auto const below_zero{at(1u, ::std::array<::std::uint8_t, 2u>{0xebu, 0xfcu})};
    auto const over_max{at(UINTPTR_MAX - 16u, ::std::array<::std::uint8_t, 2u>{0xebu, 0x7fu})};
    auto const zero{at(1u, ::std::array<::std::uint8_t, 2u>{0xebu, 0xfdu})};
    CHECK(below_zero && !below_zero.destination() && below_zero.destination().reason == branch::unavailability::arithmetic_overflow);
    CHECK(over_max && !over_max.destination() && over_max.destination().reason == branch::unavailability::arithmetic_overflow);
    CHECK(zero.destination() && zero.destination().display_pc == 0u);
# else
    constexpr ::std::array<::std::uint8_t, 4u> call_bytes{0x02u, 0u, 0u, 0x94u}; // bl .+8
    auto const direct_call{at(0x1000u, call_bytes)};
    auto const back_call{at(0x1000u, ::std::array<::std::uint8_t, 4u>{0xffu, 0xffu, 0xffu, 0x97u})}; // bl .-4
    auto const branch_forward{at(0x1000u, ::std::array<::std::uint8_t, 4u>{0x40u, 0u, 0u, 0x54u})};
    auto const branch_backward{at(0x1000u, ::std::array<::std::uint8_t, 4u>{0xffu, 0xffu, 0xffu, 0x17u})};
    auto const indirect_call{at(0x1000u, ::std::array<::std::uint8_t, 4u>{0u, 0u, 0x3fu, 0xd6u})};
    auto const returned{at(0x1000u, ::std::array<::std::uint8_t, 4u>{0xc0u, 0x03u, 0x5fu, 0xd6u})};
    auto const ordinary{at(0x1000u, ::std::array<::std::uint8_t, 4u>{0x20u, 0u, 0x80u, 0x52u})};
    CHECK(direct_call.destination() && direct_call.destination().display_pc == 0x1008u);
    CHECK(back_call.destination() && back_call.destination().display_pc == 0xffcu);
    CHECK(branch_forward.destination() && branch_forward.destination().display_pc == 0x1008u && branch_forward.destination().conditional);
    CHECK(branch_backward.destination() && branch_backward.destination().display_pc == 0xffcu);
    CHECK(!indirect_call.destination() && !returned.destination() && !ordinary.destination());
    CHECK(ordinary.safe_for_single_instruction() && !direct_call.safe_for_single_instruction());
    CHECK(!at(0x1001u, call_bytes).destination());
# endif
    // Independent actual backend context for malformed-MC DATA bounds checks.
    // Exact target tables/analysis own all metadata; no production credential
    // is synthesized and no forged MC instruction is executed.
    ::llvm::Triple const triple{owned_mc::details::native_triple};
    ::std::string error{};
    auto const* target{owned_mc::details::lookup_target(triple, error)};
    CHECK(target);
    auto instructions{::std::unique_ptr<::llvm::MCInstrInfo>{target->createMCInstrInfo()}};
    auto registers{owned_mc::details::make_registers(*target, triple)};
    CHECK(instructions && registers);
    auto analysis{::std::unique_ptr<::llvm::MCInstrAnalysis>{target->createMCInstrAnalysis(instructions.get())}};
    auto assembly{owned_mc::details::make_assembly(*target, *registers, triple)};
    auto subtarget{owned_mc::details::make_subtarget(*target, triple)};
    CHECK(analysis && assembly && subtarget);
    auto context{owned_mc::details::make_context(triple, *assembly, *registers, *subtarget)};
    auto actual_decoder{::std::unique_ptr<::llvm::MCDisassembler>{target->createMCDisassembler(*subtarget, *context)}};
    CHECK(actual_decoder);
    ::llvm::MCInst instruction{};
    ::std::uint64_t size{};
    // [actual local complete call_bytes ... exact array size] end
    // [safe                                                   ] LLVM receives
    //  ^^ owned static bytes; display PC 0x1000 is never dereferenced.
    auto status{actual_decoder->getInstruction(instruction, size,
        ::llvm::ArrayRef<::std::uint8_t>{call_bytes.data(), call_bytes.size()}, 0x1000u, ::llvm::nulls())};
    auto const actual_class{semantics::classify(instruction, *instructions, *registers, status, size, call_bytes.size())};
    CHECK(actual_class && actual_class.kind == semantics::flow::call);
    CHECK(branch::evaluate(triple, instruction, *instructions, analysis.get(), actual_class, 0x1000u));
    CHECK(!branch::evaluate(triple, instruction, *instructions, nullptr, actual_class, 0x1000u));
    ::llvm::MCInst short_metadata{};
    short_metadata.setOpcode(instruction.getOpcode());
    CHECK(!branch::evaluate(triple, short_metadata, *instructions, analysis.get(), actual_class, 0x1000u));
    auto extra_operand{instruction}; extra_operand.addOperand(::llvm::MCOperand::createImm(0));
    CHECK(!branch::evaluate(triple, extra_operand, *instructions, analysis.get(), actual_class, 0x1000u));
    auto invalid_opcode{instruction}; invalid_opcode.setOpcode(instructions->getNumOpcodes());
    CHECK(!branch::evaluate(triple, invalid_opcode, *instructions, analysis.get(), actual_class, 0x1000u));
    auto unresolved{instruction};
    // [actual direct-call MC operands ... count > 0] end
    // [safe                                         ] successful direct target
    //  ^^ established a PC-relative immediate before this local DATA mutation.
    CHECK(unresolved.getNumOperands() != 0u);
    unresolved.getOperand(0u) = ::llvm::MCOperand::createReg(0u);
    CHECK(!branch::evaluate(triple, unresolved, *instructions, analysis.get(), actual_class, 0x1000u));
    for(unsigned iteration{}; iteration != 16u; ++iteration)
    {
        owned_mc::decoder independent{};
        CHECK(independent && independent.decode(0x1000u, call_bytes).destination());
    }
    ::fast_io::print(::fast_io::out(), "PASS actual LLVM direct control destination DATA: checked owned MC operands/PC arithmetic/target analysis, no indirect-memory/caller inference, no live read or control authority\n");
    return 0;
#else
    CHECK(!decoder && !decoder.decode(0x1000u, ::std::array<::std::uint8_t, 1u>{0x90u}).destination());
    ::fast_io::print(::fast_io::out(), "UNAVAILABLE direct destination DATA: unchanged native platform gates\n");
    return 77;
#endif
}
