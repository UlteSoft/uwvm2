// Finite metadata and OWNED copied numeric/guest-byte semantics only. These
// fixtures do not authorize any runtime stop, host address or guest read.
#include <uwvm2/uwvm/debugger/source_dwarf_selectors.h>
#include <fast_io.h>
#include <array>
#include <cstring>
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_dwarf_utf_values: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static type_record utf(::std::uint8_t width)
{ type_record out{}; out.kind = type_kind::scalar; out.encoding = 0x10u; out.byte_count = width;
  out.byte_size = width; out.size_known = true; return out; }
static member_record member(char const* name, ::std::size_t type, ::std::uint64_t offset)
{ member_record out{}; out.name = name; out.type = type; out.byte_offset = offset; out.offset_known = true; return out; }
template<typename Carrier> static copied_numeric_local local(::std::uint8_t type, Carrier bits)
{
    static_assert(sizeof(Carrier) == 4u || sizeof(Carrier) == 8u);
    copied_numeric_local out{}; out.wasm_type = type; out.available = true; out.bytes.fill(::std::byte{0xa5u});
    // [owned 16-byte carrier] end
    // [safe                 ] exact complete native i32/i64 representation.
    //  ^^ bytes beyond sizeof(Carrier) deliberately remain dirty; the public
    //     numeric query must decode the whole carrier before source-width mask.
    ::std::memcpy(out.bytes.data(), ::std::addressof(bits), sizeof(bits)); return out;
}
static variable_record variable(::std::size_t type, ::std::uint64_t slot)
{
    variable_record out{}; out.scope = 1u; out.type = type;
    location_plan plan{}; plan.kind = plan_kind::wasm_local_value; plan.reason = unavailable_reason::none; plan.local_index = slot;
    out.locations.push_back({{}, plan}); return out;
}
int main()
{
    ::std::vector<type_record> types{utf(1u), utf(2u), utf(4u), utf(8u), utf(3u)};
    auto pointer{utf(4u)}; pointer.kind = type_kind::pointer; types.push_back(pointer);
    auto unknown{utf(4u)}; unknown.encoding = 0xffu; unknown.name = "char32_t"; types.push_back(unknown);
    ::std::vector<scope_record> scopes(2u); scopes[0u].kind = scope_kind::compile_unit;
    scopes[1u].kind = scope_kind::subprogram; scopes[1u].parent = 0u; scopes[1u].concrete = true; scopes[1u].ranges = {{10u, 30u}};
    ::std::array<copied_numeric_local, 3u> locals{
        local(0x7fu, ::std::uint32_t{0x80000380u}), local(0x7eu, ::std::uint64_t{0xfedcba980001f642u}),
        local(0x7du, ::std::uint32_t{0x0001f642u})};
    ::std::vector<variable_record> variables{variable(0u, 0u), variable(1u, 0u), variable(2u, 1u),
        variable(3u, 1u), variable(4u, 0u), variable(5u, 0u), variable(6u, 0u), variable(2u, 2u)};
    ::std::vector<numeric_variable> values{};
    auto query{[&]() { return query_numeric_variables(scopes, types, variables, 15u, locals, locals.size(), values); }};
    check(query() == inline_query_error::none && values.size() == 8u, "all source records remain explicit");
    check(values[0u].kind == numeric_kind::unsigned_integer && values[0u].bits == 0x80u && values[0u].byte_count == 1u,
          "UTF8 code unit comes from low unsigned i32 bits on both endian hosts");
    check(values[1u].kind == numeric_kind::unsigned_integer && values[1u].bits == 0x380u && values[1u].byte_count == 2u,
          "UTF16 code unit is never sign extended");
    check(values[2u].kind == numeric_kind::unsigned_integer && values[2u].bits == 0x1f642u && values[2u].byte_count == 4u,
          "UTF32/Rust char comes from complete i64 before masking source width");
    for(::std::size_t i{3u}; i != 7u; ++i)
    { check(values[i].kind == numeric_kind::unavailable && values[i].reason == numeric_unavailable_reason::unsupported_type,
            "8-byte/3-byte UTF, pointers and unknown encodings are never inferred"); }
    check(values[7u].reason == numeric_unavailable_reason::carrier_mismatch, "UTF cannot reinterpret a float/ref carrier");
    locals[0u].available = false;
    check(query() == inline_query_error::none && values[0u].reason == numeric_unavailable_reason::local_unavailable &&
          values[1u].reason == numeric_unavailable_reason::local_unavailable && values[2u].bits == 0x1f642u,
          "uninitialized carrier never acquires availability through UTF encoding");
    locals[0u].available = true;
    ::std::array<::std::byte, 4u> const implicit{::std::byte{0x9eu}, ::std::byte{2u}, ::std::byte{0x3du}, ::std::byte{0xd8u}};
    variables = {variable(1u, 0u)}; variables.front().locations.front().plan = decode_location_plan(implicit, 4u);
    check(query() == inline_query_error::none && values.front().kind == numeric_kind::unsigned_integer && values.front().bits == 0xd83du,
          "implicit UTF16 surrogate retains one code unit, with no string decoding");
    variables.front().type = 2u;
    check(query() == inline_query_error::none && values.front().reason == numeric_unavailable_reason::implicit_width_mismatch,
          "implicit UTF width must exactly match source type");

    type_record pair{}; pair.kind = type_kind::array; pair.referenced_type = 1u; pair.byte_size = 4u; pair.size_known = true;
    pair.dimensions = {{0, 2u, true, true}}; types.push_back(pair);
    type_record packet{}; packet.kind = type_kind::structure; packet.byte_size = 12u; packet.size_known = true;
    packet.members = {member("octet", 0u, 0u), member("unit", 1u, 2u), member("point", 2u, 4u), member("pair", 7u, 8u)};
    types.push_back(packet);
    ::std::array<::std::byte, 12u> const bytes{::std::byte{0x80u}, ::std::byte{0xa5u}, ::std::byte{0xbbu}, ::std::byte{3u},
        ::std::byte{0x42u}, ::std::byte{0xf6u}, ::std::byte{1u}, ::std::byte{}, ::std::byte{0x3du}, ::std::byte{0xd8u},
        ::std::byte{0x42u}, ::std::byte{0xdeu}};
    ::std::vector<object_node> nodes{};
    check(query_object_value(types, 8u, bytes, nodes) == inline_query_error::none && nodes.size() == 7u,
          "guest-copy aggregate expands UTF fields and array leaves");
    for(auto const index : {1u, 2u, 3u, 5u, 6u})
    { check(nodes[index].value_available && nodes[index].scalar_kind == numeric_kind::unsigned_integer, "UTF object leaf is available"); }
    check(nodes[1u].bits == 0x80u && nodes[2u].bits == 0x3bbu && nodes[3u].bits == 0x1f642u &&
          nodes[5u].bits == 0xd83du && nodes[6u].bits == 0xde42u,
          "Wasm little-endian copies preserve UTF8/16/32 units on every host");
    object_selector selector{};
    check(parse_source_object_selector("packet.pair[1]", selector) == object_selector_error::none &&
          query_selected_object_value(types, 8u, selector.steps, bytes, nodes) == inline_query_error::none && nodes.size() == 1u &&
          nodes.front().byte_offset == 10u && nodes.front().bits == 0xde42u, "member/index selector uses same bounded UTF classifier");
    ::std::array<::std::byte, 12u> known{}; known.fill(::std::byte{0xffu}); known[11u] = ::std::byte{};
    check(query_object_value_with_known_bits(types, 8u, bytes, known, nodes) == inline_query_error::none &&
          nodes[3u].value_available && !nodes[6u].value_available && nodes[6u].reason == object_unavailable_reason::incomplete_value,
          "unknown UTF code-unit bit remains unavailable without erasing unrelated fields");
    check(query_object_value(types, 8u, ::std::span<::std::byte const>{bytes}.first(11u), nodes) == inline_query_error::none &&
          nodes.size() == 1u && nodes.front().reason == object_unavailable_reason::object_bounds,
          "truncated guest copy denies before forming any UTF field pointer");
    ::std::array<::std::byte, 8u> const wide{};
    check(query_object_value(types, 3u, wide, nodes) == inline_query_error::none && !nodes.front().value_available &&
          nodes.front().reason == object_unavailable_reason::unsupported_scalar, "8-byte UTF remains explicit unavailable in aggregate path");
    ::fast_io::io::println("PASS exact copied UTF8/16/32 source units, carrier widths, bounds and unavailable-bit preservation");
}
