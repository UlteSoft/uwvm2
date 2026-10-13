// Compile/link using the genuine runtime library; do not call any VM API.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
import uwvm2test.checkpoint_qualified_api_friends;
namespace api = ::uwvm2test::checkpoint_qualified_api_friends;
int main()
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // Volatile pointer materialization requires the genuine global API symbol
    // to be resolved at link time even though no VM API is called.
    auto volatile bind_source{api::bind_source};
    auto volatile mint_activation{api::mint_activation};
    auto volatile mint_native{api::mint_native};
    auto volatile query_native{api::query_native};
    if(bind_source == nullptr || mint_activation == nullptr ||
       mint_native == nullptr || query_native == nullptr) { return 1; }
# if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
    auto volatile capture_thread{api::capture_thread};
    auto volatile capture_instance{api::capture_instance};
    auto volatile read_wasm{api::read_wasm};
    auto volatile mutate_wasm{api::mutate_wasm};
    auto volatile read_wasip1{api::read_wasip1};
    auto volatile retire_saved{api::retire_saved};
    if(capture_thread == nullptr || capture_instance == nullptr ||
       read_wasm == nullptr || mutate_wasm == nullptr ||
       read_wasip1 == nullptr || retire_saved == nullptr) { return 2; }
# endif
#endif
    return 0;
}
