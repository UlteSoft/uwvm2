// Finite pure metadata candidate. No caller capture, product commands or real
// producer qualification is inferred from these hand-built scope records.
#include "candidates/language_inline_frame_view.h.proposed"
#include <fast_io.h>
namespace candidate = ::uwvm2::uwvm::debugger::language_frame_candidate;
namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, ::std::string_view message)
{ if(!value) { ::fast_io::io::perrln("language_inline_frame_candidate: ", message); ::fast_io::fast_terminate(); } }
int main()
{
    ::std::vector<dwarf::scope_record> scopes(4u);
    // [owned four scope records 0,1,2,3] end
    // [safe                           ] all fixed indices below precede end.
    scopes[0u].kind = dwarf::scope_kind::compile_unit; scopes[0u].identity = {0u, 0u};
    scopes[1u].kind = dwarf::scope_kind::subprogram; scopes[1u].identity = {0u, 10u}; scopes[1u].parent = 0u;
    scopes[1u].concrete = true; scopes[1u].ranges.push_back({1u, 40u}); scopes[1u].name = ::fast_io::concat_std("outer");
    scopes[2u].kind = dwarf::scope_kind::inline_subprogram; scopes[2u].identity = {0u, 20u}; scopes[2u].parent = 1u;
    scopes[2u].concrete = true; scopes[2u].ranges.push_back({5u, 25u}); scopes[2u].name = ::fast_io::concat_std("middle");
    scopes[3u].kind = dwarf::scope_kind::inline_subprogram; scopes[3u].identity = {0u, 30u}; scopes[3u].parent = 2u;
    scopes[3u].concrete = true; scopes[3u].ranges.push_back({10u, 15u}); scopes[3u].name = ::fast_io::concat_std("inner");
    ::std::vector<candidate::frame> frames{};
    check(candidate::current_frames(scopes, 12u, frames) == candidate::error::none && frames.size() == 3u,
          "one concrete physical+inline path at actual metadata PC");
    // [owned frames 0,1,2] end  [owned scopes 0,1,2,3] end
    // [safe             ]      [safe               ]
    //  ^^ size==3 was checked BEFORE these fixed frame subscripts; failed
    //  checks terminate and do not reach the following metadata borrows.
    check(frames[0u].identity == scopes[3u].identity && frames[1u].identity == scopes[2u].identity &&
          frames[2u].identity == scopes[1u].identity, "GDB-style frame0 is innermost concrete inline instance");
    ::std::size_t selected{};
    check(candidate::move_up(frames, selected, 2u) == candidate::error::none && selected == 2u,
          "up remains inside current physical function metadata path");
    check(candidate::move_up(frames, selected) == candidate::error::bounds && selected == 2u,
          "an unwind caller label is not another captured frame");
    check(candidate::move_down(frames, selected, (~::std::uint64_t{})) == candidate::error::bounds && selected == 2u,
          "large count fails before narrowing or subtracting");
    check(candidate::move_down(frames, selected, 2u) == candidate::error::none && selected == 0u,
          "down restores innermost inline metadata cursor");
    candidate::limits cap{}; cap.max_string_bytes = 2u;
    check(candidate::current_frames(scopes, 12u, frames, cap) == candidate::error::limit_exceeded && frames.empty(),
          "failed bounded name copy exposes no partial frame path");
    scopes[3u].parent = 3u;
    check(candidate::current_frames(scopes, 12u, frames) == candidate::error::malformed && frames.empty(),
          "cyclic parent rejected before borrowing a parent record");
    ::fast_io::io::println("PASS finite inline-frame candidate component; actual frame commands/capture separate");
}
