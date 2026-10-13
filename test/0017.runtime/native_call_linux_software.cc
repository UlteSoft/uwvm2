// Actual native call/recursion/cancellation backend qualification. The test
// issuer below is deliberately separate from the genuine Wasm runtime issuer.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/debugger/native_step.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <type_traits>
#if !UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
# error Linux software backend required
#endif
namespace step=::uwvm2::uwvm::debugger::native_step;
namespace lib=::uwvm2::runtime::lib;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native call fixture line=",__LINE__);::fast_io::fast_terminate(); } } while(false)
static thread_local unsigned depth{},maximum_depth{};
static ::std::atomic_bool worker_ready{},worker_start{},block_helper{},helper_entered{},helper_continue{};
static ::std::atomic<::std::uint_least64_t> worker_tid{};
static ::std::atomic<unsigned> descendant_hits{};
extern "C" unsigned char call_begin[],call_origin[],call_return[],call_escape[],call_end[];
extern "C" unsigned long fixture_owned_call();
[[gnu::noinline]] static unsigned long helper()
{
    helper_entered.store(true,::std::memory_order_release);
    while(block_helper.load(::std::memory_order_acquire) && !helper_continue.load(::std::memory_order_acquire))
    { static_cast<void>(step::skip_pause_for_continuation());::std::this_thread::yield(); }
    if(depth==maximum_depth) { return 42u; }
    ++depth;auto const result{fixture_owned_call()};--depth;return result+1u;
}
#if defined(__s390x__)
# define CALL_NOP "nopr %%r0\n"
#elif defined(__loongarch64)
# define CALL_NOP "addi.d $r0,$r0,0\n"
#elif defined(__riscv)
# define CALL_NOP ".option push\n.option norvc\naddi x0,x0,0\n.option pop\n"
#else
# define CALL_NOP "nop\n"
#endif
// The linker fragment dedicates complete 64KiB pages to this function. Host
// code, handlers and helpers cannot share a temporarily writable guest page.
extern "C" [[gnu::noinline,gnu::section(".uwvm_call_fixture")]] unsigned long fixture_owned_call()
{
    asm volatile(".global call_begin\ncall_begin:\n" CALL_NOP
                 ".global call_origin\ncall_origin:\n" CALL_NOP ::: "memory");
    auto const result{helper()};
    asm volatile(".global call_return\ncall_return:\n" CALL_NOP
                 ".global call_escape\ncall_escape:\n" CALL_NOP
                 ".global call_end\ncall_end:\n" ::: "memory");
    return result;
}
namespace uwvm2::runtime::lib
{
    class llvm_jit_debug_native_activation_cursor final
    {
        static bool witness(void*,void const* identity,::std::uint_least64_t thread,::std::uintptr_t pc,::std::uintptr_t sp) noexcept
        { return depth==0u && step::in_owned_kernel_activation_witness(identity,thread,pc,sp); }
        static bool descendant(void*,void const* identity,::std::uint_least64_t thread,::std::uintptr_t pc,::std::uintptr_t sp) noexcept
        {
            if(depth==0u || !step::in_owned_kernel_activation_witness(identity,thread,pc,sp)) { return false; }
            descendant_hits.fetch_add(1u,::std::memory_order_relaxed);return true;
        }
        static void leave(void*,void const*,::std::uint64_t,unsigned) noexcept {}
    public:
        static llvm_jit_debug_native_activation_provider provider(llvm_jit_debug_native_activation_cursor_owner const& self,
            step::session& s,::std::uint_least64_t thread,::std::uintptr_t begin,::std::uintptr_t end) noexcept
        { return {self,const_cast<llvm_jit_debug_native_activation_cursor*>(self.get()),witness,leave,&s,thread,begin,end,begin,descendant}; }
        static llvm_jit_debug_native_breakpoint_plan plan(llvm_jit_debug_native_activation_cursor_owner const& self,
            step::session& s,::std::uint_least64_t thread,bool initial) noexcept
        {
            auto const begin{reinterpret_cast<::std::uintptr_t>(call_begin)},end{reinterpret_cast<::std::uintptr_t>(call_end)};
            auto const origin{reinterpret_cast<::std::uintptr_t>(call_origin)};
            ::std::array<::std::uintptr_t,3u> sites{initial?begin:reinterpret_cast<::std::uintptr_t>(call_return),
                initial?origin:reinterpret_cast<::std::uintptr_t>(call_escape),0u};
            ::std::array<::std::array<unsigned char,4u>,3u> original{};
            auto const width{step::details::breakpoint_width()};
            for(unsigned i{};i!=2u;++i) for(unsigned b{};b!=width;++b) { original[i][b]=reinterpret_cast<unsigned char const*>(sites[i])[b]; }
            return {self,&s,thread,begin,end,initial?begin:origin,s.trap_revision,initial,width,2u,sites,original};
        }
    };
    class llvm_jit_debug_native_call_continuation final
    {
    public:
        static bool resume(step::session& s,llvm_jit_debug_native_breakpoint_plan plan,
            llvm_jit_debug_native_call_event& stale)
        {
            auto proof{::std::make_shared<llvm_jit_debug_native_call_continuation>()};
            auto window{::std::make_shared<llvm_jit_debug_native_call_event::resume_window>()};
            CHECK(s.register_snapshot.sp()<=UINTPTR_MAX);
            llvm_jit_debug_native_call_event event{window,proof,plan,static_cast<::std::uintptr_t>(s.register_snapshot.sp())};stale=event;
            window->active.store(true,::std::memory_order_release);
            bool const result{llvm_jit_debug_continue_native_call_event_host_api(&s,event)};
            window->active.store(false,::std::memory_order_release);return result;
        }
    };
}
static_assert(!::std::is_constructible_v<lib::llvm_jit_debug_native_call_event,::std::uintptr_t>);
static void wait(step::session& s,step::phase desired)
{
    auto const deadline{::std::chrono::steady_clock::now()+::std::chrono::seconds(10)};
    while(s.state.load(::std::memory_order_seq_cst)!=desired)
    { CHECK(::std::chrono::steady_clock::now()<deadline);::std::this_thread::yield(); }
}
int main()
{
    CHECK(step::install());unsigned completed{};
    auto const begin{reinterpret_cast<::std::uintptr_t>(call_begin)},end{reinterpret_cast<::std::uintptr_t>(call_end)};
    auto const origin{reinterpret_cast<::std::uintptr_t>(call_origin)},ret{reinterpret_cast<::std::uintptr_t>(call_return)};
    CHECK(begin<origin && origin<ret && ret<end);
    for(unsigned mode{};mode!=3u;++mode)
    {
        step::session s{};unsigned long result{};
        worker_ready=false;worker_start=false;block_helper=(mode==2u);helper_entered=false;helper_continue=false;descendant_hits=0u;
        ::std::thread worker{[&]
        {
            depth=0u;maximum_depth=(mode==1u?6u:0u);
            worker_tid.store(static_cast<::std::uint_least64_t>(step::details::raw_system_call(SYS_gettid)),::std::memory_order_release);
            worker_ready.store(true,::std::memory_order_release);
            while(!worker_start.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
            CHECK(step::arm_at_return(begin));result=fixture_owned_call();step::acknowledge_continuation_execution_exit();
        }};
        while(!worker_ready.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
        auto cursor{::std::make_shared<lib::llvm_jit_debug_native_activation_cursor>()};
        auto const tid{worker_tid.load(::std::memory_order_acquire)};
        auto const provider{lib::llvm_jit_debug_native_activation_cursor::provider(cursor,s,tid,begin,end)};
        auto const initial{lib::llvm_jit_debug_native_activation_cursor::plan(cursor,s,tid,true)};
        CHECK(step::request(s,tid,begin,end,begin,provider,initial));worker_start=true;
        wait(s,step::phase::at_guest_pc);CHECK(step::continue_one(s));wait(s,step::phase::trapped);
        CHECK(s.next_pc==origin && s.trap_revision==1u);
        auto const origin_sp{s.register_snapshot.sp()};
        CHECK(!lib::llvm_jit_debug_continue_native_call_event_host_api(&s,{}));
        lib::llvm_jit_debug_native_call_event stale{};
        auto const plan{lib::llvm_jit_debug_native_activation_cursor::plan(cursor,s,tid,false)};
        CHECK(lib::llvm_jit_debug_native_call_continuation::resume(s,plan,stale));
        if(mode==2u)
        {
            while(!helper_entered.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
            CHECK(step::release(s));wait(s,step::phase::released);helper_continue=true;
            CHECK(s.register_snapshot.size()==0u);
        }
        else
        {
            wait(s,step::phase::trapped);
            CHECK(s.next_pc==ret && s.trap_revision==2u && s.register_snapshot.sp()==origin_sp);
            CHECK(descendant_hits.load(::std::memory_order_relaxed)==(mode==1u?12u:0u));
            CHECK(!lib::llvm_jit_debug_continue_native_call_event_host_api(&s,stale));
            CHECK(step::release(s));wait(s,step::phase::released);
        }
        worker.join();CHECK(result==(mode==1u?48u:42u));
        for(auto patched:s.patched) { CHECK(!patched); }
        CHECK(step::clear(s));++completed;
    }
    CHECK(step::uninstall());
    ::fast_io::io::println("PASS actual native software calls scenarios=",completed,
        " recursion_levels=6 private_descendant_hits=12 cancellation_ack=true stale_seal_refused=true runtime-qualified=false");
}
