// Finite parser/policy DATA fixtures only: no VM capture or read authority.
#include <uwvm2/uwvm/debugger/source_step_policy.h>
#include <uwvm2/uwvm/debugger/console_aliases.h>
#include <uwvm2/uwvm/debugger/console_completion.h>
#include <fast_io.h>
namespace step = uwvm2::uwvm::debugger::source_step;
namespace dbg = uwvm2::uwvm::debugger;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_run_to: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static step::position fixture()
{
    step::position value{};
    value.source_owner = ::std::make_shared<int>(1); // inert owner identity, not a runtime binding.
    value.module=1u; value.function=2u; value.runtime_epoch=7u; value.function_generation=5u;
    value.scope_path={{10u,100u}}; value.file="scope.cpp"; value.line=10u;
    value.is_statement=true; value.physical_depth=3u; value.status=step::position_status::mapped;
    value.trace=step::trace_status::complete; value.activation=step::activation_relation::same;
    value.metadata_ready=true; return value;
}
static void expect(step::origin const& origin, step::position const& current, step::action result, step::error reason=step::error::none)
{
    auto const out{step::decide(origin,current)};
    check(out.result==result && out.reason==reason,"destination decision");
    if(result!=step::action::stop) { check(out.stopped_path.empty(),"no tentative stop tokens"); }
}
int main()
{
    for(auto name : {::fast_io::string_view{"until"},::fast_io::string_view{"advance"}})
    {
        auto const text{::fast_io::concat_fast_io(name," 17 19 C:/source with spaces/file.cpp:99")};
        auto const command{dbg::parse_console_command(::fast_io::string_view{text.data(),text.size()})};
        check(command.kind==dbg::console_command_kind::source_step && command.source_policy==dbg::source_step_policy::over &&
              command.requested_step_thread==17u && command.disassembly_stop_identifier==19u && command.source_line==99u &&
              ::fast_io::string_view{command.source_path.data(),command.source_path_size}=="C:/source with spaces/file.cpp",
              "explicit labels and opaque source path");
        check(command.source_run_to==(name=="until" ? dbg::source_run_to_policy::until : dbg::source_run_to_policy::advance),"distinct scheduling mode");
        for(auto suffix : {" 0 19 f.c:1"," 17 0 f.c:1"," 17 19 f.c:0"," 17 19 f.c:-1"," 17 19 f.c:1junk",
                            " 17 19 :1"," 17 19 f.c:18446744073709551616"," 17 19 f\nc:1"})
        { auto const invalid{::fast_io::concat_fast_io(name,::fast_io::mnp::os_c_str(suffix))};
          check(dbg::parse_console_command(::fast_io::string_view{invalid.data(),invalid.size()}).kind==dbg::console_command_kind::invalid,"malformed destination refusal"); }
    }
    for(auto text : {"until scope.cpp:20","u scope.cpp:20","advance scope.cpp:20","adv scope.cpp:20"})
    {
        auto const alias{dbg::console_aliases::parse(::fast_io::string_view{text,::fast_io::cstr_len(text)},17u,19u)};
        check(alias.command.kind==dbg::console_command_kind::source_step && alias.command.requested_step_thread==17u &&
              alias.command.disassembly_stop_identifier==19u,"sole-thread alias uses inspected labels");
        check(dbg::console_aliases::parse(::fast_io::string_view{text,::fast_io::cstr_len(text)},0u,19u).current_thread_required,"no guessed participant");
    }
    for(auto text : {"until 17 19 20","advance 17 19 20"})
    {
        auto const command{dbg::parse_console_command(::fast_io::string_view{text,::fast_io::cstr_len(text)})};
        check(command.kind==dbg::console_command_kind::source_step && command.source_path_size==0u && command.source_line==20u &&
              command.requested_step_thread==17u && command.disassembly_stop_identifier==19u,"current-file explicit request is scheduling data only");
    }
    for(auto text : {"until 20","u 20","advance 20","adv 20"})
    {
        auto const result{dbg::console_aliases::parse(::fast_io::string_view{text,::fast_io::cstr_len(text)},17u,19u)};
        check(result.command.kind==dbg::console_command_kind::source_step && result.command.source_path_size==0u &&
              result.command.source_line==20u,"native-style current-file line aliases");
    }
    for(auto text : {"until 17 19 0","until 17 19 -1","advance 17 19 20junk","until 17 19 :20"})
    { check(dbg::parse_console_command(::fast_io::string_view{text,::fast_io::cstr_len(text)}).kind==dbg::console_command_kind::invalid,"malformed current-file line"); }
    auto base{fixture()}; step::origin until{},advance{};
    check(step::prepare_destination(base,step::destination_policy::until,"scope.cpp",20u,until)==step::error::none,"until setup");
    check(step::prepare_destination(base,step::destination_policy::advance,"scope.cpp",20u,advance)==step::error::none,"advance setup");
    auto current{base}; current.line=20u;
    expect(until,current,step::action::stop); expect(advance,current,step::action::stop);
    current.line=21u; expect(until,current,step::action::keep_running); expect(advance,current,step::action::keep_running);
    current.line=20u; current.is_statement=false; expect(until,current,step::action::keep_running);
    current.is_statement=true; current.file="other.cpp"; expect(until,current,step::action::keep_running);
    current=base; current.line=20u; ++current.physical_depth; current.activation=step::activation_relation::deeper;
    expect(until,current,step::action::keep_running); expect(advance,current,step::action::stop); // recursion with identical function/source labels.
    current.function=9u; expect(until,current,step::action::keep_running); expect(advance,current,step::action::stop);
    current.module=2u; current.source_owner=::std::make_shared<int>(2); expect(advance,current,step::action::keep_running);
    current=base; --current.physical_depth; current.activation=step::activation_relation::returned; current.function=1u;
    expect(until,current,step::action::stop); expect(advance,current,step::action::stop); // actual caller statement before target.
    current.status=step::position_status::unmapped; current.scope_path.clear();
    expect(until,current,step::action::keep_running);
    check(step::observe_return(until,current)==step::error::none,"unmapped proven return anchor");
    auto caller{current}; caller.status=step::position_status::mapped; caller.scope_path={{10u,50u}}; caller.line=3u;
    caller.activation=step::activation_relation::same; expect(until,caller,step::action::stop);
    current=base; current.scope_path.push_back({10u,200u}); step::origin inlined{};
    check(step::prepare_destination(current,step::destination_policy::until,"scope.cpp",20u,inlined)==step::error::none,"concrete inline origin");
    expect(inlined,base,step::action::stop); // actual inline exit, not a same-name child.
    current=base; current.line=20u; current.activation=step::activation_relation::tail_successor; current.function=9u;
    step::origin tail_until{}; check(step::prepare_destination(base,step::destination_policy::until,"scope.cpp",20u,tail_until)==step::error::none,"tail origin");
    expect(tail_until,current,step::action::keep_running); expect(advance,current,step::action::stop);
    for(auto mode : {step::destination_policy::until,step::destination_policy::advance})
    {
        step::origin out{}; check(step::prepare_destination(base,mode,"scope.cpp",20u,out)==step::error::none,"negative origin");
        current=base; ++current.runtime_epoch; expect(out,current,step::action::decline,step::error::stale_generation);
        current=base; ++current.function_generation; expect(out,current,step::action::decline,step::error::stale_generation);
        current=base; current.source_owner=::std::make_shared<int>(1); expect(out,current,step::action::decline,step::error::stale_generation);
        current=base; current.activation=step::activation_relation::unknown; expect(out,current,step::action::decline,step::error::unknown_activation);
        current=base; current.trace=step::trace_status::truncated; expect(out,current,step::action::decline,step::error::truncated_trace);
        current=base; current.status=step::position_status::native; expect(out,current,step::action::decline,step::error::native_position);
        current=base; ++current.physical_depth; expect(out,current,step::action::decline,step::error::inconsistent_activation);
        current=base; current.scope_path.clear(); expect(out,current,step::action::decline,step::error::malformed_path);
        check(step::prepare_destination(base,mode,"",20u,out)==step::error::malformed_position && !out.ready,"failed setup clears origin");
    }
    for(auto text : {"until file.cpp:20","advance 17 19 file.cpp:20"})
    {
        auto const view{::fast_io::string_view{text,::fast_io::cstr_len(text)}};
        auto const cursor{view.find(".cpp")}; auto const context{dbg::console_completion::context(view,cursor)};
        check(context.type==dbg::console_completion::kind::path && context.prefix=="file","bounded path completion");
        check(dbg::console_completion::context(view,view.size()).type==dbg::console_completion::kind::none,"line number is not a filesystem prefix");
    }
    for(auto text : {"until 20","u 20","advance 20","adv 20","until 17 19 20","advance 17 19 20"})
    { auto const view{::fast_io::string_view{text,::fast_io::cstr_len(text)}};
      check(dbg::console_completion::context(view,view.size()).type==dbg::console_completion::kind::none,"current-file line is not a filesystem prefix"); }
    ::fast_io::io::println("PASS destination parser, recursion/return/inline policy and stale/native refusals (DATA only)");
}
