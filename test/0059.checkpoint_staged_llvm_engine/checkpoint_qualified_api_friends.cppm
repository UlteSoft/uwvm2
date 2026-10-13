// Actual runtime API names only. The keeper must first compile the real
// runtime implementation containing the original class friend declarations.
module;
#include <cstddef>
#include <memory>
#include <type_traits>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
export module uwvm2test.checkpoint_qualified_api_friends;
import uwvm2.runtime;
export namespace uwvm2test::checkpoint_qualified_api_friends
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    namespace runtime = ::uwvm2::runtime::lib;
    using source_owner = runtime::llvm_jit_debug_source_binding_owner;
    using activation_owner = runtime::llvm_jit_debug_activation_capture_owner;
    using native_owner = runtime::llvm_jit_debug_native_activation_cursor_owner;
    inline constexpr auto bind_source = &runtime::llvm_jit_debug_bind_source_host_api;
    inline constexpr auto mint_activation = &runtime::llvm_jit_debug_capture_activation_host_api;
    inline constexpr auto mint_native = &runtime::llvm_jit_debug_mint_native_activation_host_api;
    inline constexpr auto query_native = &runtime::llvm_jit_debug_native_activation_provider_host_api;
    static_assert(::std::is_same_v<::std::invoke_result_t<decltype(bind_source), ::std::size_t>, source_owner>);
    static_assert(::std::is_same_v<::std::invoke_result_t<decltype(mint_native), activation_owner const&, void const*>, native_owner>);
# if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
    using checkpoint_owner = runtime::llvm_jit_checkpoint_thread_capture_owner;
    inline constexpr auto capture_thread = &runtime::llvm_jit_checkpoint_capture_thread_host_api;
    inline constexpr auto capture_instance = &runtime::llvm_jit_checkpoint_capture_instance_host_api;
    inline constexpr auto read_wasm = &runtime::llvm_jit_debug_query_wasm_state_host_api;
    inline constexpr auto mutate_wasm = &runtime::llvm_jit_debug_mutate_wasm_state_host_api;
    inline constexpr auto read_wasip1 = &runtime::llvm_jit_debug_query_wasip1_state_host_api;
    inline constexpr auto retire_saved = &runtime::llvm_jit_checkpoint_retire_saved_execution_host_api;
    static_assert(::std::is_same_v<checkpoint_owner, ::std::shared_ptr<runtime::llvm_jit_checkpoint_thread_capture const>>);
# endif
#endif
}
