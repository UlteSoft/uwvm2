// Policy DATA only. Real paused activation ownership is tested with -Rdbg.
#include <uwvm2/uwvm/debugger/command.h>
#include <uwvm2/uwvm/debugger/source_step_policy.h>
#include <array>
#include <vector>
namespace dbg=::uwvm2::uwvm::debugger;
namespace step=dbg::source_step;
struct frame
{
    ::std::uint64_t incarnation,parent,continuation,module,function,function_generation,runtime_epoch;
};
static void require(bool good,::fast_io::string_view why)
{ if(!good) { ::fast_io::io::perrln("Wasm step policy: ",why); ::fast_io::fast_terminate(); } }
template<::std::size_t A,::std::size_t B>
static step::activation_relation compare(::std::array<frame,A> const& initial,::std::array<frame,B> const& current)
{ return step::compare_event_chains(::std::span{initial},::std::span{current},256u); }
int main()
{
    using relation=step::activation_relation;using action=step::action;using policy=step::policy;
    ::std::array<frame,2> origin{{{1,0,1,0,0,1,7},{2,1,2,0,1,1,7}}};
    auto same=origin;
    ::std::array<frame,3> recursive{{origin[0],origin[1],{3,2,3,0,1,1,7}}};
    ::std::array<frame,2> tail{{origin[0],{4,1,2,0,1,1,7}}};
    ::std::array<frame,3> tail_child{{origin[0],tail[1],{5,4,4,1,2,3,7}}};
    ::std::array<frame,1> caller{{origin[0]}};
    require(compare(origin,same)==relation::same,"same incarnation");
    require(compare(origin,recursive)==relation::deeper,"recursive call is not same function activation");
    require(compare(origin,tail)==relation::tail_successor,"same-function tail recursion has a new incarnation");
    require(compare(origin,tail_child)==relation::deeper,"tail successor child stays inside continuation");
    require(compare(origin,caller)==relation::returned,"return or exception unwind reaches actual caller");
    for(auto r:{relation::same,relation::deeper,relation::returned,relation::tail_successor})
    { require(step::wasm_instruction_action(policy::into,r)==action::stop,"into stops at next opcode"); }
    for(auto r:{relation::deeper,relation::tail_successor})
    { require(step::wasm_instruction_action(policy::over,r)==action::keep_running,"over skips child and tail successor"); }
    for(auto r:{relation::same,relation::deeper,relation::tail_successor})
    { require(step::wasm_instruction_action(policy::out,r)==action::keep_running,"out follows origin continuation"); }
    require(step::wasm_instruction_action(policy::over,relation::same)==action::stop,"over reaches next origin opcode");
    for(auto p:{policy::over,policy::out})
    { require(step::wasm_instruction_action(p,relation::returned)==action::stop,"caller landing"); }
    require(step::wasm_instruction_action(policy::out,relation::unknown)==action::decline,"unknown chain rejected");
    require(step::wasm_instruction_action(policy::into,static_cast<relation>(255))==action::decline,"invalid relation rejected");
    require(step::wasm_instruction_action(static_cast<policy>(255),relation::same)==action::decline,"invalid policy rejected");
    tail[1].continuation=99;require(compare(origin,tail)==relation::unknown,"unrelated replacement activation rejected");
    same[1].function_generation=2;require(compare(origin,same)==relation::unknown,"stale function generation rejected");
    same=origin;same[1].parent=99;require(compare(origin,same)==relation::unknown,"broken parent chain rejected");
    same=origin;same[1].runtime_epoch=8;require(compare(origin,same)==relation::unknown,"mixed epochs rejected");
    ::std::vector<frame> deep;
    for(::std::uint64_t i=1;i<=257;++i){deep.push_back({i,i-1,i,0,1,1,7});}
    require(step::compare_event_chains(::std::span{::std::as_const(deep)},::std::span{::std::as_const(deep)},256u)==relation::unknown,"frame cap");
    deep.pop_back();require(step::compare_event_chains(::std::span{::std::as_const(deep)},::std::span{::std::as_const(deep)},256u)==relation::same,"all256 runtime frames accepted");
    for(::std::uint64_t i=257;i<=10000;++i){deep.push_back({i,i-1,i,0,1,1,7});}
    require(step::compare_event_chains(::std::span{::std::as_const(deep)},::std::span{::std::as_const(deep)})==relation::same,"default policy accepts10000 exact identities");
    auto deeper{deep};deeper.push_back({10001,10000,10001,0,1,1,7});
    require(step::compare_event_chains(::std::span{::std::as_const(deep)},::std::span{::std::as_const(deeper)})==relation::deeper,"deep next follows child");
    deeper=deep;deeper.back().incarnation=10002;
    require(step::compare_event_chains(::std::span{::std::as_const(deep)},::std::span{::std::as_const(deeper)})==relation::tail_successor,"deep finish follows tail continuation");
    deeper=deep;deeper.pop_back();
    require(step::compare_event_chains(::std::span{::std::as_const(deep)},::std::span{::std::as_const(deeper)})==relation::returned,"deep return or EH landing");
    for(auto text:{"frames 1 9 4096 128","frames wasm 1 9 4096 128"})
    {require(dbg::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).kind==dbg::console_command_kind::source_frame,"paged canonical chain grammar");}
    for(auto text:{"frames 1 9 4096 129","frames wasm 1 0 0 128","frames wasm 1 9 0 0"})
    {require(dbg::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).kind==dbg::console_command_kind::invalid,"invalid page grammar");}
    for(auto text:{"step wasm 1","step wasm 1 into"})
    { require(dbg::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).kind==dbg::console_command_kind::protocol,"existing into protocol retained"); }
    for(auto text:{"step wasm 1 over","step wasm 1 out"})
    { require(dbg::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).kind==dbg::console_command_kind::wasm_step,"Wasm policy grammar"); }
    for(auto text:{"step wasm 0 over","step wasm -1 out","step wasm 1 unknown","step wasm 1 over extra"})
    { require(dbg::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).kind==dbg::console_command_kind::invalid,"bad policy grammar rejected"); }
    ::fast_io::io::println("Wasm opcode stepping policy PASS; real runtime capture=false");
}
