// Pure finite boundary policy; mock decode is NOT a native ownership proof.
#include <uwvm2/uwvm/debugger/native_disassembly_window.h>
#include <uwvm2/uwvm/debugger/command.h>
#include <fast_io.h>
#include <array>
#include <limits>
namespace nd = ::uwvm2::uwvm::debugger::native_disassembly;
namespace dbg = ::uwvm2::uwvm::debugger;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native window policy failure line=", __LINE__); ::fast_io::fast_terminate(); } } while(false)
struct decoder
{
    bool enabled{true};
    ::std::size_t calls{};
    [[nodiscard]] explicit operator bool() const noexcept { return enabled; }
    nd::instruction decode(::std::uintptr_t pc, ::std::span<::std::uint8_t const> bytes) noexcept
    {
        ++calls;
        nd::instruction out{};
        if(bytes.empty() || bytes[0] == 0u || bytes[0] > 15u || bytes[0] > bytes.size()) { return out; }
        out.pc = pc; out.size = bytes[0]; out.text[0] = 'x';
        for(::std::size_t index{}; index != out.size; ++index)
        { out.bytes[index] = bytes[index]; } // checked owned fixed extents above.
        return out;
    }
};
static auto command(::fast_io::string_view bytes)
{
    // [host-owned bounded immutable literal] end
    // [safe                                ] borrowed synchronously only.
    return dbg::parse_console_command(bytes);
}
int main()
{
    decoder fake{};
    ::std::array<::std::uint8_t, 7u> bytes{3u,0u,0u,1u,2u,0u,1u};
    auto good{nd::decode_window_with(fake, bytes, 0x1000u, 0x1003u, 0, -2, 6u)};
    CHECK(good.available && good.count == 6u);
    CHECK(!good.instructions[0] && good.instructions[0].pc == 0u);
    CHECK(good.instructions[1].pc == 0x1000u && good.instructions[1].size == 3u);
    CHECK(good.instructions[2].pc == 0x1003u && good.instructions[3].pc == 0x1004u);
    CHECK(good.instructions[4].pc == 0x1006u && !good.instructions[5] && good.instructions[5].pc == 0u);
    CHECK(!nd::decode_window_with(fake, bytes, 0x1000u, 0x1001u, 0, 0, 1u).available);
    CHECK(!nd::decode_window_with(fake, bytes, 0x1000u, 0x1003u, -2, 0, 1u).available);
    CHECK(nd::decode_window_with(fake, bytes, 0x1000u, 0x1003u, -3, 0, 1u).instructions[0].pc == 0x1000u);
    for(auto offset : {-65536, 65536})
    {
        auto const outside{nd::decode_window_with(fake, bytes, 0x1000u, 0x1003u, offset, 0, 32u)};
        CHECK(outside.available && outside.count == 32u);
        for(auto const& row : outside.instructions) { CHECK(!row && row.pc == 0u); }
    }
    CHECK(!nd::decode_window_with(fake, bytes, 0x1000u, 0x1003u, INT64_MIN, 0, 1u).available);
    CHECK(!nd::decode_window_with(fake, bytes, 0x1000u, 0x1003u, 0, INT64_MIN, 1u).available);
    CHECK(!nd::decode_window_with(fake, bytes, UINTPTR_MAX - 1u, UINTPTR_MAX, 0, 0, 1u).available);
    CHECK(!nd::decode_window_with(fake, bytes, 0x1000u, 0x1003u, 0, 0, 33u).available);
    ::std::array<::std::uint8_t, 3u> failure{1u,0u,1u};
    auto const partial{nd::decode_window_with(fake, failure, 0x1000u, 0x1000u, 0, 0, 3u)};
    CHECK(partial.available && partial.instructions[0] && !partial.instructions[1] && !partial.instructions[2]);
    CHECK(!nd::decode_window_with(fake, failure, 0x1000u, 0x1002u, 0, 0, 1u).available);
    ::std::array<::std::uint8_t, 70001u> large{}; large.fill(1u);
    CHECK(nd::decode_window_with(fake, {large.data(),8193u}, 0x1000u, 0x1000u, 0, 0, 1u).available);
    auto const tail{nd::decode_window_with(fake, large, 0x1000u, 0x1000u + 70000u, 0, -2, 5u)};
    CHECK(tail.available && tail.count == 5u);
    CHECK(tail.instructions[0u].pc == 0x1000u + 69998u && tail.instructions[2u].pc == 0x1000u + 70000u);
    CHECK(!tail.instructions[3u] && !tail.instructions[4u]);
    CHECK(nd::decode_window_with(fake, large, 0x1000u, 0x1000u + 70000u, -65536, 1, 1u).instructions[0u].pc == 0x1000u + 4465u);
    // Bound real query work independently of the function length. Forward
    // rows come from the same entry-to-stop proof; proving a later stop must
    // still happen even when its requested page lies far earlier.
    decoder one_pass{};
    auto const forward_tail{nd::decode_window_with(one_pass,large,0x1000u,0x1000u+70000u,0,0,1u)};
    ::fast_io::io::println("forward tail decode calls=",one_pass.calls," expected=",large.size());
    CHECK(forward_tail.available && forward_tail.instructions[0u].pc==0x1000u+70000u && one_pass.calls==large.size());
    one_pass.calls=0u;
    auto const early_page{nd::decode_window_with(one_pass,large,0x1000u,0x1000u+70000u,-65536,1,1u)};
    CHECK(early_page.available && early_page.instructions[0u].pc==0x1000u+4465u && one_pass.calls==large.size());
    one_pass.calls=0u;
    auto const forward_page{nd::decode_window_with(one_pass,large,0x1000u,0x1000u+60000u,0,0,32u)};
    CHECK(forward_page.available && forward_page.count==32u && one_pass.calls==60032u);
    for(::std::size_t i{};i<32u;++i) { CHECK(forward_page.instructions[i].pc==0x1000u+60000u+i); }
    large[69999u] = 0u;
    auto const gap{nd::decode_window_with(fake, large, 0x1000u, 0x1000u + 69998u, 0, 0, 3u)};
    CHECK(gap.available && gap.instructions[0u] && !gap.instructions[1u] && !gap.instructions[2u]);
    CHECK(!nd::decode_window_with(fake, large, 0x1000u, 0x1000u + 70000u, 0, 0, 1u).available);
    CHECK(!nd::decode_window_with(fake,large,0x1000u,0x1000u+70000u,-65536,0,1u).available);
    // A page decoded before an unknown gap cannot authenticate a later stop.

    // Pure mapping policy DATA. Only a runtime-authenticated ARM ELF map
    // can supply this mask in the product; literal bytes are not instructions.
    ::std::array<::std::uint8_t,16u> island{4u,0u,0u,0u,15u,1u,2u,3u,4u,5u,6u,7u,4u,0u,0u,0u};
    ::std::array<::std::uint8_t,16u> mapping{1u,1u,1u,1u,0u,0u,0u,0u,0u,0u,0u,0u,1u,1u,1u,1u};
    CHECK(!nd::decode_window_with(fake,island,0x1000u,0x100cu,0,0,1u).available);
    auto const mapped{nd::decode_window_with(fake,island,0x1000u,0x100cu,0,-1,3u,mapping)};
    CHECK(mapped.available && mapped.instructions[0u].pc==0x1000u &&
        mapped.instructions[1u].pc==0x100cu && !mapped.instructions[2u]);
    CHECK(!nd::decode_window_with(fake,island,0x1000u,0x1004u,0,0,1u,mapping).available);
    CHECK(!nd::decode_window_with(fake,island,0x1000u,0x100cu,-8,0,1u,mapping).available);
    CHECK(!nd::decode_window_with(fake,island,0x1000u,0x100cu,0,0,1u,{mapping.data(),15u}).available);
    auto bad_mapping{mapping};bad_mapping[4u]=1u;
    CHECK(!nd::decode_window_with(fake,island,0x1000u,0x100cu,0,0,1u,bad_mapping).available);
    bad_mapping=mapping;bad_mapping[0u]=2u;
    CHECK(!nd::decode_window_with(fake,island,0x1000u,0x100cu,0,0,1u,bad_mapping).available);
    island[0u]=8u; // A decoder cannot consume across a proved DATA edge.
    CHECK(!nd::decode_window_with(fake,island,0x1000u,0x100cu,0,0,1u,mapping).available);
    island[0u]=0u; // Unknown TEXT never resynchronizes at a later map island.
    CHECK(!nd::decode_window_with(fake,island,0x1000u,0x100cu,0,0,1u,mapping).available);
    fake.enabled = false; CHECK(!nd::decode_window_with(fake, bytes, 0x1000u, 0x1003u, 0, 0, 1u).available);
    auto const parsed{command("disassemble-range 3 17 32 -65536 -200 1")};
    CHECK(parsed.kind == dbg::console_command_kind::assembly_disassemble_range && parsed.disassembly_resolve_symbols);
    CHECK(parsed.disassembly_byte_offset == -65536 && parsed.disassembly_instruction_offset == -200 && parsed.payload_size == 8u);
    for(auto const* invalid : {"disassemble-range 3 17 0 0 0 1","disassemble-range 3 17 33 0 0 1",
        "disassemble-range 3 17 1 -65537 0 1","disassemble-range 3 17 1 0 8705 1",
        "disassemble-range 3 17 1 0 -9223372036854775808 1","disassemble-range 3 17 1 0 0 2",
        "disassemble-range 3 17 1 0x1000 0 1","disassemble-range 3 17 1 0 0 1;quit",
        "disassemble-range 0 17 1 0 0 1","disassemble-range 3 0 1 0 0 1"})
    { CHECK(command(::fast_io::mnp::os_c_str(invalid)).kind == dbg::console_command_kind::invalid); }
    CHECK(command("disassemble-range 3 17 1 65536 8704 0").kind == dbg::console_command_kind::assembly_disassemble_range);
    ::fast_io::io::println("native window policy: PASS owned finite forward boundaries, offsets/fillers/budgets/grammar");
}
