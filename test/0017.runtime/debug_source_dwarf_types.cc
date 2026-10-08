#include <uwvm2/uwvm/debugger/source_dwarf_types.h>
#include <fast_io.h>
#include <array>
#include <cstdlib>

using namespace uwvm2::uwvm::debugger::source_dwarf;
[[noreturn]] static void fail(char const* message)
{ ::fast_io::io::perrln("debug_source_dwarf_types: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
static void check(bool value, char const* message) { if(!value) { fail(message); } }
template<::std::size_t N> static location_plan plan(::std::array<unsigned char, N> const& bytes,
    position_role role = position_role::variable)
{
    // [safe] this fixed array owns N bytes until the synchronous decoder returns.
    return decode_location_plan({reinterpret_cast<::std::byte const*>(bytes.data()), bytes.size()}, 4u, {}, role);
}
int main()
{
    ::std::size_t used{};
    check(budget::charge(3u, 5u, used) && used == 3u, "budget charge");
    check(!budget::charge(3u, 5u, used) && used == 3u, "budget rejection preserves count");
    used = (::std::numeric_limits<::std::size_t>::max)();
    check(!budget::charge(1u, used, used), "budget cannot wrap");
    auto value{plan(::std::array<unsigned char, 5u>{0xedu, 0u, 0x81u, 1u, 0x9fu})};
    check(value.kind == plan_kind::wasm_local_value && value.local_index == 129u, "local stack-value plan");
    value = plan(::std::array<unsigned char, 3u>{0xedu, 0u, 1u});
    check(value.kind == plan_kind::unavailable && value.reason == unavailable_reason::indirect_or_ambiguous_local, "plain local never grants value read");
    value = plan(::std::array<unsigned char, 3u>{0xedu, 0u, 1u}, position_role::frame_base);
    check(value.kind == plan_kind::wasm_local_frame_base && value.local_index == 1u, "frame-base metadata only");
    value = plan(::std::array<unsigned char, 2u>{0x91u, 0x7cu});
    check(value.kind == plan_kind::frame_relative_offset && value.displacement == -4, "signed frame displacement metadata");
    value = plan(::std::array<unsigned char, 3u>{0x10u, 0x2au, 0x9fu});
    check(value.kind == plan_kind::constant_value && value.constant_bits == 42u, "constant value plan");
    value = plan(::std::array<unsigned char, 3u>{0x11u, 0x7fu, 0x9fu});
    check(value.signed_constant && value.constant_bits == (::std::numeric_limits<::std::uint64_t>::max)(), "signed constant preserves bits");
    value = plan(::std::array<unsigned char, 4u>{0x9eu, 2u, 0x12u, 0x34u});
    check(value.implicit_constant && value.byte_count == 2u && value.implicit_bytes[1u] == ::std::byte{0x34}, "implicit bytes are copied");
    value = plan(::std::array<unsigned char, 3u>{0xedu, 0u, 0x80u});
    check(value.reason == unavailable_reason::malformed_expression, "truncated known local LEB");
    value = plan(::std::array<unsigned char, 3u>{0xedu, 1u, 0u});
    check(value.reason == unavailable_reason::indirect_or_ambiguous_local, "global requires terminal value");
    value = plan(::std::array<unsigned char, 3u>{0xedu, 2u, 0u});
    check(value.reason == unavailable_reason::indirect_or_ambiguous_local, "operand requires terminal value");
    value = plan(::std::array<unsigned char,4u>{0xedu,2u,1u,0x9fu});
    check(value.kind == plan_kind::wasm_local_value && value.storage == wasm_location_space::operand && value.local_index == 1u,"bottom-first operand metadata");
    value = plan(::std::array<unsigned char,6u>{0xedu,3u,2u,0u,0u,0u},position_role::frame_base);
    check(value.kind == plan_kind::wasm_local_frame_base && value.storage == wasm_location_space::global && value.local_index == 2u,"fixed32 global frame base");
    value = plan(::std::array<unsigned char, 1u>{0x06u});
    check(value.reason == unavailable_reason::unsupported_expression, "deref unavailable without execution");
    value = plan(::std::array<unsigned char, 1u>{0x98u});
    check(value.reason == unavailable_reason::unsupported_expression, "call expression unavailable");
    limits cap{}; cap.max_expression_bytes = 0u;
    ::std::array<::std::byte, 1u> bytes{::std::byte{0x30}};
    check(decode_location_plan(bytes, 4u, cap).reason == unavailable_reason::expression_limit, "expression budget");
    check(decode_location_plan(bytes, 2u).reason == unavailable_reason::malformed_expression, "unsupported address width");
    ::fast_io::io::println("debug_source_dwarf_types: PASS");
}
