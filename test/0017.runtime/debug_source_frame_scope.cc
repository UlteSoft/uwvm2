// Finite production metadata only: hand-built scopes do not qualify real producer values.
#include <uwvm2/uwvm/debugger/source_frames.h>
#include <fast_io.h>
namespace candidate = ::uwvm2::uwvm::debugger::source_frame_variables;
namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, ::std::string_view message)
{ if(!value) { ::fast_io::io::perrln("language_selected_scope_candidate: ", message); ::fast_io::fast_terminate(); } }
int main()
{
    ::std::vector<dwarf::scope_record> scopes(7u);
    // [owned seven scopes: 0..6] end
    // [safe                    ] every fixed index below is <7 BEFORE borrow.
    scopes[0u].kind = dwarf::scope_kind::compile_unit; scopes[0u].identity = {0u, 0u};
    scopes[1u].kind = dwarf::scope_kind::subprogram; scopes[1u].parent = 0u; scopes[1u].concrete = true;
    scopes[1u].identity = {0u, 10u}; scopes[1u].ranges.push_back({1u, 40u});
    scopes[2u].kind = dwarf::scope_kind::inline_subprogram; scopes[2u].parent = 1u; scopes[2u].concrete = true;
    scopes[2u].identity = {0u, 20u}; scopes[2u].ranges.push_back({5u, 25u});
    scopes[3u].kind = dwarf::scope_kind::lexical_block; scopes[3u].parent = 2u; scopes[3u].identity = {0u, 30u};
    scopes[3u].ranges.push_back({8u, 22u}); scopes[3u].own_ranges_declared = true;
    scopes[4u].kind = dwarf::scope_kind::inline_subprogram; scopes[4u].parent = 3u; scopes[4u].concrete = true;
    scopes[4u].identity = {0u, 40u}; scopes[4u].ranges.push_back({10u, 15u});
    scopes[5u].kind = dwarf::scope_kind::lexical_block; scopes[5u].parent = 4u; scopes[5u].identity = {0u, 50u};
    scopes[5u].ranges.push_back({10u, 15u}); scopes[5u].own_ranges_declared = true;
    scopes[6u].kind = dwarf::scope_kind::lexical_block; scopes[6u].parent = 1u; scopes[6u].identity = {0u, 60u};
    scopes[6u].own_ranges_declared = true; // empty explicit block cannot inherit physical activity.
    ::std::vector<dwarf::variable_record> variables{};
    for(::std::size_t scope{1u}; scope != 7u; ++scope)
    {
        dwarf::variable_record value{}; value.scope = scope; value.identity = {0u, 100u + scope};
        value.name = ::fast_io::concat_std("shadow"); variables.push_back(::std::move(value));
    }
    candidate::selection chosen{};
    check(candidate::named(scopes, variables, 0u, 12u, 0u, "shadow", chosen) == candidate::error::none && chosen.variable_index == 4u,
          "innermost selected inline frame sees its active lexical shadow");
    check(candidate::named(scopes, variables, 0u, 12u, 1u, "shadow", chosen) == candidate::error::none && chosen.variable_index == 2u,
          "outer inline frame sees its own lexical shadow, excluding deeper inline declarations");
    check(candidate::named(scopes, variables, 0u, 12u, 2u, "shadow", chosen) == candidate::error::none && chosen.variable_index == 0u,
          "physical selected frame excludes inline children and explicit empty lexical block");
    check(candidate::named(scopes, variables, 0u, 12u, (~::std::size_t{}), "shadow", chosen) == candidate::error::bounds &&
          chosen.variable_index == dwarf::no_record, "untrusted frame ordinal rejected before subscript with no partial selection");
    variables.push_back(variables[2u]);
    check(candidate::named(scopes, variables, 0u, 12u, 1u, "shadow", chosen) == candidate::error::ambiguous,
          "two active same-scope declarations never pick a guessed value");
    variables.pop_back(); variables[3u].identity = variables[2u].identity;
    check(candidate::named(scopes, variables, 0u, 12u, 1u, "shadow", chosen) == candidate::error::ambiguous,
          "duplicate chosen identity in another scope cannot alias a metadata selection");
    variables[3u].identity = {0u, 104u};
    // [owned seven scopes 0..6, six variables 0..5] end
    // [safe                                     ] fixed indices remain within original extents.
    scopes[6u].ranges.push_back({10u, 15u}); scopes[6u].parent = 2u;
    check(candidate::named(scopes, variables, 0u, 12u, 1u, "shadow", chosen) == candidate::error::ambiguous,
          "incomparable overlapping lexical branches cannot resolve by nesting depth");
    scopes[6u].ranges.clear(); scopes[6u].parent = 1u;
    // [owned six variables ... variable 0 ... end]
    // [safe                                    ] original size==6 BEFORE fixed borrow.
    variables[0u].qualified_name = ::fast_io::concat_std(::std::string_view{"physical::shadow"});
    check(candidate::named(scopes, variables, 0u, 12u, 2u, "physical::shadow", chosen) == candidate::error::none && chosen.variable_index == 0u,
          "qualified current-frame name retains the actual selected scope");
    check(candidate::named(scopes, variables, 0u, 12u, 0u, "physical::shadow", chosen) == candidate::error::unavailable,
          "qualified name cannot borrow another selected frame's local");
    candidate::limits cap{}; cap.max_string_bytes = 1u;
    check(candidate::named(scopes, variables, 0u, 12u, 1u, "shadow", chosen, cap) == candidate::error::limit_exceeded,
          "global metadata name budget applies before selection");
    ::fast_io::io::println("PASS finite selected-scope metadata; production activation/frame/value qualification separate");
}
