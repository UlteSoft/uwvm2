/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
// Textual mode deliberately includes the new header FIRST. A preincluded
// command.h/FastIO DSAL alias must not conceal a missing public dependency.
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/debugger/wasm_state.h>
#endif
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <fast_io.h>
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/debugger/command.h>
#else
import uwvm2.uwvm.debugger;
import uwvm2.uwvm.debugger.wasm_state;
#endif

namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
namespace dbg = ::uwvm2::uwvm::debugger;
namespace
{
    unsigned checks{};
    [[nodiscard]] bool expect(bool condition, ::fast_io::string_view name)
    {
        ++checks;
        if(!condition) { ::fast_io::println(::fast_io::err(), "FAILED Wasm state view: ", name); }
        return condition;
    }
    template<unsigned Bits> [[nodiscard]] ws::value numeric(ws::value_kind kind, ::std::uint64_t bits)
    {
        ws::value result{}; result.type.kind = kind; result.type.known = true; result.available = true;
        static_assert(Bits == 32u || Bits == 64u);
        auto* first{reinterpret_cast<unsigned char*>(result.bits.data())};
        // [owned numeric array ... Bits/8 <= 16] end
        // [safe                               ] unsafe (one-past)
        //  ^^ exact fixed destination extent BEFORE end formation/LE output.
        ::fast_io::basic_obuffer_view<unsigned char> output{first, first + Bits / 8u};
        ::fast_io::print(output, ::fast_io::mnp::le_put<Bits>(bits));
        return result;
    }
    [[nodiscard]] ws::value ref(ws::reference_kind kind, ::std::int64_t heap, ::std::uint64_t id = 0u)
    {
        ws::value result{}; result.type = {ws::value_kind::reference, heap, true, true};
        result.type.type_module = 3u;
        result.ref.kind = kind; result.ref.object = id; result.available = true; return result;
    }
    [[nodiscard]] ws::view base(ws::selection selected = ws::selection::operands)
    {
        ws::view result{}; result.result = ws::status::available;
        result.requested.selected = selected; result.requested.participant = 7u;
        result.requested.count = ws::maximum_rows; result.runtime_epoch = 11u; result.module = 3u;
        return result;
    }
    [[nodiscard]] bool contains(::std::string const& text, ::std::string_view pattern) noexcept
    { return ::std::string_view{text}.find(pattern) != ::std::string_view::npos; }
}
int main(int argc, char const** argv)
{
    if(argc == 2 && ::std::string_view{argv[1]} == "--require-big")
    { if(::std::endian::native != ::std::endian::big) { return 90; } }
    else if(argc != 1) { return 91; }
    bool good{true};
    auto command{dbg::parse_console_command("info globals 7 3 4 12")};
    good &= expect(command.kind == dbg::console_command_kind::wasm_state &&
        command.operation == ::uwvm2::utils::control::operation::status && command.payload_size == 0u &&
        command.wasm_state_request.selected == ws::selection::globals && command.wasm_state_request.participant == 7u &&
        command.wasm_state_request.module == 3u && command.wasm_state_request.first == 4u && command.wasm_state_request.count == 12u,
        "authenticated global page selector");
    command = dbg::parse_console_command("info table 7 3 2 4294967301 64");
    good &= expect(command.kind == dbg::console_command_kind::wasm_state && command.wasm_state_request.index == 2u &&
        command.wasm_state_request.first == 4294967301ull, "table64 offset is not truncated to i32");
    command = dbg::parse_console_command("operands 7 2 1 8");
    good &= expect(command.kind == dbg::console_command_kind::wasm_state && command.wasm_state_request.frame == 2u &&
        command.wasm_state_request.first == 1u && command.wasm_state_request.count == 8u, "operand exact frame and page");
    command = dbg::parse_console_command("locals wasm 7 0 1 8");
    good &= expect(command.kind == dbg::console_command_kind::wasm_state &&
        command.wasm_state_request.selected == ws::selection::locals, "typed original-index locals grammar");
    for(auto const text : {"globals 0 3", "globals 7 3 0 65", "table 7 3 2 0 0", "operands 7 -1", "globals 7 3 0x10 2",
        "globals 7 3 18446744073709551616 2", "inspect 0x1000", "info table 7 3 2 4", "operands 7 0 1 2 extra"})
    { good &= expect(dbg::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).kind == dbg::console_command_kind::invalid,
        "reject malformed, overflow, native address or excessive page"); }

    command = dbg::parse_console_command("members globals 7 3 0 0 2 960 64 4 1000");
    good &= expect(command.kind == dbg::console_command_kind::wasm_state && command.wasm_state_request.count == 1u &&
        command.wasm_state_request.first == 2u && command.wasm_state_request.member_first == 960u &&
        command.wasm_state_request.member_count == 64u && command.wasm_state_request.path_size == 2u &&
        command.wasm_state_request.path[0u] == 4u && command.wasm_state_request.path[1u] == 1000u,
        "member page accepts ORIGINAL root and path, not dense object ID");
    command = dbg::parse_console_command("members table 7 3 0 2 4294967301 0 1");
    good &= expect(command.kind == dbg::console_command_kind::wasm_state && command.wasm_state_request.first == 4294967301ull &&
        command.wasm_state_request.index == 2u, "member root table64 index stays u64");
    for(auto const invalid : {"members globals 7 3 1 0 0 0 64", "members globals 7 3 0 1 0 0 64",
        "members globals 7 3 0 0 0 0 0", "members globals 7 3 0 0 0 0 65", "members globals 7 3 0 0 0 0 64 -1",
        "members globals 7 3 0 0 0 0 64 18446744073709551616", "members native 7 3 0 0 0 0 64",
        "members globals 7 3 0 0 0 0 64 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0"})
    {
        auto const actual{dbg::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(invalid)}).kind};
        if(actual != dbg::console_command_kind::invalid)
        { ::fast_io::io::perr("unexpected member command admitted: ", ::fast_io::mnp::os_c_str(invalid), "\n"); }
        good &= expect(actual == dbg::console_command_kind::invalid,
            "bounded member grammar refuses invalid locus/page/path before borrow");
    }

    auto result{base()}; result.total_values = 4u;
    result.rows = {{0u, numeric<32u>(ws::value_kind::i32, 0xfffffff9u)},
        {1u, numeric<64u>(ws::value_kind::i64, 0xffffffffffffffffull)},
        {2u, numeric<32u>(ws::value_kind::f32, 0x7fc12345u)},
        {3u, numeric<64u>(ws::value_kind::f64, 0x7ff8123456789abcull)}};
    auto text{ws::format(result)};
    good &= expect(ws::valid(result) && contains(text, "operand 0 i32 = -7") && contains(text, "operand 1 i64 = -1") &&
        contains(text, "f32 = bits=0x7fc12345") && contains(text, "f64 = bits=0x7ff8123456789abc"),
        "canonical LE integers and unchanged NaN bits on either host endian");
    good &= expect(contains(text, "module=3 epoch=11"), "actual module and epoch are copied labels");
    good &= expect(contains(text, "Note: Last Wasm safepoint snapshot; may differ from current native state.\n"),
        "operand preview identifies its Wasm safepoint origin");
    auto const empty_text{ws::format(base())};
    good &= expect(contains(empty_text, "Note: Last Wasm safepoint snapshot;") && !contains(empty_text, "operand 0 "),
        "empty operand stack still identifies its snapshot origin");

    auto graph{base(ws::selection::globals)}; graph.total_values = 5u;
    ws::object structure{}; structure.identifier = 1u; structure.module = 3u; structure.type_index = 6u;
    structure.kind = ws::object_kind::structure; structure.total_members = 2u; structure.next_member = 2u;
    auto packed{numeric<32u>(ws::value_kind::i32, 255u)}; packed.packed_bits = 8u;
    structure.members = {{0u, ref(ws::reference_kind::structure, 6u, 1u)}, {1u, packed}};
    ws::object exception{}; exception.identifier = 2u; exception.kind = ws::object_kind::exception;
    exception.tag_identity_available = true; exception.tag_module = 3u; exception.tag_index = 1u; exception.total_members = 2u; exception.next_member = 2u;
    auto i31{ref(ws::reference_kind::i31, -20)}; i31.ref.i31_bits = 0x7fffffffu;
    exception.members = {{0u, numeric<64u>(ws::value_kind::i64, 42u)}, {1u, i31}};
    ws::object host{}; host.identifier = 3u; host.kind = ws::object_kind::host_reference;
    ws::object wrapper{}; wrapper.identifier = 4u; wrapper.kind = ws::object_kind::external_wrapper; wrapper.total_members = 1u; wrapper.next_member = 1u;
    wrapper.members = {{0u, ref(ws::reference_kind::structure, 6u, 1u)}};
    graph.objects = {structure, exception, host, wrapper};
    auto function{ref(ws::reference_kind::function, -16)}; function.ref.function_identity_available = true;
    function.ref.function_module = 3u; function.ref.function_index = 9u;
    graph.rows = {{0u, ref(ws::reference_kind::structure, 6u, 1u)}, {1u, ref(ws::reference_kind::structure, 6u, 1u)},
        {2u, ref(ws::reference_kind::exception, -23, 2u)}, {3u, ref(ws::reference_kind::external_wrapper, -17, 4u)}, {4u, function}};
    text = ws::format(graph);
    good &= expect(ws::valid(graph) && contains(text, "global 0 (ref null type-index=6 module=3) = struct #1") &&
        contains(text, "global 1 (ref null type-index=6 module=3) = struct #1") && contains(text, "object #1 struct module=3 type=6") &&
        contains(text, "0 (ref null type-index=6 module=3) = struct #1") && !contains(text, "funcref"),
        "GC actual struct kind, alias and recursive field are not mislabeled funcref");
    good &= expect(contains(text, "exception tag-module=3 tag=1 members=2") && contains(text, "i64 = 42") &&
        contains(text, "i31 signed=-1 unsigned=2147483647"), "exception exact tag and typed immutable payload");
    good &= expect(contains(text, "extern-wrapper #4") && contains(text, "opaque host-reference (payload unavailable)") &&
        contains(text, "function module=3 index=9"), "host/wrapper/function identities contain no native address");
    good &= expect(!graph.snapshot_or_restore_authority(), "copied state graph is never restore authority");
    auto imported{graph}; imported.objects[0].module = 4u;
    imported.objects[0].members[0].data.type.type_module = 4u;
    auto const imported_text{ws::format(imported)};
    good &= expect(ws::valid(imported) && contains(imported_text, "global 0 (ref null type-index=6 module=3) = struct #1") &&
        contains(imported_text, "object #1 struct module=4 type=6") &&
        contains(imported_text, "0 (ref null type-index=6 module=4) = struct #1"),
        "declared heap module stays distinct from actual imported object owner");

    auto bad{graph}; bad.rows[0].data.ref.object = 0u;
    good &= expect(!ws::valid(bad), "missing GC logical ID");
    bad = graph; bad.rows[0].data.ref.object = 65u;
    good &= expect(!ws::valid(bad), "out of range GC logical ID checked before index");
    bad = graph; bad.objects[1].identifier = 1u;
    good &= expect(!ws::valid(bad), "duplicate dense object ID");
    bad = graph; bad.rows[0].data.ref.kind = ws::reference_kind::exception;
    good &= expect(!ws::valid(bad), "reference actual kind does not match logical object");
    bad = graph; bad.objects[3].members[0].data = ref(ws::reference_kind::external_wrapper, -17, 4u);
    good &= expect(!ws::valid(bad), "wrapper-only cycle rejected while GC cycle stays valid");
    bad = graph; bad.rows[0].data.bits[8] = ::std::byte{0xa5u};
    good &= expect(!ws::valid(bad), "reference raw native carrier bytes never accepted");
    bad = result; bad.rows[0].data.bits[8] = ::std::byte{1u};
    good &= expect(!ws::valid(bad), "unused numeric padding is canonical zero");
    bad = graph; bad.objects[0].total_members = 1u;
    good &= expect(!ws::valid(bad), "member count bounds checked before formatting");
    bad = graph; bad.objects[0].members.resize(257u);
    good &= expect(!ws::valid(bad), "aggregate member quota enforced");
    bad = result; bad.requested.count = 65u;
    good &= expect(!ws::valid(bad), "format independently enforces query quota");

    auto uninitialized{base(ws::selection::locals)}; uninitialized.total_values = 1u;
    auto unset{ref(ws::reference_kind::structure, 6u)}; unset.available = false; unset.unavailable = ws::unavailable_reason::not_initialized;
    uninitialized.rows = {{0u, unset}};
    text = ws::format(uninitialized);
    good &= expect(ws::valid(uninitialized) && contains(text, "unavailable (local is not initialized)") &&
        !contains(text, "= null"), "nondefaultable unset local remains unavailable, never fake null");
    uninitialized.rows[0].data.bits[0] = ::std::byte{1u};
    good &= expect(!ws::valid(uninitialized), "unavailable local cannot carry stale payload");
    ws::view unsupported{}; unsupported.result = ws::status::unavailable_typed_site;
    good &= expect(contains(ws::format(unsupported), "this control or exception site has no complete typed packet"),
        "unsupported legal control site is reported precisely");
    auto wasi{dbg::parse_console_command("info wasip1 env 0 2 3")};
    good &= expect(wasi.kind == dbg::console_command_kind::wasip1_state &&
        wasi.wasip1_state_request.operation == dbg::wasip1_state::action::environment &&
        wasi.wasip1_state_request.module == 0u && wasi.wasip1_state_request.first == 2u &&
        wasi.wasip1_state_request.count == 3u, "independent WASIp1 query console route");
    wasi = dbg::parse_console_command("set wasip1 env 0 4b4559 76616c7565");
    good &= expect(wasi.kind == dbg::console_command_kind::wasip1_state &&
        wasi.wasip1_state_request.operation == dbg::wasip1_state::action::set_environment &&
        wasi.wasip1_state_request.name == u8"KEY" && wasi.wasip1_state_request.value == u8"value",
        "host-only hexadecimal environment mutation route");
    for(auto const invalid : {"info wasip1 env 0 0 65", "set wasip1 env 0 00 01", "set wasip1 env 0 4b45593d -",
        "info wasip1 args -1", "set wasip1 rights 0 2147483648 0 0 0 0", "unset wasip1 env 0 0g"})
    { good &= expect(dbg::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(invalid)}).kind == dbg::console_command_kind::invalid,
        "invalid WASIp1 spelling is refused before runtime admission"); }
    if(!good) { return 1; }
    ::fast_io::print(::fast_io::out(), "Wasm state DATA grammar/format checks=", ::fast_io::mnp::dec(checks),
        " native endian=", ::fast_io::mnp::os_c_str(::std::endian::native == ::std::endian::big ? "big" : "little"),
        "; no runtime capture or restore qualification\n");
    return 0;
}
