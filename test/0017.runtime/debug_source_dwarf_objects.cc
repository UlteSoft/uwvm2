// Finite layout/value semantics; these owned fixtures are NOT runtime read
// authority. Real producer DWARF and actual stopped-frame witnesses are separate.
#include <fast_io.h>
// Enter the umbrella first: language headers must still supply their optional
// standard-string hooks after their own standard-string includes.
#include <uwvm2/uwvm/debugger/source_dwarf_objects.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_dwarf_objects: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static type_record scalar(::std::string name, ::std::uint64_t encoding, ::std::uint8_t width)
{ type_record out{}; out.name = ::std::move(name); out.kind = type_kind::scalar; out.encoding = encoding; out.byte_count = width;
  out.byte_size = width; out.size_known = true; return out; }
static type_record aggregate(::std::string name, type_kind kind, ::std::uint64_t size)
{ type_record out{}; out.name = ::std::move(name); out.kind = kind; out.byte_size = size; out.size_known = true; return out; }
static member_record member(::std::string name, ::std::size_t type, ::std::uint64_t offset)
{ member_record out{}; out.name = ::std::move(name); out.type = type; out.byte_offset = offset; out.offset_known = true; return out; }
static void little(::std::vector<::std::byte>& out, ::std::uint64_t value, ::std::size_t count)
{
    check(count <= sizeof(value), "fixture little-endian emission width is bounded");
    if(count == 0u) { return; }
    auto const old_size{out.size()};
    check(old_size <= out.max_size() && count <= out.max_size() - old_size, "fixture byte vector growth does not overflow");
    auto const encoded{::fast_io::little_endian(value)};
    out.resize(old_size + count);
    // [owned scalar encoded: exactly sizeof(value) bytes] end
    // [safe                                             ] count <= sizeof(value).
    //  ^^ addressof(encoded) is a fixed owned scalar borrow; no endian shifts.
    // [owned resized vector ... old_size ... old_size+count] end
    // [safe                                               ] growth proved before resize.
    //                            ^^ out.data()+old_size is formed only after
    //                               positive count and successful resize.
    ::std::memcpy(out.data() + old_size, ::std::addressof(encoded), count);
}
static copied_numeric_local local32(::std::uint32_t value)
{ copied_numeric_local out{}; out.wasm_type = 0x7fu; out.available = true; ::std::memcpy(out.bytes.data(), ::std::addressof(value), sizeof(value)); return out; }
template<typename Char> static void escaped_formatter()
{
    ::std::basic_string<Char> text{}; ::fast_io::basic_ostring_ref_std<Char> stream{::std::addressof(text)};
    ::std::string const unsafe{"A\x1b\n\xc2\x9b\"\\"};
    ::fast_io::io::print(stream, escaped_metadata_text{unsafe});
    ::std::basic_string<Char> expected{};
    ::fast_io::basic_ostring_ref_std<Char> expected_stream{::std::addressof(expected)};
    ::fast_io::io::print(expected_stream, ::fast_io::mnp::code_cvt(::std::string_view{"A\\x1b\\x0a\\xc2\\x9b\\\"\\\\"}));
    check(text == expected, "fast_io formatter escapes terminal controls/invalid UTF-8 in all five character domains");
    object_node node{}; node.name = ::fast_io::concat_std(::std::string_view{unsafe}); node.type_name = ::fast_io::concat_std(::std::string_view{unsafe});
    ::fast_io::io::print(stream, object_details_of(node));
    check(text.find(static_cast<Char>(0x1bu)) == ::std::basic_string<Char>::npos &&
          text.find(static_cast<Char>(0x9bu)) == ::std::basic_string<Char>::npos, "owned object formatter cannot emit a terminal escape");
}
int main()
{
    escaped_formatter<char>(); escaped_formatter<wchar_t>(); escaped_formatter<char8_t>(); escaped_formatter<char16_t>(); escaped_formatter<char32_t>();
    ::std::vector<::std::byte> endian_fixture{};
    little(endian_fixture, 0x0807060504030201u, 8u);
    check(endian_fixture == ::std::vector<::std::byte>{::std::byte{1u}, ::std::byte{2u}, ::std::byte{3u}, ::std::byte{4u},
          ::std::byte{5u}, ::std::byte{6u}, ::std::byte{7u}, ::std::byte{8u}}, "fixture fast_io emission is portable Wasm little endian");
    auto const unchanged{endian_fixture}; little(endian_fixture, 0u, 0u);
    check(endian_fixture == unchanged, "zero-width fixture emission never derives an empty vector pointer");
    ::std::vector<type_record> types{scalar("int", 0x05u, 4u), scalar("unsigned char", 0x08u, 1u), scalar("float", 0x04u, 4u)};
    auto pointer{aggregate("Node*", type_kind::pointer, 4u)}; pointer.byte_count = 4u; pointer.referenced_type = 4u; types.push_back(pointer);
    auto node{aggregate("Node", type_kind::structure, 8u)}; node.members = {member("value", 0u, 0u), member("next", 3u, 4u)}; types.push_back(node);
    ::std::vector<::std::byte> bytes{}; little(bytes, 0xfffffff9u, 4u); little(bytes, 0xffffffffu, 4u);
    ::std::vector<object_node> out{};
    check(query_object_value(types, 4u, bytes, out) == inline_query_error::none && out.size() == 3u, "bounded self-referential pointer terminates as a leaf");
    check(out[1u].bits == 0xfffffff9u && out[1u].scalar_kind == numeric_kind::signed_integer && out[1u].value_available, "struct signed scalar exact bits");
    check(out[2u].bits == 0xffffffffu && out[2u].kind == type_kind::pointer && out[2u].value_available, "guest pointer is printed, never followed");
    check(query_type_layout(types, 4u, out) == inline_query_error::none && !out[1u].value_available && out[1u].byte_offset == 0u, "ptype has no memory-read capability");
    bytes.resize(7u);
    check(query_object_value(types, 4u, bytes, out) == inline_query_error::none && out.size() == 1u && out[0u].reason == object_unavailable_reason::object_bounds, "truncated copied object read denied before pointer formation");
    auto matrix{aggregate("int[2][3]", type_kind::array, 24u)}; matrix.referenced_type = 0u;
    matrix.dimensions = {{0, 2u, true, true}, {0, 3u, true, true}}; types.push_back(matrix); bytes.clear();
    for(::std::uint64_t value{10u}; value != 16u; ++value) { little(bytes, value, 4u); }
    check(query_object_value(types, 5u, bytes, out) == inline_query_error::none && out.size() == 9u, "multidimensional row-major array shape");
    check(out[1u].name == "[0]" && out[5u].name == "[1]" && out[4u].bits == 12u && out[8u].bits == 15u && out[8u].byte_offset == 20u, "array dimensions and indices use exact element stride");
    object_query_limits cap{}; cap.max_array_elements = 2u;
    check(query_object_value(types, 5u, bytes, out, cap) == inline_query_error::none && out[1u].omitted_children == 1u, "GDB-style bounded array truncation is explicit");
    types[5u].contiguous_array = false;
    check(query_object_value(types, 5u, bytes, out) == inline_query_error::none && out[0u].reason == object_unavailable_reason::unsupported_array_stride, "dynamic stride not guessed");
    types[5u].contiguous_array = true; types[5u].dimensions[0u].count_known = false;
    check(query_object_value(types, 5u, bytes, out) == inline_query_error::none && out[0u].reason == object_unavailable_reason::dynamic_array, "unknown count never invents array elements");
    auto enumeration{aggregate("Color", type_kind::enumeration, 4u)}; enumeration.byte_count = 4u; enumeration.encoding = 0x05u;
    enumeration.enumerators = {{"red", 2u, false}, {"negative", 0xffffffffffffffffu, true}}; types.push_back(enumeration);
    bytes.clear(); little(bytes, 0xffffffffu, 4u);
    check(query_object_value(types, 6u, bytes, out) == inline_query_error::none && out[0u].enumerator == "negative" && out[0u].bits == 0xffffffffu, "signed enumerator name matches declared-width bits");
    auto bits{aggregate("Bits", type_kind::structure, 2u)};
    auto signed_bits{member("signed_bits", 0u, 0u)}; signed_bits.bit_field = true; signed_bits.bit_size = 5u; signed_bits.data_bit_offset = 3u;
    auto crossing{member("crossing", 0u, 0u)}; crossing.bit_field = true; crossing.bit_size = 7u; crossing.data_bit_offset = 8u;
    bits.members = {signed_bits, crossing}; types.push_back(bits); bytes = {::std::byte{0xf8u}, ::std::byte{0x5au}};
    check(query_object_value(types, 7u, bytes, out) == inline_query_error::none && out[1u].bits == 0xffffffffu && out[2u].bits == 0xffffffdau, "DWARF5 data_bit_offset exact signed extraction");
    auto union_type{aggregate("U", type_kind::union_type, 4u)}; union_type.members = {member("integer", 0u, 0u), member("real", 2u, 0u)}; types.push_back(union_type);
    bytes.clear(); little(bytes, 0x3f800000u, 4u);
    check(query_object_value(types, 8u, bytes, out) == inline_query_error::none && out[1u].bits == out[2u].bits && out[2u].scalar_kind == numeric_kind::f32_bits, "union alternatives intentionally share copied bytes");
    for(auto const& value : out) { ::fast_io::io::println(object_details_of(value)); }
    types[8u].members[1u].offset_known = false;
    check(query_object_value(types, 8u, bytes, out) == inline_query_error::none && out[2u].reason == object_unavailable_reason::unknown_member_offset, "virtual/dynamic member remains unavailable");
    types[8u].members[0u].type = types.size();
    check(query_type_layout(types, 8u, out) == inline_query_error::malformed && out.empty(), "invalid graph reference rejects without partial publication");
    types[8u].members[0u].type = 0u;
    cap = {}; cap.max_results = 1u;
    check(query_type_layout(types, 4u, out, cap) == inline_query_error::limit_exceeded && out.empty(), "result budget clears partial tree");
    types[4u].members[0u].type = 4u; cap = {}; cap.max_depth = 3u;
    check(query_type_layout(types, 4u, out, cap) == inline_query_error::limit_exceeded && out.empty(), "illegal by-value cycle cannot recurse unboundedly");
    location_plan fb{}; fb.kind = plan_kind::frame_relative_offset; fb.address_bytes = 4u; fb.displacement = -16;
    location_plan base{}; base.kind = plan_kind::wasm_local_frame_base; base.address_bytes = 4u;
    ::std::array<location_record, 1u> bases{location_record{{}, base}}; ::std::array<copied_numeric_local, 1u> locals{local32(32u)};
    ::std::uint64_t offset{};
    check(resolve_frame_relative_offset(fb, bases, 15u, locals, 1u, offset) == object_location_error::none && offset == 16u, "fbreg computes guest offset from complete captured carrier");
    locals[0u].available = false;
    check(resolve_frame_relative_offset(fb, bases, 15u, locals, 1u, offset) == object_location_error::local_unavailable && offset == 0u, "unavailable frame-base slot cannot become a guest address even with numeric type/bytes");
    locals[0u].available = true;
    fb.displacement = -33;
    check(resolve_frame_relative_offset(fb, bases, 15u, locals, 1u, offset) == object_location_error::address_overflow && offset == 0u, "negative underflow fails closed");
    fb.displacement = 1; locals[0u] = local32(0xffffffffu);
    check(resolve_frame_relative_offset(fb, bases, 15u, locals, 1u, offset) == object_location_error::address_overflow, "Wasm32 wrap is forbidden for debugger object reads");
    locals[0u].wasm_type = 0x7du;
    check(resolve_frame_relative_offset(fb, bases, 15u, locals, 1u, offset) == object_location_error::carrier_mismatch, "float/ref snapshots are not frame-base addresses");
    fb.address_bytes = 8u; fb.displacement = (::std::numeric_limits<::std::int64_t>::min)();
    bases[0u].plan.address_bytes = 8u; locals[0u].wasm_type = 0x7eu;
    auto const high{::std::uint64_t{0x8000000000000000u}}; ::std::memcpy(locals[0u].bytes.data(), ::std::addressof(high), sizeof(high));
    check(resolve_frame_relative_offset(fb, bases, 15u, locals, 1u, offset) == object_location_error::none && offset == 0u, "Wasm64 INT64_MIN displacement has no signed negation overflow");
    ::std::vector<scope_record> scopes(3u);
    scopes[0u].kind = scope_kind::compile_unit;
    scopes[1u].kind = scope_kind::subprogram; scopes[1u].parent = 0u; scopes[1u].concrete = true; scopes[1u].ranges = {{10u, 30u}};
    scopes[2u].kind = scope_kind::lexical_block; scopes[2u].parent = 1u; scopes[2u].ranges = {{12u, 22u}};
    variable_record outer{}; outer.identity = {0u, 1u}; outer.scope = 1u; outer.type = 0u; outer.name = "shadow";
    variable_record inner{outer}; inner.identity = {0u, 2u}; inner.scope = 2u;
    ::std::vector<variable_record> variables{outer, inner}; variable_selection selected{};
    check(query_named_variable(scopes, types, variables, 15u, "shadow", selected) == inline_query_error::none &&
          selected.identity == inner.identity && selected.physical_scope == 1u, "innermost active lexical variable shadows outer declaration");
    check(query_named_variable(scopes, types, variables, 25u, "shadow", selected) == inline_query_error::none &&
          selected.identity == outer.identity, "inactive lexical scope cannot shadow outer variable");
    inner.identity = {0u, 3u}; variables.push_back(inner);
    check(query_named_variable(scopes, types, variables, 15u, "shadow", selected) == inline_query_error::ambiguous && selected.type == no_record, "ambiguous same-depth declarations fail closed");
    ::fast_io::io::println("PASS bounded source aggregate layouts, arrays, enum, bit-fields, copied bytes and frame-relative offsets");
}
