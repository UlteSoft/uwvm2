// Import-only consumer: no product header may conceal a missing export.
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
# include <llvm/BinaryFormat/Dwarf.h>
# include <llvm/DebugInfo/DWARF/DWARFFormValue.h>
#endif
import fast_io;
import uwvm2.uwvm.debugger;
namespace frames = ::uwvm2::uwvm::debugger::source_frames;
namespace scope = ::uwvm2::uwvm::debugger::source_frame_variables;
namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, ::std::string_view message)
{ if(!value) { ::fast_io::io::perrln("debug_source_frames_module: ", message); ::fast_io::fast_terminate(); } }
int main()
{
    ::std::vector<frames::frame> unavailable{};
    check(frames::current_frames({}, 0u, unavailable) == frames::error::unavailable && unavailable.empty(),
          "imported frame path refuses empty current source metadata");
    ::std::array<dwarf::scope_record, 4u> scopes{};
    scopes[0u].kind = dwarf::scope_kind::compile_unit; scopes[0u].identity = {1u, 1u};
    scopes[1u].kind = dwarf::scope_kind::subprogram; scopes[1u].identity = {1u, 2u};
    scopes[1u].parent = 0u; scopes[1u].concrete = true; scopes[1u].ranges.push_back({1u, 10u});
    scopes[1u].name = ::fast_io::concat_std(::std::string_view{"physical"});
    scopes[2u].kind = dwarf::scope_kind::inline_subprogram; scopes[2u].identity = {1u, 3u};
    scopes[2u].parent = 1u; scopes[2u].concrete = true; scopes[2u].ranges.push_back({2u, 9u});
    scopes[2u].name = ::fast_io::concat_std(::std::string_view{"outer inline"});
    scopes[3u].kind = dwarf::scope_kind::inline_subprogram; scopes[3u].identity = {1u, 4u};
    scopes[3u].parent = 2u; scopes[3u].concrete = true; scopes[3u].ranges.push_back({3u, 8u});
    scopes[3u].name = ::fast_io::concat_std(::std::string_view{"inner inline"});
    check(frames::current_frames(scopes, 5u, unavailable) == frames::error::none && unavailable.size() == 3u,
          "imported metadata path enumerates actual concrete inline ancestry");
    // [owned returned frame values ... 0/2 ... end]
    // [safe                                        ] size==3 proved BEFORE borrow.
    check(unavailable[0u].identity == scopes[3u].identity && unavailable[2u].identity == scopes[1u].identity,
          "frame zero inner, then outer inline, then current physical");
    ::std::size_t cursor{};
    check(frames::move_up(unavailable, cursor, 2u) == frames::error::none && cursor == 2u &&
          frames::move_up(unavailable, cursor, (~::std::uint64_t{})) == frames::error::bounds && cursor == 2u,
          "imported cursor refuses overflow without changing selection");
    ::std::array<dwarf::variable_record, 3u> variables{};
    for(::std::size_t i{}; i != variables.size(); ++i)
    {
        // [owned 3 records ... i ... end]
        // [safe                         ] i<size BEFORE record borrow.
        auto& variable{variables[i]}; variable.identity = {1u, 100u + i}; variable.scope = i + 1u; variable.type = 0u;
        variable.name = ::fast_io::concat_std(::std::string_view{"shadow"});
    }
    scope::selection selected{};
    check(scope::named(scopes, variables, 1u, 5u, 1u, "shadow", selected) == scope::error::none && selected.variable_index == 1u,
          "imported selected-frame variable excludes deeper inline shadow");
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
    namespace constants = ::uwvm2::uwvm::debugger::source_dwarf_constants;
    dwarf::type_record type{}; type.kind = dwarf::type_kind::scalar; type.size_known = true;
    type.encoding = ::llvm::dwarf::DW_ATE_signed; type.byte_count = 4u; type.byte_size = 4u;
    dwarf::location_plan plan{};
    auto const form{::llvm::DWARFFormValue::createFromSValue(::llvm::dwarf::DW_FORM_sdata, -31)};
    check(constants::decode(form, type, 4u, plan) == constants::error::none && plan.kind == dwarf::plan_kind::constant_value &&
          plan.constant_bits == static_cast<::std::uint64_t>(-31), "imported bounded integer constant has value semantics only");
#endif
    ::fast_io::io::println("PASS imported production frame/constant metadata components; real producer/controller qualification separate");
}
