#include <uwvm2/uwvm/debugger/source_dwarf_values.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_dwarf_pieces: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
template<::std::size_t N> static location_plan decode(::std::array<unsigned char, N> const& bytes, limits const& cap = {})
{ return decode_location_plan({reinterpret_cast<::std::byte const*>(bytes.data()), bytes.size()}, 4u, cap); }
static copied_numeric_local local(::std::uint32_t bits)
{ copied_numeric_local out{}; out.wasm_type = 0x7fu; out.available = true; ::std::memcpy(out.bytes.data(), ::std::addressof(bits), sizeof(bits)); return out; }
int main()
{
    auto plan{decode(::std::array<unsigned char, 12u>{0xedu, 0u, 0u, 0x9fu, 0x93u, 2u, 0xedu, 0u, 1u, 0x9fu, 0x93u, 2u})};
    check(plan.kind == plan_kind::composite_value && plan.pieces.size() == 2u && plan.composite_bits == 32u, "piece grammar preserves complete low-half fragments");
    ::std::array<copied_numeric_local, 2u> locals{local(0xdead1234u), local(0xcafe5678u)}; composite_value out{};
    check(materialize_location_pieces(plan, locals, 2u, {}, 4u, out) == piece_query_error::none && out.fully_available &&
          out.bytes == ::std::vector<::std::byte>{::std::byte{0x34u}, ::std::byte{0x12u}, ::std::byte{0x78u}, ::std::byte{0x56u}}, "pieces compose Wasm little endian from native carrier copies on any host");
    ::std::vector<scope_record> scopes(2u); scopes[0u].kind = scope_kind::compile_unit;
    scopes[1u].parent = 0u; scopes[1u].kind = scope_kind::subprogram; scopes[1u].concrete = true; scopes[1u].ranges = {{10u, 30u}};
    ::std::vector<type_record> types(1u);
    types[0u].name = ::fast_io::concat_std("int"); types[0u].kind = type_kind::scalar;
    types[0u].encoding = 0x05u; types[0u].byte_count = 4u;
    types[0u].byte_size = 4u; types[0u].size_known = true;
    variable_record variable{}; variable.scope = 1u; variable.type = 0u; variable.name = "split"; variable.locations = {{{}, plan}};
    ::std::vector<numeric_variable> values{};
    check(query_numeric_variables(scopes, types, {&variable, 1u}, 15u, locals, 2u, values) == inline_query_error::none &&
          values[0u].bits == 0x56781234u && values[0u].reason == numeric_unavailable_reason::none, "complete composite scalar reaches existing source-local query");
    locals[0u].available = false;
    check(materialize_location_pieces(plan, locals, 2u, {}, 4u, out) == piece_query_error::none && !out.fully_available &&
          out.piece_reasons[0u] == piece_unavailable_reason::local_unavailable && out.known_bits[0u] == ::std::byte{} && out.known_bits[1u] == ::std::byte{} &&
          out.known_bits[2u] == ::std::byte{0xffu} && out.known_bits[3u] == ::std::byte{0xffu} && out.bytes[2u] == ::std::byte{0x78u},
          "unavailable copied carrier leaves only its fragment unknown without compacting later local indices");
    check(query_numeric_variables(scopes, types, {&variable, 1u}, 15u, locals, 2u, values) == inline_query_error::none &&
          values[0u].reason == numeric_unavailable_reason::incomplete_composite && values[0u].kind == numeric_kind::unavailable,
          "composite scalar never turns unavailable carrier bits into a false zero");
    locals[0u].available = true;
    plan = decode(::std::array<unsigned char, 14u>{0xedu, 0u, 0u, 0x9fu, 0x9du, 4u, 4u, 0xedu, 0u, 1u, 0x9fu, 0x9du, 4u, 8u});
    locals = {local(0xa0u), local(0xb00u)};
    check(materialize_location_pieces(plan, locals, 2u, {}, 1u, out) == piece_query_error::none && out.fully_available &&
          out.bytes[0u] == ::std::byte{0xbau}, "bit_piece source offsets produce exact packed value");
    plan = decode(::std::array<unsigned char, 7u>{0x93u, 1u, 0x10u, 42u, 0x9fu, 0x93u, 1u});
    check(materialize_location_pieces(plan, locals, 2u, {}, 2u, out) == piece_query_error::none && !out.fully_available &&
          out.known_bits[0u] == ::std::byte{} && out.known_bits[1u] == ::std::byte{0xffu} && out.bytes[1u] == ::std::byte{42u}, "empty piece leaves explicit unknown bits, never a false zero value");
    plan = decode(::std::array<unsigned char, 5u>{0x91u, 0x7cu, 0x9du, 8u, 4u});
    copied_location_piece copy{}; copy.piece_index = 0u; copy.bytes = {::std::byte{0xa0u}, ::std::byte{0x0bu}};
    check(materialize_location_pieces(plan, locals, 2u, {&copy, 1u}, 1u, out) == piece_query_error::none && out.fully_available &&
          out.bytes[0u] == ::std::byte{0xbau}, "fbreg piece consumes only bounded owned copy, no memory callback");
    copy.first_source_byte = 1u;
    check(materialize_location_pieces(plan, locals, 2u, {&copy, 1u}, 1u, out) == piece_query_error::malformed && out.bytes.empty(), "misassociated memory slice is rejected without partial result");
    check(materialize_location_pieces(plan, locals, 2u, {}, 1u, out) == piece_query_error::none && !out.fully_available &&
          out.piece_reasons[0u] == piece_unavailable_reason::memory_not_copied, "missing coherent memory copy is unavailable");
    plan = decode(::std::array<unsigned char, 5u>{0xedu, 0u, 0u, 0x93u, 4u});
    check(plan.kind == plan_kind::composite_value && materialize_location_pieces(plan, locals, 2u, {}, 4u, out) == piece_query_error::none &&
          !out.fully_available, "ambiguous plain Wasm local never gains indirect read authority");
    auto const truncated{decode(::std::array<unsigned char, 4u>{0x10u, 1u, 0x9fu, 0x93u})};
    check(truncated.reason == unavailable_reason::malformed_expression, "known descriptor truncation is malformed");
    limits cap{}; cap.max_location_pieces = 1u;
    auto const limited{decode(::std::array<unsigned char, 4u>{0x93u, 1u, 0x93u, 1u}, cap)};
    check(limited.kind == plan_kind::unavailable && limited.reason == unavailable_reason::expression_limit, "piece budget cannot publish a partial composite");
    auto nested{plan}; plan.pieces[0u].atom = ::std::move(nested);
    check(materialize_location_pieces(plan, locals, 2u, {}, 4u, out) == piece_query_error::malformed && out.bytes.empty(), "nested composite atom is rejected without recursion");
    ::fast_io::io::println("PASS finite DW_OP_piece/bit_piece grammar, owned copies, known-bit mask and complete numeric composites");
}
