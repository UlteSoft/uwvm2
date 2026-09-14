/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <cfenv>
#include <cstdint>
#include <exception>
#include <memory>

namespace uwvm2::runtime::lib::details
{
    // Interpreter and generated Wasm FP instructions both assume the IEEE default environment: round-to-nearest/
    // ties-to-even, gradual underflow, and masked exceptions. Save the embedding environment once at a public execution
    // entry, not in individual Wasm opfuncs. Keep the historical LLVM names for existing users of this header.
    // The caller owns the per-thread active marker; do not force C++ thread_local into map-backed runtime builds.
    [[nodiscard]] inline constexpr bool is_llvm_wasm_fp_environment_active(bool active) noexcept { return active; }

    class scoped_llvm_wasm_fp_environment
    {
        ::std::fenv_t saved_environment{};
        bool* active_state{};
        bool previous_active{};
        bool restore_environment{};
        bool ready_state{true};

    public:
        explicit scoped_llvm_wasm_fp_environment(bool& active, bool enable = true) noexcept
            : active_state{::std::addressof(active)}, previous_active{active}
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
        ::std::fenv_t saved_environment{};
        bool restore_environment{};
        bool ready_state{true};

    public:
        explicit scoped_llvm_wasm_host_fp_environment_restore(bool active) noexcept
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
    class scoped_wasm_host_fp_control_restore
    {
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))
        ::std::uint16_t x87_control;
# if defined(__SSE__)
        ::std::uint32_t sse_control;
# endif
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__aarch64__)
        ::std::uint64_t control;
#else
        scoped_llvm_wasm_host_fp_environment_restore environment;
#endif

    public:
        scoped_wasm_host_fp_control_restore() noexcept
#if !((defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__) || defined(__aarch64__)))
            : environment{true}
#endif
        {
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))
            __asm__ volatile("fnstcw %0" : "=m"(x87_control) : : "memory");
# if defined(__SSE__)
            __asm__ volatile("stmxcsr %0" : "=m"(sse_control) : : "memory");
# endif
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__aarch64__)
            __asm__ volatile("mrs %0, fpcr" : "=r"(control) : : "memory");
#endif
        }

        scoped_wasm_host_fp_control_restore(scoped_wasm_host_fp_control_restore const&) = delete;
        scoped_wasm_host_fp_control_restore& operator=(scoped_wasm_host_fp_control_restore const&) = delete;

        ~scoped_wasm_host_fp_control_restore() noexcept
        {
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))
            // FLDCW can itself raise #MF for a callback's pending unmasked exception, or unmask a pending flag when
            // restoring the caller's controls. Status is caller-saved here. Use only no-wait instructions to clear it
            // first; keep the relatively expensive FNCLEX off the usual SSE-only/no-x87-exception path.
            ::std::uint16_t status{};
            __asm__ volatile("fnstsw %0" : "=a"(status) : : "memory");
            if((status & 0x00ffu) != 0u) { __asm__ volatile("fnclex" : : : "memory"); }
            __asm__ volatile("fldcw %0" : : "m"(x87_control) : "memory");
# if defined(__SSE__)
            __asm__ volatile("ldmxcsr %0" : : "m"(sse_control) : "memory");
# endif
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__aarch64__)
            __asm__ volatile("msr fpcr, %0" : : "r"(control) : "memory");
#endif
        }

        [[nodiscard]] bool ready() const noexcept
        {
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__) || defined(__aarch64__))
            return true;
#else
            return environment.ready();
#endif
        }
    };
}
