// Actual LLVM parses serialized in-memory DWARF4/5 attribute tables. This
// component checks index rejection/constant metadata; it does not qualify a
// C/C++/Rust producer, a live stop, copied guest memory or a product value.
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <uwvm2/uwvm/debugger/source_dwarf_objects.h>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <algorithm>
#include <string>
#include <vector>
namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
using metadata_index = dwarf::index;
static void check(bool value, ::std::string_view reason)
{ if(!value) { ::fast_io::io::perrln("constant_attribute_index: ", reason); ::fast_io::fast_terminate(); } }
static void byte(::std::string& owner, ::std::uint8_t value)
{ ::fast_io::ostring_ref_std output{__builtin_addressof(owner)}; ::fast_io::io::print(output, ::fast_io::mnp::le_put<8>(value)); }
static void uleb(::std::string& owner, ::std::uint64_t value)
{ ::fast_io::ostring_ref_std output{__builtin_addressof(owner)}; ::fast_io::io::print(output, ::fast_io::mnp::leb128_put(value)); }
static void word(::std::string& owner, ::std::uint16_t value)
{ ::fast_io::ostring_ref_std output{__builtin_addressof(owner)}; ::fast_io::io::print(output, ::fast_io::mnp::le_put<16>(value)); }
static void dword(::std::string& owner, ::std::uint32_t value)
{ ::fast_io::ostring_ref_std output{__builtin_addressof(owner)}; ::fast_io::io::print(output, ::fast_io::mnp::le_put<32>(value)); }
static void text(::std::string& owner, ::std::string_view value)
{ ::fast_io::ostring_ref_std output{__builtin_addressof(owner)}; ::fast_io::io::print(output, value, ::fast_io::mnp::chvw('\0')); }
static void patch(::std::string& owner, ::std::size_t offset, ::std::uint32_t value)
{
    check(offset <= owner.size() && 4u <= owner.size() - offset, "owned reference patch bounds");
    ::std::string replacement{}; dword(replacement, value);
    // [owned CU bytes ... offset ... offset+4 ... end]
    // [safe                                          ] checked BEFORE derive;
    //                   ^^ exact four bytes, fast_io provides endian emission.
    ::std::copy(replacement.begin(), replacement.end(), owner.begin() + static_cast<::std::ptrdiff_t>(offset));
}
struct fixture
{
    ::std::string info{}, abbrev{}, locations{}; unsigned version{};
    [[nodiscard]] ::std::vector<dwarf::section> sections() const
    {
        // [complete immutable owned binary strings] end
        // [safe                                  ] synchronous span borrow;
        //  ^^ parse() copies every selected section before this fixture dies.
        ::std::vector<dwarf::section> result{{".debug_info", ::std::as_bytes(::std::span{info.data(), info.size()})},
            {".debug_abbrev", ::std::as_bytes(::std::span{abbrev.data(), abbrev.size()})}};
        if(!locations.empty()) { result.push_back({version == 4u ? ".debug_loc" : ".debug_loclists",
            ::std::as_bytes(::std::span{locations.data(), locations.size()})}); }
        return result;
    }
};
static fixture make(unsigned version, bool empty_location, unsigned duplicate)
{
    using namespace ::llvm::dwarf;
    fixture result{}; result.version = version;
    auto const abbreviation{[&](unsigned code, Tag tag, bool children,
        ::std::initializer_list<::std::pair<Attribute, Form>> attributes)
    {
        uleb(result.abbrev, code); uleb(result.abbrev, tag); byte(result.abbrev, children ? 1u : 0u);
        for(auto [name, form] : attributes) { uleb(result.abbrev, name); uleb(result.abbrev, form); }
        uleb(result.abbrev, 0u); uleb(result.abbrev, 0u);
    }};
    abbreviation(1u, DW_TAG_compile_unit, true, {{DW_AT_name, DW_FORM_string}});
    abbreviation(2u, DW_TAG_subprogram, true, {{DW_AT_name, DW_FORM_string}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
    if(duplicate == 1u || duplicate == 2u)
    { abbreviation(3u, DW_TAG_variable, false, {{DW_AT_name, DW_FORM_string}, {DW_AT_type, DW_FORM_ref4},
        {DW_AT_const_value, DW_FORM_udata}, {DW_AT_const_value, DW_FORM_udata}}); }
    else if(empty_location)
    { abbreviation(3u, DW_TAG_variable, false, {{DW_AT_name, DW_FORM_string}, {DW_AT_type, DW_FORM_ref4},
        {DW_AT_const_value, DW_FORM_udata}, {DW_AT_location, DW_FORM_sec_offset}}); }
    else { abbreviation(3u, DW_TAG_variable, false, {{DW_AT_name, DW_FORM_string}, {DW_AT_type, DW_FORM_ref4}, {DW_AT_const_value, DW_FORM_udata}}); }
    if(duplicate == 3u)
    { abbreviation(4u, DW_TAG_base_type, false, {{DW_AT_name, DW_FORM_string}, {DW_AT_encoding, DW_FORM_data1},
        {DW_AT_byte_size, DW_FORM_data1}, {DW_AT_byte_size, DW_FORM_data1}}); }
    else { abbreviation(4u, DW_TAG_base_type, false, {{DW_AT_name, DW_FORM_string}, {DW_AT_encoding, DW_FORM_data1}, {DW_AT_byte_size, DW_FORM_data1}}); }
    uleb(result.abbrev, 0u);
    ::std::string body{}; word(body, static_cast<::std::uint16_t>(version));
    if(version == 5u) { byte(body, DW_UT_compile); byte(body, 4u); dword(body, 0u); }
    else { dword(body, 0u); byte(body, 4u); }
    uleb(body, 1u); text(body, "constant-attributes.c");
    uleb(body, 2u); text(body, "physical"); dword(body, 10u); dword(body, 20u);
    uleb(body, 3u); text(body, "value"); auto const type_reference{body.size()}; dword(body, 0u);
    uleb(body, duplicate == 2u ? 23u : 31u);
    if(duplicate == 1u || duplicate == 2u) { uleb(body, duplicate == 2u ? 31u : 23u); }
    if(empty_location)
    {
        if(version == 4u) { dword(body, 0u); dword(result.locations, 0u); dword(result.locations, 0u); }
        else
        {
            dword(body, 12u); // exact start of empty DW_LLE_end_of_list.
            dword(result.locations, 9u); word(result.locations, 5u); byte(result.locations, 4u); byte(result.locations, 0u);
            dword(result.locations, 0u); byte(result.locations, DW_LLE_end_of_list);
        }
    }
    uleb(body, 0u); // actual physical child list ends BEFORE forward type.
    auto const type_offset{body.size() + 4u};
    check(type_offset <= 0xffffffffu, "finite fixture CU-relative reference width");
    uleb(body, 4u); text(body, "int"); byte(body, DW_ATE_signed); byte(body, 4u);
    if(duplicate == 3u) { byte(body, 8u); }
    uleb(body, 0u); // CU children end.
    patch(body, type_reference, static_cast<::std::uint32_t>(type_offset));
    check(body.size() <= 0xffffffffu, "finite fixture unit length width");
    dword(result.info, static_cast<::std::uint32_t>(body.size()));
    ::fast_io::ostring_ref_std output{__builtin_addressof(result.info)}; ::fast_io::io::print(output, ::std::string_view{body});
    return result;
}
static dwarf::error parse(fixture const& input, ::std::unique_ptr<metadata_index>& out, dwarf::limits const& cap = {})
{ auto const sections{input.sections()}; return metadata_index::parse({sections, 100u, 4u}, out, cap); }
int main()
{
    for(unsigned version : {4u, 5u})
    {
        ::std::unique_ptr<metadata_index> out{};
        auto const normal{make(version, false, 0u)};
        check(parse(normal, out) == dwarf::error::none && out && out->variables().size() == 1u, "actual LLVM normal direct constant DIE");
        dwarf::variable_selection selected{};
        check(dwarf::query_named_variable(out->scopes(), out->types(), out->variables(), 12u, "value", selected) == dwarf::inline_query_error::none &&
            selected.location_available && selected.location.kind == dwarf::plan_kind::constant_value &&
            selected.location.constant_bits == 31u && selected.location.direct_constant_attribute,
            "accepted direct value carries exact metadata origin, not address authority");
        auto const empty{make(version, true, 0u)};
        check(parse(empty, out) == dwarf::error::none && out && out->variables().size() == 1u && out->variables()[0u].locations.empty(),
            "actual LLVM empty location list remains an explicitly present attribute");
        check(dwarf::query_named_variable(out->scopes(), out->types(), out->variables(), 12u, "value", selected) == dwarf::inline_query_error::none &&
            !selected.location_available && !selected.location.direct_constant_attribute, "empty storage cannot be replaced by a direct constant");
        for(unsigned duplicate : {1u, 2u, 3u})
        {
            auto const invalid{make(version, false, duplicate)};
            check(parse(invalid, out) == dwarf::error::malformed && !out,
                "duplicate attribute rejects before first-value or forward-type semantic find, old owner reset");
        }
        dwarf::limits cap{}; cap.max_attributes = 10u;
        check(parse(normal, out, cap) == dwarf::error::none && out, "each of ten attributes charged once across preflight/reference/walk");
        cap.max_attributes = 9u;
        check(parse(normal, out, cap) == dwarf::error::limit_exceeded && !out, "attribute limit remains exact, not bypassed by proof memo");
    }
    ::fast_io::io::println("PASS actual LLVM DWARF4/5 constant-attribute component; live producer/product qualification remains separate");
}
