// Actual Linux/QEMU native backend fixture, without the runtime implementation.
// This test-only definition issues capabilities for an owned executable page;
// it is NOT a claim that the full Wasm runtime issuer has been qualified.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/debugger/native_step.h>
#include <fast_io.h>
#include <fast_io_dsal/string_view.h>
#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <sys/mman.h>

#if !UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
# error A qualified non-x86 Linux software backend is required.
#endif
namespace step = ::uwvm2::uwvm::debugger::native_step;
namespace regs = ::uwvm2::uwvm::debugger::native_registers;
namespace uwvm2::runtime::lib
{
    class llvm_jit_debug_native_activation_cursor final
    {
        static bool witness(void*,void const* identity,::std::uint_least64_t thread,::std::uintptr_t pc,::std::uintptr_t sp) noexcept
        { return step::in_owned_kernel_activation_witness(identity,thread,pc,sp); }
        static void leave(void*,void const*,::std::uint64_t,unsigned) noexcept {}
    public:
        static llvm_jit_debug_native_activation_provider provider(llvm_jit_debug_native_activation_cursor_owner const& self,
            step::session& s,::std::uint_least64_t thread,::std::uintptr_t begin,::std::uintptr_t end) noexcept
        { return {self,const_cast<llvm_jit_debug_native_activation_cursor*>(self.get()),witness,leave,&s,thread,begin,end,begin}; }
        static llvm_jit_debug_native_breakpoint_plan plan(llvm_jit_debug_native_activation_cursor_owner const& self,
            step::session& s,::std::uint_least64_t thread,::std::uintptr_t begin,::std::uintptr_t end,
            ::std::uintptr_t pc,::std::uint64_t revision,bool initial,::std::uintptr_t next) noexcept
        {
            ::std::array<::std::uintptr_t,3u> sites{}; ::std::array<::std::array<unsigned char,4u>,3u> original{};
            unsigned count{}; if(initial) { sites[count++] = pc; } sites[count++] = next;
            auto const width{step::details::breakpoint_width()};
            for(unsigned i{}; i != count; ++i) { for(unsigned j{}; j != width; ++j) { original[i][j] = reinterpret_cast<unsigned char const*>(sites[i])[j]; } }
            return {self,&s,thread,begin,end,pc,revision,initial,width,count,sites,original};
        }
    };
}
#define CHECK(value) do { if(!(value)) { ::fast_io::io::perrln("FAIL native Linux fixture line=",__LINE__); ::fast_io::fast_terminate(); } } while(false)
static void wait(step::session& s,step::phase phase)
{
    auto const deadline{::std::chrono::steady_clock::now()+::std::chrono::seconds(5)};
    while(s.state.load(::std::memory_order_seq_cst) != phase)
    { CHECK(::std::chrono::steady_clock::now()<deadline); ::std::this_thread::yield(); }
}
#if defined(__powerpc64__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
// ELFv1 callable pointers address a descriptor, not the first instruction.
// Keep the descriptor load across a real call boundary so optimized fixture
// code cannot fold its stack storage into a speculative indirect entry load.
[[gnu::noinline]] static unsigned long invoke_descriptor(::std::uintptr_t callable, double seed)
{ return reinterpret_cast<unsigned long (*)(double)>(callable)(seed); }
#endif
int main()
{
    CHECK(step::platform_available() && step::install());
    auto const page_size{static_cast<::std::size_t>(::sysconf(_SC_PAGESIZE))};
    auto* page{::mmap(nullptr,page_size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0)}; CHECK(page != MAP_FAILED);
    auto* bytes{static_cast<unsigned char*>(page)};
    unsigned first_size{4u},add_size{4u},value_index{},fp_index{};
#if defined(__aarch64__)
    ::std::uint32_t words[]{0xd2800540u,0x91000400u,0x91000400u,0xd65f03c0u}; value_index = 0u;
#elif defined(__powerpc__)
    ::std::uint32_t words[]{0x3860002au,0x38630001u,0x38630001u,0x4e800020u}; value_index = 3u; fp_index = 1u;
#elif defined(__mips__)
    ::std::uint32_t words[]{0x6402002au,0x64420001u,0x64420001u,0x03e00008u,0u}; value_index = 2u; fp_index = 12u;
#elif defined(__riscv)
    ::std::uint32_t words[]{0x02a00513u,0x00150513u,0x00150513u,0x00008067u}; value_index = 10u; fp_index = 10u;
#elif defined(__loongarch64)
    ::std::uint32_t words[]{0x0380a804u,0x02c00484u,0x02c00484u,0x4c000020u}; value_index = 4u;
#elif defined(__sparc__)
    ::std::uint32_t words[]{0x9010202au,0x90022001u,0x90022001u,0x81c3e008u,0x01000000u}; value_index = 8u;
#elif defined(__arm__)
    ::std::uint32_t words[]{0xe3a0002au,0xe2800001u,0xe2800001u,0xe12fff1eu}; value_index = 0u;
#elif defined(__i386__)
    unsigned char words[]{0xb8u,42u,0u,0u,0u,0x40u,0x40u,0xc3u}; first_size=5u;add_size=1u;value_index=0u;
#elif defined(__s390x__)
    unsigned char words[]{0xa7u,0x29u,0u,42u,0xa7u,0x2bu,0u,1u,0xa7u,0x2bu,0u,1u,0x07u,0xfeu}; value_index=2u;
#endif
#if defined(__arm__) && !defined(__ARM_PCS_VFP)
    // The base ARM PCS passes f64 in core registers/stack. It supplies no
    // VFP d0 seed, even if the saved signal frame has floating storage.
    constexpr bool abi_float_seed{false};
#else
    constexpr bool abi_float_seed{true};
#endif
#if defined(__powerpc__) && defined(__ALTIVEC__)
    constexpr bool abi_vmx_seed{true};
    struct alignas(16) vector_seed { ::std::array<unsigned char,16u> bytes; };
    vector_seed const vector_0{{0x00u,0x11u,0x22u,0x33u,0x44u,0x55u,0x66u,0x77u,
                                0x88u,0x99u,0xaau,0xbbu,0xccu,0xddu,0xeeu,0xffu}};
    vector_seed const vector_31{{0xffu,0xeeu,0xddu,0xccu,0xbbu,0xaau,0x99u,0x88u,
                                 0x77u,0x66u,0x55u,0x44u,0x33u,0x22u,0x11u,0x00u}};
    CHECK(::uwvm2::uwvm::debugger::native_linux_context::kernel_has_altivec);
    auto const check_vmx{[&](regs::snapshot const& raw)
    {
        CHECK((raw.values[33u]&0x02000000u)!=0u); // Actual signal-frame MSR, never a synthetic flag.
        CHECK(raw.floating.values[32u].width==16u && raw.floating.values[63u].width==16u);
        CHECK(raw.floating.values[32u].bytes==vector_0.bytes && raw.floating.values[63u].bytes==vector_31.bytes);
        regs::numeric_location const locations[]{{77u,128u},{108u,128u}};
        auto const view{regs::project(raw,locations,2u)};
        CHECK(view.floating.values[32u]==raw.floating.values[32u] && view.floating.values[63u]==raw.floating.values[63u]);
        for(unsigned n{};n!=64u;++n)
        {
            if(n==32u || n==63u) { continue; }
            CHECK(view.floating.values[n].width==0u);
            for(auto byte:view.floating.values[n].bytes) { CHECK(byte==0u); }
        }
        CHECK(view.sp()==0u && view.fp()==0u);
    }};
#else
    constexpr bool abi_vmx_seed{false};
#endif
    unsigned fp_dwarf{};
#if defined(__aarch64__)
    fp_dwarf=64u;
#elif defined(__powerpc__)
    fp_dwarf=32u+fp_index;
#elif defined(__mips__) || defined(__riscv) || defined(__loongarch64)
    fp_dwarf=32u+fp_index;
#elif defined(__sparc__)
    fp_dwarf=72u;
#elif defined(__arm__)
    fp_dwarf=256u;
#elif defined(__i386__)
    fp_dwarf=21u;
#elif defined(__s390x__)
    fp_dwarf=16u;
#endif
    ::std::memcpy(bytes,words,sizeof(words)); __builtin___clear_cache(reinterpret_cast<char*>(bytes),reinterpret_cast<char*>(bytes+sizeof(words)));
    CHECK(::mprotect(page,page_size,PROT_READ|PROT_EXEC) == 0);
    auto const begin{reinterpret_cast<::std::uintptr_t>(page)},end{begin+sizeof(words)};
    step::session s{}; ::std::atomic<::std::uint_least64_t> thread{}; ::std::atomic_bool go{};
    using cursor = ::uwvm2::runtime::lib::llvm_jit_debug_native_activation_cursor;
    auto const owner{::std::make_shared<cursor>()};
    ::std::thread worker{[&]()
    {
        thread.store(step::details::raw_system_call(SYS_gettid),::std::memory_order_release);
        while(!go.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
        CHECK(step::arm_at_return(begin));
#if defined(__powerpc64__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        ::std::uintptr_t descriptor[]{begin,0u,0u};
        auto const callable{reinterpret_cast<::std::uintptr_t>(descriptor)};
#else
        auto const function{reinterpret_cast<unsigned long (*)(double)>(begin)};
#endif
#if defined(__i386__)
        double const seed{42.0};
        __asm__ volatile("movsd %0, %%xmm0" : : "m"(seed) : "xmm0");
#endif
#if defined(__powerpc__) && defined(__ALTIVEC__)
        // Seed first/last physical vector registers immediately before the
        // fixture-owned entry. These are ABI controls, not Wasm provenance.
        __asm__ volatile("lvx 0,0,%0\n\tlvx 31,0,%1" : :
            "b"(vector_0.bytes.data()),"b"(vector_31.bytes.data()) : "v0","v31","memory");
#endif
#if defined(__powerpc64__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        CHECK(invoke_descriptor(callable,42.0) == 44u);
#else
        CHECK(function(42.0) == 44u);
#endif
    }};
    while(thread.load(::std::memory_order_acquire) == 0u) { ::std::this_thread::yield(); }
    auto const tid{thread.load(::std::memory_order_acquire)};
    CHECK(!step::request(s,tid,begin,end,begin));
    auto const provider{cursor::provider(owner,s,tid,begin,end)};
    auto const plan{cursor::plan(owner,s,tid,begin,end,begin,0u,true,begin+first_size)};
    CHECK(step::request(s,tid,begin,end,begin,provider,plan));
    CHECK(!step::arm_at_return(begin)); // Controller is not the selected worker.
    go.store(true,::std::memory_order_release); wait(s,step::phase::at_guest_pc);
    CHECK(s.first_pc == begin && s.first_signal_code > 0 && s.trap_revision == 0u);
    CHECK(step::continue_one(s)); wait(s,step::phase::trapped);
    CHECK(s.next_pc == begin+first_size && s.second_signal_code > 0 && s.trap_revision == 1u);
    unsigned queries{};
    CHECK(step::with_owned_registers_and_revision(&s,[&](auto t,auto pc,auto b,auto e,auto const& r,auto rev) noexcept
    { CHECK(t==tid && pc==begin+first_size && b==begin && e==end && r.pc()==pc && r.sp()!=0u && rev==1u); CHECK(r.values[value_index]==42u);
      // These fixture-owned locations test kernel bits through the ABI
      // projection; they are not compiler-issued Wasm location evidence.
      regs::numeric_location const integer{value_index,32u};
      auto const numeric{regs::project(r,&integer,1u)};
      CHECK(numeric.known_bits[value_index]==32u && numeric.values[value_index]==42u);
      for(::std::size_t i{};i!=numeric.size();++i)
      { if(i!=value_index && i!=regs::pc_index(r.machine)) { CHECK(numeric.known_bits[i]==0u && numeric.values[i]==0u); } }
      for(auto const& value:numeric.floating.values)
      { CHECK(value.width==0u);for(auto byte:value.bytes) { CHECK(byte==0u); } }
      if constexpr(abi_float_seed)
      {
      CHECK(r.floating.available && r.floating.values[fp_index].width >= 8u);
      ::std::uint64_t fp_bits{};
      for(unsigned i{}; i != 8u; ++i) { fp_bits |= static_cast<::std::uint64_t>(r.floating.values[fp_index].bytes[i]) << (8u*i); }
      CHECK(fp_bits == 0x4045000000000000ull);
      regs::numeric_location const floating{fp_dwarf,64u};
      auto const projected{regs::project(r,&floating,1u)};
      CHECK(projected.floating.values[fp_index].width==8u);
      for(unsigned i{};i!=8u;++i)
      { CHECK(projected.floating.values[fp_index].bytes[i]==static_cast<unsigned char>(fp_bits>>(8u*i))); }
      for(::std::size_t i{};i!=projected.floating.values.size();++i)
      {
          auto const& value{projected.floating.values[i]};
          for(unsigned byte{};byte!=16u;++byte)
          { if(i!=fp_index || byte>=8u) { CHECK(value.bytes[byte]==0u); } }
          if(i!=fp_index) { CHECK(value.width==0u); }
      }
      CHECK(projected.sp()==0u && projected.fp()==0u && r.sp()!=0u);
      }

#if defined(__powerpc__) && defined(__ALTIVEC__)
      check_vmx(r);
#endif
      ++queries; }));
    CHECK(step::with_owned_registers_and_revision(&s,[&](auto t,auto pc,auto b,auto e,auto const& r,auto rev,bool delay_slot) noexcept
    { CHECK(t==tid && pc==begin+first_size && b==begin && e==end && r.pc()==pc && rev==1u && !delay_slot);
      CHECK(r.values[value_index]==42u); }));
    CHECK(!step::with_owned_registers(reinterpret_cast<void const*>(::std::uintptr_t{1u}),[](auto...) noexcept { CHECK(false); }));
    auto next{cursor::plan(owner,s,tid,begin,end,s.next_pc,s.trap_revision,false,begin+first_size+add_size)};
    CHECK(step::stage_plan(s,next) && step::continue_from_trap(s)); wait(s,step::phase::trapped);
    CHECK(s.next_pc==begin+first_size+add_size && s.trap_revision==2u);
    CHECK(step::with_owned_registers(&s,[&](auto,auto,auto,auto,auto const& r) noexcept
    {
        CHECK(r.values[value_index]==43u);
#if defined(__powerpc__) && defined(__ALTIVEC__)
        check_vmx(r); // Vector state survives the preceding real signal return.
#endif
        ++queries;
    }));
    CHECK(!step::stage_plan(s,next)); // A prior real trap revision cannot resume this episode.
    CHECK(step::release(s)); wait(s,step::phase::released); worker.join();
    CHECK(step::clear(s) && queries==2u);
    // Cancellation before the selected worker leaves its cooperative poll
    // restores the entry patch without granting a physical register query.
    step::session pending{};
    auto const main_tid{static_cast<::std::uint_least64_t>(step::details::raw_system_call(SYS_gettid))};
    auto const pending_provider{cursor::provider(owner,pending,main_tid,begin,end)};
    auto const pending_plan{cursor::plan(owner,pending,main_tid,begin,end,begin,0u,true,begin+first_size)};
    CHECK(step::request(pending,main_tid,begin,end,begin,pending_provider,pending_plan));
    CHECK(step::release(pending) && pending.state.load()==step::phase::released);
    CHECK(!step::with_owned_registers(&pending,[](auto...) noexcept { CHECK(false); }));
    CHECK(step::clear(pending) && ::std::memcmp(bytes,words,sizeof(words))==0);
    // A real selected-worker execution exit before its first owned PC must
    // not leave an armed patch or a dangling worker borrow in the session.
    step::session exited{};go.store(false);thread.store(0u);
    ::std::thread exiting_worker{[&]
    {
        thread.store(step::details::raw_system_call(SYS_gettid),::std::memory_order_release);
        while(!go.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
        CHECK(step::arm_at_return(begin));step::acknowledge_continuation_execution_exit();
    }};
    while(thread.load(::std::memory_order_acquire)==0u) { ::std::this_thread::yield(); }
    auto const exit_tid{thread.load(::std::memory_order_acquire)};
    CHECK(step::request(exited,exit_tid,begin,end,begin,cursor::provider(owner,exited,exit_tid,begin,end),
          cursor::plan(owner,exited,exit_tid,begin,end,begin,0u,true,begin+first_size)));
    step::acknowledge_continuation_execution_exit(); // Controller has no worker TLS.
    CHECK(exited.state.load()==step::phase::ready);
    go.store(true,::std::memory_order_release);wait(exited,step::phase::released);exiting_worker.join();
    CHECK(!step::with_owned_registers(&exited,[](auto...) noexcept { CHECK(false); }));
    CHECK(step::clear(exited) && step::uninstall());
    CHECK(::std::memcmp(bytes,words,sizeof(words))==0); CHECK(::munmap(page,page_size)==0);
    ::fast_io::io::println("PASS genuine Linux native breakpoint stops=3 executed_instructions=2 actual_kernel_register_values=42,43 actual_kernel_float64=",abi_float_seed ? ::fast_io::string_view{"42.0"} : ::fast_io::string_view{"unqualified-soft-float-ABI"}," actual_kernel_numeric_projection=true unqualified_register_bytes_cleared=true stale_revision_denied=true original_code_restored=true ready_cancel_and_worker_exit_retired=true pointer_bits=",sizeof(void*)*8u,
        " actual_kernel_vmx128=",abi_vmx_seed ? ::fast_io::string_view{"true"} : ::fast_io::string_view{"unqualified-compiler-mode"});
}
