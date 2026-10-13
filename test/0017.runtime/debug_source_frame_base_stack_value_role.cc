// Finite metadata/owned-carrier regression only. This does not qualify a live
// stop, caller capture, production guest-memory read or compiler output.
#include <uwvm2/uwvm/debugger/source_dwarf_objects.h>
#include <fast_io.h>
#include <array>
#include <limits>

using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool condition, char const* message)
{ if(!condition) { ::fast_io::io::perrln("frame-base-role: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
template<::std::size_t N> static location_plan decode(::std::array<unsigned char, N> const& bytes,
    position_role role, ::std::uint8_t width = 4u)
{
    // [fixed owned expression bytes ... N] end
    // [safe                             ] immutable borrow until decode returns;
    //  ^^ the array extent is exact; no host or guest pointer is formed from an operand.
    return decode_location_plan({reinterpret_cast<::std::byte const*>(bytes.data()), bytes.size()}, width, {}, role);
}
template<::std::size_t N> static void rejected(::std::array<unsigned char, N> const& bytes)
{
    for(auto const role : {position_role::variable, position_role::frame_base})
    { check(decode(bytes, role).kind == plan_kind::unavailable, "unsupported/truncated/trailing expression cannot become a local plan"); }
}
int main()
{
    ::std::array<unsigned char, 4u> const terminal{0xedu, 0u, 0u, 0x9fu};
    auto const base{decode(terminal, position_role::frame_base)};
    auto const value{decode(terminal, position_role::variable)};
    check(base.kind == plan_kind::wasm_local_frame_base && base.reason == unavailable_reason::none && base.local_index == 0u,
          "Clang terminal stack_value in frame-base role is a frame-base carrier");
    check(value.kind == plan_kind::wasm_local_value && value.reason == unavailable_reason::none && value.local_index == 0u,
          "identical variable expression retains copied numeric value role");
    ::std::array<unsigned char, 3u> const bare{0xedu, 0u, 0u};
    check(decode(bare, position_role::frame_base).kind == plan_kind::wasm_local_frame_base, "existing bare frame-base compatibility");
    auto const ambiguous{decode(bare, position_role::variable)};
    check(ambiguous.kind == plan_kind::unavailable && ambiguous.reason == unavailable_reason::indirect_or_ambiguous_local,
          "bare variable local remains unavailable and cannot grant an indirect read");
    rejected(::std::array<unsigned char, 3u>{0xedu, 0u, 0x80u});
    rejected(::std::array<unsigned char, 5u>{0x96u, 0xedu, 0u, 0u, 0x9fu});
    rejected(::std::array<unsigned char, 5u>{0xedu, 0u, 0u, 0x9fu, 0x96u});
    rejected(::std::array<unsigned char, 5u>{0xedu, 0u, 0u, 0x9fu, 0x9fu});
    // Indexed global/operand metadata is supported. It must retain its storage
    // space and cannot consume an ordinary copied local before coherent remapping.
    ::std::array<location_plan, 3u> const indexed{
        decode(::std::array<unsigned char, 4u>{0xedu, 1u, 0u, 0x9fu}, position_role::frame_base),
        decode(::std::array<unsigned char, 4u>{0xedu, 2u, 0u, 0x9fu}, position_role::frame_base),
        decode(::std::array<unsigned char, 7u>{0xedu, 3u, 0u, 0u, 0u, 0u, 0x9fu}, position_role::frame_base)};
    for(::std::size_t i{}; i != indexed.size(); ++i)
    { check(indexed[i].kind == plan_kind::wasm_local_frame_base && indexed[i].local_index == 0u &&
            indexed[i].storage == (i == 1u ? wasm_location_space::operand : wasm_location_space::global),
            "indexed metadata preserves exact storage space and frame-base role"); }
    auto const global_value{decode(::std::array<unsigned char, 4u>{0xedu, 1u, 0u, 0x9fu}, position_role::variable)};
    check(global_value.kind == plan_kind::wasm_local_value && global_value.storage == wasm_location_space::global,
          "indexed variable retains copied value role");
    rejected(::std::array<unsigned char, 4u>{0xedu, 4u, 0u, 0x9fu});
    rejected(::std::array<unsigned char, 4u>{0xedu, 3u, 0u, 0x9fu});
    rejected(::std::array<unsigned char, 13u>{0xedu, 0u, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 2u, 0x9fu});
    auto const fbreg{decode(::std::array<unsigned char, 2u>{0x91u, 0x7cu}, position_role::variable)};
    check(fbreg.kind == plan_kind::frame_relative_offset && fbreg.displacement == -4, "signed fbreg displacement remains metadata");
    ::std::array<location_record, 1u> bases{{{::std::nullopt, base}}};
    ::std::array<copied_numeric_local, 1u> locals{};
    locals[0u].wasm_type = 0x7fu; locals[0u].available = true;
    ::std::uint32_t carrier{20u};
    // [owned fixed local slot: 16 bytes] end
    // [safe                          ] exact i32 carrier, no generated-frame borrow.
    ::fast_io::freestanding::my_memcpy(locals[0u].bytes.data(), ::std::addressof(carrier), sizeof(carrier));
    ::std::uint64_t offset{};
    check(resolve_frame_relative_offset(fbreg, bases, 0u, locals, 1u, offset) == object_location_error::none && offset == 16u,
          "terminal frame-base plan reaches the unchanged bounded owned-carrier consumer");
    for(auto const& plan : indexed)
    {
        bases[0u].plan = plan;
        check(resolve_frame_relative_offset(fbreg, bases, 0u, locals, 1u, offset) == object_location_error::local_not_captured && offset == 0u,
              "unremapped global/operand never aliases same-numbered captured local");
    }
    bases[0u].plan = base;
    locals[0u].available = false;
    check(resolve_frame_relative_offset(fbreg, bases, 0u, locals, 1u, offset) == object_location_error::local_unavailable,
          "unavailable local remains unreadable");
    locals[0u].available = true; locals[0u].wasm_type = 0x7eu;
    check(resolve_frame_relative_offset(fbreg, bases, 0u, locals, 1u, offset) == object_location_error::carrier_mismatch,
          "Wasm32 frame base still requires exact i32 carrier");
    locals[0u].wasm_type = 0x7fu;
    check(resolve_frame_relative_offset(fbreg, bases, 0u, {}, 1u, offset) == object_location_error::local_not_captured,
          "metadata never invents a missing captured carrier");
    bases[0u].plan = value;
    check(resolve_frame_relative_offset(fbreg, bases, 0u, locals, 1u, offset) == object_location_error::unavailable_frame_base,
          "variable-role numeric plan cannot substitute for a frame-base plan");
    bases[0u].plan = decode(terminal, position_role::frame_base, 8u);
    auto wide{fbreg}; wide.address_bytes = 8u;
    locals[0u].wasm_type = 0x7eu;
    ::std::uint64_t wide_carrier{0u};
    ::fast_io::freestanding::my_memcpy(locals[0u].bytes.data(), ::std::addressof(wide_carrier), sizeof(wide_carrier));
    check(resolve_frame_relative_offset(wide, bases, 0u, locals, 1u, offset) == object_location_error::address_overflow,
          "negative displacement cannot underflow the captured guest base");
    wide.displacement = 1; wide_carrier = (::std::numeric_limits<::std::uint64_t>::max)();
    ::fast_io::freestanding::my_memcpy(locals[0u].bytes.data(), ::std::addressof(wide_carrier), sizeof(wide_carrier));
    check(resolve_frame_relative_offset(wide, bases, 0u, locals, 1u, offset) == object_location_error::address_overflow,
          "positive displacement cannot overflow the captured guest base");
    wide.displacement = (::std::numeric_limits<::std::int64_t>::min)(); wide_carrier = ::std::uint64_t{1u} << 63u;
    ::fast_io::freestanding::my_memcpy(locals[0u].bytes.data(), ::std::addressof(wide_carrier), sizeof(wide_carrier));
    check(resolve_frame_relative_offset(wide, bases, 0u, locals, 1u, offset) == object_location_error::none && offset == 0u,
          "INT64_MIN displacement is supported without signed negation overflow");
    rejected(::std::array<unsigned char, 11u>{0x91u, 0x80u, 0x80u, 0x80u, 0x80u, 0x80u, 0x80u, 0x80u, 0x80u, 0x80u, 1u});
    ::fast_io::io::println("PASS finite frame-base role and owned-carrier boundaries; live runtime qualification separate");
}
