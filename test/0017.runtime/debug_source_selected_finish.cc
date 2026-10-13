// Bounded DATA-policy and shorthand grammar checks ONLY. Synthetic identities
// below are never authentic runtime captures or pause/source permissions.
#include <uwvm2/uwvm/debugger/source_step_policy.h>
#include <uwvm2/uwvm/debugger/console_aliases.h>
#include <fast_io.h>
#include <array>
#include <utility>
namespace policy = uwvm2::uwvm::debugger::source_step;
namespace aliases = uwvm2::uwvm::debugger::console_aliases;
namespace debugger = uwvm2::uwvm::debugger;
static void check(bool ok, char const* message)
{
    if(!ok) { ::fast_io::io::perrln("debug_source_selected_finish: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
struct frame { ::std::uint64_t incarnation{}, parent{}, continuation{}, module{}, function{}, function_generation{}, runtime_epoch{}; };
static policy::position label()
{
    policy::position value{};
    value.source_owner = ::std::make_shared<int>(1); // Inert DATA owner, never an actual source binding.
    value.module = 0u; value.function = 3u; value.runtime_epoch = 1u; value.function_generation = 1u;
    value.scope_path = {{1u, 30u}}; value.file = "selected.cpp"; value.line = 30u; value.column = 1u;
    value.physical_depth = 2u; value.trace = policy::trace_status::complete;
    value.status = policy::position_status::mapped; value.activation = policy::activation_relation::same;
    value.is_statement = true; return value;
}
int main()
{
    auto const shorthand{aliases::parse("fin", 17u, 29u)};
    check(aliases::uses_current_thread(" fin ") && !shorthand.current_thread_required &&
        shorthand.command.kind == debugger::console_command_kind::source_step &&
        shorthand.command.requested_step_thread == 17u && shorthand.command.source_policy == debugger::source_step_policy::out &&
        shorthand.command.source_selected_finish,
        "fin retains actual manager-selected thread grammar and source-out policy");
    check(aliases::parse("fin", 0u, 29u).current_thread_required && aliases::parse("fin", 17u, 0u).current_thread_required,
        "fin without a fresh stopped participant/stop is not an admitted command");
    auto const explicit_finish{aliases::parse("fin 17", 0u, 0u)};
    check(!explicit_finish.current_thread_required && explicit_finish.command.kind == debugger::console_command_kind::source_step &&
        explicit_finish.command.source_selected_finish && explicit_finish.command.requested_step_thread == 17u,
        "explicit fin THREAD retains selected policy while controller must authenticate the actual current stop");
    check(!debugger::parse_console_command("step source 17 out").source_selected_finish &&
        !aliases::parse("n", 17u, 29u).command.source_selected_finish && !aliases::parse("s", 17u, 29u).command.source_selected_finish,
        "ordinary source-out and DAP stepOut preserve innermost policy; next/step never inherit selected finish");
    check(aliases::parse("fin 0", 17u, 29u).command.kind == debugger::console_command_kind::invalid &&
        aliases::parse("fin 17 extra", 17u, 29u).command.kind == debugger::console_command_kind::invalid,
        "invalid explicit finish operands cannot silently redirect a selected frame");
    ::std::array<frame, 2u> selected{{{10u, 0u, 10u, 0u, 2u, 1u, 1u}, {20u, 10u, 20u, 0u, 3u, 1u, 1u}}};
    ::std::array<frame, 3u> leaf{{selected[0u], selected[1u], {30u, 20u, 30u, 0u, 4u, 1u, 1u}}};
    auto initial{label()}; policy::origin origin{};
    check(policy::prepare(initial, policy::policy::out, origin) == policy::error::none, "selected physical policy DATA setup");
    auto current{initial}; current.function = 4u; current.scope_path = {{1u, 40u}}; current.line = 40u;
    current.physical_depth = leaf.size();
    current.activation = policy::compare_event_chains(::std::span{::std::as_const(selected)}, ::std::span{::std::as_const(leaf)});
    check(current.activation == policy::activation_relation::deeper && policy::decide(origin, current).result == policy::action::keep_running,
        "finishing a selected caller does not finish merely because its current leaf changes line");
    current = initial; current.line = 31u;
    current.activation = policy::compare_event_chains(::std::span{::std::as_const(selected)}, ::std::span{::std::as_const(selected)});
    check(policy::decide(origin, current).result == policy::action::keep_running,
        "return from the leaf into the selected caller is not return from that caller");
    auto tail{selected}; tail.back().incarnation = 21u; tail.back().function = 5u;
    current = initial; current.function = 5u; current.scope_path = {{1u, 50u}}; current.line = 50u;
    current.activation = policy::compare_event_chains(::std::span{::std::as_const(selected)}, ::std::span{::std::as_const(tail)});
    check(current.activation == policy::activation_relation::tail_successor && policy::decide(origin, current).result == policy::action::keep_running,
        "physical finish follows a genuine tail continuation instead of guessed equal native depth");
    ::std::array<frame, 1u> ancestor{{selected.front()}};
    current = initial; current.function = 2u; current.scope_path = {{1u, 20u}}; current.line = 21u;
    current.physical_depth = ancestor.size();
    current.activation = policy::compare_event_chains(::std::span{::std::as_const(selected)}, ::std::span{::std::as_const(ancestor)});
    check(current.activation == policy::activation_relation::returned && policy::decide(origin, current).result == policy::action::stop,
        "only the new returned ancestor position supplies a physical finish stop label");
    current.is_statement = false;
    check(policy::decide(origin, current).result == policy::action::keep_running,
        "nonstatement ancestor rows cannot manufacture a source finish stop");
    auto invalid{ancestor}; invalid.front().incarnation = 99u;
    current.activation = policy::compare_event_chains(::std::span{::std::as_const(selected)}, ::std::span{::std::as_const(invalid)});
    check(current.activation == policy::activation_relation::unknown && policy::decide(origin, current).result == policy::action::decline,
        "a different resumed root cannot be stitched into the selected caller's return episode");
    auto outer_inline{initial}; outer_inline.scope_path.push_back({1u, 31u});
    policy::origin inline_origin{};
    check(policy::prepare(outer_inline, policy::policy::out, inline_origin) == policy::error::none, "selected outer inline policy DATA setup");
    current = outer_inline; current.scope_path.push_back({1u, 32u}); current.line = 32u;
    check(policy::decide(inline_origin, current).result == policy::action::keep_running,
        "finishing an outer inline frame skips its concrete nested inline child");
    current = initial; current.line = 33u;
    check(policy::decide(inline_origin, current).result == policy::action::stop,
        "outer inline exit uses concrete path identity while keeping the genuine physical activation");
    ::fast_io::io::println("debug_source_selected_finish: PASS bounded DATA-only selected caller/inline policy and fin grammar; no runtime authority");
}
