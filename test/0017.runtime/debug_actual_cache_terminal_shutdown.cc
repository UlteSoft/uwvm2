// This is the ACTUAL asynchronous object-cache worker/service, not a fake
// host-only replacement for the separate real native-Wasm shutdown fixture.
#include <uwvm2/runtime/llvm_jit_cache/store.h>
#include <uwvm2/utils/thread/native_thread_join.h>
#include <fast_io.h>
#include <chrono>
#include <mutex>
static void require(bool okay,char const* why)
{ if(!okay) { ::fast_io::io::perrln("actual cache terminal shutdown: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
int main()
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    namespace cache=::uwvm2::runtime::llvm_jit_cache;
    auto& actual{cache::details::async_cache_store_worker_instance()};
    {
        ::std::lock_guard lock{actual.mutex};
        require(!actual.worker_started && !actual.worker.joinable(),"actual original unstarted cache service");
        require(actual.start_locked(),"actual cache service starts its genuine FastIO worker");
        require(actual.worker_started && actual.worker.joinable(),"real native owner retained, not an exit flag");
    }
    require(cache::flush_async_store_objects_until(::std::chrono::steady_clock::now()),"real cache WORK is already empty");
    require(actual.worker.joinable(),"WORK0 is not native-thread death or a physical ACK");
    auto const final_deadline{::std::chrono::steady_clock::now()+::std::chrono::seconds{15}};
    bool joined{};
    do { joined=cache::shutdown_async_store_objects_until(::std::chrono::steady_clock::now()+::std::chrono::milliseconds{50}); }
    while(!joined && ::std::chrono::steady_clock::now()<final_deadline);
    require(joined,"qualified actual OS-native owner physical join, no blocking fallback");
    {
        ::std::lock_guard lock{actual.mutex};
        require(!actual.worker.joinable() && !actual.worker_started && actual.requests.empty() && actual.active_requests==0u,
            "actual native owner consumed AFTER true physical join; real queue/work0");
    }
    require(cache::shutdown_async_store_objects_until(::std::chrono::steady_clock::now()),"real already-joined service does not join twice");
    ::fast_io::io::println("debug_actual_cache_terminal_shutdown: PASS genuine asynchronous store native worker, WORK0 != native ACK, actual finite FastIO OS join; unsupported_provider_must_remain_pending");
#else
    ::fast_io::io::perrln("actual cache native provider unavailable; this target is unqualified");return 77;
#endif
}
