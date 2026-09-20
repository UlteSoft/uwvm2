// Scalar fenv alone cannot prove auxiliary FP controls are restored. Exercise
// architecture-specific controls at entry/callback boundaries, while leaving
// unsupported-target coverage explicit rather than treating a skipped branch
// as execution evidence for that architecture.
// Cross-architecture numeric and supplementary-vector controls, independent of the runtime's C++26 optable size.
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
#include "../0008.imported/wasi/wasip1/func/fp_control_probe.h"
#include <bit>
#include <cstdio>

namespace
{
    namespace fp = ::uwvm2::runtime::lib::details;
    namespace probe = ::uwvm2test::wasip1_fp_control;

#if defined(__ALTIVEC__)
    using vector_bits = unsigned int __attribute__((vector_size(16)));
    [[nodiscard]] vector_bits read_aux() noexcept
    { vector_bits value; __asm__ volatile("mfvscr %0" : "=v"(value) : : "memory"); return value; }
    void write_aux(vector_bits value) noexcept { __asm__ volatile("mtvscr %0" : : "v"(value) : "memory"); }
    void poison_aux() noexcept { write_aux(read_aux() | vector_bits{0x10000u, 0x10000u, 0x10000u, 0x10000u}); }
    [[nodiscard]] unsigned aux_controls() noexcept
    { auto value{read_aux()}; return (value[0] | value[1] | value[2] | value[3]) & 0x10000u; }
    struct restore_aux { vector_bits saved{read_aux()}; ~restore_aux() { write_aux(saved); } };
    [[nodiscard]] bool vector_arithmetic() noexcept
    {
        using vector_float = float __attribute__((vector_size(16)));
        vector_bits const tiny{1u, 1u, 1u, 1u};
        auto operand{::std::bit_cast<vector_float>(tiny)};
        vector_float sum;
        __asm__ volatile("vaddfp %0, %1, %1" : "=v"(sum) : "v"(operand) : "memory");
        auto bits{::std::bit_cast<vector_bits>(sum)};
        return bits[0] == 2u && bits[1] == 2u && bits[2] == 2u && bits[3] == 2u;
    }
#elif defined(__mips_msa)
    [[nodiscard]] unsigned read_aux() noexcept
    { unsigned value; __asm__ volatile("cfcmsa %0, $1" : "=r"(value) : : "memory"); return value; }
    void write_aux(unsigned value) noexcept { __asm__ volatile("ctcmsa $1, %0" : : "r"(value) : "memory"); }
    void poison_aux() noexcept { write_aux(0x01000002u); }
    [[nodiscard]] unsigned aux_controls() noexcept { return read_aux() & 0x01000f83u; }
    struct restore_aux { unsigned saved{read_aux()}; ~restore_aux() { write_aux(saved); } };
    [[nodiscard]] bool vector_arithmetic() noexcept { return true; }
#else
    void poison_aux() noexcept {}
    [[nodiscard]] unsigned aux_controls() noexcept { return 0u; }
    struct restore_aux {};
    [[nodiscard]] bool vector_arithmetic() noexcept { return true; }
#endif

    template <typename Float, typename Bits>
    [[nodiscard]] Bits arithmetic(Bits left_bits, Bits right_bits, bool multiply) noexcept
    {
        volatile Float left{::std::bit_cast<Float>(left_bits)}, right{::std::bit_cast<Float>(right_bits)};
        Float const result{multiply ? static_cast<Float>(left * right) : static_cast<Float>(left + right)};
        return ::std::bit_cast<Bits>(result);
    }

    [[nodiscard]] bool numeric_checks() noexcept
    {
        using u32 = ::std::uint32_t;
        using u64 = ::std::uint64_t;
        return ::std::fegetround() == FE_TONEAREST &&
               arithmetic<float>(u32{0x3f800000u}, u32{0x33800000u}, false) == 0x3f800000u &&
               arithmetic<float>(u32{0x00800000u}, u32{0x3f000000u}, true) == 0x00400000u &&
               arithmetic<float>(u32{1u}, u32{0x40000000u}, true) == 2u &&
               arithmetic<double>(u64{0x3ff0000000000000ull}, u64{0x3ca0000000000000ull}, false) == 0x3ff0000000000000ull &&
               arithmetic<double>(u64{0x0010000000000000ull}, u64{0x3fe0000000000000ull}, true) == 0x0008000000000000ull &&
               arithmetic<double>(u64{1u}, u64{0x4000000000000000ull}, true) == 2u && vector_arithmetic();
    }

    [[nodiscard]] int run() noexcept
    {
        probe::initial_state_restore original{};
        [[maybe_unused]] restore_aux auxiliary{};
        if(!original.valid || ::uwvm2::runtime::lib::details::set_default_wasm_fp_environment() != 0) { return 1; }
        probe::snapshot host{};
        if(!probe::prepare_hostile(host)) { return 2; }
        poison_aux();
        auto const host_aux{aux_controls()};
        if(::std::feraiseexcept(FE_INVALID) != 0) { return 3; }
        auto const host_status{::std::fetestexcept(FE_ALL_EXCEPT)};
        bool active{};
        {
            fp::scoped_llvm_wasm_fp_environment entry{active};
            if(!entry.ready() || !active || ::std::fegetround() != FE_TONEAREST || aux_controls() != 0u || !numeric_checks()) { return 4; }
            {
                fp::scoped_llvm_wasm_host_fp_environment_restore callback{active};
                if(!callback.ready() || ::std::fesetround(probe::hostile_rounding_up) != 0) { return 5; }
                probe::enable_flush_control();
                poison_aux();
            }
            if(aux_controls() != 0u || !numeric_checks()) { return 6; }
            {
                fp::scoped_wasm_host_fp_control_restore callback{};
                if(!callback.ready() || ::std::fesetround(probe::hostile_rounding_up) != 0) { return 7; }
                probe::enable_flush_control();
                poison_aux();
            }
            if(aux_controls() != 0u || !numeric_checks()) { return 8; }
        }
        if(active || !probe::unchanged(host) || aux_controls() != host_aux || ::std::fetestexcept(FE_ALL_EXCEPT) != host_status) { return 9; }
        return 0;
    }
}

int main()
{
    int const result{run()};
    if(result != 0) { ::std::fprintf(stderr, "wasm_fp_arch_environment: failure %d\n", result); }
    return result;
}
