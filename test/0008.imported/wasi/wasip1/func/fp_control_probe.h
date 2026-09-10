/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <cfenv>
#include <cstdint>
#include <memory>

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
# include <xmmintrin.h>
#endif

namespace uwvm2test::wasip1_fp_control
{
    // Accumulated exception status is intentionally excluded: public ABIs make it caller-saved and WebAssembly cannot
    // observe it. This probe covers control that can change Wasm arithmetic results or synchronous exception behavior.
    [[nodiscard]] inline ::std::uint_least64_t read_arch_control() noexcept
    {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
        // MXCSR bits 0..5 are exception status; bits 6..15 are DAZ, masks, rounding and FTZ controls.
        return static_cast<::std::uint_least64_t>(_mm_getcsr() & 0x0000ffc0u);
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        ::std::uint_least64_t value{};
        __asm__ volatile("mrs %0, fpcr" : "=r"(value));
        return value;
#else
        return 0u;
#endif
    }

    inline void enable_flush_control() noexcept
    {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
        unsigned value{_mm_getcsr()};
        value |= 1u << 15u;  // FTZ
# if defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__)
        value |= 1u << 6u;  // DAZ
# endif
        _mm_setcsr(value);
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        ::std::uint_least64_t value{};
        __asm__ volatile("mrs %0, fpcr" : "=r"(value));
        value |= 1ull << 24u;  // FZ
        __asm__ volatile("msr fpcr, %0" : : "r"(value));
#endif
    }

    struct initial_state_restore
    {
        ::std::fenv_t environment{};
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
        unsigned raw_arch_control{_mm_getcsr()};
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        ::std::uint_least64_t raw_arch_control{};
#endif
        bool valid{};

        inline initial_state_restore() noexcept : valid{::std::fegetenv(::std::addressof(environment)) == 0}
        {
#if defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
            __asm__ volatile("mrs %0, fpcr" : "=r"(raw_arch_control));
#endif
        }

        inline ~initial_state_restore()
        {
            if(valid) { static_cast<void>(::std::fesetenv(::std::addressof(environment))); }
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
            _mm_setcsr(raw_arch_control);
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
            __asm__ volatile("msr fpcr, %0" : : "r"(raw_arch_control));
#endif
        }
    };

    struct snapshot
    {
        int rounding{};
        ::std::uint_least64_t arch_control{};
    };

    [[nodiscard]] inline bool prepare_hostile(snapshot& out) noexcept
    {
        if(::std::fesetround(FE_DOWNWARD) != 0) { return false; }
        enable_flush_control();
        out = snapshot{::std::fegetround(), read_arch_control()};
        return out.rounding == FE_DOWNWARD;
    }

    [[nodiscard]] inline bool unchanged(snapshot const& expected) noexcept
    { return ::std::fegetround() == expected.rounding && read_arch_control() == expected.arch_control; }
}
