#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
#include "../0008.imported/wasi/wasip1/func/fp_control_probe.h"

namespace
{
    namespace probe = ::uwvm2test::wasip1_fp_control;
    using guard = ::uwvm2::runtime::lib::details::scoped_wasm_host_fp_control_restore;

    [[nodiscard]] int test_control_restore() noexcept
    {
        probe::initial_state_restore restore_initial{};
        if(!restore_initial.valid || ::std::fesetenv(FE_DFL_ENV) != 0) { return 1; }
        probe::snapshot const original{::std::fegetround(), probe::read_arch_control()};
        {
            guard outer{};
            if(!outer.ready()) { return 2; }
            probe::snapshot hostile{};
            if(!probe::prepare_hostile(hostile)) { return 3; }
            {
                guard inner{};
                if(!inner.ready() || ::std::fesetenv(FE_DFL_ENV) != 0) { return 4; }
            }
            if(!probe::unchanged(hostile)) { return 5; }
        }
        if(!probe::unchanged(original)) { return 6; }

#if (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__)) && !defined(__arm64ec__) && !defined(_M_ARM64EC)
        if(::std::fesetenv(FE_DFL_ENV) != 0) { return 9; }
        ::std::uint16_t original_x87{};
        __asm__ volatile("fnstcw %0" : "=m"(original_x87) : : "memory");
# if defined(__SSE__) || defined(__x86_64__)
        ::std::uint32_t original_sse{};
        __asm__ volatile("stmxcsr %0" : "=m"(original_sse) : : "memory");
# endif
        {
            guard callback{};
            if(!callback.ready()) { return 10; }
            // Change precision, rounding, and the divide-by-zero mask independently of MXCSR. Leave exception flags
            // clear so this probe can safely unmask a class before the guard restores the original masks.
            auto const changed_x87{static_cast<::std::uint16_t>((original_x87 & ~0x0f00u) | 0x0600u)};
            auto const unmasked_x87{static_cast<::std::uint16_t>(changed_x87 & ~0x0004u)};
            __asm__ volatile("fnclex\n\tfldcw %0" : : "m"(unmasked_x87) : "memory");
# if defined(__SSE__) || defined(__x86_64__)
            auto const changed_sse{static_cast<::std::uint32_t>((original_sse & ~0x0000023fu) | 0x0000c000u)};
            __asm__ volatile("ldmxcsr %0" : : "m"(changed_sse) : "memory");
# endif
        }
        ::std::uint16_t restored_x87{};
        __asm__ volatile("fnstcw %0" : "=m"(restored_x87) : : "memory");
        if(restored_x87 != original_x87) { return 11; }
# if defined(__SSE__) || defined(__x86_64__)
        ::std::uint32_t restored_sse{};
        __asm__ volatile("stmxcsr %0" : "=m"(restored_sse) : : "memory");
        if(restored_sse != original_sse) { return 12; }
# endif
        {
            guard callback{};
            if(!callback.ready()) { return 13; }
            auto const unmasked_invalid{static_cast<::std::uint16_t>(original_x87 & ~0x0001u)};
            // Leave a deferred, unmasked 0/0 exception pending. The guard must use a no-wait clear before FLDCW,
            // otherwise restoring perfectly valid Wasm controls itself raises SIGFPE/#MF.
            __asm__ volatile("fldcw %0\n\tfldz\n\tfdiv %%st(0), %%st(0)" : : "m"(unmasked_invalid) : "st", "memory");
        }
        __asm__ volatile("fstp %%st(0)" : : : "st", "memory");
        __asm__ volatile("fnstcw %0" : "=m"(restored_x87) : : "memory");
        if(restored_x87 != original_x87) { return 14; }
#endif
        return 0;
    }
}

int main() { return test_control_restore(); }
