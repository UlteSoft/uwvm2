// Genuine SPARC64 kernel PC/NPC single-instruction fixture plus actual LLVM MC
// containment negatives. The test issuer owns one isolated executable page;
// this component does not certify the Wasm runtime issuer or public projection.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/debugger/native_step.h>
#include <uwvm2/uwvm/debugger/native_wasm_step_boundary.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <sys/mman.h>
#if !defined(__sparc__) || !defined(__arch64__) || !UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
# error This real kernel fixture requires SPARC64 Linux.
#endif
namespace dbg=::uwvm2::uwvm::debugger;
namespace step=dbg::native_step;
namespace lib=::uwvm2::runtime::lib;
static void check(bool yes,char const* why)
{ if(!yes) { ::fast_io::io::perrln("native_sparc_delay_step: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
namespace uwvm2::runtime::lib
{
    class llvm_jit_debug_native_activation_cursor final
    {
        static bool witness(void*,void const* session,::std::uint_least64_t thread,::std::uintptr_t pc,::std::uintptr_t sp) noexcept
        { return step::in_owned_kernel_activation_witness(session,thread,pc,sp); }
        static void leave(void*,void const*,::std::uint64_t,unsigned) noexcept {}
    public:
        static llvm_jit_debug_native_activation_provider provider(llvm_jit_debug_native_activation_cursor_owner const& self,
            step::session& s,::std::uint_least64_t thread,::std::uintptr_t begin,::std::uintptr_t end) noexcept
        { return {self,const_cast<llvm_jit_debug_native_activation_cursor*>(self.get()),witness,leave,&s,thread,begin,end,begin}; }
        static llvm_jit_debug_native_breakpoint_plan plan(llvm_jit_debug_native_activation_cursor_owner const& self,
            step::session& s,::std::uint_least64_t thread,::std::uintptr_t begin,::std::uintptr_t end,
            ::std::uintptr_t pc,::std::uint64_t revision,bool initial,dbg::native_wasm_step_boundary::selection const& proof)
        {
            check(bool(proof),"complete owned MC boundary proof before fixture plan");
            ::std::array<::std::uintptr_t,3u> sites{};::std::array<::std::array<unsigned char,4u>,3u> original{};
            unsigned count{};if(initial) { sites[count++]=pc; }sites[count++]=proof.first_successor;
            if(proof.second_successor && proof.second_successor!=proof.first_successor) { sites[count++]=proof.second_successor; }
            for(unsigned i{};i!=count;++i)
            {
                check(sites[i]>=begin && sites[i]<end && end-sites[i]>=4u,"fixture-owned complete trap extent");
                for(unsigned j{};j!=4u;++j) { original[i][j]=reinterpret_cast<unsigned char const*>(sites[i])[j]; }
            }
            return {self,&s,thread,begin,end,pc,revision,initial,4u,count,sites,original,
                {proof.first_successor,proof.second_successor},proof.successor_npc,proof.successor_delay_slot};
        }
    };
}
struct target_description
{
    bool available{true},little_endian{false};unsigned description_version{1u},pointer_bits{64u};
    unsigned maximum_instruction_bytes{4u},minimum_instruction_alignment{1u};
    ::std::array<char,64u> triple{};::std::array<char,1u> cpu{},features{};
    ::std::size_t triple_size{},cpu_size{},features_size{};
    target_description()
    { constexpr char name[]{"sparcv9-unknown-linux-gnu"};triple_size=sizeof(name)-1u;for(::std::size_t i{};i!=sizeof(name);++i) { triple[i]=name[i]; } }
};
static void wait(step::session& s,step::phase phase)
{
    auto const deadline{::std::chrono::steady_clock::now()+::std::chrono::seconds{5}};
    while(s.state.load(::std::memory_order_seq_cst)!=phase)
    { check(::std::chrono::steady_clock::now()<deadline,"bounded actual worker trap/retirement");::std::this_thread::yield(); }
}
static void run(bool taken,bool annul,bool register_branch=false,bool predicted=true,unsigned integer_form=0u,bool delay_nop=false,
    unsigned integer_condition=16u,bool skipped_delay_trap=false)
{
    auto const page_size{static_cast<::std::size_t>(::sysconf(_SC_PAGESIZE))};
    auto* page{::mmap(nullptr,page_size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0)};
    check(page!=MAP_FAILED,"owned executable fixture allocation");
    check(integer_condition==16u || (integer_form!=0u && (integer_condition==0u || integer_condition==8u)),"exact static integer or floating condition fixture");
    if(integer_condition!=16u) { taken=integer_condition==8u; }
    ::std::uint32_t words[]{0x9010202au,taken ? 0x80a22029u : 0x80a2202au,
        annul ? 0x32480005u : 0x12800005u,0x90022001u,0x90022001u,
        0x10800004u,0x01000000u,0x90022002u,0x01000000u,0x81c3e008u,0x01000000u};
    if(integer_form!=0u)
    {
        words[2u]=(annul ? 1u<<29u : 0u) | (9u<<25u) |
            (integer_form==1u ? (2u<<22u) : (1u<<22u) | (integer_form==3u ? 2u<<20u : 0u) |
                (predicted ? 1u<<19u : 0u)) | 5u;
    }
    if(integer_form>=4u)
    {
        check(integer_condition!=16u && integer_form<=8u,"only exact floating always/never forms; no assumed FCC state");
        words[2u]=(annul ? 1u<<29u : 0u) | (integer_condition<<25u) |
            (integer_form==4u ? 6u<<22u : (5u<<22u) | ((integer_form-5u)<<20u) |
                (predicted ? 1u<<19u : 0u)) | 5u;
    }
    if(register_branch)
    {
        // Exact F2_4 encoding: brnz %o0 is taken; brz %o0 is not taken.
        // The genuine numeric register remains 42 on either path.
        words[2u]=(annul ? 1u<<29u : 0u) | ((taken ? 5u : 1u)<<25u) | (3u<<22u) |
            (predicted ? 1u<<19u : 0u) | (8u<<14u) | 5u;
    }
    if(integer_condition!=16u) { words[2u]=(words[2u]&~(15u<<25u)) | (integer_condition<<25u); }
    if(delay_nop) { words[3u]=0x01000000u; }
    if(skipped_delay_trap)
    {
        check(annul && integer_condition!=16u,"trap delay slot is statically skipped");
        words[3u]=0x91d02001u; // ta 1; reaching this would fail the real fixture.
    }
    ::std::array<::std::uint8_t,sizeof(words)> owned{};::std::memcpy(owned.data(),words,sizeof(words));
    ::std::memcpy(page,words,sizeof(words));__builtin___clear_cache(static_cast<char*>(page),static_cast<char*>(page)+sizeof(words));
    check(::mprotect(page,page_size,PROT_READ|PROT_EXEC)==0,"fixture RX before execution");
    auto const begin{reinterpret_cast<::std::uintptr_t>(page)},end{begin+sizeof(words)};
    target_description target{};dbg::native_disassembly::decoder display{target};dbg::native_owned_instruction_semantics::decoder semantics{target};
    check(bool(display) && bool(semantics),"genuine SPARC LLVM MC target, no skipped provider");
    auto prepare{[&](::std::uintptr_t pc,::std::uintptr_t npc=0u,bool pending_slot=false)
    { return dbg::native_wasm_step_boundary::prepare(display,semantics,owned,begin,end,pc,false,npc,{},pending_slot); }};
    auto const branch{prepare(begin+8u)};
    auto const immediate{integer_condition==8u && annul ? begin+28u : !taken && annul ? begin+16u : begin+12u};
    auto const npc{integer_condition==8u && annul ? begin+32u : taken ? begin+28u : !annul ? begin+16u : begin+20u};
    auto const expected_first{integer_condition!=16u ? immediate : begin+12u};
    if(!branch || branch.first_successor!=expected_first)
    {
        auto const raw_decode{semantics.decode(begin+8u,{owned.data()+8u,owned.size()-8u})};
        auto const rendered{display.decode(begin+8u,{owned.data()+8u,owned.size()-8u})};
        ::fast_io::io::perrln("owned fixture branch decode=",bool(raw_decode)," kind=",static_cast<unsigned>(raw_decode.semantics().kind),
            " delayed=",raw_decode.delayed_direct_branch()," destination-status=",static_cast<unsigned>(raw_decode.destination().reason),
            " prepare-reason=",static_cast<unsigned>(branch.unavailable_reason)," text=",::fast_io::mnp::os_c_str(rendered.text.data()));
    }
    check(bool(branch) && branch.first_successor==expected_first,"exact architectural branch successor");
    if(integer_condition==16u)
    {
        check(branch.second_successor==(annul ? begin+16u : begin+12u),"annulled not-taken path skips exactly the delay slot");
        check(branch.successor_npc[0u]==begin+28u && branch.successor_npc[1u]==(annul ? begin+20u : begin+16u),"both exact owned PC/NPC continuations");
    }
    else
    {
        check(branch.second_successor==0u && branch.successor_npc[0u]==npc && branch.successor_npc[1u]==0u,"one proved static PC/NPC continuation");
        auto const next{dbg::native_wasm_step_boundary::prepare(display,semantics,owned,begin,end,begin+8u,true)};
        check(next && next.first_successor==branch.first_successor && next.second_successor==0u &&
            next.successor_npc==branch.successor_npc,"SI and NI share the complete static branch proof");
        auto const actual{semantics.decode(begin+8u,{owned.data()+8u,owned.size()-8u})};
        check(actual && !actual.destination().conditional &&
            actual.destination().never_taken==(integer_condition==0u),"actual MC exact always/never condition");
        if(annul)
        {
            auto const short_size{integer_condition==8u ? 32u : 20u};
            check(!dbg::native_wasm_step_boundary::prepare(display,semantics,
                ::std::span<::std::uint8_t const>{owned.data(),short_size},begin,begin+short_size,begin+8u,true),
                "annulled next PC inside owner but NPC at owner end is refused");
        }
    }
    check(!prepare(begin+12u,end+4u),"kernel continuation outside this complete owner is refused");
    auto bad{owned};bad[8u]=0x12u;bad[9u]=0x80u;bad[10u]=0x01u;bad[11u]=0u;
    check(!dbg::native_wasm_step_boundary::prepare(display,semantics,bad,begin,end,begin+8u,false),"outside-owner direct target refused before any native execution");
    auto nested{owned};nested[12u]=0x81u;nested[13u]=0xc3u;nested[14u]=0xe0u;nested[15u]=0x08u;
    check(!dbg::native_wasm_step_boundary::prepare(display,semantics,nested,begin,end,begin+12u,false,begin+28u),"return/control transfer inside pending delay slot is refused");
    auto nested_branch{owned};
    for(unsigned i{};i!=4u;++i) { nested_branch[12u+i]=owned[8u+i]; }
    check(!dbg::native_wasm_step_boundary::prepare(display,semantics,nested_branch,begin,end,begin+12u,false,begin+28u),
        "static or dynamic branch inside pending delay slot is refused");
    check(!dbg::native_wasm_step_boundary::prepare(display,semantics,nested_branch,begin,end,begin+12u,false,begin+16u,{},true),
        "pending delay branch is refused even when NPC equals sequential fallthrough");
    check(!dbg::native_wasm_step_boundary::prepare(display,semantics,owned,begin,end,begin+12u,false,0u,{},true),
        "pending delay state without an actual kernel NPC is refused");
    step::session s{};::std::atomic<::std::uint_least64_t> thread{};::std::atomic_bool go{};
    using cursor=lib::llvm_jit_debug_native_activation_cursor;auto const owner{::std::make_shared<cursor>()};
    ::std::thread worker{[&]
    {
        thread.store(step::details::raw_system_call(SYS_gettid),::std::memory_order_release);
        while(!go.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
        check(step::arm_at_return(begin),"selected genuine worker arms its own entry");
        auto const function{reinterpret_cast<unsigned long(*)()>(begin)};
        check(function()==(taken ? delay_nop || (integer_condition==8u && annul) ? 44u : 45u : annul || delay_nop ? 43u : 44u),"actual branch, delay instruction and resumed result");
    }};
    while(thread.load(::std::memory_order_acquire)==0u) { ::std::this_thread::yield(); }
    auto const tid{thread.load(::std::memory_order_acquire)};
    check(step::request(s,tid,begin,end,begin,cursor::provider(owner,s,tid,begin,end),
        cursor::plan(owner,s,tid,begin,end,begin,0u,true,prepare(begin))),"sealed owned fixture initial plan");
    go.store(true,::std::memory_order_release);wait(s,step::phase::at_guest_pc);
    check(step::continue_one(s),"execute exactly initial numeric instruction");wait(s,step::phase::trapped);
    auto issue{[&](dbg::native_wasm_step_boundary::selection const& proof)
    { return cursor::plan(owner,s,tid,begin,end,s.next_pc,s.trap_revision,false,proof); }};
    check(s.next_pc==begin+4u && step::stage_plan(s,issue(prepare(begin+4u))) && step::continue_from_trap(s),"execute actual compare only");wait(s,step::phase::trapped);
    check(s.next_pc==begin+8u,"branch instruction is still unexecuted");auto const branch_plan{issue(branch)};
    check(!branch_plan.accepts_kernel_npc(begin+12u,end+4u,false),"unproved NPC cannot become a genuine kernel witness");
    check(branch_plan.accepts_kernel_npc(immediate,npc,false) &&
        !branch_plan.accepts_kernel_npc(immediate,end+4u,false),"exact expected PC/NPC pair only");
    check(step::stage_plan(s,branch_plan) && step::continue_from_trap(s),"execute exactly branch instruction");wait(s,step::phase::trapped);
    check(s.next_pc==immediate,"single branch preserves or annuls its architectural delay slot");
    check(step::with_owned_registers(&s,[&](auto t,auto pc,auto b,auto e,auto const& raw) noexcept
    { check(t==tid && pc==immediate && b==begin && e==end && raw.values[33u]==npc && raw.values[8u]==42u,
        "true kernel NPC and pre-delay numeric register; no simulated register result"); }),"current actual owned kernel snapshot");
    check(!step::stage_plan(s,branch_plan),"prior branch revision has no repeated resume authority");
    check(s.plan.in_delay_slot(immediate,npc)==(immediate==begin+12u) &&
        !s.plan.in_delay_slot(immediate,end+4u),"closed plan retains exact delay status, including sequential NPC");
    bool actual_delay_slot{};
    check(step::with_owned_registers_and_revision(&s,[&](auto,auto pc,auto,auto,auto const& raw,auto,bool delay_slot) noexcept
    { check(pc==immediate && raw.values[33u]==npc,"same genuine trap for delay status");actual_delay_slot=delay_slot; }),
        "locked actual trap exposes only its private delay fact");
    check(actual_delay_slot==(immediate==begin+12u),"actual trap retains sequential and nonsequential delay status");
    auto const after{prepare(immediate,npc,actual_delay_slot)};
    check(bool(after) && after.first_successor==npc,"pending delay resumes at its authenticated owned kernel continuation");
    check(step::stage_plan(s,issue(after)),"stage only proved numerical continuation");
    check(step::with_owned_registers_and_revision(&s,[&](auto,auto,auto,auto,auto const&,auto,bool delay_slot) noexcept
    { check(delay_slot==actual_delay_slot,"staging a future plan cannot overwrite current trap delay status"); }),
        "actual delay fact survives staging and a parked retry");
    check(step::continue_from_trap(s),"execute exactly next numerical instruction");wait(s,step::phase::trapped);
    check(step::with_owned_registers_and_revision(&s,[&](auto,auto,auto,auto,auto const&,auto,bool delay_slot) noexcept
    { check(!delay_slot,"completed ordinary instruction retires the old delay state"); }),"next genuine trap delay status");
    check(s.next_pc==npc,"one numerical instruction reaches exactly its kernel continuation");
    check(step::with_owned_registers(&s,[&](auto,auto,auto,auto,auto const& raw) noexcept
    { check(raw.values[8u]==(integer_condition==8u && annul ? 44u : delay_nop && (taken || !annul) ? 42u : 43u),"exact numerical effect of one real instruction"); }),"post-delay genuine kernel numeric bits");
    check(step::release(s),"release only this owned session");wait(s,step::phase::released);worker.join();
    check(step::clear(s) && ::std::memcmp(page,words,sizeof(words))==0,"exact original code restored and worker retired");
    check(::munmap(page,page_size)==0,"fixture ownership retired after actual join");
}
int main()
{
    check(step::install(),"actual signal backend installed");
    run(true,false);run(false,false);run(true,true);run(false,true);
    for(bool predicted:{false,true})
    { run(true,false,true,predicted);run(false,false,true,predicted);run(true,true,true,predicted);run(false,true,true,predicted); }
    for(unsigned integer_form:{1u,2u,3u})
    { for(bool predicted:{false,true}) { for(bool taken:{false,true}) { for(bool annul:{false,true}) { run(taken,annul,false,predicted,integer_form); } } } }
    for(bool taken:{false,true}) { for(bool annul:{false,true}) { run(taken,annul,false,true,1u,true); } }
    for(unsigned integer_form:{1u,2u,3u,4u,5u,6u,7u,8u})
    {
        for(bool predicted:{false,true})
        {
            for(unsigned condition:{0u,8u})
            {
                for(bool annul:{false,true}) { run(condition==8u,annul,false,predicted,integer_form,false,condition); }
                run(condition==8u,true,false,predicted,integer_form,false,condition,true);
            }
        }
    }
    check(step::uninstall(),"actual signal backend retired");
    ::fast_io::io::println("PASS actual SPARC branch single-step, dynamic and static always/never conditions, annulled/plain and trap delay slots, kernel NPC/register effects, outside/control/sequential-delay/stale refusal and exact restoration");
}
