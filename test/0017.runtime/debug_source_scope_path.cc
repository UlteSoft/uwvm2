// Pure synthetic metadata selection only. No real ticket/source owner/frame
// is constructed and no runtime, Wasm guest or value access occurs in this unit.
#include <uwvm2/uwvm/debugger/source_scope_path.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_scope_path: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static ::std::vector<scope_record> fixture()
{
    ::std::vector<scope_record> value(5u);
    for(::std::size_t i{}; i != value.size(); ++i) { value[i].identity = {10u, 100u + i}; }
    value[0].kind = scope_kind::compile_unit;
    value[1].parent = 0u; value[1].kind = scope_kind::subprogram; value[1].concrete = true; value[1].ranges = {{10u, 30u}};
    value[2].parent = 1u; value[2].kind = scope_kind::lexical_block;
    value[3].parent = 2u; value[3].kind = scope_kind::inline_subprogram; value[3].concrete = true; value[3].ranges = {{12u, 22u}};
    value[4].parent = 3u; value[4].kind = scope_kind::inline_subprogram; value[4].concrete = true; value[4].ranges = {{14u, 18u}};
    value[3].name = value[4].name = "same_name"; // names are not identities.
    return value;
}
static void clean(concrete_scope_path const& out)
{ check(out.physical_and_inline.empty() && out.record_indices.empty(), "failure never emits partial identities/indices"); }
int main()
{
    auto scopes{fixture()}; concrete_scope_path out{};
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::none && out.physical_and_inline.size() == 3u &&
        out.physical_and_inline[0] == scopes[1].identity && out.physical_and_inline[1] == scopes[3].identity &&
        out.physical_and_inline[2] == scopes[4].identity && out.record_indices == ::std::vector<::std::size_t>{1u, 3u, 4u},
        "physical + outer-to-inner concrete keys despite same names");
    scopes[4].identity.offset = 999u;
    check(out.physical_and_inline[2].offset == 104u, "result owns key values");
    scopes = fixture();
    check(query_concrete_scope_path(scopes, 22u, out) == scope_path_error::none && out.record_indices.size() == 1u,
        "half-open inline end leaves physical key");
    check(query_concrete_scope_path(scopes, 35u, out) == scope_path_error::unavailable, "no fabricated caller physical scope"); clean(out);
    scopes[2].own_ranges_declared = true;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::none && out.record_indices.size() == 1u,
        "explicit empty lexical scope excludes its inline descendants");
    scopes[2].own_ranges_declared = false;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::none && out.record_indices.size() == 3u,
        "absent lexical range attributes inherit");
    scopes[2].ranges = {{24u, 26u}};
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::none && out.record_indices.size() == 1u,
        "explicit inactive lexical range excludes child");
    scopes[2].ranges = {{10u, 16u}};
    check(query_concrete_scope_path(scopes, 16u, out) == scope_path_error::none && out.record_indices.size() == 1u,
        "lexical range is also half-open");
    scopes = fixture(); scopes[4].parent = 1u;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::ambiguous, "overlapping inline siblings fail closed"); clean(out);
    scopes = fixture(); scopes[4].kind = scope_kind::subprogram;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::ambiguous, "overlapping physical scopes fail closed"); clean(out);
    scopes = fixture(); scopes[4].concrete = false;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::none && out.record_indices.size() == 2u,
        "abstract/descriptive scope never supplies concrete identity");
    scopes = fixture(); scopes[4].identity = scopes[1].identity;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::malformed, "duplicate selected concrete key rejected"); clean(out);
    scopes = fixture(); scopes[4].identity.unit = 11u; scopes[4].identity.offset = scopes[1].identity.offset;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::none && out.record_indices.size() == 3u,
        "unit identity distinguishes equal DIE offsets");
    scopes = fixture(); scopes[4].parent = 4u;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::malformed, "self parent rejected"); clean(out);
    scopes = fixture(); scopes[4].parent = 99u;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::malformed, "out-of-span parent rejected"); clean(out);
    scopes = fixture(); scopes[2].kind = static_cast<scope_kind>(99u);
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::malformed, "unknown scope enum rejected"); clean(out);
    scopes = fixture(); scopes[1].ranges = {{30u, 10u}};
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::malformed, "reversed range rejected before selection"); clean(out);
    scopes = fixture(); scopes[4].ranges = {{14u, 14u}};
    check(query_concrete_scope_path(scopes, 11u, out) == scope_path_error::malformed, "late malformed inactive record still rejects"); clean(out);
    scopes = fixture(); scope_path_limits cap{}; cap.max_keys = 2u;
    check(query_concrete_scope_path(scopes, 15u, out, cap) == scope_path_error::limit_exceeded, "physical counts toward total key budget"); clean(out);
    cap.max_keys = 0u;
    check(query_concrete_scope_path(scopes, 11u, out, cap) == scope_path_error::limit_exceeded, "zero key budget cannot emit physical key"); clean(out);
    cap = {}; cap.max_keys = 1u;
    check(query_concrete_scope_path(scopes, 22u, out, cap) == scope_path_error::none && out.record_indices.size() == 1u,
        "exact one-key physical budget");
    cap = {}; cap.max_scopes = 4u;
    check(query_concrete_scope_path(scopes, 15u, out, cap) == scope_path_error::limit_exceeded, "scope budget"); clean(out);
    cap = {}; cap.max_ranges = 2u;
    check(query_concrete_scope_path(scopes, 15u, out, cap) == scope_path_error::limit_exceeded, "sum ranges budget"); clean(out);
    cap = {}; cap.max_depth = 1u;
    check(query_concrete_scope_path(scopes, 15u, out, cap) == scope_path_error::limit_exceeded, "complete ancestry budget"); clean(out);
    ::std::size_t used{(::std::numeric_limits<::std::size_t>::max)() - 1u};
    check(!budget::charge(2u, (::std::numeric_limits<::std::size_t>::max)(), used) &&
        used == (::std::numeric_limits<::std::size_t>::max)() - 1u, "sum overflow rejected without changing budget");
    check(query_concrete_scope_path({}, 0u, out) == scope_path_error::unavailable, "empty metadata unavailable"); clean(out);
    ::fast_io::io::println("debug_source_scope_path: PASS synthetic bounded concrete DIE path; no runtime authority/integration");
}
