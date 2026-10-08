// Finite pure metadata / OWNED copied guest-byte tests only. No runtime stop,
// host address, execution, producer fixture or memory-read authority is minted.
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#if defined(UWVM_DWARF_EXPRESSION_TEST_MODULE)
import fast_io;
import uwvm2.uwvm.debugger;
#else
# include <uwvm2/uwvm/debugger/source_dwarf_expression.h>
#endif
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, char const* message)
{
    if(!value)
    { ::fast_io::io::perrln("debug_source_dwarf_expression: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static source_expression parse(::std::string_view text)
{
    source_expression out{};
    check(parse_source_expression(text, out) == object_selector_error::none, "accepted finite expression");
    return out;
}
static bool step(source_expression const& expression, ::std::size_t at, source_expression_step_kind kind,
    ::std::string_view name = {}, ::std::int64_t index = {})
{
    if(at >= expression.steps.size()) { return false; }
    auto const& value{expression.steps[at]}; // [safe] checked scalar index before owned step borrow.
    return value.kind == kind && value.member == name && value.index == index;
}
static void deny(::std::string_view text, source_expression_limits const& cap = {})
{
    source_expression out{::fast_io::concat_std("stale"), {{source_expression_step_kind::dereference, {}, {}}}};
    check(parse_source_expression(text, out, cap) != object_selector_error::none && out.root_name.empty() && out.steps.empty(),
        "invalid input clears both root and partial operation sequence");
}
static type_record scalar(::std::uint8_t bytes, ::std::uint64_t encoding = 7u)
{
    type_record out{}; out.kind = type_kind::scalar; out.encoding = encoding;
    out.byte_count = bytes; out.byte_size = bytes; out.size_known = true; return out;
}
static type_record pointer(::std::size_t pointee, ::std::uint8_t bytes)
{
    type_record out{}; out.kind = type_kind::pointer; out.referenced_type = pointee;
    out.byte_count = bytes; out.byte_size = bytes; out.size_known = true; return out;
}
static member_record member(::std::string_view name, ::std::size_t type, ::std::uint64_t offset)
{
    member_record out{}; out.name = ::fast_io::concat_std(name); out.type = type;
    out.byte_offset = offset; out.offset_known = true; return out;
}
static type_record aggregate(::std::uint64_t bytes, ::std::vector<member_record> members = {})
{
    type_record out{}; out.kind = type_kind::structure; out.byte_size = bytes; out.size_known = true;
    out.members = ::std::move(members); return out;
}
template<unsigned Bits> static ::std::array<::std::byte, Bits / 8u> encoded(::std::uint64_t value)
{
    static_assert(Bits == 32u || Bits == 64u);
    ::std::array<::std::byte, Bits / 8u> out{};
    // [owned fixed Bits/8 bytes] end
    // [safe                   ] unsafe (one-past)
    //  ^^ exact nonzero array extent established BEFORE char borrow/end derivation.
    auto const first{reinterpret_cast<char*>(out.data())};
    ::fast_io::basic_obuffer_view<char> sink{first, first + out.size()};
    ::fast_io::io::print(sink, ::fast_io::mnp::le_put<Bits>(value)); return out;
}
template<::std::size_t Size> static void put(::std::array<::std::byte, Size>& out, ::std::size_t offset,
    ::std::span<::std::byte const> bytes)
{
    check(offset <= out.size() && bytes.size() <= out.size() - offset, "fixture destination extent");
    for(::std::size_t i{}; i < bytes.size(); ++i) // [safe] scalar advance bounded by supplied owned byte span.
    { out[offset + i] = bytes[i]; } // [safe] full sum/destination and byte indices proved above before both reads/writes.
}
static ::std::vector<object_selector_step> selectors(::std::string_view text)
{
    object_selector out{}; check(parse_source_object_selector(text, out) == object_selector_error::none, "finite existing selector");
    return ::std::move(out.steps);
}
static void cleared(copied_guest_pointer_plan const& out)
{ check(out.guest_offset == 0u && out.type == no_record && out.extent == 0u && out.address_bytes == 0u, "failed plan clears every scalar"); }
int main()
{
    auto const first{parse("*p[1]")};
    check(first.root_name == "p" && first.steps.size() == 2u &&
        step(first, 0u, source_expression_step_kind::index, {}, 1) && step(first, 1u, source_expression_step_kind::dereference),
        "postfix binds before unary dereference");
    auto const second{parse("(*p)[1]")};
    check(second.steps.size() == 2u && step(second, 0u, source_expression_step_kind::dereference) &&
        step(second, 1u, source_expression_step_kind::index, {}, 1), "parentheses select the distinct dereference/index order");
    auto const path{parse(" ( outer::value -> a [ -2 ] )-> b.0 ")};
    check(path.root_name == "outer::value" && path.steps.size() == 6u &&
        step(path, 0u, source_expression_step_kind::dereference) && step(path, 1u, source_expression_step_kind::member, "a") &&
        step(path, 2u, source_expression_step_kind::index, {}, -2) && step(path, 3u, source_expression_step_kind::dereference) &&
        step(path, 4u, source_expression_step_kind::member, "b") && step(path, 5u, source_expression_step_kind::member, "0"),
        "whitespace, exact namespace root, arrows, signed indices and Rust numeric member");
    auto const star_arrow{parse("*p->a")};
    check(star_arrow.steps.size() == 3u && step(star_arrow, 0u, source_expression_step_kind::dereference) &&
        step(star_arrow, 1u, source_expression_step_kind::member, "a") && step(star_arrow, 2u, source_expression_step_kind::dereference),
        "unary operand owns complete arrow postfix");
    auto const numeric{parse("tuple.01.1")};
    check(step(numeric, 0u, source_expression_step_kind::member, "01") && step(numeric, 1u, source_expression_step_kind::member, "1"),
        "numeric member exact spelling is not decimal conversion or index");
    auto const low{parse("p[-9223372036854775808]")}, high{parse("p[9223372036854775807]")};
    check(step(low, 0u, source_expression_step_kind::index, {}, (::std::numeric_limits<::std::int64_t>::min)()) &&
        step(high, 0u, source_expression_step_kind::index, {}, (::std::numeric_limits<::std::int64_t>::max)()),
        "FastIO scan handles both signed endpoints without negating MIN");
    for(auto const text : {"", " ", "()", "(p", "p)", "*", "p.", "p->", "p..x", "p[1", "p[]", "p[-]", "p[+1]",
        "p[+-1]", "p[1 2]", "p[1+2]", "p[0x10]", "p[1u]", "p[1.0]", "p[x]", "p()", "f(p)", "p=q", "p+=1",
        "p+1", "&p", "(int*)p", "p as T", "reinterpret_cast<T>(p)", "0x10", "123", "$0", "numericREF(1)",
        "p[1]junk", "a:::b", "a::", "a.b::c", "0::p", "::a", "a :: b", "p.0x", "p.-1", "p.+1", "p.[0]",
        "p[9223372036854775808]", "p[-9223372036854775809]", "p.a[1]->"})
    { deny(text); }
    ::std::array<char, 3u> nul{'p', '\0', 'x'};
    deny({nul.data(), nul.size()}); // [safe] fixed three initialized chars define complete bounded input, embedded NUL is not end.
    ::std::array<char, 2u> control{'p', '\x01'}; deny({control.data(), control.size()});
    auto stars{::fast_io::concat_std(::std::string(32u, '*'), "p")};
    check(parse(stars).steps.size() == 32u, "32 unary operations and recursion are accepted");
    deny(::fast_io::concat_std("*", stars));
    auto parens{::fast_io::concat_std(::std::string(32u, '('), "p", ::std::string(32u, ')'))};
    check(parse(parens).steps.empty(), "32 parenthesis nests are accepted without fake operations");
    deny(::fast_io::concat_std("(", parens, ")"));
    auto mixed{::fast_io::concat_std(::std::string(16u, '('), ::std::string(16u, '*'), "p", ::std::string(16u, ')'))};
    check(parse(mixed).steps.size() == 16u, "mixed unary and parenthesis recursion shares one bounded depth");
    deny(::fast_io::concat_std("*", mixed));
    ::std::string members{"p"}, arrows{"p"};
    for(unsigned i{}; i != 32u; ++i) // [safe] fixture-only scalar counter has finite 32 bound.
    { members = ::fast_io::concat_std(::std::string_view{members}, ".x"); }
    for(unsigned i{}; i != 16u; ++i) // [safe] each arrow costs exactly two bounded operations.
    { arrows = ::fast_io::concat_std(::std::string_view{arrows}, "->x"); }
    check(parse(members).steps.size() == 32u && parse(arrows).steps.size() == 32u, "32 member steps and 16 arrows accepted");
    deny(::fast_io::concat_std(::std::string_view{members}, ".x")); deny(::fast_io::concat_std(::std::string_view{arrows}, "->x"));
    auto bytes4096{::fast_io::concat_std(::std::string(4095u, ' '), "p")};
    check(parse(bytes4096).root_name == "p", "4096 total bytes including whitespace accepted");
    deny(::fast_io::concat_std(::std::string_view{bytes4096}, " "));
    source_expression_limits lowered{}; lowered.max_steps = 1u; deny("p->x", lowered);
    lowered = {}; lowered.max_nesting = 0u; deny("(p)", lowered); deny("*p", lowered);
    lowered = {}; lowered.max_expression_bytes = 1u; deny(" p", lowered);
    source_expression_limits raised{99999u, 99999u, 99999u};
    deny(::fast_io::concat_std("*", stars), raised); deny(::fast_io::concat_std("(", parens, ")"), raised);
    deny(::fast_io::concat_std(::std::string_view{members}, ".x"), raised);
    deny(::fast_io::concat_std(::std::string_view{bytes4096}, " "), raised);

    ::std::vector<type_record> types{scalar(4u), pointer(0u, 4u), pointer(0u, 8u)};
    auto const memory32{encoded<32u>(0xfffffffcu)}; auto const memory64{encoded<64u>(0x100000044ULL)};
    copied_guest_pointer_plan out{};
    check(plan_copied_guest_pointer(types, 1u, {}, memory32, out) == inline_query_error::none && out.guest_offset == 0xfffffffcu &&
        out.type == 0u && out.extent == 4u && out.address_bytes == 4u, "copied wasm32 pointer ending at 4GiB is not a host pointer");
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::none && out.guest_offset == 0x100000044ULL &&
        out.type == 0u && out.extent == 4u && out.address_bytes == 8u, "copied wasm64 pointer retains offset above 4GiB on every host");
    check(plan_copied_guest_pointer(types, 0u, {}, memory32, out) == inline_query_error::unavailable,
        "identical numeric scalar bits cannot forge a pointer plan or reference"); cleared(out);
    auto const maximum64{encoded<64u>((::std::numeric_limits<::std::uint64_t>::max)())};
    check(plan_copied_guest_pointer(types, 2u, {}, maximum64, out) == inline_query_error::none &&
        out.guest_offset == (::std::numeric_limits<::std::uint64_t>::max)(), "planner makes no overflowing offset addition; runtime must bounds-check actual memory");
    auto const zero{encoded<64u>(0u)};
    check(plan_copied_guest_pointer(types, 2u, {}, zero, out) == inline_query_error::unavailable, "null pointer explicitly unavailable"); cleared(out);
    // [owned memory64 eight bytes] end
    // [safe                    ] unsafe (one-past)
    //  ^^ first(7) count checked against its static extent before creating a truncated borrow.
    check(plan_copied_guest_pointer(types, 2u, {}, ::std::span<::std::byte const>{memory64}.first(7u), out) == inline_query_error::malformed,
        "truncated pointer value never becomes a plan"); cleared(out);
    types.push_back(aggregate(16u, {member("ptr", 2u, 4u)}));
    ::std::array<::std::byte, 16u> copied_struct{}; put(copied_struct, 4u, memory64);
    auto const member_path{selectors("value.ptr")};
    check(plan_copied_guest_pointer(types, 3u, member_path, copied_struct, out) == inline_query_error::none && out.guest_offset == 0x100000044ULL,
        "member pointer comes from selected bytes of entire owned aggregate");
    ::std::array<::std::byte, 16u> struct_known{}; struct_known.fill(::std::byte{0xffu});
    check(plan_copied_guest_pointer_with_known_bits(types, 3u, member_path, copied_struct, struct_known, out) == inline_query_error::none,
        "explicit complete known-bit mask selects copied member pointer");
    struct_known[11u] = ::std::byte{0xfeu};
    check(plan_copied_guest_pointer_with_known_bits(types, 3u, member_path, copied_struct, struct_known, out) == inline_query_error::unavailable,
        "one unknown pointer bit cannot be silently zero-filled"); cleared(out);
    struct_known[11u] = ::std::byte{0xffu}; struct_known[15u] = ::std::byte{};
    check(plan_copied_guest_pointer_with_known_bits(types, 3u, member_path, copied_struct, struct_known, out) == inline_query_error::none,
        "unknown unrelated padding byte does not hide known pointer");
    check(plan_copied_guest_pointer_with_known_bits(types, 3u, member_path, copied_struct, {}, out) == inline_query_error::malformed,
        "explicit known-bit API refuses empty mask"); cleared(out);
    // [owned known mask sixteen bytes] end
    // [safe                         ] unsafe (one-past)
    //  ^^ fixed extent proves first(15) before truncated mask borrow.
    check(plan_copied_guest_pointer_with_known_bits(types, 3u, member_path, copied_struct,
        ::std::span<::std::byte const>{struct_known}.first(15u), out) == inline_query_error::malformed,
        "known mask exact length is required before any selection"); cleared(out);
    check(plan_copied_guest_pointer(types, 3u, {}, copied_struct, out) == inline_query_error::unavailable, "aggregate cannot forge exactly-one pointer"); cleared(out);
    type_record array{}; array.kind = type_kind::array; array.size_known = true; array.byte_size = 16u; array.referenced_type = 2u;
    array.dimensions = {{-1, 2u, true, true}}; types.push_back(array);
    ::std::array<::std::byte, 16u> copied_array{}; put(copied_array, 0u, zero); put(copied_array, 8u, memory64);
    auto const element_path{selectors("value[0]")};
    check(plan_copied_guest_pointer(types, 4u, element_path, copied_array, out) == inline_query_error::none && out.guest_offset == 0x100000044ULL,
        "real signed array lower bound selects the second copied pointer");
    auto const invalid_element{selectors("value[1]")};
    check(plan_copied_guest_pointer(types, 4u, invalid_element, copied_array, out) == inline_query_error::unavailable, "outside array domain unavailable"); cleared(out);
    types.push_back(scalar(1u)); types.push_back(aggregate(16u));
    variant_part_record part{}; part.has_discriminant = true; part.discriminant_supported = true; part.discriminant_bytes = 1u;
    part.discriminant = member("tag", 5u, 0u);
    for(unsigned tag{1u}; tag != 3u; ++tag) // [safe] finite copied-tag fixture counter advances 1,2 only.
    {
        variant_record alternative{}; alternative.selector_kind = variant_selector_kind::intervals;
        alternative.selectors.push_back({tag, tag}); alternative.members.push_back(member("ptr", 2u, tag == 1u ? 4u : 8u));
        part.variants.push_back(::std::move(alternative));
    }
    types[6u].variant_parts.push_back(part); // [safe] seven-element owned metadata vector already established.
    ::std::array<::std::byte, 16u> copied_variant{}; copied_variant[0u] = ::std::byte{2u}; put(copied_variant, 8u, memory64);
    check(plan_copied_guest_pointer(types, 6u, member_path, copied_variant, out) == inline_query_error::none && out.guest_offset == 0x100000044ULL,
        "active variant discriminant selects exactly its pointer storage");
    ::std::array<::std::byte, 16u> variant_known{}; variant_known.fill(::std::byte{0xffu});
    variant_known[0u] = ::std::byte{0xfeu};
    check(plan_copied_guest_pointer_with_known_bits(types, 6u, member_path, copied_variant, variant_known, out) == inline_query_error::unavailable,
        "unknown discriminator bit never picks an arbitrary variant pointer"); cleared(out);
    variant_known[0u] = ::std::byte{0xffu}; variant_known[1u] = ::std::byte{};
    check(plan_copied_guest_pointer_with_known_bits(types, 6u, member_path, copied_variant, variant_known, out) == inline_query_error::none &&
        out.guest_offset == 0x100000044ULL, "known tag/pointer with unrelated unknown bytes remains precise");
    copied_variant[0u] = ::std::byte{3u};
    check(plan_copied_guest_pointer(types, 6u, member_path, copied_variant, out) == inline_query_error::unavailable, "no active variant unavailable"); cleared(out);
    types[3u].members.push_back(member("ptr", 2u, 4u));
    check(plan_copied_guest_pointer(types, 3u, member_path, copied_struct, out) == inline_query_error::ambiguous, "duplicate member never picks an arbitrary pointer"); cleared(out);
    types[3u].members.pop_back(); types[3u].members[0u].offset_known = false;
    check(plan_copied_guest_pointer(types, 3u, member_path, copied_struct, out) == inline_query_error::unavailable, "unknown member offset not guessed"); cleared(out);
    types[3u].members[0u].offset_known = true;
    auto const good_pointer{types[2u]}, good_pointee{types[0u]};
    types[2u].address_class_known = true; types[2u].address_class = 0u;
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::none,
        "explicit conventional class zero is only a pure guest-offset candidate");
    types[2u].address_class = 1u;
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::unavailable,
        "nondefault address class is not guessed as guest linear memory"); cleared(out);
    types[2u] = good_pointer;
    types[2u].reference_type = true;
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::unavailable, "reference metadata is not a pointer plan"); cleared(out);
    types[2u] = good_pointer; types[2u].rvalue_reference_type = true;
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::unavailable, "rvalue reference cannot forge pointer semantics"); cleared(out);
    types[2u] = good_pointer; types[2u].referenced_type = no_record;
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::unavailable, "void or unknown pointee unavailable"); cleared(out);
    types[2u] = good_pointer; types[2u].referenced_type = types.size();
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::malformed, "pointee index checked before metadata borrow"); cleared(out);
    types[2u] = good_pointer; types[2u].size_known = false;
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::unavailable, "fallback display width does not authorize pointer plan"); cleared(out);
    types[2u] = good_pointer; types[2u].byte_count = 4u;
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::malformed, "inconsistent pointer widths are malformed"); cleared(out);
    types[2u] = pointer(0u, 2u);
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::unavailable, "two-byte pointer unsupported"); cleared(out);
    types[2u] = pointer(0u, 16u);
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) != inline_query_error::none, "wide pointer never becomes a plan"); cleared(out);
    types[2u] = good_pointer; types[0u].kind = type_kind::unavailable;
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::unavailable, "function/opaque unavailable pointee rejected even with size"); cleared(out);
    types[0u] = good_pointee; types[0u].size_known = false;
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::unavailable, "unknown pointee extent not inferred from scalar fallback"); cleared(out);
    types[0u] = aggregate(0u);
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::unavailable, "zero extent has no dereference plan"); cleared(out);
    types[0u] = aggregate(65536u);
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::none && out.extent == 65536u, "exact 64KiB pointee extent accepted without copying it");
    types[0u] = aggregate(65537u);
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out) == inline_query_error::limit_exceeded, "pointee extent above 64KiB denied before allocation"); cleared(out);
    object_selector_limits wide{}; wide.object_limits.max_object_bytes = 999999u;
    check(plan_copied_guest_pointer(types, 2u, {}, memory64, out, wide) == inline_query_error::limit_exceeded, "caller cannot raise hard pointee cap"); cleared(out);
    types[0u] = good_pointee;
    ::std::vector<::std::byte> excessive(65537u);
    check(plan_copied_guest_pointer(types, 2u, {}, excessive, out, wide) == inline_query_error::limit_exceeded, "copied input hard cap is enforced before graph work"); cleared(out);
    ::std::vector<object_selector_step> too_many(33u, {object_selector_kind::index, {}, 0});
    check(plan_copied_guest_pointer(types, 2u, too_many, memory64, out) == inline_query_error::limit_exceeded, "selector segment hard cap"); cleared(out);
    ::std::vector<object_selector_step> invalid{{static_cast<object_selector_kind>(999u), {}, 0}};
    check(plan_copied_guest_pointer(types, 2u, invalid, memory64, out) == inline_query_error::malformed, "invalid operation enum rejected by selected-object query"); cleared(out);
    check(plan_copied_guest_pointer(types, types.size(), {}, memory64, out) == inline_query_error::malformed, "root type index checked before borrow"); cleared(out);
    ::fast_io::io::println("PASS finite source-expression precedence/bounds and copied guest-pointer planning; no runtime qualification");
}
