// Real LLVM MC decoder component. No native instruction is executed, no trap
// adapter is qualified, and no caller-supplied PC is a live-memory request.
#define UWVM_USE_LLVM_JIT 1
#if defined(_WIN32)
# define UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT 1
#endif
#if defined(__APPLE__) && defined(__x86_64__)
# define UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT 1
#endif
#include <uwvm2/uwvm/debugger/native_owned_instruction_semantics.h>
#include <array>
#include <cstring>
#include <uwvm2/uwvm/debugger/native_wasm_step_boundary.h>
#include <concepts>
#include <span>
#include <type_traits>
#include <fast_io.h>
#include <fast_io_dsal/string_view.h>

namespace owned_mc = ::uwvm2::uwvm::debugger::native_owned_instruction_semantics;
namespace semantics = ::uwvm2::uwvm::debugger::native_instruction_semantics;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("owned native MC failure line=", __LINE__); return 1; } } while(false)

static_assert(!::std::is_constructible_v<owned_mc::decoded_instruction, semantics::classification>);
static_assert(::std::same_as<decltype(::std::declval<owned_mc::decoder&>().decode(
    ::std::uintptr_t{}, ::std::span<::std::uint8_t const>{})), owned_mc::decoded_instruction>);
static_assert(!owned_mc::decoded_instruction{}.safe_for_single_instruction());


static void failed_actual_mc_word(::std::uint32_t word)
{
    ::llvm::Triple const triple{owned_mc::details::native_triple};::std::string error{};
    auto const* target{owned_mc::details::lookup_target(triple,error)};
    if(target==nullptr)return;
    auto info{::std::unique_ptr<::llvm::MCInstrInfo>{target->createMCInstrInfo()}};
    auto registers{owned_mc::details::make_registers(*target,triple)};
    ::llvm::MCTargetOptions options{};
    auto assembly{owned_mc::details::make_assembly(*target,*registers,triple,options)};
    auto subtarget{owned_mc::details::make_subtarget(*target,triple)};
    auto context{owned_mc::details::make_context(triple,*assembly,*registers,*subtarget)};
    auto raw{::std::unique_ptr<::llvm::MCDisassembler>{target->createMCDisassembler(*subtarget,*context)}};
    ::std::array<::std::uint8_t,4u> bytes{};::std::memcpy(bytes.data(),&word,4u);
    ::llvm::MCInst instruction{};::std::uint64_t size{};
    auto const status{raw->getInstruction(instruction,size,bytes,0x1000u,::llvm::nulls())};
    ::fast_io::io::perrln("actual MC refusal status=",static_cast<unsigned>(status)," bytes=",size,
        " flags=",instruction.getFlags()," operands=",instruction.getNumOperands()," opcode=",instruction.getOpcode());
    if(status==::llvm::MCDisassembler::Success && instruction.getOpcode()<info->getNumOpcodes())
    {
        auto const name{info->getName(instruction.getOpcode())};auto const& descriptor{info->get(instruction.getOpcode())};
        ::fast_io::io::perrln("actual MC name=",::fast_io::string_view{name.data(),name.size()},
            " declared=",descriptor.getNumOperands()," effects=",descriptor.hasUnmodeledSideEffects(),
            " load=",descriptor.mayLoad()," store=",descriptor.mayStore()," control=",descriptor.isTerminator());
        for(unsigned i{};i<instruction.getNumOperands() && i<descriptor.getNumOperands();++i)
        {
            auto const& operand{instruction.getOperand(i)};auto const& info{*(descriptor.operands().begin()+i)};
            ::fast_io::io::perrln("actual operand=",i," type=",static_cast<unsigned>(info.OperandType),
                " predicate=",info.isPredicate()," optional=",info.isOptionalDef(),
                " reg=",operand.isReg() ? static_cast<long>(operand.getReg()) : -1l,
                " imm=",operand.isImm() ? operand.getImm() : 0);
        }

    }
}

int main()
{
    // Target MC identifiers, not displayed aliases: infrastructure must stay
    // private even for an otherwise ordinary register/immediate ALU opcode.
    struct protected_case { char const* triple; char const* hidden; char const* numeric; };
    for(auto const test: ::std::array{
        protected_case{"x86_64-linux-gnu","SPL","RAX"}, protected_case{"aarch64-linux-gnu","W29","X4"},
        protected_case{"powerpc64-linux-gnu","X2","X4"}, protected_case{"powerpc-linux-gnu","R1","R4"},
        protected_case{"riscv64-linux-gnu","X2","X10"}, protected_case{"mips64-linux-gnuabi64","GP_64","T0_64"},
        protected_case{"loongarch64-linux-gnu","R3","R4"}, protected_case{"sparc64-linux-gnu","O6","O0"},
        protected_case{"s390x-linux-gnu","R15D","R4D"}, protected_case{"arm-linux-gnueabihf","R13","R4"}})
    {
        ::llvm::Triple const triple{test.triple};
        CHECK(owned_mc::details::private_register_operand(triple,test.hidden));
        CHECK(!owned_mc::details::private_register_operand(triple,test.numeric));
    }
    owned_mc::decoder decoder{};
#if (defined(__linux__) && defined(__x86_64__)) || \
    (defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))) || \
    (defined(__APPLE__) && (defined(__aarch64__) || defined(__x86_64__)) && defined(TARGET_OS_OSX) && TARGET_OS_OSX)
    CHECK(decoder);
    auto decode{[&](auto const& bytes) { return decoder.decode(0x1000u, ::std::span<::std::uint8_t const>{bytes}); }};
# if defined(__x86_64__) || defined(_M_X64)
    // The production constructor consumes an owned target description. LLVM23
    // X86 MCAsmInfo's inline-assembly estimate must not reject its 15-byte bound.
    struct target_description
    {
        ::std::array<char,64u> triple{};
        ::std::array<char,1u> cpu{}, features{};
        ::std::size_t triple_size{}, cpu_size{}, features_size{};
        unsigned description_version{1u}, pointer_bits{64u}, maximum_instruction_bytes{15u}, minimum_instruction_alignment{1u};
        bool available{true}, little_endian{true};
    } target_description{};
    for(; owned_mc::details::native_triple[target_description.triple_size] != '\0'; ++target_description.triple_size)
    {
        CHECK(target_description.triple_size + 1u < target_description.triple.size());
        target_description.triple[target_description.triple_size] = owned_mc::details::native_triple[target_description.triple_size];
    }
    owned_mc::decoder described{target_description};
    CHECK(described);
    constexpr ::std::array<::std::uint8_t,10u> wide{0x48u,0xb8u,1u,2u,3u,4u,5u,6u,7u,8u}; // movabs rax, imm64
    auto const wide_decoded{described.decode(0x1000u,wide)};
    CHECK(wide_decoded && wide_decoded.safe_for_single_instruction() && wide_decoded.semantics().size == wide.size());
    CHECK(!described.decode(0x1000u,{wide.data(),wide.size()-1u}));
    target_description.maximum_instruction_bytes = 4u;
    owned_mc::decoder mismatched{target_description};
    CHECK(!mismatched); // Unmatched target data still refuses.
    constexpr ::std::array<::std::uint8_t, 2u> plain{0x31u, 0xc0u}; // xor eax, eax
    auto const ordinary{decode(plain)};
    auto const direct_call{decode(::std::array<::std::uint8_t, 5u>{0xe8u, 0u, 0u, 0u, 0u})};
    auto const indirect_call{decode(::std::array<::std::uint8_t, 2u>{0xffu, 0xd0u})};
    auto const returned{decode(::std::array<::std::uint8_t, 1u>{0xc3u})};
    CHECK(returned && returned.near_return_pop_bytes() == 0u);
    for(unsigned pop : {0u,8u,16u,65535u})
    {
        auto const callee_pop{decode(::std::array<::std::uint8_t,3u>{0xc2u,
            static_cast<::std::uint8_t>(pop),static_cast<::std::uint8_t>(pop >> 8u)})};
        CHECK(callee_pop && callee_pop.semantics().kind == semantics::flow::return_instruction &&
            callee_pop.near_return_pop_bytes() == pop && !callee_pop.safe_for_single_instruction());
    }
    CHECK(!decode(::std::array<::std::uint8_t,2u>{0xc2u,8u}));
    CHECK(decode(::std::array<::std::uint8_t,1u>{0xcbu}).near_return_pop_bytes() == SIZE_MAX);

    auto const branch{decode(::std::array<::std::uint8_t, 2u>{0x75u, 0x02u})};
    auto const indirect_branch{decode(::std::array<::std::uint8_t, 2u>{0xffu, 0xe0u})};
    auto const trap{decode(::std::array<::std::uint8_t, 2u>{0x0fu, 0x0bu})};
    CHECK(!decode(::std::array<::std::uint8_t, 1u>{0xe8u}));
    CHECK(!decode(::std::array<::std::uint8_t, 1u>{0x0fu}));
    CHECK(!decode(::std::array<::std::uint8_t, 15u>{
        0x66u,0x66u,0x66u,0x66u,0x66u,0x66u,0x66u,0x66u,0x66u,0x66u,0x66u,0x66u,0x66u,0x66u,0x66u}).safe_for_single_instruction());

    // These are actual MC-decodable bytes, not forged descriptor flags. REP
    // can trap after an iteration at the SAME PC; interrupts/system operations
    // may not have an MC PC definition. Neither is an ordinary NI fallthrough.
    auto const repeated{decode(::std::array<::std::uint8_t, 2u>{0xf3u, 0xa4u})}; // rep movsb
    auto const interrupted{decode(::std::array<::std::uint8_t, 1u>{0xccu})}; // int3
    auto const system_call{decode(::std::array<::std::uint8_t, 2u>{0x0fu, 0x05u})};
    auto const halt{decode(::std::array<::std::uint8_t, 1u>{0xf4u})};
    auto const interrupts{decode(::std::array<::std::uint8_t, 1u>{0xfbu})}; // sti
    auto const shadow{decode(::std::array<::std::uint8_t, 2u>{0x8eu, 0xd0u})}; // mov ss, ax
    auto const flags_restore{decode(::std::array<::std::uint8_t, 1u>{0x9du})}; // popfq can clear TF
    auto const interrupt_return{decode(::std::array<::std::uint8_t, 2u>{0x48u, 0xcfu})}; // iretq
    CHECK(repeated && interrupted && system_call && halt && interrupts && shadow && flags_restore && interrupt_return);
    CHECK(!repeated.safe_for_single_instruction() && !interrupted.safe_for_single_instruction() &&
          !system_call.safe_for_single_instruction() && !halt.safe_for_single_instruction() &&
          !interrupts.safe_for_single_instruction() && !shadow.safe_for_single_instruction() &&
          !flags_restore.safe_for_single_instruction() && !interrupt_return.safe_for_single_instruction());
    CHECK(repeated.semantics().kind == semantics::flow::other_control);

    // Operand width changes are ordinary; repeating that same width still
    // refuses. These real decoded forms exercise both x64 and i686 MC tables.
    auto const width_move{decode(::std::array<::std::uint8_t,3u>{0x66u,0x89u,0xc0u})};
    auto const width_repeat{decode(::std::array<::std::uint8_t,3u>{0x66u,0xf3u,0xa5u})};
    CHECK(width_move && width_move.safe_for_single_instruction() && width_move.safe_for_public_display());
    CHECK(width_repeat && !width_repeat.safe_for_single_instruction() && !width_repeat.safe_for_public_display());

    // Mandatory SSE prefixes select these genuine numeric MC opcodes. They
    // must not be confused with REP iteration at the same PC. This DATA test
    // never executes the literal bytes or grants a register/memory capability.
    auto const vector_move{decode(::std::array<::std::uint8_t,4u>{0xf3u,0x0fu,0x6fu,0xc1u})};
    auto const vector_load{decode(::std::array<::std::uint8_t,4u>{0xf3u,0x0fu,0x6fu,0x01u})};
    auto const vector_store{decode(::std::array<::std::uint8_t,4u>{0xf3u,0x0fu,0x7fu,0x01u})};
    auto const vector_xor{decode(::std::array<::std::uint8_t,4u>{0x66u,0x0fu,0xefu,0xc1u})};
    auto const vector_unpack{decode(::std::array<::std::uint8_t,4u>{0x66u,0x0fu,0x6cu,0xc0u})};
    for(auto const value: {vector_move,vector_load,vector_store,vector_xor,vector_unpack})
    { CHECK(value && value.semantics().size == 4u && value.safe_for_single_instruction()); }
    CHECK(vector_move.safe_for_public_display() && vector_xor.safe_for_public_display() && vector_unpack.safe_for_public_display());
    CHECK(!vector_load.safe_for_public_display() && !vector_store.safe_for_public_display());

    // Additional REAL native-x64 MC forms. LLVM decodes their width/opcode;
    // this fixture never executes a selector load or restores machine flags.
    auto const flags_restore16{decode(::std::array<::std::uint8_t, 2u>{0x66u, 0x9du})};
    auto const shadow64{decode(::std::array<::std::uint8_t, 3u>{0x48u, 0x8eu, 0xd0u})};
    auto const shadow_memory{decode(::std::array<::std::uint8_t, 2u>{0x8eu, 0x10u})};
    auto const stack_load32{decode(::std::array<::std::uint8_t, 3u>{0x0fu, 0xb2u, 0u})};
    auto const stack_load64{decode(::std::array<::std::uint8_t, 4u>{0x48u, 0x0fu, 0xb2u, 0u})};
    auto const interrupt_return16{decode(::std::array<::std::uint8_t, 2u>{0x66u, 0xcfu})};
    auto const interrupt_return32{decode(::std::array<::std::uint8_t, 1u>{0xcfu})};
    CHECK(flags_restore16 && shadow64 && shadow_memory && stack_load32 && stack_load64 &&
          interrupt_return16 && interrupt_return32);
    CHECK(!flags_restore16.safe_for_single_instruction() && !shadow64.safe_for_single_instruction() &&
          !shadow_memory.safe_for_single_instruction() && !stack_load32.safe_for_single_instruction() &&
          !stack_load64.safe_for_single_instruction() && !interrupt_return16.safe_for_single_instruction() &&
          !interrupt_return32.safe_for_single_instruction());
    CHECK(interrupt_return.semantics().kind == semantics::flow::return_instruction &&
          interrupt_return16.semantics().kind == semantics::flow::return_instruction &&
          interrupt_return32.semantics().kind == semantics::flow::return_instruction);
    // POP SS is illegal in native long mode. Any decoder refusal is safe; if a
    // provider still yields its known opcode, the semantic veto must refuse it.
    CHECK(!decode(::std::array<::std::uint8_t, 1u>{0x17u}).safe_for_single_instruction());

    // Inspect actual MC tables, rather than constructing a fake descriptor or
    // treating a 32-bit opcode as native-x64 execution. Verify all known family
    // identities are present in THIS SDK, even forms not decodable in long mode.
    ::llvm::Triple const checked_triple{owned_mc::details::native_triple};
    ::std::string checked_error{};
    auto const* checked_target{owned_mc::details::lookup_target(checked_triple, checked_error)};
    CHECK(checked_target != nullptr);
    auto checked_instructions{::std::unique_ptr<::llvm::MCInstrInfo>{checked_target->createMCInstrInfo()}};
    CHECK(checked_instructions != nullptr);
    ::std::size_t semantic_veto_identities{};
    for(unsigned opcode{}; opcode != checked_instructions->getNumOpcodes(); ++opcode)
    {
        // [actual owned MC opcode/name tables ... getNumOpcodes) end
        // [safe                                                   ] opcode < N
        //  ^^ before name/descriptor lookup, no native PC or guest bytes read.
        if(owned_mc::details::changes_native_debug_trap_state(checked_instructions->getName(opcode)))
        { ++semantic_veto_identities; }
    }
    CHECK(semantic_veto_identities == 16u);

    // Actual near memory calls retain only decoded scalar operands. No
    // operand grants a native read, TF step, or publicly printable address.
    auto const slot_call{decode(::std::array<::std::uint8_t, 2u>{0xffu, 0x10u})}; // call [rax]
    auto const indexed_call{decode(::std::array<::std::uint8_t, 4u>{0xffu, 0x54u, 0xcbu, 0x08u})}; // [rbx+rcx*8+8]
    auto const rip_call{decode(::std::array<::std::uint8_t, 6u>{0xffu, 0x15u, 0x04u, 0u, 0u, 0u})};
    auto const tls_call{decode(::std::array<::std::uint8_t, 3u>{0x64u, 0xffu, 0x10u})};
    auto const narrow_call{decode(::std::array<::std::uint8_t, 3u>{0x67u, 0xffu, 0x10u})};
    CHECK(slot_call && indexed_call && rip_call && tls_call && narrow_call);
    CHECK(slot_call.call_memory().valid && slot_call.call_memory().base == 0u &&
          slot_call.call_memory().index == SIZE_MAX && slot_call.call_memory().scale == 1u);
    CHECK(indexed_call.call_memory().valid && indexed_call.call_memory().base == 1u &&
          indexed_call.call_memory().index == 2u && indexed_call.call_memory().scale == 8u &&
          indexed_call.call_memory().displacement == 8);
    CHECK(rip_call.call_memory().valid && rip_call.call_memory().rip_relative &&
          rip_call.call_memory().index == SIZE_MAX && rip_call.call_memory().displacement == 4);
    CHECK(!tls_call.call_memory().valid && !narrow_call.call_memory().valid);
    CHECK(!slot_call.safe_for_public_display() && !indexed_call.safe_for_public_display() && !rip_call.safe_for_public_display());
    CHECK(!slot_call.safe_for_single_instruction() && !indexed_call.safe_for_single_instruction() &&
          !rip_call.safe_for_single_instruction() && !tls_call.safe_for_single_instruction() &&
          !narrow_call.safe_for_single_instruction());

    // An impossible display address is intentionally usable for a scalar XOR
    // decode. It cannot become a read through address 1 in this DATA helper.
    CHECK(decoder.decode(1u, plain).safe_for_single_instruction());
# else
    constexpr ::std::array<::std::uint8_t, 4u> plain{0x20u, 0u, 0x80u, 0x52u}; // mov w0, #1
    auto const ordinary{decode(plain)};
    auto const direct_call{decode(::std::array<::std::uint8_t, 4u>{0u, 0u, 0u, 0x94u})};
    auto const indirect_call{decode(::std::array<::std::uint8_t, 4u>{0u, 0u, 0x3fu, 0xd6u})};
    auto const returned{decode(::std::array<::std::uint8_t, 4u>{0xc0u, 0x03u, 0x5fu, 0xd6u})};
    auto const branch{decode(::std::array<::std::uint8_t, 4u>{0x40u, 0u, 0u, 0x54u})};
    auto const indirect_branch{decode(::std::array<::std::uint8_t, 4u>{0u, 0u, 0x1fu, 0xd6u})};
    auto const trap{decode(::std::array<::std::uint8_t, 4u>{0u, 0u, 0x20u, 0xd4u})};
    CHECK(!decode(::std::array<::std::uint8_t, 3u>{0u, 0u, 0u}));
    CHECK(!decode(::std::array<::std::uint8_t, 4u>{0u, 0u, 0u, 0u}));
    CHECK(!decoder.decode(0x1001u, plain));
    CHECK(decoder.decode(4u, plain).safe_for_single_instruction());

    // LLVM's real DecodeSignedLdStInstruction explicitly returns SoftFail
    // when an indexed load writes back to its non-SP transfer register.
    // ldr w0, [x0], #4 is disassemblable, architecturally constrained, rejected.
    constexpr ::std::array<::std::uint8_t, 4u> constrained{0u, 0x44u, 0x40u, 0xb8u};
    CHECK(!decode(constrained));
    ::llvm::Triple const raw_triple{owned_mc::details::native_triple};
    ::std::string raw_error{};
    auto const* raw_target{owned_mc::details::lookup_target(raw_triple, raw_error)};
    CHECK(raw_target != nullptr);
    auto raw_registers{owned_mc::details::make_registers(*raw_target, raw_triple)};
    CHECK(raw_registers != nullptr);
    auto raw_assembly{owned_mc::details::make_assembly(*raw_target, *raw_registers, raw_triple)};
    auto raw_subtarget{owned_mc::details::make_subtarget(*raw_target, raw_triple)};
    CHECK(raw_assembly != nullptr && raw_subtarget != nullptr);
    auto raw_context{owned_mc::details::make_context(raw_triple, *raw_assembly, *raw_registers, *raw_subtarget)};
    auto raw_decoder{::std::unique_ptr<::llvm::MCDisassembler>{raw_target->createMCDisassembler(*raw_subtarget, *raw_context)}};
    CHECK(raw_decoder != nullptr);
    ::llvm::MCInst raw_instruction{};
    ::std::uint64_t raw_size{};
    // [owned exact four-byte constrained instruction] end
    // [safe                                       ] actual LLVM backend receives
    //  ^^ this fixture's bounded copy, never the display address as a pointer.
    auto const constrained_status{raw_decoder->getInstruction(raw_instruction, raw_size,
        ::llvm::ArrayRef<::std::uint8_t>{constrained.data(), constrained.size()}, 0x1000u, ::llvm::nulls())};
    CHECK(constrained_status == ::llvm::MCDisassembler::SoftFail && raw_size == constrained.size());
# endif
    CHECK(ordinary && ordinary.safe_for_single_instruction());
    CHECK(ordinary.semantics().kind == semantics::flow::ordinary && ordinary.semantics().size == plain.size());
    CHECK(direct_call.semantics().kind == semantics::flow::call && !direct_call.safe_for_single_instruction());
    CHECK(indirect_call.semantics().kind == semantics::flow::call && !indirect_call.safe_for_single_instruction());
    CHECK(returned.semantics().kind == semantics::flow::return_instruction && !returned.safe_for_single_instruction());
    CHECK(branch.semantics().kind == semantics::flow::branch && branch.semantics().conditional_branch && !branch.safe_for_single_instruction());
    CHECK(indirect_branch.semantics().kind == semantics::flow::branch && indirect_branch.semantics().indirect_branch &&
          !indirect_branch.safe_for_single_instruction());
    CHECK(trap.semantics().kind == semantics::flow::trap && !trap.safe_for_single_instruction());
    CHECK(!decoder.decode(0u, plain));
    CHECK(!decoder.decode(UINTPTR_MAX, plain));
    CHECK(!decoder.decode(0x1000u, {}));
    for(unsigned iteration{}; iteration != 32u; ++iteration)
    {
        owned_mc::decoder independent{};
        CHECK(independent && independent.decode(0x1000u, plain).safe_for_single_instruction());
    }
    ::fast_io::io::println("PASS real LLVM owned MC decoder: DATA-only scalar result, call/return/branch/trap refusal, conservative effects/prefix filtering, bounded copy, truncated/invalid/overflow rejection, context retirement");
    return 0;
#elif defined(__linux__) && defined(__i386__)
    CHECK(decoder);
    auto decode{[&](auto const& bytes) { return decoder.decode(0x1000u,::std::span<::std::uint8_t const>{bytes}); }};
    // Genuine LLVM i686 decodes of the VEX materializer's physical XMM
    // moves, including the width flag which previously stopped before every
    // f64/v128 lexical witness. Memory operands remain completely hidden.
    for(auto const bytes: ::std::array{
        ::std::array<::std::uint8_t,4u>{0xc5u,0xf9u,0x6eu,0xc0u}, // vmovd eax,xmm0
        ::std::array<::std::uint8_t,4u>{0xc5u,0xf9u,0x6fu,0xc1u}}) // vmovdqa xmm1,xmm0
    {
        auto const value{decode(bytes)};
        CHECK(value && value.safe_for_single_instruction() && value.safe_for_public_display());
        CHECK(!decode(::std::span<::std::uint8_t const>{bytes.data(),bytes.size()-1u}));
    }
    auto const spill{decode(::std::array<::std::uint8_t,5u>{0xc5u,0xf9u,0x6fu,0x45u,0x98u})};
    CHECK(spill && spill.safe_for_single_instruction() && !spill.safe_for_public_display());
    auto const width{decode(::std::array<::std::uint8_t,3u>{0x66u,0x89u,0xc0u})};
    CHECK(width && width.safe_for_single_instruction() && width.safe_for_public_display());
    auto const repeated{decode(::std::array<::std::uint8_t,3u>{0x66u,0xf3u,0xa5u})};
    auto const locked{decode(::std::array<::std::uint8_t,4u>{0x66u,0xf0u,0x01u,0x00u})};
    auto const flags_restore{decode(::std::array<::std::uint8_t,2u>{0x66u,0x9du})};
    auto const interrupt_return{decode(::std::array<::std::uint8_t,2u>{0x66u,0xcfu})};
    auto const stack_segment{decode(::std::array<::std::uint8_t,2u>{0x8eu,0xd0u})};
    auto const address_override{decode(::std::array<::std::uint8_t,3u>{0x67u,0x8bu,0x00u})};
    for(auto const value: {repeated,locked,flags_restore,interrupt_return,stack_segment,address_override})
    { CHECK(value && !value.safe_for_single_instruction() && !value.safe_for_public_display()); }
    ::fast_io::io::println("PASS actual i686 MC width/VEX numeric moves; hidden stack operands and REP/LOCK/address/trap-state refusal");
    return 0;
#elif defined(__linux__) && defined(__powerpc__)
    CHECK(decoder);
    auto decode{[&](::std::uint32_t word)
    {
        ::std::array<::std::uint8_t,4u> bytes{};::std::memcpy(bytes.data(),&word,4u);
        return decoder.decode(0x1000u,bytes);
    }};
    auto const nop{decode(0x60000000u)};
    CHECK(nop && nop.safe_for_single_instruction() && nop.safe_for_public_display());
    auto const numeric{decode(0x60840001u)}; // ori r4,r4,1
    CHECK(numeric && numeric.safe_for_single_instruction() && numeric.safe_for_public_display());
    for(auto word: {0x60210000u,0x60420000u}) // stack/TOC operands
    { auto const hidden{decode(word)};CHECK(hidden && !hidden.safe_for_public_display()); }
    for(auto word: {0x4e800020u,0x7c6903a6u}) // return/CTR write
    { auto const control{decode(word)};CHECK(control && !control.safe_for_public_display()); }
    ::fast_io::io::println("PASS PPC actual no-op and numeric MC decode; stack/TOC/control operands hidden");
    return 0;
#elif defined(__linux__) && defined(__mips__) && __SIZEOF_POINTER__ == 8
    CHECK(decoder);
    ::std::array<::std::uint32_t,6u> const words{0x10430003u,0x64420001u,0x64420002u,0u,0x64420003u,0u};
    ::std::array<::std::uint8_t,sizeof(words)> bytes{}; ::std::memcpy(bytes.data(),words.data(),bytes.size());
    auto const alu{decoder.decode(0x1004u,{bytes.data()+4u,4u})};
    if(!alu || !alu.safe_for_public_display()) { failed_actual_mc_word(words[1u]); }
    CHECK(alu && alu.safe_for_single_instruction() && alu.safe_for_public_display());
    auto decode_word{[&](::std::uint32_t word)
    { ::std::array<::std::uint8_t,4u> data{};::std::memcpy(data.data(),&word,4u);return decoder.decode(0x1000u,data); }};
    auto const sync{decode_word(0x0000000fu)};
    if(!sync.safe_for_single_instruction()) { failed_actual_mc_word(0x0000000fu); }
    CHECK(sync && sync.safe_for_single_instruction() && !sync.safe_for_public_display());
    for(auto word:{0x0000004fu,0x000007cfu,0x043f0000u,0x42000020u,0x0000000cu,0x0000000du,0x42000018u,0x41606000u})
    { CHECK(!decode_word(word).safe_for_single_instruction()); }
    for(auto base:{0x64420000u,0x24420000u,0x28420000u,0x2c420000u})
    { for(auto immediate:{0u,1u,32767u,32768u,65535u})
      { auto const value{decode_word(base|immediate)};
        if(!value.safe_for_public_display()) { failed_actual_mc_word(base|immediate); }
        CHECK(value && value.safe_for_single_instruction() && value.safe_for_public_display()); } }
    for(auto base:{0x30420000u,0x34420000u,0x38420000u,0x3c020000u})
    { for(auto immediate:{0u,1u,32768u,65535u})
      { auto const value{decode_word(base|immediate)};
        if(!value.safe_for_public_display()) { failed_actual_mc_word(base|immediate); }
        CHECK(value && value.safe_for_single_instruction() && value.safe_for_public_display()); } }
    for(auto base:{0x00021000u,0x00021002u,0x00021003u,0x00021038u,0x0002103au,0x0002103bu,0x0002103cu,0x0002103eu,0x0002103fu})
    { for(auto shift:{0u,1u,31u})
      { auto const value{decode_word(base|(shift<<6u))};
        if(!value.safe_for_public_display()) { failed_actual_mc_word(base|(shift<<6u)); }
        CHECK(value && value.safe_for_single_instruction() && value.safe_for_public_display()); } }
    for(auto reg:{26u,27u,28u,29u,30u,31u})
    {
        for(auto word:{0x64000001u|(reg<<21u)|(2u<<16u),0x64000001u|(2u<<21u)|(reg<<16u),0x3c000001u|(reg<<16u)})
        { auto const value{decode_word(word)};CHECK(value && !value.safe_for_public_display()); }
    }
    for(auto word:{0xdc420000u,0xfc420000u,0xc4420000u,0xe4420000u,0x03e00008u})
    { auto const value{decode_word(word)};CHECK(value && !value.safe_for_public_display()); }
    CHECK(!decoder.decode(0x1000u,{bytes.data()+4u,3u}));
    auto const branch{decoder.decode(0x1000u,bytes)};
    CHECK(branch && branch.safe_for_same_owner_direct_branch() && branch.delayed_branch_pair() && branch.destination().display_pc == 0x1010u);
    auto zero_branch{bytes};::std::uint32_t const zero_word{0x10200003u};::std::memcpy(zero_branch.data(),&zero_word,4u);
    auto const beq_zero{decoder.decode(0x1000u,zero_branch)};
    if(!beq_zero.safe_for_same_owner_direct_branch()) { failed_actual_mc_word(zero_word); }
    CHECK(beq_zero && beq_zero.safe_for_same_owner_direct_branch() && beq_zero.delayed_branch_pair() &&
        beq_zero.destination().display_pc==0x1010u);

    struct description
    {
        ::std::array<char,64u> triple{}; ::std::array<char,1u> cpu{},features{};
        ::std::size_t triple_size{},cpu_size{},features_size{};
        unsigned description_version{1u},pointer_bits{64u},maximum_instruction_bytes{4u},minimum_instruction_alignment{4u};
        bool available{true},little_endian{__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__};
    } target{};
    while(owned_mc::details::native_triple[target.triple_size] != '\0')
    { target.triple[target.triple_size] = owned_mc::details::native_triple[target.triple_size]; ++target.triple_size; }
    ::uwvm2::uwvm::debugger::native_disassembly::decoder display{target}; CHECK(display);
    for(bool ni: {false,true})
    {
        auto const permitted{::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(display,decoder,bytes,0x1000u,0x1018u,0x1000u,ni)};
        CHECK(permitted && permitted.first_successor == 0x1010u && permitted.second_successor == 0x1008u);
        auto bad{bytes}; auto replace_word{[&](unsigned index,::std::uint32_t value) { ::std::memcpy(bad.data()+4u*index,&value,4u); }};
        replace_word(1u,0x03e00008u); // JR in delay slot cannot be nested.
        CHECK(!::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(display,decoder,bad,0x1000u,0x1018u,0x1000u,ni));
        bad=bytes;replace_word(0u,0x1043ffffu); // Taken target equals the current patched branch.
        CHECK(!::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(display,decoder,bad,0x1000u,0x1018u,0x1000u,ni));
        bad=bytes;replace_word(0u,0x10430010u); // Outside this complete owner.
        CHECK(!::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(display,decoder,bad,0x1000u,0x1018u,0x1000u,ni));
        bad=bytes;replace_word(0u,0x50430003u); // Annulled BEQL is not this contract.
        CHECK(!::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(display,decoder,bad,0x1000u,0x1018u,0x1000u,ni));
    }
    for(bool ni:{false,true})
    {
        auto absolute{bytes};auto set{[&](::std::uint32_t word) { ::std::memcpy(absolute.data(),&word,4u); }};
        set(0x08000404u); // J 0x1010 within this exact owned region.
        auto const jump{decoder.decode(0x1000u,absolute)};
        if(!jump.safe_for_same_owner_direct_branch()) { failed_actual_mc_word(0x08000404u); }
        CHECK(jump && jump.safe_for_same_owner_direct_branch() && jump.delayed_branch_pair() && jump.destination().display_pc==0x1010u);
        auto const selected{::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(display,decoder,absolute,0x1000u,0x1018u,0x1000u,ni)};
        CHECK(selected && selected.first_successor==0x1010u && selected.second_successor==0u);
        set(0x08000400u); // Same patched branch.
        CHECK(!::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(display,decoder,absolute,0x1000u,0x1018u,0x1000u,ni));
        set(0x08000401u); // Current delay slot.
        CHECK(!::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(display,decoder,absolute,0x1000u,0x1018u,0x1000u,ni));
        set(0x08000800u); // Complete decode, escaping destination.
        CHECK(!::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(display,decoder,absolute,0x1000u,0x1018u,0x1000u,ni));
        set(0x0c000404u); // JAL may not borrow non-linking J permission.
        CHECK(!::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(display,decoder,absolute,0x1000u,0x1018u,0x1000u,ni));
        set(0x08000404u);
        CHECK(!decoder.decode(0x0ffffffcu,absolute).safe_for_same_owner_direct_branch());
    }
    ::fast_io::io::println("PASS real MIPS MC branch/delay pair, both owned successors, nested/annulled/self/escaping refusal");
    return 0;
#elif defined(__linux__) && defined(__sparc__) && __SIZEOF_POINTER__ == 8
    CHECK(decoder);
    auto decode{[&](auto const& bytes) { return decoder.decode(0x1000u, ::std::span<::std::uint8_t const>{bytes}); }};
    auto const add{decode(::std::array<::std::uint8_t,4u>{0x84u,0u,0x60u,7u})}; // add g1,7,g2
    auto const nop{decode(::std::array<::std::uint8_t,4u>{1u,0u,0u,0u})};
    auto const branch{decode(::std::array<::std::uint8_t,4u>{0x12u,0x80u,0u,4u})}; // bne +16
    auto const negative{decode(::std::array<::std::uint8_t,4u>{0x12u,0xbfu,0xffu,0xffu})}; // bne -4
    auto const returned{decode(::std::array<::std::uint8_t,4u>{0x81u,0xc3u,0xe0u,8u})};
    CHECK(add && add.safe_for_single_instruction());
    CHECK(nop && nop.safe_for_single_instruction());
    for(unsigned mask{};mask!=16u;++mask)
    {
        auto const order{decode(::std::array<::std::uint8_t,4u>{0x81u,0x43u,0xe0u,static_cast<::std::uint8_t>(mask)})};
        CHECK(order && order.safe_for_single_instruction() && !order.safe_for_public_display());
    }
    for(unsigned mask:{16u,31u,32u,64u,127u,128u,255u})
    {
        auto const other{decode(::std::array<::std::uint8_t,4u>{0x81u,0x43u,0xe0u,static_cast<::std::uint8_t>(mask)})};
        CHECK(!other || (!other.safe_for_single_instruction() && !other.safe_for_public_display()));
    }
    CHECK(!decode(::std::array<::std::uint8_t,3u>{0x81u,0x43u,0xe0u}));
    CHECK(branch && branch.safe_for_same_owner_direct_branch() && branch.delayed_direct_branch() &&
        branch.destination().conditional && branch.destination().display_pc == 0x1010u);
    CHECK(negative && negative.safe_for_same_owner_direct_branch() && negative.destination().display_pc == 0xffcu);
    CHECK(returned && !returned.safe_for_single_instruction() && !returned.safe_for_same_owner_direct_branch() && !returned.safe_for_public_display());
    CHECK(!decode(::std::array<::std::uint8_t,3u>{0x12u,0x80u,0u}));
    ::fast_io::io::println("PASS real SPARC64 MC ALU/NOP, exact hidden memory-order masks, bounded signed delayed branches and indirect/truncated refusal without a registered analyzer");
    return 0;
#elif defined(__linux__) && defined(__s390x__)
    CHECK(decoder);
    auto decode{[&](auto const& bytes) { return decoder.decode(0x1000u, ::std::span<::std::uint8_t const>{bytes}); }};
    auto const branch{decode(::std::array<::std::uint8_t,4u>{0xa7u,0x74u,0u,4u})}; // brc 7,+8
    auto const backwards{decode(::std::array<::std::uint8_t,4u>{0xa7u,0x74u,0xffu,0xfeu})}; // brc 7,-4
    auto const jump{decode(::std::array<::std::uint8_t,6u>{0xc0u,0xf4u,0u,0u,0u,4u})}; // brcl 15,+8
    CHECK(branch && branch.safe_for_same_owner_direct_branch() && branch.destination().conditional && branch.destination().display_pc == 0x1008u);
    CHECK(backwards && backwards.safe_for_same_owner_direct_branch() && backwards.destination().display_pc == 0xffcu);
    CHECK(jump && jump.safe_for_same_owner_direct_branch() && !jump.destination().conditional && jump.destination().display_pc == 0x1008u);
    CHECK(!decode(::std::array<::std::uint8_t,3u>{0xa7u,0x74u,0u}));
    auto const indirect{decode(::std::array<::std::uint8_t,2u>{7u,0xfeu})}; // bcr 15,r14
    CHECK(indirect && !indirect.safe_for_same_owner_direct_branch() && !indirect.safe_for_public_display());
    ::fast_io::io::println("PASS real SystemZ signed BRC/BRCL table analysis and indirect/truncated refusal");
    return 0;
#elif defined(__linux__) && defined(__loongarch64)
    CHECK(decoder);
    auto decode{[&](auto const& bytes) { return decoder.decode(0x1000u, ::std::span<::std::uint8_t const>{bytes}); }};
    auto const add{decode(::std::array<::std::uint8_t,4u>{0x84u,0x14u,0xc0u,0x02u})}; // addi.d r4,r4,5
    auto const stack{decode(::std::array<::std::uint8_t,4u>{0x63u,0x14u,0xc0u,0x02u})}; // addi.d r3,r3,5
    auto const branch{decode(::std::array<::std::uint8_t,4u>{0x85u,0x08u,0u,0x58u})}; // beq r4,r5,+8
    auto const negative{decode(::std::array<::std::uint8_t,4u>{0x85u,0xfcu,0xffu,0x5bu})}; // beq r4,r5,-4
    CHECK(add && add.safe_for_single_instruction() && add.safe_for_public_display());
    CHECK(stack && stack.safe_for_single_instruction() && !stack.safe_for_public_display());
    CHECK(branch && branch.safe_for_same_owner_direct_branch() && branch.destination().conditional && branch.destination().display_pc == 0x1008u);
    CHECK(negative && negative.safe_for_same_owner_direct_branch() && negative.destination().display_pc == 0xffcu);
    CHECK(!decode(::std::array<::std::uint8_t,3u>{0x85u,0x08u,0u}));
    struct description
    {
        ::std::array<char,64u> triple{}; ::std::array<char,1u> cpu{},features{};
        ::std::size_t triple_size{},cpu_size{},features_size{};
        unsigned description_version{1u},pointer_bits{64u},maximum_instruction_bytes{4u},minimum_instruction_alignment{4u};
        bool available{true},little_endian{true};
    } target{};
    while(owned_mc::details::native_triple[target.triple_size] != '\0')
    { target.triple[target.triple_size] = owned_mc::details::native_triple[target.triple_size]; ++target.triple_size; }
    ::uwvm2::uwvm::debugger::native_disassembly::decoder display{target}; CHECK(display);
    // JIRL has no static TD control-flow flags. Its real target analyzer
    // must veto execution for every operand-dependent call/return/jump form.
    // The address is owned-byte DATA, not a register or memory capability.
    for(unsigned rd: {0u,1u,4u,31u})
    for(unsigned rj: {0u,1u,4u,31u})
    for(::std::int32_t displacement: {0,1,-1,32767,-32768})
    {
        ::std::uint32_t const word{0x4c000000u | ((static_cast<::std::uint32_t>(displacement) & 0xffffu) << 10u) |
            (rj << 5u) | rd};
        ::std::array<::std::uint8_t,12u> bytes{0u,0u,0u,0u,0x84u,0x14u,0xc0u,0x02u,0x84u,0x14u,0xc0u,0x02u};
        ::std::memcpy(bytes.data(),&word,4u);
        auto const indirect{decoder.decode(0x1000u,bytes)};
        CHECK(indirect && !indirect.safe_for_single_instruction() &&
            !indirect.safe_for_same_owner_direct_branch() && !indirect.safe_for_public_display());
        auto const expected{rd != 0u ? semantics::flow::call : rj == 1u ?
            semantics::flow::return_instruction : semantics::flow::branch};
        CHECK(indirect.semantics().kind == expected && indirect.semantics().may_change_pc);
        CHECK(!::uwvm2::uwvm::debugger::native_next_policy::choose(indirect,bytes.size()));
        for(bool ni: {false,true})
        {
            auto const refused{::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(
                display,decoder,bytes,0x1000u,0x100cu,0x1000u,ni)};
            CHECK(!refused && refused.first_successor == 0u && refused.second_successor == 0u);
        }
    }
    ::fast_io::io::println("PASS real LoongArch MC ALU, protected stack, signed direct branches, 80 JIRL forms refused by both SI and NI");
    return 0;
#elif defined(__linux__) && defined(__arm__)
    CHECK(decoder);
    // Explicit ARMv7 DATA fixture description. A default generic ARM decoder
    // is ARMv4 and correctly refuses HINT; this supplies no live owner or
    // native-step capability. Product tests use the real engine description.
    struct arm_v7_description
    {
        ::std::array<char,64u> triple{},cpu{},features{};
        ::std::size_t triple_size{sizeof("armv7-linux-gnueabihf")-1u},cpu_size{sizeof("cortex-a15")-1u},features_size{};
        unsigned description_version{1u},pointer_bits{32u},maximum_instruction_bytes{4u},minimum_instruction_alignment{4u};
        bool available{true},little_endian{true};
        arm_v7_description()
        {
            constexpr char t[]{"armv7-linux-gnueabihf"},c[]{"cortex-a15"};
            for(::std::size_t i{};i!=triple_size;++i) { triple[i]=t[i]; }
            for(::std::size_t i{};i!=cpu_size;++i) { cpu[i]=c[i]; }
            ::llvm::Triple const actual_triple{t};::std::string error{};
            auto const* target{owned_mc::details::lookup_target(actual_triple,error)};
            if(target==nullptr) { available=false;return; }
            auto registers{owned_mc::details::make_registers(*target,actual_triple)};
            ::llvm::MCTargetOptions options{};
            auto assembly{owned_mc::details::make_assembly(*target,*registers,actual_triple,options)};
            auto subtarget{owned_mc::details::make_subtarget(*target,actual_triple,c)};
            if(!assembly || !subtarget) { available=false;return; }
            maximum_instruction_bytes=owned_mc::details::maximum_instruction_length(*assembly,*subtarget,actual_triple);
            minimum_instruction_alignment=assembly->getMinInstAlignment();
            little_endian=assembly->isLittleEndian();
        }
    } description;
    owned_mc::decoder arm_v7_decoder{description};CHECK(arm_v7_decoder);
    auto decode{[&](::std::uint32_t word)
    {
        ::std::array<::std::uint8_t,4u> bytes{};::std::memcpy(bytes.data(),&word,4u);
        return arm_v7_decoder.decode(0x1000u,bytes);
    }};
    auto const nop{decode(0xe320f000u)};
    if(!nop || !nop.safe_for_single_instruction() || !nop.safe_for_public_display()) { failed_actual_mc_word(0xe320f000u); }
    CHECK(nop && nop.safe_for_single_instruction() && nop.safe_for_public_display());
    for(unsigned hint{1u}; hint!=240u; ++hint)
    { auto const hidden{decode(0xe320f000u | hint)};CHECK(!hidden.safe_for_public_display() && !hidden.safe_for_single_instruction()); }
    for(unsigned condition{};condition<14u;++condition)
    {
        auto const conditional{decode((condition<<28u)|0x0320f000u)};
        CHECK(!conditional.safe_for_public_display() && !conditional.safe_for_single_instruction());
    }
    // Condition bits 0xf select the unconditional encoding space. Verify the
    // actual MC opcode before treating that DATA word as a conditional HINT.
    // A valid distinct vector opcode must not become a fabricated NV-NOP test.
    {
        ::llvm::Triple const actual_triple{"armv7-linux-gnueabihf"};::std::string error{};
        auto const* target{owned_mc::details::lookup_target(actual_triple,error)};CHECK(target);
        auto info{::std::unique_ptr<::llvm::MCInstrInfo>{target->createMCInstrInfo()}};CHECK(info);
        auto registers{owned_mc::details::make_registers(*target,actual_triple)};CHECK(registers);
        ::llvm::MCTargetOptions options{};
        auto assembly{owned_mc::details::make_assembly(*target,*registers,actual_triple,options)};CHECK(assembly);
        auto subtarget{owned_mc::details::make_subtarget(*target,actual_triple,"cortex-a15")};CHECK(subtarget);
        auto context{owned_mc::details::make_context(actual_triple,*assembly,*registers,*subtarget)};CHECK(context);
        auto raw{::std::unique_ptr<::llvm::MCDisassembler>{target->createMCDisassembler(*subtarget,*context)}};CHECK(raw);
        ::std::array<::std::uint8_t,4u> const bytes{0u,0xf0u,0x20u,0xf3u};
        ::llvm::MCInst instruction{};::std::uint64_t size{};
        CHECK(raw->getInstruction(instruction,size,bytes,0x1000u,::llvm::nulls())==::llvm::MCDisassembler::Success &&
              size==4u && instruction.getOpcode()<info->getNumOpcodes());
        auto const name{info->getName(instruction.getOpcode())};CHECK(name!="HINT");
        ::fast_io::io::println("PASS actual ARM unconditional opcode differs from HINT: ",::fast_io::string_view{name.data(),name.size()});
    }
    for(auto word:{0xf57ff04fu,0xf57ff06fu}) // Actual unconditional DSB/ISB.
    { auto const barrier{decode(word)};CHECK(!barrier.safe_for_public_display() && !barrier.safe_for_single_instruction()); }
    // Actual MC DATA proves a direct branch cannot enter a literal island.
    // A valid branch across that island remains bounded to real ARM text.
    ::uwvm2::uwvm::debugger::native_disassembly::decoder display{description};CHECK(display);
    ::std::array<::std::uint8_t,12u> mapped_bytes{0xffu,0xffu,0xffu,0xeau,0u,0xf0u,0x20u,0xe3u,0u,0xf0u,0x20u,0xe3u};
    ::std::array<::std::uint8_t,12u> mapped_text{1u,1u,1u,1u,0u,0u,0u,0u,1u,1u,1u,1u};
    auto const into_pool{::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(
        display,arm_v7_decoder,mapped_bytes,0x1000u,0x100cu,0x1000u,false,0u,mapped_text)};
    CHECK(!into_pool);
    mapped_bytes[0u]=0u;mapped_bytes[1u]=0u;mapped_bytes[2u]=0u; // B 0x1008
    auto const over_pool{::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(
        display,arm_v7_decoder,mapped_bytes,0x1000u,0x100cu,0x1000u,false,0u,mapped_text)};
    CHECK(over_pool && over_pool.first_successor==0x1008u && over_pool.second_successor==0u);
    mapped_bytes[3u]=0x0au; // BEQ 0x1008 still has a fallthrough into DATA.
    CHECK(!::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(
        display,arm_v7_decoder,mapped_bytes,0x1000u,0x100cu,0x1000u,false,0u,mapped_text));
    mapped_text.fill(1u);
    for(bool ordinary_next:{false,true})
    {
        auto const conditional_branch{::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(
            display,arm_v7_decoder,mapped_bytes,0x1000u,0x100cu,0x1000u,ordinary_next,0u,mapped_text)};
        CHECK(conditional_branch && conditional_branch.first_successor==0x1008u &&
              conditional_branch.second_successor==0x1004u);
    }
    auto const carrier{decode(0xec410b30u)}; // actual VMOV D16, R0, R1
    CHECK(carrier && carrier.safe_for_single_instruction() && carrier.safe_for_public_display());
    auto const move{decode(0xe3a0002au)},add{decode(0xe2800001u)};
    if(!move || !move.safe_for_single_instruction() || !move.safe_for_public_display()) { failed_actual_mc_word(0xe3a0002au); }
    CHECK(move && move.safe_for_single_instruction() && move.safe_for_public_display());
    CHECK(add && add.safe_for_single_instruction() && add.safe_for_public_display());
    // Actual modified-immediate extrema in MOV/ALU encodings. Conditional
    // predicates, CPSR writes, protected registers and memory stay hidden.
    // MC Fail/SoftFail for reserved forms must also remain unavailable.
    for(unsigned immediate:{0u,255u,4095u})
    {
        for(unsigned operation:{0u,1u,2u,3u,4u,12u,14u})
        {
            auto const numeric{decode(0xe2000000u | (operation << 21u) | immediate)};
            CHECK(numeric && numeric.safe_for_single_instruction() && numeric.safe_for_public_display());
        }
        auto const constant{decode(0xe3a00000u | immediate)};
        CHECK(constant && constant.safe_for_public_display());
    }
    for(auto word:{0x03a0002au,0xe3b0002au,0xe2900001u,0xe3a0d02au,0xe3a0e02au,
                   0xe3a0f02au,0xe28dd001u,0xe5910000u,0xe12fff1eu})
    { auto const hidden{decode(word)};if(hidden.safe_for_public_display()) { failed_actual_mc_word(word); }CHECK(!hidden.safe_for_public_display()); }
    auto const branch{decode(0x0a000000u)},negative{decode(0x1afffffdu)};
    CHECK(branch && branch.safe_for_same_owner_direct_branch() && branch.destination().conditional &&
        branch.destination().display_pc==0x1008u);
    CHECK(negative && negative.safe_for_same_owner_direct_branch() && negative.destination().display_pc==0xffcu);
    ::std::array<::std::uint8_t,4u> bytes{};::std::uint32_t const word{0xe2800001u};::std::memcpy(bytes.data(),&word,4u);
    CHECK(!decoder.decode(0x1001u,bytes) && !decoder.decode(0x1000u,{bytes.data(),3u}));
    ::fast_io::io::println("PASS actual ARM-state MC exact NOP and complete D carrier, other hints/stack/load/return hiding, signed direct branches and alignment/truncation refusal");
    return 0;
#elif defined(__linux__) && defined(__riscv) && __riscv_xlen==64
    CHECK(decoder);
    auto decode{[&](::std::uint32_t word)
    {
        ::std::array<::std::uint8_t,4u> bytes{};::std::memcpy(bytes.data(),&word,4u);
        return decoder.decode(0x1000u,bytes);
    }};
    auto const add{decode(0x00550513u)},stack{decode(0x00510113u)};
    if(!add || !add.safe_for_single_instruction() || !add.safe_for_public_display()) { failed_actual_mc_word(0x00550513u); }
    CHECK(add && add.safe_for_single_instruction() && add.safe_for_public_display());
    CHECK(stack && !stack.safe_for_public_display());
    for(unsigned operation:{0u,2u,3u,4u,6u,7u})
    for(::std::int32_t immediate:{-2048,0,2047})
    {
        auto const numeric{decode(((static_cast<::std::uint32_t>(immediate)&0xfffu)<<20u) |
            (10u<<15u) | (operation<<12u) | (10u<<7u) | 0x13u)};
        CHECK(numeric && numeric.safe_for_single_instruction() && numeric.safe_for_public_display());
    }
    for(unsigned shift:{0u,31u,63u})
    for(unsigned operation:{0x1013u,0x5013u,0x40005013u})
    {
        auto const numeric{decode(operation | (shift<<20u) | (10u<<15u) | (10u<<7u))};
        CHECK(numeric && numeric.safe_for_public_display());
    }
    for(auto word:{0x00550503u,0x00550523u,0x00550517u,0x00151573u,
                   0x00508093u,0x00518193u,0x00520213u,0x00540413u})
    { auto const hidden{decode(word)};if(hidden.safe_for_public_display()) { failed_actual_mc_word(word); }CHECK(!hidden.safe_for_public_display()); }
    auto const branch{decode(0x00b50463u)},negative{decode(0xfeb50ee3u)};
    CHECK(branch && branch.safe_for_same_owner_direct_branch() && branch.destination().conditional &&
        branch.destination().display_pc==0x1008u);
    CHECK(negative && negative.safe_for_same_owner_direct_branch() && negative.destination().display_pc==0xffcu);
    for(unsigned rd:{0u,1u,4u,31u})
    for(unsigned base:{0u,1u,4u,31u})
    for(::std::int32_t displacement:{0,1,-1,2047,-2048})
    {
        ::std::uint32_t const word{((static_cast<::std::uint32_t>(displacement)&0xfffu)<<20u)|
            (base<<15u)|(rd<<7u)|0x67u};
        auto const indirect{decode(word)};
        CHECK(indirect && !indirect.safe_for_single_instruction() &&
            !indirect.safe_for_same_owner_direct_branch() && !indirect.safe_for_public_display());
    }
    ::std::array<::std::uint8_t,4u> bytes{};::std::uint32_t const word{0x00550513u};::std::memcpy(bytes.data(),&word,4u);
    CHECK(!decoder.decode(0x1001u,bytes) && !decoder.decode(0x1000u,{bytes.data(),3u}));
    ::fast_io::io::println("PASS actual RV64 MC numeric ALU, signed direct branches, protected stack, and 80 indirect JALR forms refused");
    return 0;
#else
    ::fast_io::io::println("UNAVAILABLE owned MC instruction fixture for this target");
    return 77;
#endif
}
