#include <uwvm2/uwvm/debugger/source_dwarf_objects.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool ok, char const* message)
{ if(!ok) { ::fast_io::io::perrln("debug_source_dwarf_globals: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
int main()
{
    ::std::array<::std::byte, 5u> const addr{::std::byte{3u}, ::std::byte{16u}, {}, {}, {}};
    auto const absolute{decode_location_plan(addr, 4u)}; ::std::uint64_t offset{};
    check(absolute.kind == plan_kind::absolute_guest_offset && resolve_absolute_guest_offset(absolute, offset) == object_location_error::none && offset == 16u,
          "DW_OP_addr becomes only a guest offset");
    check(decode_location_plan(::std::span<::std::byte const>{addr}.first(4u), 4u).reason == unavailable_reason::malformed_expression, "short DW_OP_addr bounded before fixed-width read");
    auto bad{absolute}; bad.constant_bits = ::std::uint64_t{1u} << 32u;
    check(resolve_absolute_guest_offset(bad, offset) == object_location_error::address_overflow && offset == 0u, "Wasm32 cannot truncate absolute offset");
    ::std::vector<scope_record> scopes(4u); scopes[0u].kind = scope_kind::compile_unit; scopes[0u].identity = {0u, 1u};
    scopes[1u].kind = scope_kind::subprogram; scopes[1u].identity = {0u, 2u}; scopes[1u].parent = 0u; scopes[1u].concrete = true; scopes[1u].ranges.push_back({10u, 20u});
    scopes[2u].kind = scope_kind::lexical_block; scopes[2u].identity = {0u, 3u}; scopes[2u].parent = 1u;
    scopes[3u].kind = scope_kind::compile_unit; scopes[3u].identity = {100u, 101u};
    ::std::vector<type_record> types(1u); types[0u].kind = type_kind::scalar; types[0u].encoding = 5u; types[0u].byte_count = 4u;
    variable_record global{}; global.identity = {0u, 10u}; global.scope = 0u; global.type = 0u; global.name = ::fast_io::concat_std("value");
    global.qualified_name = ::fast_io::concat_std("outer::value"); global.global = true; global.static_storage = true; global.locations.push_back({{}, absolute});
    variable_record local{}; local.identity = {0u, 11u}; local.scope = 2u; local.type = 0u; local.name = ::fast_io::concat_std("value");
    ::std::vector<variable_record> variables{global, local}; variable_selection selection{};
    check(query_named_variable(scopes, types, variables, 12u, "value", selection) == inline_query_error::none && selection.identity == local.identity && !selection.global,
          "active lexical local shadows global");
    check(query_named_variable(scopes, types, variables, 12u, "outer::value", selection) == inline_query_error::none && selection.global && selection.static_storage &&
          selection.physical_scope == 1u && selection.location.kind == plan_kind::absolute_guest_offset, "qualified global still binds actual physical stop CU");
    variables.pop_back();
    auto other{global}; other.scope = 3u; other.identity = {100u, 110u}; other.qualified_name = ::fast_io::concat_std("other::value"); variables.push_back(other);
    check(query_named_variable(scopes, types, variables, 12u, "value", selection) == inline_query_error::none && selection.identity == global.identity, "current CU file static has priority");
    check(query_named_variable(scopes, types, variables, 12u, "other::value", selection) == inline_query_error::unavailable, "file static in unrelated CU is not guessed");
    variables[1u].external = true;
    check(query_named_variable(scopes, types, variables, 12u, "other::value", selection) == inline_query_error::none && selection.identity == other.identity, "explicit unique external definition can be named across CUs");
    variables[1u].declaration = true;
    check(query_named_variable(scopes, types, variables, 12u, "other::value", selection) == inline_query_error::unavailable, "declaration-only DIE is never a value definition");
    auto duplicate{global}; duplicate.identity.offset = 12u; variables.push_back(duplicate);
    check(query_named_variable(scopes, types, variables, 12u, "outer::value", selection) == inline_query_error::ambiguous, "ambiguous actual CU globals rejected");
    check(query_named_variable(scopes, types, variables, 20u, "value", selection) == inline_query_error::unavailable, "global query cannot fabricate a physical source stop");
    ::std::vector<numeric_variable> locals{};
    check(query_numeric_variables(scopes, types, variables, 12u, {}, 0u, locals) == inline_query_error::none && locals.empty(), "globals never appear as copied local numeric slots");
    ::fast_io::io::println("PASS bounded global/static metadata and absolute guest-offset component");
}
