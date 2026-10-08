// Actual LLVM-full debugger admission, code-owner proof and one native step.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/uwvm/debugger/native_step.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::abort(); } } while(false)
namespace strict = uwvm2test::uwvm_int_strict;
namespace lib = uwvm2::runtime::lib;
namespace mode = uwvm2::uwvm::runtime::runtime_mode;
namespace threads = uwvm2::utils::thread;
namespace step = uwvm2::uwvm::debugger::native_step;
using domain = threads::cooperative_pause_domain;

auto deadline() { return std::chrono::steady_clock::now()+std::chrono::seconds{15}; }
void await_phase(step::session& session, step::phase expected)
{
    auto const end=deadline();
    while(session.state.load(std::memory_order_acquire)!=expected)
    {
        CHECK(std::chrono::steady_clock::now()<end);
        std::this_thread::yield();
    }
}
struct observer
{
    std::shared_ptr<domain> control{};
    std::mutex mutex{};
    std::condition_variable changed{};
    domain::pause_ticket ticket{};
    threads::cooperative_pause_location location{};
    lib::llvm_jit_debug_native_step_site site{};
    std::atomic_bool requested{};
    bool captured{};

    static void at_safe_point(void* opaque,std::uint_least64_t participant,
                              threads::cooperative_pause_location location) noexcept
    {
        auto& self=*static_cast<observer*>(opaque);
        CHECK(participant!=0);
        if(self.requested.exchange(true,std::memory_order_acq_rel)) { return; }
        auto ticket=self.control->request_pause(); CHECK(ticket);
        std::lock_guard lock{self.mutex};
        self.ticket=std::move(ticket); self.location=location;
    }
    static void before_park(void* opaque,std::uint_least64_t participant,
                            threads::cooperative_pause_location location,
                            lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self=*static_cast<observer*>(opaque);
        auto const site=lib::llvm_jit_capture_debug_native_step_site_host_api();
        std::lock_guard lock{self.mutex};
        if(!self.ticket || participant!=site.participant || location!=self.location)
        {
            std::fprintf(stderr,"native-step capture: ticket=%d participant=%llu site.participant=%llu site.valid=%d return_pc=%p owner=[%p,%p) location=(%llu,%llu,%llu,%llu) expected=(%llu,%llu,%llu,%llu)\n",
                static_cast<bool>(self.ticket),static_cast<unsigned long long>(participant),
                static_cast<unsigned long long>(site.participant),site.valid,
                reinterpret_cast<void*>(site.return_pc),reinterpret_cast<void*>(site.owner_begin),
                reinterpret_cast<void*>(site.owner_end),
                static_cast<unsigned long long>(location.code_unit),static_cast<unsigned long long>(location.function),
                static_cast<unsigned long long>(location.offset),static_cast<unsigned long long>(location.code_generation),
                static_cast<unsigned long long>(self.location.code_unit),static_cast<unsigned long long>(self.location.function),
                static_cast<unsigned long long>(self.location.offset),static_cast<unsigned long long>(self.location.code_generation));
        }
        CHECK(self.ticket && participant==site.participant && location==self.location);
        self.site=site;self.captured=true;self.changed.notify_all();
    }
};

int main(int argc,char** argv)
{
    CHECK(argc==3 && step::platform_available());
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<char> input{std::istreambuf_iterator<char>{file},{}};CHECK(file&&!input.empty());
    strict::byte_vec bytes(input.size());std::memcpy(bytes.data(),input.data(),input.size());
    auto features=strict::make_wasm1p1_feature_parameter();
    auto prepared=strict::prepare_runtime_from_wasm(bytes,u8"native-step",{},features);
    mode::global_runtime_mode=mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler=mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack=std::string_view{argv[2]}=="instruction"
        ? mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    CHECK(!lib::llvm_jit_enable_debug_native_step_host_api());
    CHECK(!lib::llvm_jit_capture_debug_native_step_site_host_api().valid);
    auto state=std::make_shared<observer>();state->control=std::make_shared<domain>(1);
    CHECK(lib::llvm_jit_configure_debug_session_host_api(state->control,
        {state,observer::at_safe_point,observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction)==lib::llvm_jit_debug_configure_result::ok);
    CHECK(lib::llvm_jit_prepare_debug_host_api());
    CHECK(lib::llvm_jit_enable_debug_native_step_host_api());
    std::uint32_t output{};
    std::thread worker{[&]
    {
        lib::full_compile_run_config config{};config.entry_function_index=0;
        config.entry_abi_buffers.result_buffer=reinterpret_cast<std::byte*>(&output);
        config.entry_abi_buffers.result_bytes=sizeof(output);
        lib::full_compile_and_run_main_module(u8"native-step",config);
    }};
    lib::llvm_jit_debug_native_step_site site{};
    domain::pause_ticket ticket{};
    threads::cooperative_pause_location location{};
    {
        std::unique_lock lock{state->mutex};
        CHECK(state->changed.wait_until(lock,deadline(),[&]{return state->captured;}));
        site=state->site;ticket=state->ticket;location=state->location;
    }
    CHECK(site.valid && site.participant!=0 && site.native_thread!=0);
    CHECK(site.return_pc>=site.owner_begin && site.return_pc<site.owner_end);
    CHECK(!lib::llvm_jit_capture_debug_native_step_site_host_api().valid);
    CHECK(state->control->wait_until_paused(ticket,deadline())==threads::cooperative_pause_result::paused);
    auto snapshot=state->control->capture(ticket);
    CHECK(snapshot.participants.size()==1 && snapshot.participants[0].id==site.participant);
    step::session session{};
    CHECK(!step::request(session,site.native_thread,site.owner_begin,site.return_pc,site.return_pc));
    CHECK(step::request(session,site.native_thread,site.owner_begin,site.owner_end,site.return_pc));
    CHECK(state->control->release_one_for_native_step(ticket,site.participant));
    await_phase(session,step::phase::at_guest_pc);
    CHECK(session.first_pc==site.return_pc && session.first_signal_code==TRAP_TRACE);
    CHECK(state->control->external_park(ticket,site.participant,location));
    CHECK(state->control->wait_until_paused(ticket,deadline())==threads::cooperative_pause_result::paused);
    CHECK(state->control->external_unpark(ticket,site.participant));
    step::continue_one(session);
    await_phase(session,step::phase::trapped);
    CHECK(session.next_pc!=session.first_pc && session.second_signal_code==TRAP_TRACE);
    CHECK(state->control->external_park(ticket,site.participant,location));
    CHECK(state->control->wait_until_paused(ticket,deadline())==threads::cooperative_pause_result::paused);
    CHECK(state->control->external_unpark(ticket,site.participant));
    step::release(session);await_phase(session,step::phase::released);
    CHECK(step::clear(session));
    CHECK(state->control->resume(ticket));
    worker.join();CHECK(output==42);
    lib::reset_runtime_state_host_api();CHECK(state->control->is_closed());
    CHECK(step::uninstall());
    std::printf("PASS actual LLVM full native step: owner [%p,%p), PC %p -> %p, two precise SIGTRAP stops (%s)\n",
        reinterpret_cast<void*>(site.owner_begin),reinterpret_cast<void*>(site.owner_end),
        reinterpret_cast<void*>(session.first_pc),reinterpret_cast<void*>(session.next_pc),argv[2]);
}
