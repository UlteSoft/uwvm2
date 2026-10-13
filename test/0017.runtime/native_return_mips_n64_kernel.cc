// Real N64 BE/LE kernel return/cancellation backend, with a TEST-ONLY issuer.
// These owned synthetic functions are not Wasm, CFI or full-product evidence.
#include <uwvm2/uwvm/debugger/native_step.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <type_traits>
#include <sys/mman.h>
#if !defined(__linux__) || !defined(__mips64) || __SIZEOF_POINTER__ != 8 || __mips_isa_rev != 2 || defined(__mips16) || defined(__mips_micromips)
# error Ordinary Linux MIPS N64 R2 required.
#endif
namespace step=uwvm2::uwvm::debugger::native_step;
namespace lib=uwvm2::runtime::lib;
namespace regs=uwvm2::uwvm::debugger::native_registers;
#define CHECK(x) do { if(!(x)) { fast_io::io::perrln("N64 return fixture line=",__LINE__);fast_io::fast_terminate(); } } while(false)
static std::uintptr_t expected_stack{};
namespace uwvm2::runtime::lib
{
    class llvm_jit_debug_native_activation_cursor final
    {
        static bool witness(void*,void const* identity,std::uint_least64_t thread,std::uintptr_t pc,std::uintptr_t sp) noexcept
        { return (expected_stack==0u || sp==expected_stack) && step::in_owned_kernel_activation_witness(identity,thread,pc,sp); }
        static bool descendant(void*,void const* identity,std::uint_least64_t thread,std::uintptr_t pc,std::uintptr_t sp) noexcept
        { return expected_stack!=0u && sp<expected_stack && (expected_stack-sp)%48u==0u && step::in_owned_kernel_activation_witness(identity,thread,pc,sp); }
        static void leave(void*,void const*,std::uint64_t,unsigned) noexcept {}
    public:
        static llvm_jit_debug_native_activation_provider provider(llvm_jit_debug_native_activation_cursor_owner const& self,
            step::session& s,std::uint_least64_t thread,std::uintptr_t begin,std::uintptr_t end,std::uintptr_t pc) noexcept
        { return {self,const_cast<llvm_jit_debug_native_activation_cursor*>(self.get()),witness,leave,&s,thread,begin,end,pc,descendant}; }
        static llvm_jit_debug_native_breakpoint_plan initial(llvm_jit_debug_native_activation_cursor_owner const& self,
            step::session& s,std::uint_least64_t thread,std::uintptr_t begin,std::uintptr_t end) noexcept
        {
            std::array<std::uintptr_t,3u> sites{begin+4u,begin+8u,0u};
            std::array<std::array<unsigned char,4u>,3u> original{};
            for(unsigned i{};i!=2u;++i) { std::memcpy(original[i].data(),reinterpret_cast<void const*>(sites[i]),4u); }
            return {self,&s,thread,begin,end,sites[0u],0u,true,4u,2u,sites,original};
        }
    };
    class llvm_jit_debug_native_return_continuation final
    {
    public:
        static bool resume(step::session& s,llvm_jit_debug_native_activation_cursor_owner const& parent,
            std::uintptr_t begin,std::uintptr_t end,std::uintptr_t target,llvm_jit_debug_native_return_event& stale)
        {
            auto window{std::make_shared<llvm_jit_debug_native_return_event::resume_window>()};
            auto proof{std::make_shared<llvm_jit_debug_native_return_continuation>()};
            std::array<unsigned char,4u> original{},skip{};
            std::memcpy(original.data(),reinterpret_cast<void const*>(target),4u);
            std::memcpy(skip.data(),reinterpret_cast<void const*>(target+4u),4u);
            auto const provider{llvm_jit_debug_native_activation_cursor::provider(parent,s,s.target_thread,begin,end,target)};
            // Only this fixture's known child frame is 32 bytes. Production
            // uses its freshly authenticated Wasm/CFI chain, never this formula.
            expected_stack=s.register_snapshot.sp()+32u;
            llvm_jit_debug_native_return_event event{window,proof,parent,provider,&s,s.target_thread,
                s.next_pc,static_cast<std::uintptr_t>(s.register_snapshot.sp()),s.owner_begin,s.owner_end,s.trap_revision,
                target,expected_stack,begin,end,target+4u,original,skip};stale=event;
            window->active.store(true,std::memory_order_release);
            CHECK(!step::continue_to_return(s,event,0u,-1));
            CHECK(!step::continue_to_return(s,event,1u,0));
            CHECK(!lib::llvm_jit_debug_continue_native_return_event_host_api(nullptr,event,1u,-1));
            bool const result{step::continue_to_return(s,event,1u,-1)};
            window->active.store(false,std::memory_order_release);return result;
        }
    };
}
static_assert(!std::is_constructible_v<lib::llvm_jit_debug_native_return_event,std::uintptr_t>);
static void wait(step::session& s,step::phase desired)
{
    auto const end{std::chrono::steady_clock::now()+std::chrono::seconds(20)};
    while(s.state.load(std::memory_order_seq_cst)!=desired)
    { CHECK(std::chrono::steady_clock::now()<end);std::this_thread::yield(); }
}
int main()
{
    CHECK(step::install());auto const size{static_cast<std::size_t>(::sysconf(_SC_PAGESIZE))};
    void* const page{::mmap(nullptr,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0)};CHECK(page!=MAP_FAILED);
    // Parent frame 16 bytes, child frame 32 bytes. A real JALR plus owned NOP
    // delay slot returns to parent+20. Each recursive parent shares that PC
    // while its kernel SP is lower by another 48 bytes.
    std::uint32_t const parent_words[]{0x67bdfff0u,0xffbf0008u,0x00a0c82du,0x0320f809u,0u,
        0x64420001u,0u,0xdfbf0008u,0x67bd0010u,0x03e00008u,0u};
    std::uint32_t const child_words[]{0x67bdffe0u,0xffbf0018u,0u,0x10800008u,0u,
        0x6484ffffu,0x00c0c82du,0x0320f809u,0u,0x10000003u,0u,0u,0x6402002au,
        0x3c0c0010u,0x658cffffu,0x1580fffeu,0u,0xdfbf0018u,0x67bd0020u,0x03e00008u,0u};
    auto* bytes{static_cast<unsigned char*>(page)};
    std::memcpy(bytes,parent_words,sizeof(parent_words));std::memcpy(bytes+128u,child_words,sizeof(child_words));
    __builtin___clear_cache(reinterpret_cast<char*>(page),reinterpret_cast<char*>(page)+size);
    CHECK(::mprotect(page,size,PROT_READ|PROT_EXEC)==0);
    auto const parent{reinterpret_cast<std::uintptr_t>(page)},child{parent+128u};
    auto const parent_end{parent+sizeof(parent_words)},child_end{child+sizeof(child_words)},target{parent+20u};
    unsigned actual_returns{},actual_cancels{},recursive_stops{};
    for(unsigned mode{};mode!=3u;++mode)
    {
        // Cancellation gets a longer, bounded contained loop before the
        // parent event. There is no host helper or arbitrary native read.
        CHECK(::mprotect(page,size,PROT_READ|PROT_WRITE)==0);
        auto patched_child=std::to_array(child_words);
        if(mode==2u) { patched_child[13u]=0x3c0c1000u; }
        std::memcpy(bytes+128u,patched_child.data(),sizeof(child_words));
        __builtin___clear_cache(reinterpret_cast<char*>(bytes+128u),reinterpret_cast<char*>(bytes+128u+sizeof(child_words)));
        CHECK(::mprotect(page,size,PROT_READ|PROT_EXEC)==0);
        step::session s{};expected_stack=0u;std::atomic_bool go{};std::atomic<std::uint_least64_t> tid{};unsigned long result{};
        unsigned const depth{mode==1u?6u:0u};
        auto const other_before{step::details::return_other_stack_hits.load(std::memory_order_relaxed)};
        auto const skip_before{step::details::return_skip_hits.load(std::memory_order_relaxed)};
        std::thread worker{[&] {
            tid.store(step::details::raw_system_call(SYS_gettid),std::memory_order_release);
            while(!go.load(std::memory_order_acquire)) { std::this_thread::yield(); }
            CHECK(step::arm_at_return(child+4u));
            using fn=unsigned long(*)(unsigned long,std::uintptr_t,std::uintptr_t);
            result=reinterpret_cast<fn>(parent)(depth,child,parent);
            step::acknowledge_continuation_execution_exit();
        }};
        while(tid.load(std::memory_order_acquire)==0u) { std::this_thread::yield(); }
        auto const owner{std::make_shared<lib::llvm_jit_debug_native_activation_cursor>()};
        auto const thread{tid.load(std::memory_order_acquire)};
        auto const provider{lib::llvm_jit_debug_native_activation_cursor::provider(owner,s,thread,child,child_end,child+4u)};
        CHECK(step::request(s,thread,child,child_end,child+4u,provider,
            lib::llvm_jit_debug_native_activation_cursor::initial(owner,s,thread,child,child_end)));
        go.store(true,std::memory_order_release);wait(s,step::phase::at_guest_pc);
        CHECK(step::continue_one(s));wait(s,step::phase::trapped);CHECK(s.next_pc==child+8u && s.trap_revision==1u);
        lib::llvm_jit_debug_native_return_event empty{},stale{};
        CHECK(!step::continue_to_return(s,empty,1u,-1));
        CHECK(lib::llvm_jit_debug_native_return_continuation::resume(s,owner,parent,parent_end,target,stale));
        CHECK(!step::continue_to_return(s,stale,1u,-1));
        if(mode==2u)
        {
            CHECK(s.state.load(std::memory_order_seq_cst)==step::phase::continuation_running);
            CHECK(step::release(s));wait(s,step::phase::released);++actual_cancels;
            CHECK(s.register_snapshot.size()==0u);
        }
        else
        {
            wait(s,step::phase::trapped);CHECK(s.next_pc==target && s.trap_revision==2u && s.register_snapshot.sp()==expected_stack);
            CHECK(step::with_owned_registers(&s,[&](auto t,auto pc,auto b,auto e,auto const& raw) noexcept {
                CHECK(t==thread && pc==target && b==parent && e==parent_end && raw.values[2u]==42u+depth);
                regs::numeric_location const locations[]{{2u,64u},{28u,64u},{29u,64u},{30u,64u},{31u,64u}};
                auto const numeric{regs::project(raw,locations,5u)};
                CHECK(numeric.known_bits[2u]==64u && numeric.values[2u]==42u+depth);
                CHECK(numeric.known_bits[28u]==0u && numeric.known_bits[29u]==0u && numeric.known_bits[30u]==0u && numeric.known_bits[31u]==0u);
            }));
            auto const other{step::details::return_other_stack_hits.load(std::memory_order_relaxed)-other_before};
            auto const skipped{step::details::return_skip_hits.load(std::memory_order_relaxed)-skip_before};
            CHECK(other==depth && skipped==depth);recursive_stops+=other;++actual_returns;
            CHECK(step::release(s));wait(s,step::phase::released);
        }
        worker.join();CHECK(result==43u+depth);
        step::acknowledge_continuation_event_retired(s);CHECK(step::clear(s));
        CHECK(!s.return_patched && !s.return_skip_patched && std::memcmp(bytes,parent_words,sizeof(parent_words))==0);
        CHECK(std::memcmp(bytes+128u,patched_child.data(),sizeof(child_words))==0);
    }
    CHECK(::munmap(page,size)==0);CHECK(step::uninstall());
    fast_io::io::println("PASS real N64 kernel return: returns=",actual_returns," recursive-other-stack=",recursive_stops,
        " cancel-running=",actual_cancels," patch-restoration=true hidden-ABI=true public-native-stack-bytes=0; test-only issuer, zero Wasm VM");
}
