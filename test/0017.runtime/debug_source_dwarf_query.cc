#include <uwvm2/uwvm/debugger/source_dwarf_query.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_dwarf_query: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static ::std::vector<scope_record> fixture()
{
    ::std::vector<scope_record> value(5u);
    for(::std::size_t i{}; i != value.size(); ++i) { value[i].identity = {10u, 100u + i}; }
    value[0].kind = scope_kind::compile_unit;
    value[1].parent = 0u; value[1].kind = scope_kind::subprogram; value[1].concrete = true; value[1].ranges = {{10u, 30u}};
    value[2].parent = 1u; value[2].kind = scope_kind::lexical_block;
    value[3].parent = 2u; value[3].kind = scope_kind::inline_subprogram; value[3].concrete = true;
    value[3].name = "outer_inline"; value[3].call_file = "alpha.c"; value[3].call_line = 7u; value[3].ranges = {{12u, 22u}};
    value[4].parent = 3u; value[4].kind = scope_kind::inline_subprogram; value[4].concrete = true;
    value[4].name = "inner_inline"; value[4].call_file = "beta.c"; value[4].call_line = 9u; value[4].ranges = {{14u, 18u}};
    return value;
}
int main()
{
    auto scopes{fixture()}; ::std::vector<inline_frame> frames{};
    check(query_inline_frames(scopes, 15u, frames) == inline_query_error::none && frames.size() == 2u &&
        frames[0].name == "inner_inline" && frames[1].name == "outer_inline" && frames[0].call_file == "beta.c", "concrete innermost-first chain");
    check(query_inline_frames(scopes, 22u, frames) == inline_query_error::none && frames.empty(), "half-open ranges and old output cleared");
    check(query_inline_frames(scopes, 35u, frames) == inline_query_error::unavailable && frames.empty(), "no guessed physical caller");
    scopes[2].ranges = {{24u, 26u}};
    check(query_inline_frames(scopes, 15u, frames) == inline_query_error::none && frames.empty(), "inactive explicit lexical ancestor");
    scopes = fixture(); scopes[2].own_ranges_declared = true;
    check(query_inline_frames(scopes, 15u, frames) == inline_query_error::none && frames.empty(), "explicit empty lexical range excludes child inline");
    scopes[2].own_ranges_declared = false;
    check(query_inline_frames(scopes, 15u, frames) == inline_query_error::none && frames.size() == 2u, "absent lexical ranges inherit parent");
    scopes = fixture(); scopes[4].parent = 1u;
    check(query_inline_frames(scopes, 15u, frames) == inline_query_error::ambiguous && frames.empty(), "overlapping siblings fail closed");
    scopes = fixture(); scopes[4].concrete = false;
    check(query_inline_frames(scopes, 15u, frames) == inline_query_error::none && frames.size() == 1u, "abstract origin never supplies ranges");
    scopes = fixture(); scopes[4].parent = 4u;
    check(query_inline_frames(scopes, 15u, frames) == inline_query_error::malformed && frames.empty(), "parent cycle rejected");
    scopes = fixture(); scopes[1].ranges = {{30u, 10u}};
    check(query_inline_frames(scopes, 15u, frames) == inline_query_error::malformed, "malformed range rejected");
    scopes = fixture(); inline_query_limits cap{}; cap.max_frames = 1u;
    check(query_inline_frames(scopes, 15u, frames, cap) == inline_query_error::limit_exceeded && frames.empty(), "no partial chain after frame limit");
    cap = {}; cap.max_string_bytes = 1u;
    check(query_inline_frames(scopes, 15u, frames, cap) == inline_query_error::limit_exceeded && frames.empty(), "copied string budget");
    cap = {}; cap.max_scopes = 1u;
    check(query_inline_frames(scopes, 15u, frames, cap) == inline_query_error::limit_exceeded, "scope budget");
    cap = {}; cap.max_ranges = 1u;
    check(query_inline_frames(scopes, 15u, frames, cap) == inline_query_error::limit_exceeded, "range budget");
    cap = {}; cap.max_depth = 1u;
    check(query_inline_frames(scopes, 15u, frames, cap) == inline_query_error::limit_exceeded && frames.empty(), "ancestry depth budget");
    scopes = fixture(); cap = {}; cap.max_frames = 0u;
    check(query_inline_frames(scopes, 22u, frames, cap) == inline_query_error::none && frames.empty(), "physical-only query has zero inline frames");
    check(query_inline_frames(scopes, 15u, frames, cap) == inline_query_error::limit_exceeded && frames.empty(), "zero frame budget refuses inline chain");
    cap = {}; cap.max_frames = (::std::numeric_limits<::std::size_t>::max)();
    check(query_inline_frames(scopes, 22u, frames, cap) == inline_query_error::limit_exceeded && frames.empty(), "physical-key budget addition cannot overflow");
    cap = {}; scopes[4].identity = scopes[1].identity;
    check(query_inline_frames(scopes, 15u, frames, cap) == inline_query_error::malformed && frames.empty(), "display also rejects duplicate concrete identity");
    scopes = fixture(); scopes[2].kind = static_cast<scope_kind>(99u);
    check(query_inline_frames(scopes, 15u, frames, cap) == inline_query_error::malformed && frames.empty(), "display uses selector enum validation");
    ::fast_io::io::println("debug_source_dwarf_query: PASS");
}
