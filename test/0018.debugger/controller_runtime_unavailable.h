// Detached controller tests never create runtime capture or memory authority.
// Keep new host interfaces explicitly unavailable; real VM tests link runtime.o.
#include <uwvm2/utils/macro/push_macros.h>
namespace uwvm2::runtime::lib
{
    llvm_jit_checkpoint_capture_result llvm_jit_checkpoint_capture_thread_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&) noexcept { return {}; }
    ::uwvm2::uwvm::debugger::wasm_state::view llvm_jit_debug_query_wasm_state_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        ::uwvm2::uwvm::debugger::wasm_state::request const&) noexcept { return {}; }
    ::uwvm2::uwvm::debugger::wasm_mutation::result llvm_jit_debug_mutate_wasm_state_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        ::uwvm2::uwvm::debugger::wasm_mutation::request const&) noexcept { return {}; }
    ::uwvm2::uwvm::debugger::wasip1_state::view llvm_jit_debug_query_wasip1_state_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        ::uwvm2::uwvm::debugger::wasip1_state::request const&) noexcept { return {}; }
    llvm_jit_wasip1_environment_capsule_result llvm_jit_checkpoint_capture_wasip1_environment_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        llvm_jit_wasip1_environment_capsule_request const&) noexcept { return {}; }
    llvm_jit_wasip1_environment_capsule_data_result llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(
        llvm_jit_wasip1_environment_capsule_owner const&) noexcept { return {}; }
    llvm_jit_wasip1_environment_capsule_status llvm_jit_checkpoint_restore_wasip1_environment_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>, llvm_jit_wasip1_environment_capsule_owner const&,
        llvm_jit_wasip1_environment_restore_request const&) noexcept
    { return llvm_jit_wasip1_environment_capsule_status::not_selected; }
    bool llvm_jit_debug_with_source_memory_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>, llvm_jit_debug_activation_capture_owner const&,
        llvm_jit_debug_source_binding_owner const&, void*, llvm_jit_debug_source_memory_callback) noexcept { return false; }
    bool llvm_jit_debug_with_source_frame_memory_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>, llvm_jit_debug_activation_capture_owner const&,
        llvm_jit_debug_source_binding_owner const&, ::std::uint_least64_t, void*, llvm_jit_debug_source_memory_callback) noexcept { return false; }
    ::std::uint_least8_t llvm_jit_debug_source_memory_view::address_bytes() const noexcept { ::std::abort(); }
    bool llvm_jit_debug_source_memory_view::copy_position(llvm_jit_debug_source_activation_snapshot&) const noexcept { ::std::abort(); }
    bool llvm_jit_debug_source_memory_view::copy_frame_locals(llvm_jit_debug_source_frame_locals&) const noexcept { ::std::abort(); }
    bool llvm_jit_debug_source_memory_view::copy_guest(::std::uint_least64_t, ::std::size_t, llvm_jit_debug_source_object_copy&) noexcept { ::std::abort(); }
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    llvm_jit_debug_native_activation_cursor_owner llvm_jit_debug_mint_native_activation_host_api(
        llvm_jit_debug_activation_capture_owner const&, void const*) noexcept { return {}; }
    bool llvm_jit_debug_native_activation_provider_host_api(
        llvm_jit_debug_native_activation_cursor_owner const&, llvm_jit_debug_native_activation_provider&) noexcept { return false; }
    bool llvm_jit_debug_native_activation_host_api(
        llvm_jit_debug_native_activation_cursor_owner const&, void const*) noexcept { return false; }
    bool runtime_llvm_jit_debug_guest_worker_matches_control_host_api(llvm_jit_debug_guest_worker_owner const&,
        ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const&) noexcept { ::std::abort(); }
    ::std::uint_least32_t runtime_debug_shutdown_terminal_cleanup_abi_host_api() noexcept { ::std::abort(); }
    llvm_jit_debug_shutdown_start runtime_begin_llvm_jit_debug_shutdown_host_api(
        ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const&) noexcept { ::std::abort(); }
    llvm_jit_debug_shutdown_status runtime_poll_llvm_jit_debug_shutdown_host_api(
        llvm_jit_debug_shutdown_request_owner const&, ::std::uint_least64_t) noexcept { ::std::abort(); }
    ::uwvm2::utils::thread::physical_join_result runtime_join_llvm_jit_debug_guest_worker_until_host_api(
        llvm_jit_debug_guest_worker_owner const&, ::std::chrono::steady_clock::time_point) noexcept { ::std::abort(); }
    bool runtime_release_llvm_jit_debug_shutdown_host_api(llvm_jit_debug_shutdown_request_owner const&) noexcept { ::std::abort(); }
#endif
}
#include <uwvm2/utils/macro/pop_macros.h>
