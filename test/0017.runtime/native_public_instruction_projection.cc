// Actual MC + public projection DATA; no synthetic buffer qualifies a live trap.
#include <uwvm2/uwvm/debugger/native_branch_display.h>
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <array>
#include <span>
namespace dbg = ::uwvm2::uwvm::debugger;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("public native projection line=", __LINE__); return 1; } } while(false)
struct image
{
    ::std::array<::std::uint8_t, 16u> bytes{}, guest_code{};
    ::std::span<::std::uint8_t const> instruction_code{};
    ::std::size_t size{16u};
    ::std::uintptr_t owner_begin{0x1000u}, owner_end{0x1010u};
};
int main()
{
#if !defined(__x86_64__) && !defined(_M_X64)
    return 77;
#else
    dbg::native_disassembly::decoder display{};
    dbg::native_owned_instruction_semantics::decoder semantics{};
    CHECK(display && semantics);
    image owned{};
    auto fresh = [&] { owned.bytes.fill(0x90u); owned.guest_code.fill(1u); owned.instruction_code = {}; };
    auto shown = [&](::std::span<::std::uint8_t const> bytes)
    {
        fresh();
        for(::std::size_t index{}; index != bytes.size(); ++index) { owned.bytes[index] = bytes[index]; }
        return display.decode(owned.owner_begin, owned.bytes);
    };
    auto project = [&](dbg::native_disassembly::instruction const& instruction)
    { return dbg::native_branch_display::project(owned, display, semantics, instruction); };
    auto instruction{shown(::std::array<::std::uint8_t,2u>{0x75u,0x02u})};
    auto approved{project(instruction)};
    CHECK(approved && approved.size == 2u);
    auto target{dbg::native_branch_display::decode(semantics, approved)};
    CHECK(target && target.destination.display_pc == 0x1004u);
    CHECK(dbg::native_branch_display::displayable(semantics, approved, target, 0x1000u, 0x1010u));
    CHECK(!dbg::native_branch_display::displayable(semantics, approved, {}, 0x1000u, 0x1010u));
    owned.guest_code[4u] = 0u; CHECK(!project(instruction)); // in-owner scaffold target
    owned.guest_code[4u] = 1u; owned.guest_code[0u] = 0u; CHECK(!project(instruction));
    owned.guest_code[0u] = 1u; owned.bytes[0u] ^= 1u; CHECK(!project(instruction));
    instruction = shown(::std::array<::std::uint8_t,4u>{0x75u,0x01u,0x31u,0xc0u});
    CHECK(!project(instruction)); // target in middle of XOR, not an ISA boundary
    instruction = shown(::std::array<::std::uint8_t,2u>{0x75u,0x10u});
    CHECK(!project(instruction)); // original 0x1012 regression, outside half-open owner
    target = dbg::native_branch_display::decode(semantics, instruction);
    for(bool resolve : {false,true})
    {
        ::fast_io::string output{};
        dbg::native_branch_display::print(::fast_io::ostring_ref_fast_io{__builtin_addressof(output)}, target, 0x1000u, 0x1010u, resolve, 1u, 2u, 3u);
        CHECK(output.empty());
        dbg::native_branch_display::print(::fast_io::ostring_ref_fast_io{__builtin_addressof(output)}, target, 0u, 0u, resolve, 1u, 2u, 3u);
        CHECK(output.empty());
    }
    instruction = shown(::std::array<::std::uint8_t,2u>{0xebu,0x0eu}); CHECK(!project(instruction)); // owner_end
    instruction = shown(::std::array<::std::uint8_t,2u>{0xebu,0xfcu}); CHECK(!project(instruction)); // backward outside
    instruction = shown(::std::array<::std::uint8_t,5u>{0xe8u,0u,4u,0u,0u}); CHECK(!project(instruction));
    instruction = shown(::std::array<::std::uint8_t,2u>{0xffu,0xd0u}); CHECK(!project(instruction));
    instruction = shown(::std::array<::std::uint8_t,2u>{0xffu,0xe0u}); CHECK(!project(instruction));
    instruction = shown(::std::array<::std::uint8_t,1u>{0xc3u}); CHECK(!project(instruction));
    instruction = shown(::std::array<::std::uint8_t,7u>{0x48u,0x8bu,0x05u,1u,2u,3u,4u}); CHECK(!project(instruction));
    instruction = shown(::std::array<::std::uint8_t,7u>{0x48u,0x8du,0x05u,1u,2u,3u,4u}); CHECK(!project(instruction));
    instruction = shown(::std::array<::std::uint8_t,4u>{0x48u,0x8bu,0x04u,0x24u}); CHECK(!project(instruction));
    instruction = shown(::std::array<::std::uint8_t,3u>{0x48u,0x89u,0xe0u}); CHECK(!project(instruction));
    instruction = shown(::std::array<::std::uint8_t,10u>{0x48u,0xb8u,1u,2u,3u,4u,5u,6u,7u,8u}); CHECK(!project(instruction));
    instruction = shown(::std::array<::std::uint8_t,2u>{0x0fu,0x05u}); CHECK(!project(instruction));
    instruction = shown(::std::array<::std::uint8_t,2u>{0x31u,0xc0u}); approved = project(instruction); CHECK(approved);
    instruction = shown(::std::array<::std::uint8_t,2u>{0x90u,0xccu});
    CHECK(instruction && instruction.size == 1u);
    for(::std::size_t index{1u}; index != instruction.bytes.size(); ++index) { CHECK(instruction.bytes[index] == 0u); }
    // Also clean a caller's stale fixed-array tails, not just decoder lookahead.
    instruction.bytes[8u] = 0xabu; instruction.text[100u] = 'X'; owned.guest_code[1u] = 0u;
    approved = project(instruction); CHECK(approved && approved.size == 1u);
    for(::std::size_t index{1u}; index != approved.bytes.size(); ++index) { CHECK(approved.bytes[index] == 0u); }
    bool ended{};
    for(char character : approved.text) { if(ended) { CHECK(character == '\0'); } if(character == '\0') { ended = true; } }
    CHECK(ended);
    // Actual MC projection consumes a private layout DATA mask separately
    // from public numeric permission. Even deliberately permissive guest_code
    // cannot publish a literal byte or a branch into the literal island.
    // This component fixture grants no real owner or native trap authority.
    ::std::array<::std::uint8_t,16u> layout{};
    layout.fill(1u);
    instruction = shown(::std::array<::std::uint8_t,2u>{0xebu,0x02u});
    owned.instruction_code = layout;
    CHECK(project(instruction)); // Empty/default and complete TEXT remain valid.
    for(::std::size_t i{4u};i<12u;++i) { layout[i]=0u; }
    CHECK(!project(instruction)); // Valid decode at 0x1004 is still literal DATA.
    auto literal{display.decode(owned.owner_begin+4u,{owned.bytes.data()+4u,12u})};
    CHECK(literal && !project(literal));
    instruction = shown(::std::array<::std::uint8_t,2u>{0xebu,0x0au});
    owned.instruction_code = layout;
    CHECK(project(instruction)); // Exact forward TEXT boundary after known DATA.
    auto tail{display.decode(owned.owner_begin+12u,{owned.bytes.data()+12u,4u})};
    CHECK(tail && project(tail));
    owned.instruction_code = {layout.data(),15u};
    CHECK(!project(instruction)); // Incomplete mask never qualifies a shown row.
    owned.instruction_code = layout;
    layout[1u]=0u;
    CHECK(!project(instruction)); // Every source byte must independently be TEXT.
    layout[1u]=1u;layout[12u]=2u;
    CHECK(!project(tail)); // Unknown layout state fails closed before publication.
    layout[12u]=1u; CHECK(project(instruction));
    owned.owner_end = owned.owner_begin; CHECK(!project(instruction));
    ::fast_io::io::println("PASS public native instruction projection: target/source provenance, MC operands, array tails");
#endif
}
