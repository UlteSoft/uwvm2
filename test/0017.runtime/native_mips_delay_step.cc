// Real MIPS Linux/QEMU branch and architectural delay-slot retirement.
// A test-only issuer owns this executable page; no debugger authority is added.
#include <uwvm2/uwvm/debugger/native_step.h>
#include <fast_io.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <sys/mman.h>
#if !defined(__linux__) || !defined(__mips__) || __SIZEOF_POINTER__ != 8
# error This fixture requires MIPS64 Linux.
#endif
namespace step = uwvm2::uwvm::debugger::native_step;
namespace uwvm2::runtime::lib {
class llvm_jit_debug_native_activation_cursor final {
    static bool witness(void*,void const* identity,std::uint_least64_t thread,std::uintptr_t pc,std::uintptr_t sp) noexcept
    { return step::in_owned_kernel_activation_witness(identity,thread,pc,sp); }
    static void leave(void*,void const*,std::uint64_t,unsigned) noexcept {}
public:
    static llvm_jit_debug_native_activation_provider provider(llvm_jit_debug_native_activation_cursor_owner const& self,
        step::session& s,std::uint_least64_t thread,std::uintptr_t begin,std::uintptr_t end,std::uintptr_t pc) noexcept
    { return {self,const_cast<llvm_jit_debug_native_activation_cursor*>(self.get()),witness,leave,&s,thread,begin,end,pc}; }
    static llvm_jit_debug_native_breakpoint_plan plan(llvm_jit_debug_native_activation_cursor_owner const& self,
        step::session& s,std::uint_least64_t thread,std::uintptr_t begin,std::uintptr_t end,std::uintptr_t pc) noexcept
    {
        std::array<std::uintptr_t,3u> sites{pc,begin+20u,begin+12u};
        std::array<std::array<unsigned char,4u>,3u> original{};
        for(unsigned i{};i!=3u;++i) { std::memcpy(original[i].data(),reinterpret_cast<void const*>(sites[i]),4u); }
        return {self,&s,thread,begin,end,pc,0u,true,4u,3u,sites,original};
    }
};
}
#define CHECK(x) do { if(!(x)) { fast_io::io::perrln("FAIL real MIPS delay-step fixture line=",__LINE__); fast_io::fast_terminate(); } } while(false)
static void wait(step::session& s,step::phase phase)
{
    auto const deadline{std::chrono::steady_clock::now()+std::chrono::seconds(5)};
    while(s.state.load(std::memory_order_seq_cst)!=phase)
    { CHECK(std::chrono::steady_clock::now()<deadline);std::this_thread::yield(); }
}
int main()
{
    CHECK(step::platform_available() && step::install());
    auto const size{static_cast<std::size_t>(::sysconf(_SC_PAGESIZE))};
    void* const page{::mmap(nullptr,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0)};CHECK(page!=MAP_FAILED);
    std::uint32_t const words[]{0x6402002au,0x10800003u,0x64420001u,0x03e00008u,0x64420003u,0x03e00008u,0x6442000au};
    std::memcpy(page,words,sizeof(words));
    auto const begin{reinterpret_cast<std::uintptr_t>(page)},end{begin+sizeof(words)},pc{begin+4u};
    __builtin___clear_cache(reinterpret_cast<char*>(page),reinterpret_cast<char*>(page)+sizeof(words));
    CHECK(::mprotect(page,size,PROT_READ|PROT_EXEC)==0);
    using cursor=uwvm2::runtime::lib::llvm_jit_debug_native_activation_cursor;
    auto const owner{std::make_shared<cursor>()};
    for(unsigned argument{};argument!=2u;++argument)
    {
        step::session s{};std::atomic<std::uint_least64_t> tid{};std::atomic_bool go{};unsigned long result{};
        std::thread worker{[&] {
            tid.store(step::details::raw_system_call(SYS_gettid),std::memory_order_release);
            while(!go.load(std::memory_order_acquire)) { std::this_thread::yield(); }
            CHECK(step::arm_at_return(pc));
            result=reinterpret_cast<unsigned long(*)(unsigned long)>(begin)(argument);
        }};
        while(tid.load(std::memory_order_acquire)==0u) { std::this_thread::yield(); }
        auto const thread{tid.load(std::memory_order_acquire)};
        CHECK(step::request(s,thread,begin,end,pc,cursor::provider(owner,s,thread,begin,end,pc),cursor::plan(owner,s,thread,begin,end,pc)));
        go.store(true,std::memory_order_release);wait(s,step::phase::at_guest_pc);
        CHECK(s.first_pc==pc && s.trap_revision==0u);
        CHECK(step::continue_one(s));wait(s,step::phase::trapped);
        auto const expected{begin+(argument==0u?20u:12u)};
        CHECK(s.next_pc==expected && s.trap_revision==1u);
        CHECK(step::with_owned_registers(&s,[&](auto t,auto p,auto b,auto e,auto const& raw) noexcept {
            CHECK(t==thread && p==expected && b==begin && e==end && raw.pc()==expected && raw.values[2u]==43u);
        }));
        CHECK(step::release(s));wait(s,step::phase::released);worker.join();
        CHECK(result==(argument==0u?53u:46u));CHECK(step::clear(s));
        CHECK(std::memcmp(page,words,sizeof(words))==0);
    }
    CHECK(::munmap(page,size)==0);
    fast_io::io::println("PASS real MIPS branch+delay retirement, taken/fallthrough kernel stops, exact numeric value and patch restoration");
}
