/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <chrono>
# include <cstdint>
# include <memory>
# include <utility>
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/utils/thread/native_thread_join.h>
# include <uwvm2/runtime/lib/uwvm_runtime.h>
# include "controller.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    enum class managed_cli_shutdown_phase : unsigned char
    {
        inactive, pending_native, pending_execution, pending_producers,
        pending_guest_thread, pending_release, completed, busy,
        unavailable, reentrant, stale_owner, failed
    };
    struct managed_cli_shutdown_result
    {
        managed_cli_shutdown_phase phase{managed_cli_shutdown_phase::inactive};
        ::uwvm2::runtime::lib::llvm_jit_debug_shutdown_status runtime{
            ::uwvm2::runtime::lib::llvm_jit_debug_shutdown_status::unsupported_mode};
        ::std::uint_least32_t native_error{};
        bool deadline_expired{};
    };
    [[nodiscard]] inline ::fast_io::string_view managed_cli_shutdown_phase_text(
        managed_cli_shutdown_phase phase) noexcept
    {
        switch(phase)
        {
            case managed_cli_shutdown_phase::inactive: return "inactive";
            case managed_cli_shutdown_phase::pending_native: return "native borrower retirement pending";
            case managed_cli_shutdown_phase::pending_execution: return "guest execution cleanup pending";
            case managed_cli_shutdown_phase::pending_producers: return "runtime producer retirement pending";
            case managed_cli_shutdown_phase::pending_guest_thread: return "guest OS thread retirement pending";
            case managed_cli_shutdown_phase::pending_release: return "runtime maintenance release pending";
            case managed_cli_shutdown_phase::completed: return "managed guest and runtime workers retired";
            case managed_cli_shutdown_phase::busy: return "another runtime management operation is active";
            case managed_cli_shutdown_phase::unavailable: return "managed cleanup unavailable for this compiled generation/provider";
            case managed_cli_shutdown_phase::reentrant: return "managed cleanup requires the normal host console thread";
            case managed_cli_shutdown_phase::stale_owner: return "runtime shutdown owner is stale";
            default: return "managed cleanup failed; owners retained";
        }
    }
    // Exclusive normal CLI owner. This class owns the actual controller and
    // privately minted runtime request and retains the runtime-owned actual
    // guest worker registration. The caller MUST retain all VM/
    // input/code owners for this entire nonmovable object's lifetime and every
    // pending return. The normal management thread must remain the actual
    // runtime request issuer; handing this object to another host thread does
    // not transfer its runtime authority. Neither callbacks, flags nor
    // serialized IDs supply ACK.
    // Only the actual runtime R5 producer-retirement ABI may be used: its
    // resources_quiescent means real producer OS joins, not idle WORK zero.
    class managed_cli_shutdown final
    {
        using clock_type = ::std::chrono::steady_clock;
        using runtime_status = ::uwvm2::runtime::lib::llvm_jit_debug_shutdown_status;
        ::std::shared_ptr<controller> controller_{};
        ::uwvm2::runtime::lib::llvm_jit_debug_guest_worker_owner guest_{};
        ::uwvm2::runtime::lib::llvm_jit_debug_shutdown_request_owner request_{};
        managed_cli_shutdown_result last_{};
        bool guest_joined_{}, completed_{};
        [[nodiscard]] static managed_cli_shutdown_phase runtime_failure(runtime_status state) noexcept
        {
            switch(state)
            {
                case runtime_status::busy: return managed_cli_shutdown_phase::busy;
                case runtime_status::reentrant: return managed_cli_shutdown_phase::reentrant;
                case runtime_status::stale_owner: return managed_cli_shutdown_phase::stale_owner;
                case runtime_status::unavailable_cleanup: case runtime_status::unsupported_mode:
                    return managed_cli_shutdown_phase::unavailable;
                default: return managed_cli_shutdown_phase::failed;
            }
        }
    public:
        explicit managed_cli_shutdown(::std::shared_ptr<controller> control,
            ::uwvm2::runtime::lib::llvm_jit_debug_guest_worker_owner launched_guest) noexcept :
            controller_{::std::move(control)}, guest_{::std::move(launched_guest)} {}
        managed_cli_shutdown(managed_cli_shutdown const&) = delete;
        managed_cli_shutdown& operator=(managed_cli_shutdown const&) = delete;
        managed_cli_shutdown(managed_cli_shutdown&&) = delete;
        managed_cli_shutdown& operator=(managed_cli_shutdown&&) = delete;
        // Actual host-controller pointee identity only. This is a cold binding
        // check, never a stop, join, source-generation or serialized authority.
        [[nodiscard]] bool matches_controller(controller const& supplied) const noexcept
        { return controller_.get() == ::std::addressof(supplied); }
        [[nodiscard]] bool requested() const noexcept { return static_cast<bool>(request_) || completed_; }
        [[nodiscard]] managed_cli_shutdown_result inspect() const noexcept { return last_; }
        [[nodiscard]] managed_cli_shutdown_result attempt_until(clock_type::time_point deadline) noexcept
        {
            if(completed_) { return last_; }
            if(!controller_ || !guest_ ||
               !::uwvm2::runtime::lib::runtime_llvm_jit_debug_guest_worker_matches_control_host_api(guest_, controller_->domain()))
            { last_.phase = managed_cli_shutdown_phase::failed; return last_; }
            last_.deadline_expired = false;
            // Actual linked ABI symbol rejects the old idle-WORK-only cleanup
            // contract. Revision is never a stop/join receipt: every real
            // native/runtime/guest-thread ACK below is still mandatory.
            if(::uwvm2::runtime::lib::runtime_debug_shutdown_terminal_cleanup_abi_host_api() != 2u)
            { last_.phase = managed_cli_shutdown_phase::unavailable; return last_; }
            if(!request_)
            {
                auto actual{::uwvm2::runtime::lib::runtime_begin_llvm_jit_debug_shutdown_host_api(controller_->domain())};
                last_.runtime = actual.status;
                // A cold publication failure can retain a privately minted
                // owner even without `started`. Keep that exact handle for
                // diagnostics/retry: never drop it or manufacture completion.
                if(actual.owner) { request_ = ::std::move(actual.owner); }
                if(actual.status != runtime_status::started || !request_)
                { last_.phase = runtime_failure(actual.status); return last_; }
                // Actual global runtime pins also survive every pending return.
            }
            for(;;)
            {
                auto const now{clock_type::now()};
                auto const proposed{now + ::std::chrono::milliseconds{20}};
                auto const slice{proposed < deadline ? proposed : deadline};
                auto const native{controller_->try_retire_native_for_shutdown(slice)};
                if(native != controller::native_shutdown_retirement::retired)
                {
                    last_.phase = native == controller::native_shutdown_retirement::pending ?
                        managed_cli_shutdown_phase::pending_native : managed_cli_shutdown_phase::failed;
                    if(native == controller::native_shutdown_retirement::invalid_state) { return last_; }
                }
                else
                {
                    auto const remaining{slice - clock_type::now()};
                    auto const milliseconds{::std::chrono::duration_cast<::std::chrono::milliseconds>(remaining).count()};
                    last_.runtime = ::uwvm2::runtime::lib::runtime_poll_llvm_jit_debug_shutdown_host_api(request_,
                        milliseconds > 0 ? static_cast<::std::uint_least64_t>(milliseconds) : 0u);
                    if(last_.runtime == runtime_status::pending_execution)
                    { last_.phase = managed_cli_shutdown_phase::pending_execution; }
                    else if(last_.runtime == runtime_status::pending_producers || last_.runtime == runtime_status::execution_drained)
                    { last_.phase = managed_cli_shutdown_phase::pending_producers; }
                    else if(last_.runtime == runtime_status::resources_quiescent)
                    {
                        if(!guest_joined_)
                        {
                            // [runtime-owned opaque guest worker] owner_end
                            // [safe] exact control binding checked before this
                            // synchronous call; the actual registration keeps
                            // its OS owner alive until physical join succeeds.
                            auto const joined{::uwvm2::runtime::lib::runtime_join_llvm_jit_debug_guest_worker_until_host_api(guest_, slice)};
                            last_.native_error = joined.actual.native_error;
                            if(joined.actual.status == ::fast_io::thread_join_status::joined) { guest_joined_ = true; }
                            else if(joined.actual.status == ::fast_io::thread_join_status::pending)
                            { last_.phase = managed_cli_shutdown_phase::pending_guest_thread; }
                            else
                            {
                                // No external joiner is allowed. not_joinable
                                // cannot replace our own actual join receipt.
                                last_.phase = joined.actual.status == ::fast_io::thread_join_status::unsupported ?
                                    managed_cli_shutdown_phase::unavailable : managed_cli_shutdown_phase::failed;
                                return last_;
                            }
                        }
                        if(guest_joined_)
                        {
                            if(::uwvm2::runtime::lib::runtime_release_llvm_jit_debug_shutdown_host_api(request_))
                            {
                                request_.reset(); completed_ = true;
                                last_.phase = managed_cli_shutdown_phase::completed; return last_;
                            }
                            last_.phase = managed_cli_shutdown_phase::pending_release;
                        }
                    }
                    else { last_.phase = runtime_failure(last_.runtime); return last_; }
                }
                if(clock_type::now() >= deadline)
                { last_.deadline_expired = true; return last_; }
                // Normal host management only. Signal handlers never allocate,
                // execute this pipeline, release a gate or join a native thread.
                ::fast_io::this_thread::sleep_for(::std::chrono::milliseconds{1});
            }
        }
    };
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
