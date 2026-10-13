// Serialized owned DWARF4/5 goes through the actual LLVM index parser. These
// fixtures are not compiler-producer or runtime memory/stop qualification.
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
using metadata_index = dwarf::index;
static void check(bool value, char const* reason)
{ if(!value) { ::fast_io::io::perrln("address_class_index: ", ::fast_io::mnp::os_c_str(reason)); ::fast_io::fast_terminate(); } }
template<unsigned Bits, typename Value> static void fixed(::std::string& out, Value value)
{ ::fast_io::ostring_ref_std sink{::std::addressof(out)}; ::fast_io::io::print(sink, ::fast_io::mnp::le_put<Bits>(static_cast<::std::uint64_t>(value))); }
template<typename Value> static void leb(::std::string& out, Value value)
{ ::fast_io::ostring_ref_std sink{::std::addressof(out)}; ::fast_io::io::print(sink, ::fast_io::mnp::leb128_put(value)); }
static void text(::std::string& out, ::std::string_view value)
{ ::fast_io::ostring_ref_std sink{::std::addressof(out)}; ::fast_io::io::print(sink, value, ::fast_io::mnp::chvw('\0')); }
static void append(::std::string& out, ::std::string_view bytes)
{ ::fast_io::ostring_ref_std sink{::std::addressof(out)}; ::fast_io::io::print(sink, bytes); }
static void patch(::std::string& out, ::std::size_t offset, ::std::uint32_t value)
{
    check(offset <= out.size() && 4u <= out.size() - offset, "owned CU reference patch bounds");
    ::std::string bytes{}; fixed<32u>(bytes, value); check(bytes.size() == 4u, "exact FastIO dword output");
    for(::std::size_t i{}; i != 4u; ++i) // [safe] finite four-byte scalar advance.
    { out[offset + i] = bytes[i]; } // [safe] destination sum and source extent checked before both indices.
}
struct fixture
{
    ::std::string info{}, abbrev{}; ::std::uint8_t address_bytes{};
    [[nodiscard]] ::std::array<dwarf::section, 2u> sections() const noexcept
    {
        // [owned immutable info/abbrev strings] end
        // [safe                             ] bounded synchronous span borrows
        //  ^^ parse retains its own buffers, not these local fixture pointers.
        return {{{".debug_info", ::std::as_bytes(::std::span{info.data(), info.size()})},
                 {".debug_abbrev", ::std::as_bytes(::std::span{abbrev.data(), abbrev.size()})}}};
    }
};
static fixture make(unsigned version, ::std::uint8_t width, ::llvm::dwarf::Form form, ::std::uint64_t value,
    bool negative = false, ::std::uint64_t high = 0u, bool duplicate = false, ::llvm::dwarf::Tag tag = ::llvm::dwarf::DW_TAG_pointer_type)
{
    using namespace ::llvm::dwarf;
    fixture out{}; out.address_bytes = width;
    auto const abbreviation{[&](unsigned code, Tag kind, bool children,
        ::std::initializer_list<::std::pair<Attribute, Form>> attributes)
    {
        leb(out.abbrev, code); leb(out.abbrev, static_cast<unsigned>(kind)); fixed<8u>(out.abbrev, children ? 1u : 0u);
        for(auto const [attribute, format] : attributes) { leb(out.abbrev, static_cast<unsigned>(attribute)); leb(out.abbrev, static_cast<unsigned>(format)); }
        leb(out.abbrev, 0u); leb(out.abbrev, 0u);
    }};
    abbreviation(1u, DW_TAG_compile_unit, true, {{DW_AT_name, DW_FORM_string}});
    abbreviation(2u, DW_TAG_subprogram, true, {{DW_AT_name, DW_FORM_string}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
    abbreviation(3u, DW_TAG_variable, false, {{DW_AT_name, DW_FORM_string}, {DW_AT_type, DW_FORM_ref4}});
    abbreviation(4u, DW_TAG_base_type, false, {{DW_AT_name, DW_FORM_string}, {DW_AT_encoding, DW_FORM_data1}, {DW_AT_byte_size, DW_FORM_data1}});
    leb(out.abbrev, 5u); leb(out.abbrev, static_cast<unsigned>(tag)); fixed<8u>(out.abbrev, 0u);
    for(auto const attribute : {DW_AT_byte_size, DW_AT_type})
    { leb(out.abbrev, static_cast<unsigned>(attribute)); leb(out.abbrev, static_cast<unsigned>(attribute == DW_AT_type ? DW_FORM_ref4 : DW_FORM_data1)); }
    if(form != Form{0})
    {
        for(unsigned i{}; i != (duplicate ? 2u : 1u); ++i) // [safe] finite one/two attribute repetitions for duplicate oracle.
        {
            leb(out.abbrev, static_cast<unsigned>(DW_AT_address_class)); leb(out.abbrev, static_cast<unsigned>(form));
            if(form == DW_FORM_implicit_const) { leb(out.abbrev, negative ? ::std::int64_t{-1} : static_cast<::std::int64_t>(value)); }
        }
    }
    leb(out.abbrev, 0u); leb(out.abbrev, 0u); leb(out.abbrev, 0u);
    ::std::string body{}; fixed<16u>(body, version);
    if(version == 5u) { fixed<8u>(body, DW_UT_compile); fixed<8u>(body, width); fixed<32u>(body, 0u); }
    else { fixed<32u>(body, 0u); fixed<8u>(body, width); }
    leb(body, 1u); text(body, "address-class.c");
    leb(body, 2u); text(body, "physical");
    if(width == 4u) { fixed<32u>(body, 10u); } else { fixed<64u>(body, 10u); }
    fixed<32u>(body, 20u);
    leb(body, 3u); text(body, "p"); auto const variable_reference{body.size()}; fixed<32u>(body, 0u); leb(body, 0u);
    auto const base_offset{body.size() + 4u};
    leb(body, 4u); text(body, "word"); fixed<8u>(body, DW_ATE_unsigned); fixed<8u>(body, 4u);
    auto const pointer_offset{body.size() + 4u};
    check(base_offset <= 0xffffffffu && pointer_offset <= 0xffffffffu, "finite owned fixture CU offsets");
    leb(body, 5u); fixed<8u>(body, width); fixed<32u>(body, static_cast<::std::uint32_t>(base_offset));
    if(form != Form{0})
    {
        for(unsigned i{}; i != (duplicate ? 2u : 1u); ++i) // [safe] finite attribute fixture scalar advance.
        {
            switch(form)
            {
                case DW_FORM_data1: fixed<8u>(body, value); break;
                case DW_FORM_data2: fixed<16u>(body, value); break;
                case DW_FORM_data4: fixed<32u>(body, value); break;
                case DW_FORM_data8: fixed<64u>(body, value); break;
                case DW_FORM_data16: fixed<64u>(body, value); fixed<64u>(body, high); break;
                case DW_FORM_udata: leb(body, value); break;
                case DW_FORM_sdata: leb(body, negative ? ::std::int64_t{-1} : static_cast<::std::int64_t>(value)); break;
                case DW_FORM_implicit_const: break; // value was emitted in its actual abbreviation.
                case DW_FORM_string: text(body, "invalid-class"); break;
                case DW_FORM_flag: fixed<8u>(body, 1u); break;
                default: check(false, "fixture form must have bounded owned encoder");
            }
        }
    }
    leb(body, 0u); patch(body, variable_reference, static_cast<::std::uint32_t>(pointer_offset));
    check(body.size() <= 0xffffffffu, "finite fixture CU length");
    fixed<32u>(out.info, static_cast<::std::uint32_t>(body.size())); append(out.info, body); return out;
}
static dwarf::error parse(fixture const& input, ::std::unique_ptr<metadata_index>& out)
{ auto const sections{input.sections()}; return metadata_index::parse({sections, 100u, input.address_bytes}, out); }
int main()
{
    using namespace ::llvm::dwarf;
    for(auto const version : {4u, 5u})
    {
        for(::std::uint8_t const width : {::std::uint8_t{4u}, ::std::uint8_t{8u}})
        {
            ::std::unique_ptr<metadata_index> out{};
            auto const accepted{[&](Form form, ::std::uint64_t value, Tag tag = DW_TAG_pointer_type)
            {
                auto const input{make(version, width, form, value, false, 0u, false, tag)};
                check(parse(input, out) == dwarf::error::none && out && out->variables().size() == 1u, "actual LLVM valid pointer/reference metadata");
                auto const type{out->variables().front().type};
                check(type < out->types().size(), "parsed variable type index before borrow");
                auto const& pointer{out->types()[type]}; // [safe] actual parsed type index checked above.
                check(pointer.kind == dwarf::type_kind::pointer && pointer.address_class_known == (form != Form{0}) &&
                    pointer.address_class == value && pointer.byte_count == width && pointer.size_known && pointer.byte_size == width,
                    "actual index retains explicit absence/class value and pointer width");
                check(pointer.reference_type == (tag != DW_TAG_pointer_type) && pointer.rvalue_reference_type == (tag == DW_TAG_rvalue_reference_type),
                    "reference/rvalue role remains distinct from plain pointer");
            }};
            accepted(Form{0}, 0u);
            for(auto const form : {DW_FORM_data1, DW_FORM_data2, DW_FORM_data4, DW_FORM_data8, DW_FORM_udata, DW_FORM_sdata})
            { accepted(form, 0u); accepted(form, 17u); }
            if(version == 5u)
            { accepted(DW_FORM_data16, 0u); accepted(DW_FORM_data16, 17u); accepted(DW_FORM_implicit_const, 0u); accepted(DW_FORM_implicit_const, 17u); }
            accepted(DW_FORM_data1, 3u, DW_TAG_reference_type); accepted(DW_FORM_udata, 4u, DW_TAG_rvalue_reference_type);
            for(auto const form : {DW_FORM_string, DW_FORM_flag})
            {
                auto const input{make(version, width, form, 0u)};
                check(parse(input, out) == dwarf::error::malformed && !out, "wrong address-class value class cannot enter a plan");
            }
            auto const negative{make(version, width, DW_FORM_sdata, 0u, true)};
            check(parse(negative, out) == dwarf::error::malformed && !out, "negative address-class profile value rejected");
            if(version == 5u)
            {
                auto const oversized{make(version, width, DW_FORM_data16, 0u, false, 1u)};
                check(parse(oversized, out) == dwarf::error::unsupported_dwarf && !out, "data16 class high half is never truncated or parsed as block length");
                auto const negative_implicit{make(version, width, DW_FORM_implicit_const, 0u, true)};
                check(parse(negative_implicit, out) == dwarf::error::malformed && !out, "negative abbreviation constant is not unsigned bit-cast");
            }
            auto const duplicate{make(version, width, DW_FORM_data1, 0u, false, 0u, true)};
            check(parse(duplicate, out) == dwarf::error::malformed && !out, "duplicate class rejected before semantic first-value find");
        }
    }
    ::fast_io::io::println("PASS serialized DWARF4/5 address-class index; compiler producers/runtime pending");
}
