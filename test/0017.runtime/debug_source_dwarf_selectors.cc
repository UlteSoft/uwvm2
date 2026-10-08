#include <uwvm2/uwvm/debugger/source_dwarf_selectors.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool ok, char const* message)
{ if(!ok) { ::fast_io::io::perrln("debug_source_dwarf_selectors: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static object_selector parse(::std::string_view text)
{ object_selector result{}; check(parse_source_object_selector(text, result) == object_selector_error::none, "finite selector parse"); return result; }
static type_record scalar(::std::uint8_t width, ::std::uint64_t encoding)
{ type_record result{}; result.kind = type_kind::scalar; result.byte_count = width; result.byte_size = width; result.encoding = encoding; result.size_known = true; return result; }
static member_record member(::std::string_view name, ::std::size_t type, ::std::uint64_t offset, bool inherited = false)
{ member_record result{}; result.name = ::fast_io::concat_std(name); result.type = type; result.byte_offset = offset; result.offset_known = true; result.inherited = inherited; return result; }
static type_record array(::std::size_t element, ::std::uint64_t bytes, ::std::vector<dimension_record> dimensions)
{ type_record result{}; result.kind = type_kind::array; result.byte_size = bytes; result.size_known = true; result.referenced_type = element; result.dimensions = ::std::move(dimensions); return result; }
static type_record aggregate(::std::uint64_t bytes, ::std::vector<member_record> members)
{ type_record result{}; result.kind = type_kind::structure; result.byte_size = bytes; result.size_known = true; result.members = ::std::move(members); return result; }
static inline_query_error query(::std::span<type_record const> types, ::std::size_t root, ::std::string_view expression,
    ::std::span<::std::byte const> bytes, ::std::vector<object_node>& out, object_selector_limits const& cap = {})
{ auto const selector{parse(expression)}; return query_selected_object_value(types, root, selector.steps, bytes, out, cap); }
int main()
{
    auto const syntax{parse("outer::value.matrix[-2].field")};
    check(syntax.root_name == "outer::value" && syntax.steps.size() == 3u && syntax.steps[1u].index == -2, "qualified root, member and signed decimal index");
    auto const minimum{parse("value[-9223372036854775808]")};
    check(minimum.steps.front().index == (::std::numeric_limits<::std::int64_t>::min)(), "fast_io decimal parser preserves INT64_MIN");
    for(auto const expression : {"x()", "*x", "x->field", "x=1", "(int)x", "x+1", "x[1]+1", "x[1", "x[]", "x.", "x::", "x[9223372036854775808]", "x[-9223372036854775809]"})
    {
        object_selector denied{}; denied.root_name = ::fast_io::concat_std("stale");
        check(parse_source_object_selector(expression, denied) != object_selector_error::none && denied.root_name.empty() && denied.steps.empty(), "calls/assignments/dereferences/arithmetic/overflow and truncated grammar deny without partial output");
    }
    object_selector_limits tiny{}; tiny.max_steps = 1u; object_selector denied{};
    check(parse_source_object_selector("x.a.b", denied, tiny) == object_selector_error::limit_exceeded, "finite step budget");
    ::std::vector<type_record> types{}; types.push_back(scalar(4u, 5u));
    types.push_back(array(0u, 24u, {{-1, 2u, true, true}, {4, 3u, true, true}}));
    types.push_back(aggregate(40u, {member("matrix", 1u, 4u), member("tail", 0u, 32u)}));
    ::std::array<::std::byte, 40u> bytes{}; bytes[24u] = ::std::byte{55u}; bytes[32u] = ::std::byte{19u}; ::std::vector<object_node> out{};
    check(query(types, 2u, "x.matrix[0][6]", bytes, out) == inline_query_error::none && out.size() == 1u && out.front().byte_offset == 24u && out.front().bits == 55u,
          "multidimensional real lower bounds produce checked offset");
    check(query(types, 2u, "x.matrix[0]", bytes, out) == inline_query_error::none && out.size() == 4u && out.front().byte_offset == 16u && out.front().byte_size == 12u && out.back().bits == 55u,
          "partial row retains dimension cursor and exact row extent");
    check(query(types, 2u, "x.matrix[0].tail", bytes, out) == inline_query_error::unavailable && out.empty(), "unconsumed row cannot act as containing struct");
    check(query(types, 2u, "x.matrix[-2][4]", bytes, out) == inline_query_error::unavailable && out.empty(), "index below signed lower denied before offset arithmetic");
    check(query(types, 2u, "x.matrix[1][4]", bytes, out) == inline_query_error::unavailable, "index above actual count denied");
    types.push_back(array(0u, 4000u, {{0, 1000u, true, true}})); ::std::vector<::std::byte> large(4000u); large[2000u] = ::std::byte{99u};
    check(query(types, 3u, "x[500]", large, out) == inline_query_error::none && out.size() == 1u && out.front().bits == 99u, "direct selector bypasses display truncation without rendering preceding elements");
    types.push_back(array(0u, 12u, {{(::std::numeric_limits<::std::int64_t>::min)(), 3u, true, true}}));
    ::std::array<::std::byte, 12u> low{}; low[8u] = ::std::byte{33u};
    check(query(types, 4u, "x[-9223372036854775806]", low, out) == inline_query_error::none && out.front().byte_offset == 8u && out.front().bits == 33u, "MIN lower ordinal uses exact unsigned subtraction");
    types.push_back(array(0u, 8u, {{(::std::numeric_limits<::std::int64_t>::max)(), 2u, true, true}}));
    check(query(types, 5u, "x[9223372036854775807]", low, out) == inline_query_error::malformed && out.empty(), "metadata signed index domain cannot wrap");
    types[3u].contiguous_array = false;
    check(query(types, 3u, "x[500]", large, out) == inline_query_error::unavailable, "noncontiguous strides are never guessed"); types[3u].contiguous_array = true;
    types[1u].dimensions[1u].count_known = false;
    check(query(types, 2u, "x.matrix[0]", bytes, out) == inline_query_error::unavailable && out.empty(), "unknown remaining-dimension bound never forms a guessed row"); types[1u].dimensions[1u].count_known = true;
    types[1u].row_major_array = false;
    check(query(types, 2u, "x.matrix[0]", bytes, out) == inline_query_error::unavailable && out.empty(), "column-major array not guessed as row-major"); types[1u].row_major_array = true;
    types.push_back(aggregate(8u, {member("value", 0u, 4u)}));
    types.push_back(aggregate(16u, {member("Base", 6u, 0u, true), member("value", 0u, 12u)}));
    ::std::array<::std::byte, 16u> derived{}; derived[4u] = ::std::byte{33u}; derived[12u] = ::std::byte{42u};
    check(query(types, 7u, "x.value", derived, out) == inline_query_error::none && out.front().bits == 42u, "derived direct member hides every base member");
    types[7u].members.pop_back();
    check(query(types, 7u, "x.value", derived, out) == inline_query_error::none && out.front().byte_offset == 4u && out.front().bits == 33u, "unique constant nonvirtual base member");
    types[7u].members.push_back(member("OtherBase", 6u, 8u, true));
    check(query(types, 7u, "x.value", derived, out) == inline_query_error::ambiguous && out.empty(), "distinct base subobjects remain ambiguous even with equal base type");
    types[7u].members.back().offset_known = false;
    check(query(types, 7u, "x.value", derived, out) == inline_query_error::unavailable && out.empty(), "unknown virtual base prevents false unique sibling lookup");
    types[7u].members.pop_back(); types[6u].members.push_back(member("Cycle", 6u, 0u, true));
    check(query(types, 7u, "x.missing", derived, out) == inline_query_error::limit_exceeded && out.empty(), "inherited cycles have bounded shared traversal"); types[6u].members.pop_back();
    types.push_back(scalar(2u, 5u)); member_record bits{member("bits", 8u, 0u)}; bits.bit_field = true; bits.data_bit_offset = 7u; bits.bit_size = 5u;
    types.push_back(aggregate(4u, {bits})); types.push_back(aggregate(12u, {member("inside", 9u, 4u)}));
    ::std::array<::std::byte, 12u> bit_bytes{}; bit_bytes[4u] = ::std::byte{0x80u}; bit_bytes[5u] = ::std::byte{0x0eu};
    check(query(types, 10u, "x.inside.bits", bit_bytes, out) == inline_query_error::none && out.front().byte_offset == 4u && out.front().bit_field && out.front().bits == 0xfffdu,
          "nested bitfield retains original owner-relative offset and signed bits");
    check(query(types, 10u, "x.inside.bits[0]", bit_bytes, out) == inline_query_error::unavailable && out.empty(), "bitfield leaf cannot become array or pointer");
    types.push_back(scalar(1u, 7u)); types.push_back(aggregate(8u, {}));
    variant_part_record part{}; part.has_discriminant = true; part.discriminant_supported = true; part.discriminant_bytes = 1u; part.discriminant = member("tag", 11u, 0u);
    for(auto const value : {1u, 2u})
    {
        variant_record variant{}; variant.name = ::fast_io::concat_std("Case", ::fast_io::mnp::dec(value)); variant.selector_kind = variant_selector_kind::intervals;
        variant.selectors.push_back({value, value}); variant.members.push_back(member("payload", 0u, 4u)); part.variants.push_back(::std::move(variant));
    }
    types[12u].variant_parts.push_back(part);
    ::std::array<::std::byte, 8u> variant_bytes{}; variant_bytes[0u] = ::std::byte{1u}; variant_bytes[4u] = ::std::byte{77u};
    ::std::array<::std::byte, 8u> known{}; known.fill(::std::byte{0xffu}); auto const path{parse("x.payload")};
    check(query_selected_object_value_with_known_bits(types, 12u, path.steps, variant_bytes, known, out) == inline_query_error::none && out.front().bits == 77u,
          "actual copied discriminant chooses one active payload among equal names");
    known[0u] = ::std::byte{0x7fu};
    check(query_selected_object_value_with_known_bits(types, 12u, path.steps, variant_bytes, known, out) == inline_query_error::unavailable && out.empty(), "one unavailable discriminator bit denies precise variant member selection");
    known[0u] = ::std::byte{0xffu}; known[2u] = ::std::byte{};
    check(query_selected_object_value_with_known_bits(types, 12u, path.steps, variant_bytes, known, out) == inline_query_error::none && out.front().bits == 77u, "unrelated unavailable bit does not block active member precision");
    tiny = {}; tiny.max_lookup_edges = 5u;
    check(query(types, 2u, "x.matrix[0][6]", bytes, out, tiny) == inline_query_error::limit_exceeded && out.empty(), "lookup work budget shared across steps");
    tiny.max_lookup_edges = 6u;
    check(query(types, 2u, "x.matrix[0][6]", bytes, out, tiny) == inline_query_error::none && out.front().bits == 55u, "exact six-edge path budget succeeds without reset");
    types[2u].members[0u].type = no_record - 1u;
    check(query(types, 2u, "x.matrix", bytes, out) == inline_query_error::malformed && out.empty(), "bad member index fails before borrowing any type");
    ::fast_io::io::println("PASS bounded readonly source selectors, array domains, inheritance, bitfields and active variants");
}
