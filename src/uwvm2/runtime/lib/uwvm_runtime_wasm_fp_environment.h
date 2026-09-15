/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#if !defined(UWVM_ASSUME_FIXED_WASM_FP_ENVIRONMENT) && !defined(__wasm__)
# include <cfenv>
# include <cstdint>
# include <exception>
# include <memory>
# include <type_traits>
# include "uwvm_runtime_wasm_fp_aux_environment.h"
# include "uwvm_runtime_wasm_fp_native_control.h"
#endif

namespace uwvm2::runtime::lib::details
{
#if defined(UWVM_ASSUME_FIXED_WASM_FP_ENVIRONMENT) || defined(__wasm__)
    // A presence-defined macro enables the contract, even if defined as 0. It is
    // not run-time detection, and not an option to disable all sandbox checks.
    // Every entry/callback must already preserve RN-even, gradual underflow,
    // masked exceptions and sufficient precision (x87 PC=53/64, not PC=24).
    // Eliding controls cannot fix an FP ABI's sNaN transport, excess-precision
    // double rounding, or a host's different NaN encoding; those fixes stay active.
    // Opt-in whole-thread embedding contract, not a property inferred from a Wasm module: every entry already has
    // Wasm-compatible controls and every native callback/signal handler preserves them. Accrued status is caller-saved.
    // A Wasm host has these semantics by construction. See documents/runtime/floating-point-environment.md.
    inline constexpr bool wasm_fp_environment_is_fixed{true};
    [[nodiscard]] inline constexpr bool is_llvm_wasm_fp_environment_active(bool active) noexcept { return active; }

    class scoped_llvm_wasm_fp_environment
    {
        bool* active_state;
        bool previous_active;
        bool enabled;

    public:
        explicit constexpr scoped_llvm_wasm_fp_environment(bool& active, bool enable = true) noexcept
            : active_state{&active}, previous_active{active}, enabled{enable}
        { if(enabled) { active = true; } }
        scoped_llvm_wasm_fp_environment(scoped_llvm_wasm_fp_environment const&) = delete;
        scoped_llvm_wasm_fp_environment& operator=(scoped_llvm_wasm_fp_environment const&) = delete;
        constexpr ~scoped_llvm_wasm_fp_environment() noexcept { if(enabled) { *active_state = previous_active; } }
        [[nodiscard]] static constexpr bool ready() noexcept { return true; }
    };

    class scoped_llvm_wasm_host_fp_environment_restore
    {
    public:
        explicit constexpr scoped_llvm_wasm_host_fp_environment_restore(bool = true) noexcept {}
        scoped_llvm_wasm_host_fp_environment_restore(scoped_llvm_wasm_host_fp_environment_restore const&) = delete;
        scoped_llvm_wasm_host_fp_environment_restore& operator=(scoped_llvm_wasm_host_fp_environment_restore const&) = delete;
        [[nodiscard]] static constexpr bool ready() noexcept { return true; }
    };
    using scoped_wasm_host_fp_control_restore = scoped_llvm_wasm_host_fp_environment_restore;
#else
    inline constexpr bool wasm_fp_environment_is_fixed{false};

    // Interpreter and generated Wasm FP instructions both assume the IEEE default environment: round-to-nearest/
    // ties-to-even, gradual underflow, and masked exceptions. Save the embedding environment once at a public execution
    // entry, not in individual Wasm opfuncs. Keep the historical LLVM names for existing users of this header.
    // The caller owns the per-thread active marker; do not force C++ thread_local into map-backed runtime builds.
    [[nodiscard]] inline constexpr bool is_llvm_wasm_fp_environment_active(bool active) noexcept { return active; }

    class scoped_llvm_wasm_fp_environment
    {
        [[no_unique_address]] scoped_wasm_auxiliary_fp_environment auxiliary;
        ::std::fenv_t saved_environment{};
        bool* active_state{};
        bool previous_active{};
        bool restore_environment{};
        bool ready_state{true};

    public:
        explicit scoped_llvm_wasm_fp_environment(bool& active, bool enable = true) noexcept
            : auxiliary{enable, true}, active_state{::std::addressof(active)}, previous_active{active}
        {
            if(!enable) { return; }

            ready_state = false;
            if(::std::fegetenv(::std::addressof(saved_environment)) != 0) [[unlikely]] { return; }
            restore_environment = true;
            if(::std::fesetenv(FE_DFL_ENV) != 0) [[unlikely]] { return; }

            *active_state = true;
            ready_state = true;
        }

        scoped_llvm_wasm_fp_environment(scoped_llvm_wasm_fp_environment const&) = delete;
        scoped_llvm_wasm_fp_environment& operator=(scoped_llvm_wasm_fp_environment const&) = delete;

        ~scoped_llvm_wasm_fp_environment() noexcept
        {
            if(!restore_environment) { return; }
            *active_state = previous_active;
            if(::std::fesetenv(::std::addressof(saved_environment)) != 0) [[unlikely]] { ::std::terminate(); }
        }

        [[nodiscard]] bool ready() const noexcept { return ready_state; }
    };

    class scoped_llvm_wasm_host_fp_environment_restore
    {
        [[no_unique_address]] scoped_wasm_auxiliary_fp_environment auxiliary;
        ::std::fenv_t saved_environment{};
        bool restore_environment{};
        bool ready_state{true};

    public:
        explicit scoped_llvm_wasm_host_fp_environment_restore(bool active = true) noexcept : auxiliary{active}
        {
            if(!active) { return; }

            ready_state = false;
            if(::std::fegetenv(::std::addressof(saved_environment)) != 0) [[unlikely]] { return; }
            restore_environment = true;
            ready_state = true;
        }

        scoped_llvm_wasm_host_fp_environment_restore(scoped_llvm_wasm_host_fp_environment_restore const&) = delete;
        scoped_llvm_wasm_host_fp_environment_restore& operator=(scoped_llvm_wasm_host_fp_environment_restore const&) = delete;

        ~scoped_llvm_wasm_host_fp_environment_restore() noexcept
        {
            if(!restore_environment) { return; }
            if(::std::fesetenv(::std::addressof(saved_environment)) != 0) [[unlikely]] { ::std::terminate(); }
        }

        [[nodiscard]] bool ready() const noexcept { return ready_state; }
    };

    // A provider global access is a frequent native callback, not a public host entry. Wasm cannot observe accrued
    // exception flags, so this boundary only promises to restore controls (including x87 precision and all exception
    // masks). Public execution entries still restore the complete embedding environment, including status flags.
    // Avoid two libc fenv calls per global.get/set on the architectures with directly accessible control registers.
    using scoped_wasm_host_fp_control_restore = ::std::conditional_t<wasm_fp_has_native_control_guard,
        scoped_wasm_native_fp_control_restore, scoped_llvm_wasm_host_fp_environment_restore>;
#endif
}
