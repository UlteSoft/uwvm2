#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <uwvm2/uwvm/debugger/source_dwarf_values.h>
#include <uwvm2/uwvm/debugger/source_dwarf_objects.h>
#include <fast_io.h>
#include <algorithm>
#include <cstdlib>
#include <type_traits>

using namespace uwvm2::uwvm::debugger::source_dwarf;
using metadata_index = uwvm2::uwvm::debugger::source_dwarf::index;
using bytes = ::std::vector<::std::byte>;
[[noreturn]] static void fail(char const* message)
{ ::fast_io::io::perrln("debug_source_dwarf_index: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
static void check(bool value, char const* message) { if(!value) { fail(message); } }
static void fixed(bytes& out, ::std::uint64_t value, unsigned width)
{ for(unsigned i{}; i != width; ++i) { out.push_back(::std::byte{static_cast<unsigned char>(value >> (8u * i))}); } }
static void uleb(bytes& out, ::std::uint64_t value)
{
    // Fixture generation, not a text/LEB parser. All parsing uses fast_io.
    do { auto const part{static_cast<unsigned char>(value & 0x7fu)}; value >>= 7u;
         out.push_back(::std::byte{static_cast<unsigned char>(part | (value ? 0x80u : 0u))}); } while(value);
}
static void text(bytes& out, ::std::string_view value)
{ for(auto c : value) { out.push_back(::std::byte{static_cast<unsigned char>(c)}); } out.push_back(::std::byte{}); }
static void patch(bytes& out, ::std::size_t offset, ::std::uint64_t value)
{
    check(offset <= out.size() && 4u <= out.size() - offset, "fixture patch bounds");
    for(unsigned i{}; i != 4u; ++i)
    { out[offset + i] = ::std::byte{static_cast<unsigned char>(value >> (8u * i))}; } // checked live bytes, no pointer movement.
}
struct fixture
{
    bytes info{}, abbrev{}, line{}, ranges{}, loc{};
    ::std::size_t origin_patch{}, inline_variable_offset{}, expression_offset{};
    ::std::uint8_t address{4u};
    ::std::vector<section> sections() const
    {
        ::std::vector<section> result{{".debug_info", info}, {".debug_abbrev", abbrev}};
        if(!line.empty()) { result.push_back({".debug_line", line}); }
        if(!ranges.empty()) { result.push_back({".debug_ranges", ranges}); }
        if(!loc.empty()) { result.push_back({".debug_loc", loc}); }
        return result;
    }
};
static bytes line_table(::std::string_view filename, ::std::uint64_t code_begin)
{
    bytes header{};
    for(unsigned char value : {1u, 1u, 1u, 0xfbu, 14u, 13u}) { fixed(header, value, 1u); }
    for(unsigned i{1u}; i != 13u; ++i)
    { fixed(header, i == 2u || i == 3u || i == 4u || i == 5u || i == 9u || i == 12u ? 1u : 0u, 1u); }
    text(header, ""); text(header, filename); uleb(header, 0u); uleb(header, 0u); uleb(header, 0u); text(header, "");
    bytes program{};
    fixed(program, 0u, 1u); uleb(program, 5u); fixed(program, 2u, 1u); fixed(program, code_begin, 4u);
    fixed(program, 1u, 1u); fixed(program, 2u, 1u); uleb(program, 20u);
    fixed(program, 0u, 1u); uleb(program, 1u); fixed(program, 1u, 1u);
    bytes body{}; fixed(body, 4u, 2u); fixed(body, header.size(), 4u);
    body.insert(body.end(), header.begin(), header.end()); body.insert(body.end(), program.begin(), program.end());
    bytes result{}; fixed(result, body.size(), 4u); result.insert(result.end(), body.begin(), body.end()); return result;
}
static fixture make_fixture(unsigned version = 4u, ::std::uint8_t address = 4u, bool dwarf64 = false,
    bool with_line = false, ::std::string_view filename = "synthetic.c", ::std::uint64_t line_offset = 0u, ::std::uint64_t code_begin = 10u, unsigned lexical_mode = 0u,
    ::llvm::dwarf::Tag lexical_tag = ::llvm::dwarf::DW_TAG_lexical_block, bool discarded_physical = false, bool overlap_locations = false, bool reversed_inline = false, ::std::string_view compilation_directory = {})
{
    using namespace ::llvm::dwarf;
    fixture result{}; result.address = address;
    auto const abbreviation{[&](unsigned code, Tag tag, bool children,
        ::std::initializer_list< ::std::pair<Attribute, Form>> fields)
    {
        uleb(result.abbrev, code); uleb(result.abbrev, tag); fixed(result.abbrev, children, 1u);
        for(auto [attr, form] : fields) { uleb(result.abbrev, attr); uleb(result.abbrev, form); }
        uleb(result.abbrev, 0u); uleb(result.abbrev, 0u);
    }};
    if(with_line)
    { abbreviation(1u, DW_TAG_compile_unit, true, {{DW_AT_name, DW_FORM_string}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}, {DW_AT_stmt_list, DW_FORM_sec_offset}, {DW_AT_comp_dir, DW_FORM_string}}); }
    else { abbreviation(1u, DW_TAG_compile_unit, true, {{DW_AT_name, DW_FORM_string}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}}); }
    abbreviation(2u, DW_TAG_subprogram, true, {{DW_AT_name, DW_FORM_string}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}, {DW_AT_frame_base, DW_FORM_exprloc}});
    abbreviation(3u, DW_TAG_formal_parameter, false, {{DW_AT_name, DW_FORM_string}, {DW_AT_type, DW_FORM_ref4}, {DW_AT_location, overlap_locations ? DW_FORM_sec_offset : DW_FORM_exprloc}, {DW_AT_decl_file, DW_FORM_data1}});
    abbreviation(4u, DW_TAG_base_type, false, {{DW_AT_name, DW_FORM_string}, {DW_AT_encoding, DW_FORM_data1}, {DW_AT_byte_size, DW_FORM_data1}});
    abbreviation(5u, DW_TAG_inlined_subroutine, true, {{DW_AT_abstract_origin, DW_FORM_ref4}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, reversed_inline ? DW_FORM_addr : DW_FORM_data4}});
    abbreviation(6u, DW_TAG_subprogram, true, {{DW_AT_name, DW_FORM_string}});
    abbreviation(7u, DW_TAG_variable, false, {{DW_AT_name, DW_FORM_string}, {DW_AT_type, DW_FORM_ref4}, {DW_AT_location, DW_FORM_exprloc}, {DW_AT_decl_file, DW_FORM_data1}});
    abbreviation(8u, DW_TAG_formal_parameter, false, {{DW_AT_abstract_origin, DW_FORM_ref4}, {DW_AT_location, DW_FORM_exprloc}});
    abbreviation(9u, DW_TAG_formal_parameter, false, {{DW_AT_name, DW_FORM_string}, {DW_AT_type, DW_FORM_ref4}, {DW_AT_decl_file, DW_FORM_data1}});
    abbreviation(10u, DW_TAG_compile_unit, true, {{DW_AT_GNU_dwo_name, DW_FORM_string}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
    abbreviation(11u, lexical_tag, true, {});
    abbreviation(12u, lexical_tag, true, {{DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
    abbreviation(13u, lexical_tag, true, {{DW_AT_ranges, DW_FORM_sec_offset}});
    abbreviation(14u, DW_TAG_unspecified_parameters, false, {}); // Standard catch(...) child, no location.
    uleb(result.abbrev, 0u);
    auto const prefix{dwarf64 ? 12u : 4u};
    bytes body{}; fixed(body, version, 2u);
    if(version == 5u) { fixed(body, 1u, 1u); fixed(body, address, 1u); fixed(body, 0u, dwarf64 ? 8u : 4u); }
    else { fixed(body, 0u, dwarf64 ? 8u : 4u); fixed(body, address, 1u); }
    uleb(body, 1u); text(body, filename); fixed(body, code_begin, address); fixed(body, 20u, 4u);
    if(with_line) { fixed(body, line_offset, dwarf64 ? 8u : 4u); text(body, compilation_directory); result.line = line_table(filename, code_begin); }
    uleb(body, 2u); text(body, "physical"); fixed(body, discarded_physical ? ::llvm::dwarf::computeTombstoneAddress(address) : code_begin, address); fixed(body, 20u, 4u);
    uleb(body, 3u); fixed(body, 0xedu, 1u); fixed(body, 0u, 1u); fixed(body, 0u, 1u); // frame base.
    uleb(body, 3u); text(body, "value"); auto const type1{body.size()}; fixed(body, 0u, 4u);
    if(overlap_locations)
    {
        fixed(body, 0u, 4u); fixed(body, 1u, 1u);
        ::std::string encoded{}; ::fast_io::ostring_ref_std output{::std::addressof(encoded)};
        for(auto const begin : {0u,5u})
        { ::fast_io::io::print(output,::fast_io::mnp::le_put<32u>(begin),::fast_io::mnp::le_put<32u>(begin+10u),
              ::fast_io::mnp::le_put<16u>(4u),::std::string_view{"\xed\0\0\x9f",4u}); }
        ::fast_io::io::print(output,::fast_io::mnp::le_put<32u>(0u),::fast_io::mnp::le_put<32u>(0u));
        result.loc.resize(encoded.size()); ::fast_io::freestanding::my_memcpy(result.loc.data(),encoded.data(),encoded.size());
    }
    else
    {
        uleb(body, 4u); result.expression_offset = prefix + body.size();
        fixed(body, 0xedu, 1u); fixed(body, 0u, 1u); fixed(body, 0u, 1u); fixed(body, 0x9fu, 1u); fixed(body, 1u, 1u);
    }
    if(lexical_mode == 1u) { uleb(body, 11u); } // absent ranges inherit the actual physical scope.
    else if(lexical_mode == 2u) { uleb(body, 12u); fixed(body, code_begin, address); fixed(body, 0u, 4u); }
    else if(lexical_mode == 3u)
    { uleb(body, 13u); fixed(body, 0u, dwarf64 ? 8u : 4u); fixed(result.ranges, 0u, address); fixed(result.ranges, 0u, address); }
    else if(lexical_mode == 6u)
    {
        // Genuine LLVM .debug_ranges representation: list offsets under a
        // discarded ancestor, with an explicit CU-width max base. LLVM's
        // legacy range tombstone is max-1; this max base can therefore expose
        // max+offset (or wrap at width64) in its absolute range result.
        uleb(body, 13u); fixed(body, 0u, dwarf64 ? 8u : 4u);
        auto const marker{::llvm::dwarf::computeTombstoneAddress(address)};
        fixed(result.ranges, marker, address); fixed(result.ranges, marker, address);
        fixed(result.ranges, 0x9cu, address); fixed(result.ranges, 0xa3u, address);
        fixed(result.ranges, 0xa8u, address); fixed(result.ranges, 0xaeu, address);
        fixed(result.ranges, 0u, address); fixed(result.ranges, 0u, address);
    }
    else if(lexical_mode == 4u) { uleb(body, 12u); fixed(body, code_begin + 2u, address); fixed(body, 10u, 4u); }
    else if(lexical_mode == 5u) { uleb(body, 12u); fixed(body, code_begin + 12u, address); fixed(body, 4u, 4u); }
    if(lexical_mode != 0u && lexical_tag == DW_TAG_catch_block) { uleb(body, 14u); }
    uleb(body, 5u); auto const inline_origin{body.size()}; fixed(body, 0u, 4u); fixed(body, code_begin + 2u, address); fixed(body, reversed_inline ? code_begin + 1u : 8u, reversed_inline ? address : 4u);
    result.inline_variable_offset = prefix + body.size(); uleb(body, 8u);
    auto const variable_origin{body.size()}; result.origin_patch = prefix + variable_origin; fixed(body, 0u, 4u);
    uleb(body, 4u); fixed(body, 0xedu, 1u); fixed(body, 0u, 1u); fixed(body, 1u, 1u); fixed(body, 0x9fu, 1u);
    uleb(body, 0u); // inline children end.
    if(lexical_mode != 0u) { uleb(body, 0u); } // lexical children end.
    uleb(body, 7u); text(body, "late"); auto const type2{body.size()}; fixed(body, 0u, 4u);
    uleb(body, 2u); fixed(body, 0x91u, 1u); fixed(body, 0x7cu, 1u); fixed(body, 1u, 1u);
    uleb(body, 0u); // physical subprogram children end.
    auto const base_type{prefix + body.size()}; uleb(body, 4u); text(body, "int"); fixed(body, DW_ATE_signed, 1u); fixed(body, 4u, 1u);
    auto const abstract_origin{prefix + body.size()}; uleb(body, 6u); text(body, "inner");
    auto const abstract_parameter{prefix + body.size()}; uleb(body, 9u); text(body, "inner_arg");
    auto const type3{body.size()}; fixed(body, 0u, 4u); fixed(body, 1u, 1u); uleb(body, 0u); uleb(body, 0u);
    patch(body, type1, base_type); patch(body, type2, base_type); patch(body, type3, base_type);
    patch(body, inline_origin, abstract_origin); patch(body, variable_origin, abstract_parameter);
    if(dwarf64) { fixed(result.info, 0xffffffffu, 4u); fixed(result.info, body.size(), 8u); }
    else { fixed(result.info, body.size(), 4u); }
    result.info.insert(result.info.end(), body.begin(), body.end());
    return result;
}
static error parse(fixture const& data, ::std::unique_ptr<metadata_index>& out, limits cap = {})
{ auto sections{data.sections()}; return metadata_index::parse({sections, 100u, data.address}, out, cap); }
static void rejected(fixture const& data, error expected, limits cap = {})
{
    ::std::unique_ptr<metadata_index> out{}; auto good{make_fixture()};
    check(parse(good, out) == error::none && out, "prepare previous output");
    check(parse(data, out, cap) == expected && !out, "failed parse clears old and partial output");
}
static void component_tests()
{
    static_assert(!::std::is_move_constructible_v<metadata_index>);
    for(unsigned version : {4u, 5u}) for(auto address : {::std::uint8_t{4u}, ::std::uint8_t{8u}}) for(bool wide : {false, true})
    {
        auto data{make_fixture(version, address, wide)}; ::std::unique_ptr<metadata_index> out{};
        check(parse(data, out) == error::none && out, "DWARF32/64 versions4/5 Wasm32/64");
        check(out->address_bytes() == address, "owned metadata retains its validated guest address width");
        check(out->variables().size() == 3u && out->types().size() == 1u, "concrete locals share an interned owned type");
        check(out->variables()[1u].name == "inner_arg" && out->variables()[1u].locations[0u].plan.local_index == 1u,
              "abstract origin contributes name but concrete physical local location");
        auto const inlined{::std::find_if(out->scopes().begin(), out->scopes().end(), [](auto const& value) { return value.kind == scope_kind::inline_subprogram; })};
        check(inlined != out->scopes().end() && inlined->name == "inner" && inlined->ranges[0u].begin == 12u, "concrete inline range and origin name");
        data.info.clear(); data.abbrev.clear();
        check(out->variables()[0u].name == "value", "index owns copied inputs");
    }
    {
        auto data{make_fixture(4u,4u,false,false,"synthetic.c",0u,10u,3u)};
        ::std::string encoded{::fast_io::concat_std(::fast_io::mnp::le_put<32u>(2u),::fast_io::mnp::le_put<32u>(8u),
            ::fast_io::mnp::le_put<32u>(4u),::fast_io::mnp::le_put<32u>(12u),::fast_io::mnp::le_put<32u>(0u),::fast_io::mnp::le_put<32u>(0u))};
        data.ranges.resize(encoded.size()); ::fast_io::freestanding::my_memcpy(data.ranges.data(),encoded.data(),encoded.size());
        ::std::unique_ptr<metadata_index> out{}; check(parse(data,out)==error::none && out,"same-DIE overlapping range union");
        auto const lexical{::std::find_if(out->scopes().begin(),out->scopes().end(),[](auto const& scope){return scope.kind==scope_kind::lexical_block;})};
        check(lexical!=out->scopes().end() && lexical->ranges.size()==1u && lexical->ranges[0u].begin==12u && lexical->ranges[0u].end==22u,
            "union retains exact original coverage");
        auto discarded{make_fixture()}; constexpr ::std::string_view physical{"physical"};
        auto found{::std::search(discarded.info.begin(),discarded.info.end(),physical.begin(),physical.end(),
            [](auto a,auto b){return ::std::to_integer<unsigned char>(a)==static_cast<unsigned char>(b);})};
        check(found!=discarded.info.end(),"discarded range fixture spelling");
        auto const offset{static_cast<::std::size_t>(found-discarded.info.begin())+physical.size()+1u+4u};
        check(offset<=discarded.info.size() && 4u<=discarded.info.size()-offset,"complete high-PC output field");
        auto const first{reinterpret_cast<char*>(discarded.info.data()+offset)}; ::fast_io::basic_obuffer_view<char> sink{first,first+4u};
        ::fast_io::io::print(sink,::fast_io::mnp::le_put<32u>(0xfffffffbu));
        check(parse(discarded,out)==error::none && out,"unrepresentable executable range quarantined");
        ::std::vector<numeric_variable> active{};
        check(query_numeric_variables(out->scopes(),out->types(),out->variables(),12u,{},0u,active)==inline_query_error::unavailable && active.empty(),
            "unrepresentable parent cannot authorize descendant values");
        data=make_fixture(4u,4u,false,false,"synthetic.c",0u,10u,0u,::llvm::dwarf::DW_TAG_lexical_block,false,true);
        check(parse(data,out)==error::none && out,"conflicting locations do not reject unrelated metadata");
        check(out->variables()[0u].locations.size()==1u && out->variables()[0u].locations[0u].plan.kind==plan_kind::unavailable &&
            out->variables()[0u].locations[0u].plan.reason==unavailable_reason::unsupported_expression && out->variables()[1u].locations[0u].plan.kind!=plan_kind::unavailable,
            "only conflicting list is quarantined without read authority");
    }
    for(unsigned version : {4u,5u}) for(auto address : {::std::uint8_t{4u},::std::uint8_t{8u}})
    {
        auto data{make_fixture(version,address,false,false,"inline.go",0u,10u,0u,
            ::llvm::dwarf::DW_TAG_lexical_block,false,false,true)};
        ::std::unique_ptr<metadata_index> out{};
        check(parse(data,out)==error::none && out,"in-Code reversed inline coverage is quarantined");
        auto const bad_inline{::std::find_if(out->scopes().begin(),out->scopes().end(),[](auto const& scope)
            { return scope.kind==scope_kind::inline_subprogram; })};
        check(bad_inline!=out->scopes().end() && bad_inline->ranges.empty() && bad_inline->own_ranges_declared && !bad_inline->concrete,
            "quarantined inline has no execution range and cannot inherit the physical parent");
        ::std::vector<inline_frame> frames{};
        check(query_inline_frames(out->scopes(),12u,frames)==inline_query_error::none && frames.empty(),
            "reversed inline does not appear in an actual source frame chain");
        variable_selection selected{};
        check(query_named_variable(out->scopes(),out->types(),out->variables(),12u,"inner_arg",selected)==inline_query_error::unavailable,
            "child of reversed inline cannot publish source storage");
        check(query_named_variable(out->scopes(),out->types(),out->variables(),12u,"value",selected)==inline_query_error::none,
            "unrelated physical variable stays queryable");
    }
    // Parse ACTUAL synthetic embedded DWARF, not just an invented metadata
    // flag: absent / low_pc==high_pc / explicitly empty DW_AT_ranges.
    for(unsigned lexical_mode : {1u, 2u, 3u})
    {
        auto data{make_fixture(4u, 4u, false, false, "empty.c", 0u, 10u, lexical_mode)};
        ::std::unique_ptr<metadata_index> parsed{}; check(parse(data, parsed) == error::none, "synthetic lexical scope parse");
        auto const lexical{::std::find_if(parsed->scopes().begin(), parsed->scopes().end(),
            [](auto const& scope) { return scope.kind == scope_kind::lexical_block; })};
        check(lexical != parsed->scopes().end() && lexical->ranges.empty() &&
              lexical->own_ranges_declared == (lexical_mode != 1u), "own empty-range attributes retained without inheritance");
        ::std::vector<inline_frame> frames{};
        check(query_inline_frames(parsed->scopes(), 15u, frames) == inline_query_error::none &&
              frames.size() == (lexical_mode == 1u ? 1u : 0u), "empty lexical range excludes concrete inline child");
        ::std::array<copied_numeric_local, 2u> locals{}; locals[0].wasm_type = locals[1].wasm_type = 0x7fu; locals[0].available = locals[1].available = true;
        ::std::vector<numeric_variable> values{};
        check(query_numeric_variables(parsed->scopes(), parsed->types(), parsed->variables(), 15u, locals, locals.size(), values) ==
              inline_query_error::none && values.size() == (lexical_mode == 1u ? 3u : 2u), "parsed empty lexical range excludes its source variable");
    }
    // Parse real encoded embedded DWARF4/5 through LLVM; these are metadata
    // scope tests, not invented stop tickets or C++ exception-execution proofs.
    // Empty/outside parent + still-covering inline child is adversarial input:
    // never flatten the standard try/catch owner and resurrect that child.
    for(auto const block_tag : {::llvm::dwarf::DW_TAG_try_block, ::llvm::dwarf::DW_TAG_catch_block})
    { for(unsigned version : {4u, 5u}) { for(unsigned mode : {1u, 2u, 3u, 4u, 5u})
    {
        if(version == 5u && mode == 3u) { continue; } // This fixture's empty .debug_ranges is DWARF4, not v5 rnglists.
        auto data{make_fixture(version, 4u, false, false, "scope.cpp", 0u, 10u, mode, block_tag)};
        ::std::unique_ptr<metadata_index> parsed{};
        check(parse(data, parsed) == error::none && parsed, "standard try/catch embedded scope parse");
        auto const lexical{::std::find_if(parsed->scopes().begin(), parsed->scopes().end(),
            [](auto const& scope) { return scope.kind == scope_kind::lexical_block; })};
        check(lexical != parsed->scopes().end() && lexical->own_ranges_declared == (mode != 1u),
              "try/catch own range attributes retained; absent ranges inherit");
        bool const active{mode == 1u || mode == 4u};
        ::std::vector<inline_frame> frames{};
        check(query_inline_frames(parsed->scopes(), 15u, frames) == inline_query_error::none &&
              frames.size() == (active ? 1u : 0u), "try/catch bounds gate nested inline display");
        concrete_scope_path path{};
        check(query_concrete_scope_path(parsed->scopes(), 15u, path) == scope_path_error::none &&
              path.physical_and_inline.size() == (active ? 2u : 1u), "source next/finish uses bounded try/catch inline identity");
        ::std::array<copied_numeric_local, 2u> locals{}; locals[0].wasm_type = locals[1].wasm_type = 0x7fu; locals[0].available = locals[1].available = true;
        ::std::vector<numeric_variable> values{};
        check(query_numeric_variables(parsed->scopes(), parsed->types(), parsed->variables(), 15u,
              locals, locals.size(), values) == inline_query_error::none && values.size() == (active ? 3u : 2u),
              "try/catch bounds gate inner source parameter; no host memory read");
    } } }
    // This is a metadata consumer regression, NOT guest execution or Rust
    // producer qualification. The independent unchanged Rust4/5 O0 Wasm
    // fixtures and llvm-dwarfdump oracle provide the real producer cases.
    for(unsigned version : {4u, 5u}) for(auto address : {::std::uint8_t{4u}, ::std::uint8_t{8u}}) for(bool wide : {false, true})
    {
        unsigned const mode{version == 4u ? 6u : 4u};
        auto dead{make_fixture(version, address, wide, false, "discarded.rs", 0u, 10u, mode,
            ::llvm::dwarf::DW_TAG_lexical_block, true)};
        ::std::unique_ptr<metadata_index> parsed{};
        check(parse(dead, parsed) == error::none && parsed, "direct concrete tombstone retires descendant execution ranges");
        check(parsed->variables().empty(), "discarded concrete variables carry no executable storage");
        check(::std::all_of(parsed->scopes().begin(), parsed->scopes().end(), [](auto const& scope)
            { return scope.kind == scope_kind::compile_unit || (!scope.concrete && scope.ranges.empty() && scope.frame_base.empty()); }),
            "discarded descendants cannot resurrect inline ranges or frame base");
        // Same list under a LIVE concrete parent is still malformed; arbitrary
        // out-of-Code or wrapped addresses must not be accepted as tombstones.
        if(version == 4u)
        { rejected(make_fixture(version, address, wide, false, "live.rs", 0u, 10u, mode), error::malformed); }
        auto live{make_fixture(version, address, wide, false, "live.rs")};
        dead.info.insert(dead.info.end(), live.info.begin(), live.info.end());
        check(parse(dead, parsed) == error::none && parsed->variables().size() == 3u,
            "dead CU metadata does not suppress another live CU's owned locals");
        auto corrupt{dead}; corrupt.info.pop_back();
        rejected(corrupt, error::malformed); // full structural preflight remains mandatory.
    }
    auto good{make_fixture()}; auto bad{good}; bad.info.pop_back(); rejected(bad, error::malformed);
    bad = good; bad.info[4u] = ::std::byte{3u}; rejected(bad, error::unsupported_dwarf);
    bad = make_fixture(5u); bad.info[6u] = ::std::byte{4u}; rejected(bad, error::unsupported_external);
    bad = good; bad.info[11u] = ::std::byte{10u}; rejected(bad, error::unsupported_external); // DWO name is never loaded.
    bad = good; patch(bad.info, bad.origin_patch, bad.inline_variable_offset); rejected(bad, error::malformed);
    bad = good; bad.info[bad.expression_offset + 2u] = ::std::byte{0x80}; bad.info[bad.expression_offset + 3u] = ::std::byte{0x80}; rejected(bad, error::malformed);
    for(auto member : {&limits::max_sections, &limits::max_section_bytes, &limits::max_total_bytes, &limits::max_units,
        &limits::max_unit_bytes, &limits::max_dies, &limits::max_depth, &limits::max_string_bytes, &limits::max_total_string_bytes,
        &limits::max_reference_hops, &limits::max_attributes, &limits::max_ranges, &limits::max_locations,
        &limits::max_expression_bytes, &limits::max_total_expression_bytes})
    { limits cap{}; cap.*member = 0u; rejected(good, error::limit_exceeded, cap); }
    auto alpha{make_fixture(4u, 4u, false, true, "alpha.c")};
    auto beta{make_fixture(4u, 4u, false, true, "beta.c", alpha.line.size(), 40u)};
    alpha.info.insert(alpha.info.end(), beta.info.begin(), beta.info.end());
    alpha.line.insert(alpha.line.end(), beta.line.begin(), beta.line.end());
    ::std::unique_ptr<metadata_index> out{};
    check(parse(alpha, out) == error::none && out->variables().size() == 6u, "two embedded CUs");
    check(out->variables()[0u].declaration_file == "alpha.c" && out->variables()[3u].declaration_file == "beta.c",
          "same file index resolves through its own CU");
    check(out->variables()[1u].declaration_file == "alpha.c" && out->variables()[4u].declaration_file == "beta.c",
          "abstract-origin declaration file remains CU-local");
    limits no_rows{}; no_rows.max_line_rows = 0u; rejected(alpha, error::limit_exceeded, no_rows);
    auto sections{good.sections()}; sections.push_back(sections[0u]);
    check(metadata_index::parse({sections, 100u, 4u}, out) == error::malformed && !out, "duplicate section rejection");
    for(auto name : {"external_debug_info", ".gnu_debugaltlink", ".debug_sup", ".debug_info.dwo", ".debug_types", ".debug_gdb_scripts"})
    {
        sections = good.sections(); sections.push_back({name, {}});
        check(metadata_index::parse({sections, 100u, 4u}, out) == error::unsupported_external && !out, "external metadata never loaded");
    }
    sections = good.sections();
    check(metadata_index::parse({sections, 11u, 4u}, out) == error::malformed && !out, "out-of-Code metadata range rejected");
    check(metadata_index::parse({sections, 100u, 2u}, out) == error::unsupported_dwarf && !out, "invalid input address size");
    check(metadata_index::parse({{}, 100u, 4u}, out) == error::missing_sections && !out, "missing required sections");
}
static void real_fixture(char const* path, ::std::string_view expected_file, bool expect_inline)
{
    // Host test argument only: the component itself has no file loader/API.
    ::fast_io::native_file_loader file{::fast_io::mnp::os_c_str(path), ::fast_io::open_mode::in};
    check(file.size() >= 8u, "Wasm fixture header");
    // [host-owned bounded loader bytes] end
    // [safe                         ] reader never advances a pointer beyond end.
    details::reader module{{reinterpret_cast<::std::byte const*>(file.data()), file.size()}, 8u};
    ::std::vector<section> sections{}; ::std::uint64_t code_size{};
    while(module.cursor != module.bytes.size())
    {
        ::std::uint8_t id{}; ::std::uint32_t size{};
        check(module.byte(id) && module.leb(size) && size <= module.bytes.size() - module.cursor, "Wasm section bounds");
        // [safe] checked section size forms one in-loader payload borrow.
        auto const payload{module.bytes.subspan(module.cursor, size)};
        module.cursor += size; // checked scalar cursor may reach module_end.
        if(id == 10u) { check(code_size == 0u, "single Code section"); code_size = size; }
        if(id != 0u) { continue; }
        details::reader custom{payload}; ::std::uint32_t name_size{};
        check(custom.leb(name_size) && name_size <= payload.size() - custom.cursor, "custom name bounds");
        // [safe] name and custom data are inside the checked loader section;
        // their views live until parse copies its own accepted DWARF buffers.
        ::std::string_view name{reinterpret_cast<char const*>(payload.data() + custom.cursor), name_size};
        custom.cursor += name_size; // checked name consumption at most to payload_end.
        if(name.starts_with(".debug_") || name == "external_debug_info" || name == ".gnu_debugaltlink")
        { sections.push_back({name, payload.subspan(custom.cursor)}); }
    }
    check(code_size != 0u, "fixture Code section present"); ::std::unique_ptr<metadata_index> out{};
    auto const status{metadata_index::parse({sections, code_size, 4u}, out)};
    if(status != error::none) { ::fast_io::io::perrln("metadata parse status=", static_cast<unsigned>(status)); fail("official-tool fixture parse"); }
    bool has_file{}, has_inline{}, has_variable{};
    for(auto const& scope : out->scopes())
    { if(scope.kind == scope_kind::inline_subprogram && scope.concrete) { has_inline = true; } }
    for(auto const& variable : out->variables())
    {
        if(variable.declaration_file.ends_with(expected_file)) { has_file = true; }
        if(variable.name == "value" || variable.name == "adjusted" || variable.name == "inner_arg") { has_variable = true; }
    }
    check(has_variable && has_file, "real named variable and CU-local declaration file");
    check(!expect_inline || has_inline, "optimized fixture has actual concrete inline DIE");
    ::fast_io::io::println("real DWARF fixture: scopes=", out->scopes().size(), " variables=", out->variables().size(), " inline=", has_inline);
}
int main(int argc, char** argv)
{
    if(argc == 1)
    {
        using namespace ::llvm::dwarf;
        auto make{[](std::string_view directory, std::uint64_t offset = 0u, std::uint64_t begin = 10u)
        {
            return make_fixture(4u, 4u, false, true, "relative.c", offset, begin,
                0u, DW_TAG_lexical_block, false, false, false, directory);
        }};
        auto first{make("/one")};
        std::unique_ptr<metadata_index> out;
        check(parse(first, out) == error::none && out->line_directories().size() == 1u &&
              out->line_directories()[0u].directory == "/one", "owned embedded CU compilation directory");
        auto second{make("/two", first.line.size(), 40u)};
        first.info.insert(first.info.end(), second.info.begin(), second.info.end());
        first.line.insert(first.line.end(), second.line.begin(), second.line.end());
        check(parse(first, out) == error::none && out->line_directories().size() == 2u &&
              out->line_directories()[1u].directory == "/two", "two CU line-unit associations");
        first = make("/one"); second = make("/two");
        first.info.insert(first.info.end(), second.info.begin(), second.info.end());
        check(parse(first, out) == error::malformed && !out, "conflicting shared line-table compilation directories");
        first = make("/same"); second = make("/same");
        first.info.insert(first.info.end(), second.info.begin(), second.info.end());
        check(parse(first, out) == error::none && out->line_directories().size() == 1u,
              "identical shared line-table owner is deduplicated");
        component_tests();
    }
    else
    {
        check(argc == 4, "usage: index fixture.wasm expected-file 0|1");
        ::std::uint32_t inline_flag{};
        // [safe] the host argv string is terminated; fast_io performs the entire
        // decimal scan in that borrowed span, with no handwritten decimal parser.
        auto const begin{argv[3]}; auto const end{begin + ::fast_io::cstr_len(begin)};
        auto const parsed{::fast_io::parse_by_scan(begin, end, ::fast_io::mnp::dec_get<true, true>(inline_flag))};
        check(parsed.code == ::fast_io::parse_code::ok && parsed.iter == end && inline_flag <= 1u, "inline expectation flag");
        real_fixture(argv[1], {argv[2], ::fast_io::cstr_len(argv[2])}, inline_flag != 0u);
    }
    ::fast_io::io::println("debug_source_dwarf_index: PASS");
}
