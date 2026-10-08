// Synthetic POLICY semantics only. Shared owners/relations below are fixtures,
// NOT authentic runtime source bindings, pause tickets or activation witnesses.
// No runtime/guest execution or reading a source/native frame occurs here.
#include <uwvm2/uwvm/debugger/source_step_policy.h>
#include <uwvm2/uwvm/debugger/source_dwarf_query.h>
#include <fast_io.h>
namespace step = uwvm2::uwvm::debugger::source_step;
namespace dwarf = uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_step_policy: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static step::position fixture()
{
    step::position value{};
    value.source_owner = ::std::make_shared<int>(1); // inert identity token only; never source/read authority.
    value.module = 1u; value.function = 2u; value.runtime_epoch = 7u; value.function_generation = 5u;
    value.scope_path = {{10u, 100u}}; value.file = "scope.cpp"; value.line = 10u; value.column = 1u;
    value.is_statement = true; value.physical_depth = 3u; value.status = step::position_status::mapped;
    value.trace = step::trace_status::complete; value.activation = step::activation_relation::same; return value;
}
static step::origin prepare(step::position const& value, step::policy policy)
{ step::origin out{}; check(step::prepare(value, policy, out) == step::error::none, "bounded synthetic policy setup"); return out; }
static void expect(step::origin const& origin, step::position const& current, step::action result, step::error reason)
{ auto out{step::decide(origin, current)}; check(out.result == result && out.reason == reason, "precise step decision");
  if(result != step::action::stop) { check(out.stopped_path.empty(), "decline/continue never publishes partial stop tokens"); } }
int main()
{
    auto base{fixture()}; auto into{prepare(base, step::policy::into)}; auto over{prepare(base, step::policy::over)};
    auto out{prepare(base, step::policy::out)}; auto current{base};
    expect(into, current, step::action::keep_running, step::error::none);
    current.line = 11u; expect(into, current, step::action::stop, step::error::none);
    expect(over, current, step::action::stop, step::error::none); expect(out, current, step::action::keep_running, step::error::none);
    current = base; current.column = 2u; expect(into, current, step::action::stop, step::error::none);
    current = base; current.discriminator = 1u; expect(into, current, step::action::stop, step::error::none);
    expect(over, current, step::action::stop, step::error::none);
    current.is_statement = false; expect(into, current, step::action::keep_running, step::error::none);
    expect(over, current, step::action::keep_running, step::error::none);
    auto inline_origin{base}; inline_origin.scope_path.push_back({10u, 200u});
    auto inline_over{prepare(inline_origin, step::policy::over)}, inline_out{prepare(inline_origin, step::policy::out)};
    current = inline_origin; current.scope_path.push_back({10u, 300u}); current.line = 99u;
    expect(inline_over, current, step::action::keep_running, step::error::none);
    expect(inline_out, current, step::action::keep_running, step::error::none);
    expect(into, current, step::action::stop, step::error::none);
    current = inline_origin; current.line = 11u;
    expect(inline_over, current, step::action::stop, step::error::none);
    expect(inline_out, current, step::action::keep_running, step::error::none);
    current = base; // true inline exit, unchanged physical depth and source line.
    expect(inline_over, current, step::action::stop, step::error::none);
    expect(inline_out, current, step::action::stop, step::error::none);
    current.is_statement = false; expect(inline_out, current, step::action::keep_running, step::error::none);
    current = inline_origin; current.scope_path.back() = {10u, 201u};
    // Identical function/source names would not make two concrete DIE instances
    // the same inline activation. The component deliberately takes no names.
    expect(inline_out, current, step::action::stop, step::error::none);
    expect(inline_over, current, step::action::stop, step::error::none);
    current = base; current.physical_depth = 4u; current.activation = step::activation_relation::deeper;
    current.line = 99u; expect(over, current, step::action::keep_running, step::error::none);
    expect(out, current, step::action::keep_running, step::error::none); // genuine recursive physical depth, same DIE.
    current.status = step::position_status::unmapped; current.scope_path.clear();
    expect(over, current, step::action::keep_running, step::error::none); // proven deeper call, no guessed caller source.
    current = base; current.physical_depth = 2u; current.activation = step::activation_relation::returned;
    current.function = 1u; current.function_generation = 999u; current.scope_path = {{10u, 50u}};
    expect(out, current, step::action::stop, step::error::none);
    expect(inline_out, current, step::action::stop, step::error::none); // caller generation is per-function, need not equal 5.
    current.module = 2u; current.source_owner = ::std::make_shared<int>(2);
    expect(out, current, step::action::stop, step::error::none); // a separately authenticated caller would own a different module.
    current = base; current.activation = step::activation_relation::unknown;
    expect(over, current, step::action::decline, step::error::unknown_activation);
    expect(out, current, step::action::decline, step::error::unknown_activation);
    current = base; current.activation = step::activation_relation::returned;
    expect(over, current, step::action::decline, step::error::unknown_activation); // same depth cannot be guessed as returned.
    current = base; current.physical_depth = 4u;
    expect(over, current, step::action::decline, step::error::inconsistent_activation);
    current = base; current.function = 3u;
    expect(over, current, step::action::decline, step::error::inconsistent_activation);
    current = base; current.trace = step::trace_status::missing;
    expect(over, current, step::action::decline, step::error::missing_trace);
    current.trace = step::trace_status::truncated;
    expect(over, current, step::action::decline, step::error::truncated_trace);
    expect(into, current, step::action::decline, step::error::truncated_trace);
    current = base; current.status = step::position_status::native;
    expect(into, current, step::action::decline, step::error::native_position);
    expect(out, current, step::action::decline, step::error::native_position);
    current = base; ++current.function_generation;
    expect(over, current, step::action::decline, step::error::stale_generation);
    current = base; ++current.runtime_epoch;
    expect(into, current, step::action::decline, step::error::stale_generation);
    current = base; auto foreign{::std::make_shared<int>(3)};
    current.source_owner = ::std::shared_ptr<void const>{foreign, base.source_owner.get()}; // foreign CONTROL BLOCK, not a valid source pin.
    expect(into, current, step::action::decline, step::error::stale_generation);
    current.function = 1u; current.physical_depth = 2u; current.activation = step::activation_relation::returned;
    expect(out, current, step::action::decline, step::error::stale_generation); // same module still requires same binding owner.
    current = base; current.status = step::position_status::ambiguous;
    expect(over, current, step::action::decline, step::error::ambiguous);
    current = base; current.status = step::position_status::unmapped;
    expect(into, current, step::action::decline, step::error::unmapped);
    // An inert fixture token models only the metadata precondition. Actual
    // runtime/activation/source permission must still come from a joint query.
    current.metadata_ready = true; current.scope_path.clear(); current.file.clear(); current.line = 0u;
    expect(into, current, step::action::keep_running, step::error::none);
    expect(over, current, step::action::keep_running, step::error::none);
    expect(out, current, step::action::keep_running, step::error::none);
    current.function = 3u;
    expect(into, current, step::action::decline, step::error::inconsistent_activation);
    current.activation = step::activation_relation::deeper; current.physical_depth = 4u;
    expect(into, current, step::action::keep_running, step::error::none);
    expect(over, current, step::action::keep_running, step::error::none);
    expect(out, current, step::action::keep_running, step::error::none);
    current.physical_depth = 3u;
    expect(into, current, step::action::decline, step::error::inconsistent_activation);
    current.activation = step::activation_relation::tail_successor;
    expect(into, current, step::action::keep_running, step::error::none);
    expect(over, current, step::action::keep_running, step::error::none);
    expect(out, current, step::action::keep_running, step::error::none);
    current.activation = step::activation_relation::returned; current.physical_depth = 2u;
    expect(into, current, step::action::keep_running, step::error::none);
    expect(over, current, step::action::keep_running, step::error::none);
    expect(out, current, step::action::keep_running, step::error::none);
    current.activation = step::activation_relation::unknown;
    expect(into, current, step::action::decline, step::error::unmapped);
    expect(over, current, step::action::decline, step::error::unknown_activation);
    current.activation = step::activation_relation::returned; current.metadata_ready = false;
    expect(into, current, step::action::decline, step::error::unmapped);
    current.metadata_ready = true; current.trace = step::trace_status::missing;
    expect(into, current, step::action::decline, step::error::unmapped);
    current.trace = step::trace_status::complete; current.source_owner.reset();
    expect(into, current, step::action::decline, step::error::unmapped);
    current.source_owner = base.source_owner; current.status = step::position_status::ambiguous;
    expect(into, current, step::action::decline, step::error::ambiguous);
    current.status = step::position_status::native;
    expect(into, current, step::action::decline, step::error::native_position);
    current.status = step::position_status::unmapped; ++current.runtime_epoch;
    expect(into, current, step::action::decline, step::error::stale_generation);
    current = base; current.scope_path.clear();
    expect(over, current, step::action::decline, step::error::malformed_path);
    current = base; current.scope_path.push_back(current.scope_path.front());
    expect(over, current, step::action::decline, step::error::malformed_path);
    current = base; current.activation = step::activation_relation::tail_successor;
    expect(into, current, step::action::stop, step::error::none);
    expect(over, current, step::action::keep_running, step::error::none);
    expect(out, current, step::action::keep_running, step::error::none);
    current.function = 3u; current.scope_path = {{10u, 999u}}; current.line = 99u;
    expect(over, current, step::action::keep_running, step::error::none);
    expect(out, current, step::action::keep_running, step::error::none);
    struct event
    { ::std::uint64_t incarnation{}, parent{}, continuation{}, module{}, function{}, function_generation{}, runtime_epoch{}; };
    ::std::array<event, 2u> initial{{{1u, 0u, 1u, 0u, 1u, 1u, 9u}, {2u, 1u, 2u, 0u, 2u, 1u, 9u}}};
    auto equal{initial};
    check(step::compare_event_chains(::std::span{::std::as_const(initial)}, ::std::span{::std::as_const(equal)}) ==
        step::activation_relation::same, "same genuine-chain metadata structure");
    ::std::array<event, 3u> recursive{{initial[0u], initial[1u], {3u, 2u, 3u, 0u, 2u, 1u, 9u}}};
    check(step::compare_event_chains(::std::span{::std::as_const(initial)}, ::std::span{::std::as_const(recursive)}) ==
        step::activation_relation::deeper, "recursive same function has distinct incarnation");
    auto tail{initial}; tail[1u].incarnation = 4u; tail[1u].function = 3u;
    check(step::compare_event_chains(::std::span{::std::as_const(initial)}, ::std::span{::std::as_const(tail)}) ==
        step::activation_relation::tail_successor, "typed tail preserves continuation without old incarnation");
    recursive[1u] = tail[1u]; recursive[2u].parent = 4u; recursive[2u].incarnation = recursive[2u].continuation = 5u;
    check(step::compare_event_chains(::std::span{::std::as_const(initial)}, ::std::span{::std::as_const(recursive)}) ==
        step::activation_relation::deeper, "descendant of the tail continuation stays deeper");
    ::std::array<event, 1u> caller{{initial[0u]}};
    check(step::compare_event_chains(::std::span{::std::as_const(initial)}, ::std::span{::std::as_const(caller)}) ==
        step::activation_relation::returned, "actual common caller prefix after return");
    tail[1u].continuation = 4u;
    check(step::compare_event_chains(::std::span{::std::as_const(initial)}, ::std::span{::std::as_const(tail)}) ==
        step::activation_relation::unknown, "same depth/function/CFA cannot prove reused identity");
    equal[1u].function_generation = 2u;
    check(step::compare_event_chains(::std::span{::std::as_const(initial)}, ::std::span{::std::as_const(equal)}) ==
        step::activation_relation::unknown, "same incarnation cannot conceal another code generation");
    equal = initial; equal[1u].parent = 99u;
    check(step::compare_event_chains(::std::span{::std::as_const(initial)}, ::std::span{::std::as_const(equal)}) ==
        step::activation_relation::unknown, "broken parent chain rejected");
    equal = initial; equal[1u].runtime_epoch = 10u;
    check(step::compare_event_chains(::std::span{::std::as_const(initial)}, ::std::span{::std::as_const(equal)}) ==
        step::activation_relation::unknown, "mixed runtime epoch rejected");
    check(step::compare_event_chains(::std::span{::std::as_const(initial)}, ::std::span{::std::as_const(initial)}, 1u) ==
        step::activation_relation::unknown, "bounded chain comparison");
    // Explicit empty lexical scope excludes children in the shared selector.
    // Distinct synthetic DIE keys model actual separate records; zero keys for
    // both physical and inline scopes would be an invalid duplicate identity.
    ::std::array<dwarf::scope_record, 4u> scopes{}; scopes[0].kind = dwarf::scope_kind::compile_unit;
    for(::std::size_t i{}; i != scopes.size(); ++i) { scopes[i].identity = {10u, 100u + i}; }
    scopes[1].kind = dwarf::scope_kind::subprogram; scopes[1].parent = 0u; scopes[1].concrete = true; scopes[1].ranges = {{10u, 30u}};
    scopes[2].kind = dwarf::scope_kind::lexical_block; scopes[2].parent = 1u; scopes[2].own_ranges_declared = true;
    scopes[3].kind = dwarf::scope_kind::inline_subprogram; scopes[3].parent = 2u; scopes[3].concrete = true; scopes[3].ranges = {{12u, 22u}};
    ::std::vector<dwarf::inline_frame> frames{};
    check(dwarf::query_inline_frames(scopes, 15u, frames) == dwarf::inline_query_error::none && frames.empty(), "explicit empty lexical scope is not an inline policy target");
    scopes[2].own_ranges_declared = false;
    check(dwarf::query_inline_frames(scopes, 15u, frames) == dwarf::inline_query_error::none && frames.size() == 1u, "absent lexical ranges inherit parent");
    scopes[3].identity = scopes[1].identity;
    check(dwarf::query_inline_frames(scopes, 15u, frames) == dwarf::inline_query_error::malformed && frames.empty(),
        "duplicate physical/inline identity refuses a source policy target");
    step::origin pending{over}; step::limits cap{}; cap.max_path = 1u;
    check(step::prepare(inline_origin, step::policy::over, pending, cap) == step::error::limit_exceeded && !pending.ready && !pending.initial.source_owner,
        "bounded failed setup clears owned previous origin");
    cap = {}; cap.max_depth = 2u; check(step::prepare(base, step::policy::over, pending, cap) == step::error::limit_exceeded, "depth budget");
    cap = {}; cap.max_file_bytes = 1u; check(step::prepare(base, step::policy::over, pending, cap) == step::error::limit_exceeded, "file byte budget");
    current = base; current.activation = step::activation_relation::unknown;
    check(step::prepare(current, step::policy::out, pending) == step::error::unknown_activation, "unknown origin activation is never enabled");
    current.trace = step::trace_status::missing;
    check(step::prepare(current, step::policy::out, pending) == step::error::missing_trace, "missing origin trace declines out");
    check(step::prepare(base, static_cast<step::policy>(99u), pending) == step::error::unsupported_policy, "unknown policy rejected");
    current = base; current.status = static_cast<step::position_status>(99u);
    expect(over, current, step::action::decline, step::error::malformed_position);
    current = base; current.line = 11u; auto stopped{step::decide(over, current)};
    current.scope_path.front().offset = 999u;
    check(stopped.result == step::action::stop && stopped.stopped_path.front().offset == 100u, "stop tokens owned, no borrowed path after query");
    // A real-return model reaches an unmapped ancestor before that ancestor
    // calls a fresh helper. Comparing ONLY to the retired origin would mistake
    // this for an unproven sibling. The actual controller rebases its genuine
    // complete event chain at the same observe_return boundary tested here.
    auto retired{prepare(base,step::policy::out)}; auto ancestor{base};
    ancestor.function=7u;ancestor.physical_depth=2u;ancestor.activation=step::activation_relation::returned;
    ancestor.status=step::position_status::unmapped;ancestor.scope_path.clear();ancestor.file.clear();ancestor.line=0u;
    ancestor.metadata_ready=true;
    check(step::observe_return(retired,ancestor)==step::error::none,"proven unmapped return retained");
    auto helper{ancestor};helper.function=8u;helper.physical_depth=3u;helper.activation=step::activation_relation::deeper;
    expect(retired,helper,step::action::keep_running,step::error::none);
    // Overlapping DWARF scopes are transported as an unmapped guest context:
    // no guessed statement/path and no permission to originate a source step.
    step::origin ambiguous_origin{};
    check(step::prepare(helper,step::policy::out,ambiguous_origin)==step::error::unmapped,
        "unmapped helper never originates a source step");
    helper=base;helper.function=8u;helper.physical_depth=3u;helper.activation=step::activation_relation::deeper;
    expect(retired,helper,step::action::keep_running,step::error::none); // mapped helper also skipped
    helper.activation=step::activation_relation::unknown;
    expect(retired,helper,step::action::decline,step::error::unknown_activation);
    helper=ancestor;helper.status=step::position_status::native;
    expect(retired,helper,step::action::decline,step::error::native_position);
    helper=ancestor;helper.activation=step::activation_relation::same;helper.status=step::position_status::mapped;
    helper.scope_path={{10u,700u}};helper.file="caller.go";helper.line=12u;helper.is_statement=true;
    expect(retired,helper,step::action::stop,step::error::none);
    helper=ancestor;helper.physical_depth=1u;helper.function=9u;helper.activation=step::activation_relation::returned;
    check(step::observe_return(retired,helper)==step::error::none,"subsequent real ancestor return retained");
    helper=ancestor;helper.activation=step::activation_relation::same;
    check(step::observe_return(retired,helper)!=step::error::none,"depth/function equality never grants a return");
    // Line maps identify coordinates, never DIE scopes. These inert models
    // still need a complete activation relation; no numeric field grants one.
    auto line{base}; line.mapping=step::mapping_kind::line_only;
    line.scope_path.clear(); line.metadata_ready=true; line.file="original.ts";
    auto line_into{prepare(line,step::policy::into)};
    auto line_over{prepare(line,step::policy::over)}, line_out{prepare(line,step::policy::out)};
    current=line; expect(line_into,current,step::action::keep_running,step::error::none);
    ++current.column; expect(line_into,current,step::action::stop,step::error::none);
    expect(line_over,current,step::action::stop,step::error::none);
    expect(line_out,current,step::action::keep_running,step::error::none);
    current=line; current.function=99u; ++current.physical_depth; current.activation=step::activation_relation::deeper;
    expect(line_into,current,step::action::stop,step::error::none);
    expect(line_over,current,step::action::keep_running,step::error::none);
    expect(line_out,current,step::action::keep_running,step::error::none);
    current=line; current.function=99u; current.activation=step::activation_relation::tail_successor;
    expect(line_into,current,step::action::stop,step::error::none);
    expect(line_over,current,step::action::keep_running,step::error::none);
    expect(line_out,current,step::action::keep_running,step::error::none);
    current=line; current.function=1u; --current.physical_depth; current.activation=step::activation_relation::returned;
    expect(line_out,current,step::action::stop,step::error::none);
    auto line_return{line_out};current.status=step::position_status::unmapped;current.file.clear();current.line=0u;
    check(step::observe_return(line_return,current)==step::error::none,"line-only unmapped authentic return model retained");
    current.status=step::position_status::mapped;current.file="caller.ts";current.line=90u;current.activation=step::activation_relation::same;
    expect(line_return,current,step::action::stop,step::error::none);
    current=line;current.mapping=step::mapping_kind::dwarf_scope;current.scope_path=base.scope_path;
    expect(line_over,current,step::action::decline,step::error::stale_generation);
    current=line;current.scope_path=base.scope_path;
    expect(line_into,current,step::action::decline,step::error::malformed_path);
    current=line;current.mapping=static_cast<step::mapping_kind>(99u);
    expect(line_into,current,step::action::decline,step::error::malformed_position);
    current=line;current.activation=step::activation_relation::unknown;
    expect(line_into,current,step::action::decline,step::error::unknown_activation);
    step::origin rejected_line{};
    check(step::prepare(current,step::policy::into,rejected_line)==step::error::unknown_activation,"line-only into requires actual origin relation");
    current=line;current.trace=step::trace_status::missing;
    expect(line_into,current,step::action::decline,step::error::missing_trace);
    check(step::prepare(current,step::policy::into,rejected_line)==step::error::missing_trace,"line-only into requires complete origin trace");
    current=line;current.activation=step::activation_relation::deeper;
    expect(line_into,current,step::action::decline,step::error::inconsistent_activation);
    current=line;current.status=step::position_status::native;
    expect(line_into,current,step::action::decline,step::error::native_position);
    current=line;++current.function_generation;
    expect(line_into,current,step::action::decline,step::error::stale_generation);
    current=line;current.source_owner=::std::shared_ptr<void const>{foreign,line.source_owner.get()};
    expect(line_into,current,step::action::decline,step::error::stale_generation);
    ::fast_io::io::println("debug_source_step_policy: PASS synthetic finite inline/physical prefix policy; no runtime authority or product integration");
}
