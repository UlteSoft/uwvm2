// Standalone address-decision tests only; never link this with uwvm_runtime.
// The test erases emitted fatal-report blocks and records an independent trap
// byte before MCJIT compilation. If a real fatal callback survives, fail loudly.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace uwvm2::runtime::lib
{
    void llvm_jit_memory_out_of_bounds_trap(std::size_t, std::uint_least64_t, std::uint_least64_t,
        std::uint_least32_t, std::uint_least64_t, std::size_t, std::uintptr_t, std::uintptr_t) noexcept
    {
        std::fputs("FAIL: address-decision test called the fatal runtime sink\n", stderr);
        std::abort();
    }
}
