// Actual LLVM owned-byte decode + actual production formatter component.
// Numeric display fixtures are DATA and do not stand in for the separate
// debug_native_branch_display_runtime real controller/hardware-trap witness.
#include <uwvm2/uwvm/debugger/console.h>
#include <uwvm2/uwvm/debugger/native_branch_display.h>
#include <fast_io.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace dbg = ::uwvm2::uwvm::debugger;
static void check(bool value, char const* message) noexcept
{
    if(!value)
    { ::fast_io::io::perrln("native_branch_display_formatter: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
int main()
{
#if !defined(__x86_64__) && !defined(_M_X64)
    ::fast_io::io::println("native_branch_display_formatter: SKIP x64 byte corpus"); return 77;
#else
    dbg::native_disassembly::decoder display{};
    dbg::native_owned_instruction_semantics::decoder semantics{};
    if(!display || !semantics)
    { ::fast_io::io::println("native_branch_display_formatter: SKIP existing native decoder gate unavailable"); return 77; }
    auto decode = [&](::std::span<::std::uint8_t const> bytes)
    {
        auto instruction{display.decode(0x1000u, bytes)};
        check(static_cast<bool>(instruction), "actual LLVM text decode");
        return instruction;
    };
    auto format = [&](dbg::native_disassembly::instruction instruction, bool resolve, bool range,
                      ::std::uintptr_t begin=0x1000u, ::std::uintptr_t end=0x1010u)
    {
        dbg::controller_reply reply{};
        reply.disassembly_stop_identifier = 9u;
        reply.disassembly_code.pc = 0x1000u; reply.disassembly_code.participant = 3u;
        reply.disassembly_code.module = 2u; reply.disassembly_code.function = 7u;
        reply.disassembly_code.function_generation = 11u; reply.disassembly_code.runtime_epoch = 13u;
        reply.disassembly_count = 1u; reply.disassembly[0u] = instruction;
        reply.disassembly_destinations[0u] = dbg::native_branch_display::decode(semantics, instruction);
        reply.disassembly_owner_begin = begin; reply.disassembly_owner_end = end;
        auto& target{reply.disassembly_code.target};
        target.available = true; target.description_version = 1u; target.pointer_bits = 64u;
        target.maximum_instruction_bytes = 15u; target.minimum_instruction_alignment = 1u; target.little_endian = true;
        for(; dbg::native_owned_instruction_semantics::details::native_triple[target.triple_size] != '\0'; ++target.triple_size)
        { target.triple[target.triple_size] = dbg::native_owned_instruction_semantics::details::native_triple[target.triple_size]; }
        auto const input{range ? ::fast_io::string_view{"disassemble-range 3 9 1 0 0 1"} : ::fast_io::string_view{"disassemble 3 9 1"}};
        auto command{dbg::parse_console_command(input)};
        command.disassembly_resolve_symbols = resolve;
        return dbg::details::format_reply(reply, command);
    };
    ::std::array<::std::uint8_t, 2u> conditional{0x75u, 0x02u};
    auto instruction{decode(conditional)};
    auto annotation{dbg::native_branch_display::decode(semantics, instruction)};
    check(annotation && annotation.destination.display_pc == 0x1004u && annotation.destination.conditional &&
          dbg::native_branch_display::matches(annotation, instruction), "actual conditional MC destination joins exact shown row");
    auto text{format(instruction, true, true)};
    check(text.find("target=0x0000000000001004 target-conditional=1 target-symbol=current-function+0x0000000000000004 target-module=2 target-function=7 target-function-generation=11") != ::std::string::npos,
          "actual production formatter current-function offset/identity");
    check(format(instruction, false, true).find("target=0x0000000000001004 target-conditional=1 target-symbol=disabled") != ::std::string::npos,
          "resolveSymbols false");
    check(format(instruction, true, false).find("target-symbol=disabled") != ::std::string::npos,
          "plain display retains containment but requests no symbol name");
    check(format(instruction, true, true, 0x1000u, 0x1004u).find("error:") == 0u,
          "half-open owner end excludes the entire branch output");
    ::std::array<::std::uint8_t, 5u> external{0xe8u, 0x00u, 0x04u, 0x00u, 0x00u};
    auto call{decode(external)};
    check(format(call, true, true).find("error:") == 0u,
          "direct outside callee bytes/text/annotation all refused");
    ::std::array<::std::uint8_t, 2u> indirect{0xffu, 0xd0u};
    check(format(decode(indirect), true, true).find("target=") == ::std::string::npos,
          "indirect call does not follow a register/address");
    ::std::array<::std::uint8_t, 1u> ret{0xc3u};
    check(format(decode(ret), true, true).find("target=") == ::std::string::npos,
          "return does not read a stack or infer a caller target");
    auto incomplete{instruction}; incomplete.size = 1u;
    check(!dbg::native_branch_display::decode(semantics, incomplete), "partial shown instruction cannot obtain annotation");
    incomplete = instruction; incomplete.size = incomplete.bytes.size() + 1u;
    check(!dbg::native_branch_display::decode(semantics, incomplete), "oversized shown copy refused before MC read");
    incomplete = instruction; incomplete.pc = UINTPTR_MAX;
    check(!dbg::native_branch_display::decode(semantics, incomplete), "display PC overflow refused without an address read");
    ::std::array<::std::uint8_t, 2u> zero{0xebu, 0xfcu};
    auto zero_instruction{display.decode(2u, zero)};
    auto zero_target{dbg::native_branch_display::decode(semantics, zero_instruction)};
    check(zero_target && zero_target.destination.display_pc == 0u &&
          dbg::native_branch_display::resolve_current_owner(zero_target, 1u, 0x100u, true).resolution ==
            dbg::native_branch_display::symbol_resolution::outside_current_function,
          "zero destination is scalar DATA, not a null pointer or in-owner symbol");
    auto false_static{annotation};false_static.destination.never_taken=true;
    check(!dbg::native_branch_display::displayable(semantics,instruction,false_static,0x1000u,0x1010u),
          "forged never-taken condition rejected against real MC instruction");
    auto forged{annotation}; ++forged.source_pc;
    check(!dbg::native_branch_display::matches(forged, instruction), "different source PC rejected");
    forged = annotation; ++forged.source_size;
    check(!dbg::native_branch_display::matches(forged, instruction), "different shown instruction size rejected");
    dbg::controller_reply mismatch{};
    mismatch.disassembly_stop_identifier = 9u; mismatch.disassembly_code.pc = 0x1000u;
    mismatch.disassembly_code.participant = 3u; mismatch.disassembly_code.function_generation = 1u;
    mismatch.disassembly_code.runtime_epoch = 1u; mismatch.disassembly_count = 1u;
    mismatch.disassembly[0u] = instruction; mismatch.disassembly_destinations[0u] = forged;
    check(dbg::details::format_reply(mismatch, dbg::parse_console_command(::fast_io::string_view{"disassemble 3 9 1"})).find("error: native disassembly result is invalid") != ::std::string::npos,
          "actual formatter refuses a mismatched optional annotation before output");
    mismatch.disassembly_destinations[0u] = {};
    check(dbg::details::format_reply(mismatch, dbg::parse_console_command(::fast_io::string_view{"disassemble 3 9 1"})).find("error:") == 0u,
          "omitted annotation cannot bypass public row policy");
    ::fast_io::io::println("native_branch_display_formatter: PASS actual MC + production formatter DATA cases; no native stop authority");
#endif
}
