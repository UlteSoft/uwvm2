// Finite numeric source values only. These synthetic metadata/copies are NOT
// runtime read authority; separate actual publisher/stop witnesses qualify that.
#include <uwvm2/uwvm/debugger/source_dwarf_values.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_dwarf_values: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static ::std::vector<scope_record> fixture()
{
    ::std::vector<scope_record> value(4u);
    for(::std::size_t i{};i!=value.size();++i) { value[i].identity={1u,i+1u}; }
    value[0].kind = scope_kind::compile_unit;
    value[1].parent = 0u; value[1].kind = scope_kind::subprogram; value[1].concrete = true; value[1].ranges = {{10u, 30u}};
    value[2].parent = 1u; value[2].kind = scope_kind::lexical_block;
    value[3].parent = 2u; value[3].kind = scope_kind::inline_subprogram; value[3].concrete = true; value[3].ranges = {{12u, 22u}};
    value[3].name = "inner"; return value;
}
static variable_record variable(::std::size_t type, ::std::uint64_t local, ::std::size_t scope = 1u)
{
    variable_record value{}; value.scope = scope; value.type = type; value.name = "value"; value.parameter = true;
    location_plan plan{}; plan.kind = plan_kind::wasm_local_value; plan.local_index = local;
    value.locations.push_back({{}, plan}); return value;
}
template<typename T> static copied_numeric_local local(::std::uint8_t type, T bits)
{
    copied_numeric_local value{}; value.wasm_type = type; value.available = true; value.bytes.fill(::std::byte{0xa5u});
    // [owned fixed 16-byte test carrier] end
    // [safe                           ] complete actual uint32/uint64 representation, not a source-width prefix.
    ::std::memcpy(value.bytes.data(), ::std::addressof(bits), sizeof(bits)); return value;
}
int main()
{
    auto scopes{fixture()};
    auto const scalar{[](::std::string_view name, type_kind kind, ::std::uint64_t encoding, ::std::uint8_t bytes)
    { type_record type{}; type.name=name; type.kind=kind; type.encoding=encoding; type.byte_count=bytes; return type; }};
    ::std::vector<type_record> types{
        scalar("signed char",type_kind::scalar,0x06u,1u), scalar("short",type_kind::scalar,0x05u,2u),
        scalar("unsigned char",type_kind::scalar,0x08u,1u), scalar("long long",type_kind::scalar,0x05u,8u),
        scalar("float",type_kind::scalar,0x04u,4u), scalar("double",type_kind::scalar,0x04u,8u),
        scalar("bool",type_kind::scalar,0x02u,1u), scalar("pointer",type_kind::pointer,0u,8u),
        scalar("unknown",type_kind::scalar,0xffu,4u), scalar("half",type_kind::scalar,0x04u,2u)};
    ::std::array<copied_numeric_local, 4u> locals{
        local(0x7fu, ::std::uint32_t{0x89abcdfeu}), local(0x7eu, ::std::uint64_t{0x8000000000000001u}),
        local(0x7du, ::std::uint32_t{0x7fc01234u}), local(0x7cu, ::std::uint64_t{0x7ff8000012345678u})};
    ::std::vector<variable_record> variables{variable(0u, 0u), variable(1u, 0u), variable(2u, 0u), variable(3u, 1u),
        variable(4u, 2u, 3u), variable(5u, 3u), variable(6u, 0u), variable(7u, 1u), variable(8u, 0u), variable(9u, 2u)};
    ::std::vector<numeric_variable> values{};
    auto query{[&](::std::uint64_t pc = 15u, numeric_query_limits const& cap = {})
    { return query_numeric_variables(scopes, types, variables, pc, locals, locals.size(), values, cap); }};
    check(query() == inline_query_error::none && values.size() == 10u, "actual concrete inline chain and owned outputs");
    check(values[0].bits == 254u && numeric_signed_value(values[0]) == -2, "i8 from complete i32 carrier; portable BE low bits");
    check(values[1].bits == 0xcdfeu && numeric_signed_value(values[1]) == -12802, "i16 sign extension from i32");
    check(values[2].bits == 254u && values[2].kind == numeric_kind::unsigned_integer, "unsigned source width");
    check(values[3].bits == 0x8000000000000001u && numeric_signed_value(values[3]) == -9223372036854775807LL, "full i64 signed bits");
    check(values[4].kind == numeric_kind::f32_bits && values[4].bits == 0x7fc01234u &&
          values[5].kind == numeric_kind::f64_bits && values[5].bits == 0x7ff8000012345678u, "float NaN payload preserved, no integer guessing");
    check(values[6].kind == numeric_kind::boolean && values[6].bits == 254u, "boolean explicit DW_ATE");
    for(::std::size_t i{7u}; i != values.size(); ++i)
    { check(values[i].kind == numeric_kind::unavailable && values[i].reason == numeric_unavailable_reason::unsupported_type, "pointer/unknown/half not inferred"); }
    locals[0u].available = false;
    check(query() == inline_query_error::none && values[0u].reason == numeric_unavailable_reason::local_unavailable && values[0u].kind == numeric_kind::unavailable &&
          values[3u].reason == numeric_unavailable_reason::none && values[3u].bits == 0x8000000000000001u,
          "unavailable original slot never displays stale bits; later original indices remain valid");
    locals[0u].available = true;
    variables = {variable(3u, 0u), variable(4u, 0u), variable(0u, 2u), variable(0u, 0x100000000ULL)};
    check(query() == inline_query_error::none, "unsupported carriers report per-variable unavailable");
    for(::std::size_t i{}; i != 3u; ++i) { check(values[i].reason == numeric_unavailable_reason::carrier_mismatch, "no incompatible carrier reinterpretation"); }
    check(values[3].reason == numeric_unavailable_reason::local_not_captured, "no >u32 index or arbitrary local lookup");
    variables = {variable(0u, 4u)};
    check(query() == inline_query_error::none && values[0].reason == numeric_unavailable_reason::local_not_captured, "capture limit excludes remaining locals");
    variables = {variable(0u, 0u)}; locals[0].wasm_type = 0x6fu;
    check(query() == inline_query_error::none && values[0].reason == numeric_unavailable_reason::carrier_mismatch, "reference is never an address/value source");
    locals[0] = local(0x7fu, ::std::uint32_t{0x89abcdfeu});
    ::std::array<::std::byte, 7u> integer_expr{::std::byte{0x10u}, ::std::byte{0x81u}, ::std::byte{0x80u},
        ::std::byte{0x80u}, ::std::byte{0x80u}, ::std::byte{0x10u}, ::std::byte{0x9fu}};
    variables = {variable(3u, 0u)}; variables[0].locations[0].plan = decode_location_plan(integer_expr, 4u);
    check(query() == inline_query_error::none && values[0].bits == 4294967297ULL, "integer const is not truncated to Wasm32 address width");
    ::std::array<::std::byte, 6u> implicit{::std::byte{0x9eu}, ::std::byte{4u}, ::std::byte{0u},
        ::std::byte{0u}, ::std::byte{0x80u}, ::std::byte{0x3fu}};
    variables = {variable(4u, 0u)}; variables[0].locations[0].plan = decode_location_plan(implicit, 4u);
    check(query() == inline_query_error::none && values[0].bits == 0x3f800000u, "DWARF implicit float decoded little-endian on every host");
    variables[0].type = 1u;
    check(query() == inline_query_error::none && values[0].reason == numeric_unavailable_reason::implicit_width_mismatch, "implicit width must exactly match declared scalar");
    variables = {variable(4u, 0u)}; variables[0].locations[0].plan = decode_location_plan(integer_expr, 4u);
    check(query() == inline_query_error::none && values[0].reason == numeric_unavailable_reason::unsupported_plan, "integer expression does not imply float conversion");
    variables = {variable(0u, 0u)}; variables[0].locations[0].plan.kind = plan_kind::frame_relative_offset;
    check(query() == inline_query_error::none && values[0].reason == numeric_unavailable_reason::unsupported_plan, "fbreg is metadata, no memory read");
    variables[0].locations.clear();
    check(query() == inline_query_error::none && values[0].reason == numeric_unavailable_reason::no_location, "no abstract-origin location guess");
    location_plan fallback{}; fallback.kind = plan_kind::constant_value; fallback.constant_bits = 2u;
    location_plan explicit_plan{fallback}; explicit_plan.constant_bits = 1u;
    variables[0].locations = {{{}, fallback}, {code_range{14u, 17u}, explicit_plan}};
    check(query() == inline_query_error::none && values[0].bits == 1u, "explicit location takes precedence over default");
    check(query(17u) == inline_query_error::none && values[0].bits == 2u, "half-open end uses default");
    variables[0].locations.erase(variables[0].locations.begin());
    check(query(17u) == inline_query_error::none && values[0].reason == numeric_unavailable_reason::inactive_location, "inactive location not used");
    variables[0].locations.push_back({code_range{15u, 18u}, fallback});
    check(query() == inline_query_error::ambiguous && values.empty(), "overlapping active locations publish nothing");
    variables[0].locations = {{{}, fallback}, {{}, explicit_plan}};
    check(query() == inline_query_error::ambiguous && values.empty(), "multiple defaults rejected");
    variables = {variable(0u, 0u), variable(0u, 0u, 3u)}; scopes[2].ranges = {{24u, 26u}};
    check(query() == inline_query_error::none && values.size() == 1u, "inactive lexical parent excludes inline variable");
    scopes = fixture(); scopes[2].own_ranges_declared = true;
    check(query() == inline_query_error::none && values.size() == 1u, "explicit empty lexical range excludes its inline values");
    scopes[2].own_ranges_declared = false;
    check(query() == inline_query_error::none && values.size() == 2u, "absent lexical range still inherits actual parent");
    scopes = fixture(); check(query(35u) == inline_query_error::unavailable && values.empty(), "no guessed caller frame");
    variables.push_back(variable(100u, 0u));
    check(query() == inline_query_error::malformed && values.empty(), "late bad type clears all results");
    variables.back().type = 0u; variables.back().scope = 100u;
    check(query() == inline_query_error::malformed && values.empty(), "late bad scope clears all results");
    variables = {variable(0u, 0u), variable(0u, 0u, 3u)};
    numeric_query_limits cap{};
    cap.max_results = 1u; check(query(15u, cap) == inline_query_error::limit_exceeded && values.empty(), "result budget no partial output");
    cap = {}; cap.max_types = 1u; check(query(15u, cap) == inline_query_error::limit_exceeded, "type budget");
    cap = {}; cap.max_variables = 1u; check(query(15u, cap) == inline_query_error::limit_exceeded, "variable budget");
    cap = {}; cap.max_locations = 1u; check(query(15u, cap) == inline_query_error::limit_exceeded, "location budget");
    cap = {}; cap.max_captured_locals = 1u; check(query(15u, cap) == inline_query_error::limit_exceeded, "local snapshot budget");
    cap = {}; cap.max_metadata_string_bytes = 1u; check(query(15u, cap) == inline_query_error::limit_exceeded, "all metadata strings budget");
    cap = {}; cap.max_result_string_bytes = 1u; check(query(15u, cap) == inline_query_error::limit_exceeded, "owned result strings budget");
    check(query_numeric_variables(scopes, types, variables, 15u, locals, 1u, values) == inline_query_error::malformed && values.empty(), "captured count cannot exceed actual total");
    check(numeric_mask(0u) == 0u && numeric_mask(9u) == 0u, "public width helpers never shift out of range");
    // Portable finite-transform DATA controls. Actual CU/DIE resolution is
    // covered by debug_source_dwarf_integer_transforms and genuine O1 guests.
    {
        ::std::array<::std::byte,7u> expression{::std::byte{0xed},::std::byte{0},::std::byte{0},
            ::std::byte{0x10},::std::byte{1},::std::byte{0x1a},::std::byte{0x9f}};
        auto const masked{decode_location_plan(expression,4u)};
        for(::std::uint32_t low{};low!=256u;++low)
        {
            ::std::array<copied_numeric_local,1u> copied{local(0x7fu,0x89abcd00u|low)};
            numeric_variable value{};value_details::copy_value(masked,types[6],copied,1u,value);
            check(value.kind==numeric_kind::boolean&&value.bits==(low&1u),"mask evaluates actual whole native i32 carrier on every byte order");
        }
        type_record unsigned_type{};unsigned_type.kind=type_kind::scalar;unsigned_type.encoding=7u;unsigned_type.byte_count=8u;
        ::std::array<copied_numeric_local,1u> copied{local(0x7eu,~::std::uint64_t{0})};
        for(unsigned width{1u};width!=65u;++width)
        {
            auto converted{masked};converted.integer_transforms[0].kind=integer_transform_kind::unsigned_convert;
            converted.integer_transforms[0].resolved=true;converted.integer_transforms[0].bit_width=static_cast<::std::uint8_t>(width);
            numeric_variable value{};value_details::copy_value(converted,unsigned_type,copied,1u,value);
            check(value.kind==numeric_kind::unsigned_integer&&value.bits==(width==64u?~::std::uint64_t{0}:(::std::uint64_t{1u}<<width)-1u),
                "bounded unsigned conversion preserves logical low bits on actual target ABI");
            converted.integer_transforms[0].resolved=false;value={};value_details::copy_value(converted,unsigned_type,copied,1u,value);
            check(value.kind==numeric_kind::unavailable,"unresolved conversion cannot expose a value");
        }
    }
    ::fast_io::io::println("debug_source_dwarf_values: PASS finite direct numeric copies, scope/location/type rejection and budgets");
}
