// Finite metadata and copied-bit components; real Rust producer qualification
// is a separate fresh LLVM-DWARF test, never inferred from these hand-built DIEs.
#include <uwvm2/uwvm/debugger/source_dwarf_objects.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool ok, char const* message)
{ if(!ok) { ::fast_io::io::perrln("debug_source_dwarf_variants: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
template<::std::size_t N> static ::std::span<::std::byte const> encoded(::std::array<unsigned char, N> const& source) noexcept
{
    // [owned array of N one-byte elements] end
    // [safe                             ] entire fixed array is live in caller.
    return {reinterpret_cast<::std::byte const*>(source.data()), source.size()};
}
static object_node const* node_named(::std::vector<object_node> const& nodes, ::std::string_view name) noexcept
{ for(auto const& node : nodes) { if(node.name == name) { return ::std::addressof(node); } } return nullptr; }
int main()
{
    ::std::vector<variant_selector_record> selectors{};
    ::std::array<unsigned char, 5u> const signed_list{1u, 0x7du, 0x7fu, 0u, 2u};
    check(decode_variant_selectors(encoded(signed_list), 1u, true, selectors) == variant_query_error::none && selectors.size() == 2u &&
          ::std::bit_cast<::std::int64_t>(selectors[0u].low) == -3 && ::std::bit_cast<::std::int64_t>(selectors[0u].high) == -1 && selectors[1u].low == 2u,
          "signed SLEB label/range follows discriminant type");
    ::std::array<unsigned char, 3u> const reversed{1u, 3u, 1u};
    check(decode_variant_selectors(encoded(reversed), 1u, false, selectors) == variant_query_error::malformed && selectors.empty(), "reversed interval rejected with no partial result");
    ::std::array<unsigned char, 3u> const overflow{0u, 0x80u, 2u};
    check(decode_variant_selectors(encoded(overflow), 1u, false, selectors) == variant_query_error::malformed, "ULEB value cannot truncate discriminator width");
    ::std::array<unsigned char, 2u> const truncated{1u, 2u};
    check(decode_variant_selectors(encoded(truncated), 4u, false, selectors) == variant_query_error::malformed, "truncated high endpoint is bounded");
    check(decode_variant_selectors(encoded(signed_list), 1u, true, selectors, 1u) == variant_query_error::limit_exceeded, "selector count budget");
    ::std::vector<type_record> types(3u);
    types[0u].kind = type_kind::scalar; types[0u].encoding = 5u; types[0u].byte_count = 1u; types[0u].byte_size = 1u; types[0u].size_known = true;
    types[1u].kind = type_kind::scalar; types[1u].encoding = 5u; types[1u].byte_count = 4u; types[1u].byte_size = 4u; types[1u].size_known = true;
    auto& owner{types[2u]}; owner.kind = type_kind::structure; owner.name = ::fast_io::concat_std("PayloadEnum"); owner.byte_size = 12u; owner.size_known = true;
    member_record always{}; always.name = ::fast_io::concat_std("always"); always.type = 1u; always.byte_offset = 8u; always.offset_known = true; owner.members.push_back(always);
    variant_part_record part{}; part.has_discriminant = true; part.discriminant_supported = true; part.discriminant_signed = true; part.discriminant_bytes = 1u;
    part.discriminant.name = ::fast_io::concat_std("tag"); part.discriminant.type = 0u; part.discriminant.offset_known = true;
    variant_record negative{}; negative.name = ::fast_io::concat_std("Negative"); negative.selector_kind = variant_selector_kind::intervals;
    negative.selectors.push_back({static_cast<::std::uint64_t>(-3), static_cast<::std::uint64_t>(-1)});
    member_record payload{}; payload.name = ::fast_io::concat_std("payload"); payload.type = 1u; payload.byte_offset = 4u; payload.offset_known = true; negative.members.push_back(payload);
    variant_record positive{}; positive.name = ::fast_io::concat_std("Positive"); positive.selector_kind = variant_selector_kind::intervals; positive.selectors.push_back({1u, 3u});
    variant_record fallback{}; fallback.name = ::fast_io::concat_std("Default"); fallback.selector_kind = variant_selector_kind::default_case;
    part.variants.push_back(negative); part.variants.push_back(positive); part.variants.push_back(fallback); owner.variant_parts.push_back(part);
    ::std::size_t selected{};
    check(select_variant(part, 0xffu, true, selected) == variant_query_error::none && selected == 0u, "sign extend actual copied discriminant before interval match");
    check(select_variant(part, 0u, true, selected) == variant_query_error::none && selected == 2u, "default only after every explicit selector misses");
    check(select_variant(part, 0u, false, selected) == variant_query_error::unavailable && selected == no_record, "unknown discriminator cannot select default");
    auto overlap{part}; overlap.variants[1u].selectors.push_back({static_cast<::std::uint64_t>(-1), static_cast<::std::uint64_t>(-1)});
    check(select_variant(overlap, 0xffu, true, selected) == variant_query_error::ambiguous, "overlapping active variants are ambiguous");
    auto duplicate_default{part}; duplicate_default.variants.push_back(fallback);
    check(select_variant(duplicate_default, 1u, true, selected) == variant_query_error::ambiguous, "duplicate defaults never silently selected");
    variant_part_record univariant{}; univariant.variants.push_back(fallback);
    check(select_variant(univariant, 0u, false, selected) == variant_query_error::none && selected == 0u, "one untagged default requires no invented discriminant");
    ::std::array<::std::byte, 12u> bytes{}; bytes[0u] = ::std::byte{0xffu}; bytes[4u] = ::std::byte{11u}; bytes[8u] = ::std::byte{19u};
    ::std::array<::std::byte, 12u> known{}; known.fill(::std::byte{0xffu}); ::std::vector<object_node> nodes{};
    check(query_object_value_with_known_bits(types, 2u, bytes, known, nodes) == inline_query_error::none, "bounded copied object accepted");
    check(node_named(nodes, "Negative") && node_named(nodes, "Negative")->active_variant && !node_named(nodes, "Positive") && !node_named(nodes, "Default") &&
          node_named(nodes, "payload") && node_named(nodes, "payload")->value_available && node_named(nodes, "payload")->bits == 11u, "only actual active variant payload appears");
    known[0u] = ::std::byte{0x7fu};
    check(query_object_value_with_known_bits(types, 2u, bytes, known, nodes) == inline_query_error::none && !node_named(nodes, "Negative") &&
          node_named(nodes, "<variant-part>")->reason == object_unavailable_reason::unavailable_discriminant && node_named(nodes, "always")->value_available,
          "missing one discriminator bit blocks selection without erasing unrelated field precision");
    known[0u] = ::std::byte{0xffu}; known[4u] = ::std::byte{};
    check(query_object_value_with_known_bits(types, 2u, bytes, known, nodes) == inline_query_error::none && node_named(nodes, "Negative") &&
          node_named(nodes, "payload")->reason == object_unavailable_reason::incomplete_value && node_named(nodes, "always")->bits == 19u,
          "partial payload does not erase known discriminator and independent field");
    auto& bit_discriminant{owner.variant_parts[0u].discriminant}; bit_discriminant.bit_field = true; bit_discriminant.bit_size = 4u; bit_discriminant.data_bit_offset = 4u;
    known[0u] = ::std::byte{0xf0u};
    check(query_object_value_with_known_bits(types, 2u, bytes, known, nodes) == inline_query_error::none && node_named(nodes, "Negative"), "exact discriminator bitfield mask ignores unrelated unknown low bits");
    check(query_object_value_with_known_bits(types, 2u, bytes, ::std::span<::std::byte const>{known}.first(1u), nodes) == inline_query_error::malformed && nodes.empty(), "mask length must match copied bytes");
    object_query_limits cap{}; cap.max_edges = 1u;
    check(query_type_layout(types, 2u, nodes, cap) == inline_query_error::limit_exceeded && nodes.empty(), "variant members and selectors count against full graph budget");
    ::fast_io::io::println("PASS finite Rust variant discriminant, ranges, default and copied-known-bits component");
}
