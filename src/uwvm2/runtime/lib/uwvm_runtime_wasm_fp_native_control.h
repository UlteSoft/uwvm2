#pragma once

// The register-only boundary has no libc dependency, so cross-ABI assembly can be checked without a target SDK.
namespace uwvm2::runtime::lib::details
{
#if (defined(__GNUC__) || defined(__clang__)) && \
    ((defined(__i386__) && defined(__SSE__)) || defined(__x86_64__) || defined(__aarch64__) || defined(__arm64ec__) || defined(_M_ARM64EC) || \
     (defined(__riscv) && defined(__riscv_flen)))
    inline constexpr bool wasm_fp_has_native_control_guard{true};
    class scoped_wasm_native_fp_control_restore
    {
        // Clang ARM64EC defines __x86_64__ for its hybrid ABI, but executes AArch64 instructions.
#if (defined(__i386__) || defined(__x86_64__)) && !defined(__arm64ec__) && !defined(_M_ARM64EC)
        unsigned short x87_control;
# if defined(__SSE__) || defined(__x86_64__)
        unsigned sse_control;
# endif
#elif defined(__aarch64__) || defined(__arm64ec__) || defined(_M_ARM64EC)
        unsigned long long control;
#elif defined(__riscv) && defined(__riscv_flen)
        unsigned control;
#endif
    public:
        scoped_wasm_native_fp_control_restore() noexcept
        {
#if (defined(__i386__) || defined(__x86_64__)) && !defined(__arm64ec__) && !defined(_M_ARM64EC)
            __asm__ volatile("fnstcw %0" : "=m"(x87_control) : : "memory");
# if defined(__SSE__) || defined(__x86_64__)
            __asm__ volatile("stmxcsr %0" : "=m"(sse_control) : : "memory");
# endif
#elif defined(__aarch64__) || defined(__arm64ec__) || defined(_M_ARM64EC)
            __asm__ volatile("mrs %0, fpcr" : "=r"(control) : : "memory");
#elif defined(__riscv) && defined(__riscv_flen)
            __asm__ volatile("frrm %0" : "=r"(control) : : "memory");
#endif
        }
        scoped_wasm_native_fp_control_restore(scoped_wasm_native_fp_control_restore const&) = delete;
        scoped_wasm_native_fp_control_restore& operator=(scoped_wasm_native_fp_control_restore const&) = delete;
        ~scoped_wasm_native_fp_control_restore() noexcept
        {
#if (defined(__i386__) || defined(__x86_64__)) && !defined(__arm64ec__) && !defined(_M_ARM64EC)
            // FLDCW can raise #MF for a callback's deferred unmasked exception. Clear pending x87 status with
            // no-wait instructions first, keeping FNCLEX off the common SSE-only/no-exception path.
            unsigned short status;
            __asm__ volatile("fnstsw %0" : "=a"(status) : : "memory");
            if((status & 0x00ffu) != 0u) { __asm__ volatile("fnclex" : : : "memory"); }
            __asm__ volatile("fldcw %0" : : "m"(x87_control) : "memory");
# if defined(__SSE__) || defined(__x86_64__)
            __asm__ volatile("ldmxcsr %0" : : "m"(sse_control) : "memory");
# endif
#elif defined(__aarch64__) || defined(__arm64ec__) || defined(_M_ARM64EC)
            __asm__ volatile("msr fpcr, %0" : : "r"(control) : "memory");
#elif defined(__riscv) && defined(__riscv_flen)
            // FFLAGS is caller-saved. Gate on ISA FPU availability, not the hard/soft argument-passing ABI.
            __asm__ volatile("fsrm %0" : : "r"(control) : "memory");
#endif
        }
        [[nodiscard]] static constexpr bool ready() noexcept { return true; }
    };
#else
    // Unknown targets must use libc fenv; absence of a specialized guard is never permission to skip protection.
    inline constexpr bool wasm_fp_has_native_control_guard{false};
    class scoped_wasm_native_fp_control_restore;
#endif
}
