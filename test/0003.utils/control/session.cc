#include "buffer_helpers.h"
using namespace control_test;
// Run only through the SSH cgroup runner. Assertions are void helpers, including
// inside thread lambdas, so successful execution never falls off a non-void task.
#include <uwvm2/utils/control/impl.h>
#include <atomic>
#include <array>
#include <cstdlib>
#include <thread>
#include <type_traits>
namespace ctl = uwvm2::utils::control;
static std::size_t checks{};
static void check(bool yes, int line)
{ ++checks; if(!yes) { fast_io::io::perrln("FAIL control session line=",line); fast_io::fast_terminate(); } }
#define CHECK(value) check(bool(value), __LINE__)
static ctl::launch_config config()
{
    ctl::launch_config c;
    c.debug_enabled=c.replacement_enabled=true;
    c.compiler=ctl::backend::llvm;
    c.mode=ctl::compile_mode::full;
    c.instance[0]=42u; c.vm_process=100; c.launcher_process=200; c.channel_binding=999;
    return c;
}
static std::vector<ctl::wire_byte> frame(ctl::operation op, std::uint64_t id=1,
                                   ctl::input_buffer payload={}, ctl::launch_config c=config())
{
    std::vector<ctl::wire_byte> bytes(ctl::header_bytes+ctl::remaining_bytes(payload));
    ctl::frame_header h{op,static_cast<std::uint32_t>(ctl::remaining_bytes(payload)),c.instance,c.generation,id};
    ctl::output_buffer output{bytes};
    auto r=ctl::encode_frame(h,payload,output);CHECK(r.status==ctl::error::none&&r.written==bytes.size());
    return bytes;
}
static void attach(ctl::control_session& session,ctl::launch_authority& authority)
{ CHECK(session.attach_launcher(authority.issue_permit(),{200,999})==ctl::error::none); }
static std::vector<ctl::wire_byte> replacement(std::size_t n=3)
{
    std::vector<ctl::wire_byte> body(32+n);
    put_le<std::uint64_t>(body,0,71);
    put_le<std::uint64_t>(body,8,9);
    put_le<std::uint64_t>(body,16,5);
    put_le<std::uint32_t>(body,24,static_cast<std::uint32_t>(n));
    for(std::size_t i=32;i<body.size();++i)body[i]=ctl::wire_byte{0x0b};
    return body;
}
static void denied_packet(std::vector<ctl::wire_byte> const& packet,ctl::error expected,ctl::launch_config c=config())
{
    ctl::launch_authority authority(c);ctl::control_session session;attach(session,authority);
    auto result=session.receive(input(packet));CHECK(result.status==expected&&!result.request);
    CHECK(session.status()==ctl::session_state::closed);
    CHECK(session.receive(input(frame(ctl::operation::status))).status==ctl::error::closed);
}
int main()
{
    static_assert(!std::is_copy_constructible_v<ctl::launch_permit>);
    static_assert(!std::is_copy_constructible_v<ctl::request_ticket>);
    static_assert(!std::is_default_constructible_v<ctl::request_ticket>);
    static_assert(!std::is_move_constructible_v<ctl::control_session>);
    auto const initial=frame(ctl::operation::status);
    {
        ctl::launch_authority off;ctl::control_session session;
        CHECK(off.status()==ctl::error::disabled);
        CHECK(session.attach_launcher(off.issue_permit(),{200,999})==ctl::error::unauthorized);
        auto r=session.receive(input(initial));CHECK(r.status==ctl::error::unauthorized&&r.consumed==0&&!r.request);
    }
    for(auto backend:{ctl::backend::interpreter,ctl::backend::llvm})
    for(auto mode:{ctl::compile_mode::full,ctl::compile_mode::lazy,ctl::compile_mode::lazy_verified,ctl::compile_mode::tiered})
    {
        auto c=config();c.compiler=backend;c.mode=mode;
        auto want=backend!=ctl::backend::llvm?ctl::error::unsupported_backend:
                  mode!=ctl::compile_mode::full?ctl::error::unsupported_mode:ctl::error::none;
        ctl::launch_authority a(c);CHECK(a.status()==want);
        if(want!=ctl::error::none){ctl::control_session s;CHECK(s.attach_launcher(a.issue_permit(),{200,999})==ctl::error::unauthorized);}
    }
    for(unsigned bad=0;bad!=5;++bad)
    {
        auto c=config();
        if(bad==0)c.instance={};if(bad==1)c.generation=0;if(bad==2)c.vm_process=0;
        if(bad==3)c.launcher_process=c.vm_process;if(bad==4)c.channel_binding=0;
        CHECK(ctl::qualify(c)==ctl::error::invalid_launch);
    }
    {
        ctl::launch_authority a(config());ctl::control_session s;
        auto p=a.issue_permit();CHECK(s.attach_launcher(std::move(p),{100,999})==ctl::error::same_process);
        CHECK(s.attach_launcher(std::move(p),{200,999})==ctl::error::unauthorized);
        CHECK(s.attach_launcher(a.issue_permit(),{201,999})==ctl::error::wrong_peer);
        CHECK(s.attach_launcher(a.issue_permit(),{200,998})==ctl::error::wrong_peer);
        CHECK(s.attach_console(a.issue_permit())==ctl::error::unauthorized);
        attach(s,a);ctl::control_session other;CHECK(other.attach_launcher(a.issue_permit(),{200,999})==ctl::error::busy);
    }
    {
        auto c=config();c.origin=ctl::launch_origin::console;c.launcher_process=c.channel_binding=0;
        ctl::launch_authority a(c);ctl::control_session s;
        CHECK(s.attach_launcher(a.issue_permit(),{200,999})==ctl::error::unauthorized);
        CHECK(s.attach_console(a.issue_permit())==ctl::error::none);
        auto r=s.receive(input(initial));CHECK(r.request.has_value());CHECK(s.complete(*r.request,ctl::host_completion::inspected)==ctl::error::none);
    }
    // Every truncation boundary, including metadata/body boundaries. No truncated
    // packet yields a request or a host state transition.
    auto c=config();c.initial_execution=ctl::execution_state::stopped;
    auto body=replacement();auto full=frame(ctl::operation::replace_function,1,input(body),c);
    for(std::size_t n=1;n!=full.size();++n)
    {
        ctl::launch_authority a(c);ctl::control_session s;attach(s,a);
        auto r=s.receive(input(prefix(full,n)));CHECK(r.status==ctl::error::none&&!r.request&&r.consumed==n);
        CHECK(s.finish_stream()==ctl::error::truncated);
    }
    // Arbitrary byte-sized fragmentation, owned replacement bytes after decoder reuse.
    {
        ctl::launch_authority a(c);ctl::control_session s;attach(s,a);
        std::optional<ctl::request_ticket> ticket;
        for(std::size_t i=0;i!=full.size();++i)
        {
            auto r=s.receive(input(slice(full,i,1)));CHECK(r.status==ctl::error::none&&r.consumed==1);
            if(i+1==full.size())ticket=std::move(r.request);else CHECK(!r.request);
        }
        CHECK(ticket.has_value());
        auto const& req=std::get<ctl::replace_function_command>(ticket->get());
        CHECK(req.module==71&&req.function==9&&req.expected_generation==5&&req.body.size()==3&&req.body[0]==std::byte{0x0b});
        CHECK(s.receive(input(initial)).status==ctl::error::busy);
        CHECK(s.complete(*ticket,ctl::host_completion::resumed)==ctl::error::wrong_completion);
        CHECK(s.complete(*ticket,ctl::host_completion::replaced)==ctl::error::none);
        CHECK(s.complete(*ticket,ctl::host_completion::replaced)==ctl::error::wrong_completion);
        auto status=s.receive(input(frame(ctl::operation::status,2)));CHECK(status.request.has_value());
        CHECK(req.body[0]==std::byte{0x0b});CHECK(s.complete(*status.request,ctl::host_completion::inspected)==ctl::error::none);
    }
    {
        ctl::launch_authority a(config());ctl::control_session s;attach(s,a);
        auto pause=s.receive(input(frame(ctl::operation::pause)));CHECK(pause.request.has_value());
        CHECK(s.complete(*pause.request,ctl::host_completion::inspected)==ctl::error::wrong_completion);
        CHECK(s.complete(*pause.request,ctl::host_completion::paused)==ctl::error::none);
        auto replace=s.receive(input(frame(ctl::operation::replace_function,2,input(body))));CHECK(replace.request.has_value());
        CHECK(s.complete(*replace.request,ctl::host_completion::rejected)==ctl::error::none);
        auto resume=s.receive(input(frame(ctl::operation::resume,3)));CHECK(resume.request.has_value());
        CHECK(s.complete(*resume.request,ctl::host_completion::resumed)==ctl::error::none);
        auto detach=s.receive(input(frame(ctl::operation::detach,4)));CHECK(detach.request.has_value());
        CHECK(s.complete(*detach.request,ctl::host_completion::detached)==ctl::error::none);
        ctl::control_session again;attach(again,a);
        CHECK(again.receive(input(frame(ctl::operation::status,1))).status==ctl::error::replay);
    }
    // Dropping a pending pause never claims the runtime stopped. Reconnect keeps
    // the actual running state and consumes the abandoned request ID.
    {
        ctl::launch_authority a(config());std::optional<ctl::request_ticket> old;
        {ctl::control_session s;attach(s,a);auto r=s.receive(input(frame(ctl::operation::pause)));old=std::move(r.request);}
        ctl::control_session s;attach(s,a);auto next=s.receive(input(frame(ctl::operation::pause,2)));CHECK(next.request.has_value());
        CHECK(s.complete(*old,ctl::host_completion::paused)==ctl::error::wrong_completion);
        CHECK(s.complete(*next.request,ctl::host_completion::paused)==ctl::error::none);
        s.disconnect();ctl::control_session resumed;attach(resumed,a);
        auto r=resumed.receive(input(frame(ctl::operation::resume,3)));CHECK(r.request.has_value());
    }
    denied_packet(frame(ctl::operation::resume),ctl::error::invalid_state);
    denied_packet(frame(ctl::operation::replace_function,1,input(body)),ctl::error::invalid_state);
    auto changed=initial;changed[0]=ctl::wire_byte{};denied_packet(changed,ctl::error::malformed);
    changed=initial;changed[4]=ctl::wire_byte{2};denied_packet(changed,ctl::error::unsupported_version);
    changed=initial;changed[6]=ctl::wire_byte{99};denied_packet(changed,ctl::error::unsupported_command);
    changed=initial;changed[12]=ctl::wire_byte{1};denied_packet(changed,ctl::error::malformed);
    changed=initial;changed[16]=ctl::wire_byte{5};denied_packet(changed,ctl::error::wrong_instance);
    changed=initial;changed[32]=ctl::wire_byte{2};denied_packet(changed,ctl::error::stale_generation);
    changed=initial;changed[40]=ctl::wire_byte{};denied_packet(changed,ctl::error::malformed);
    changed=initial;put_le<std::uint32_t>(changed,8,0xffffffffu);denied_packet(changed,ctl::error::oversized);
    denied_packet(frame(ctl::operation::status,2),ctl::error::replay);
    denied_packet(frame(ctl::operation::status,1,prefix(body,1)),ctl::error::malformed);
    {
        auto limited=config();limited.debug_enabled=false;
        denied_packet(frame(ctl::operation::pause),ctl::error::unavailable_capability,limited);
        limited=config();limited.replacement_enabled=false;limited.initial_execution=ctl::execution_state::stopped;
        denied_packet(frame(ctl::operation::replace_function,1,input(body)),ctl::error::unavailable_capability,limited);
    }
    {
        auto b=body;b[28]=ctl::wire_byte{1};denied_packet(frame(ctl::operation::replace_function,1,input(b)),ctl::error::malformed,c);
        b=body;put_le<std::uint32_t>(b,24,100);denied_packet(frame(ctl::operation::replace_function,1,input(b)),ctl::error::malformed,c);
        b=body;put_le<std::uint32_t>(b,24,0xffffffffu);denied_packet(frame(ctl::operation::replace_function,1,input(b)),ctl::error::oversized,c);
        b=body;put_le<std::uint64_t>(b,16,0);denied_packet(frame(ctl::operation::replace_function,1,input(b)),ctl::error::malformed,c);
        b=replacement(ctl::max_function_body_bytes);
        ctl::launch_authority a(c);ctl::control_session s;attach(s,a);auto r=s.receive(input(frame(ctl::operation::replace_function,1,input(b))));
        CHECK(r.request.has_value()&&std::get<ctl::replace_function_command>(r.request->get()).body.size()==ctl::max_function_body_bytes);
    }
    {
        std::vector<ctl::wire_byte> mem(32);put_le<std::uint64_t>(mem,16,~std::uint64_t{});
        put_le<std::uint32_t>(mem,24,1);denied_packet(frame(ctl::operation::read_memory,1,input(mem)),ctl::error::malformed,c);
        put_le<std::uint64_t>(mem,16,0);put_le<std::uint32_t>(mem,24,65537);
        denied_packet(frame(ctl::operation::read_memory,1,input(mem)),ctl::error::oversized,c);
        put_le<std::uint32_t>(mem,24,65536);ctl::launch_authority a(c);ctl::control_session s;attach(s,a);
        auto r=s.receive(input(frame(ctl::operation::read_memory,1,input(mem))));CHECK(r.request.has_value());
        CHECK(s.complete(*r.request,ctl::host_completion::inspected)==ctl::error::none);
        std::array<ctl::wire_byte,8> step{};r=s.receive(input(frame(ctl::operation::step,2,input(step))));CHECK(r.request.has_value());
        CHECK(s.complete(*r.request,ctl::host_completion::stepped)==ctl::error::none);
        auto interrupted=s.receive(input(frame(ctl::operation::step,3,input(step))));CHECK(interrupted.request.has_value());
        CHECK(s.complete(*interrupted.request,ctl::host_completion::resumed)==ctl::error::wrong_completion);
        CHECK(s.status()==ctl::session_state::pending);
        CHECK(s.complete(*interrupted.request,ctl::host_completion::paused)==ctl::error::none);
        CHECK(s.status()==ctl::session_state::ready);
        auto retry=s.receive(input(frame(ctl::operation::step,4,input(step))));CHECK(retry.request.has_value());
        CHECK(s.complete(*interrupted.request,ctl::host_completion::paused)==ctl::error::wrong_completion);
        CHECK(s.complete(*retry.request,ctl::host_completion::stepped)==ctl::error::none);
    }
    {
        ctl::frame_decoder d;auto pair=initial;pair.insert(pair.end(),initial.begin(),initial.end());
        auto r=d.feed(input(pair));CHECK(r.complete&&r.consumed==initial.size());
        CHECK(d.feed(input(initial)).status==ctl::error::busy);d.reset();CHECK(d.feed(input(initial)).complete);
        std::array<ctl::wire_byte,1> short_output{};
        ctl::output_buffer output{short_output};
        CHECK(ctl::encode_frame({ctl::operation::status,0,config().instance,1,1},{},output).status==ctl::error::truncated);
    }
    {
        ctl::control_session s;ctl::launch_permit delayed;
        {ctl::launch_authority a(config());delayed=a.issue_permit();attach(s,a);}
        CHECK(s.receive(input(initial)).status==ctl::error::revoked);
        ctl::control_session other;CHECK(other.attach_launcher(std::move(delayed),{200,999})==ctl::error::revoked);
    }
    for(unsigned round=0;round!=64;++round)
    {
        ctl::launch_authority a(config());ctl::control_session s;attach(s,a);
        std::atomic_bool go{};std::thread revoker([&]{while(!go.load(std::memory_order_acquire)){} a.revoke();});
        go.store(true,std::memory_order_release);auto r=s.receive(input(initial));revoker.join();
        CHECK(r.status==ctl::error::none||r.status==ctl::error::revoked);
        if(r.request)CHECK(s.complete(*r.request,ctl::host_completion::inspected)==ctl::error::revoked);
        CHECK(s.receive(input(initial)).status==ctl::error::revoked);
    }
    // Deterministic hostile bytes under ASan/UBSan: arbitrary lengths never escape
    // the fixed frame buffer, even when the header itself happens to be valid.
    std::uint32_t random=0x681274ab;
    for(unsigned round=0;round!=4096;++round)
    {
        ctl::frame_decoder d;std::array<ctl::wire_byte,113> junk{};
        for(auto& byte:junk){random^=random<<13;random^=random>>17;random^=random<<5;byte=ctl::wire_byte(random&255);}
        auto r=d.feed(input(prefix(junk,round%junk.size())));
        CHECK(r.consumed<=junk.size());CHECK(!r.complete);
    }
    fast_io::io::println("PASS control session ",checks," checks; no runtime pause/patch or external transport claimed");
}
