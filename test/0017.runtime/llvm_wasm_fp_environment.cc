#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
#include "../0008.imported/wasi/wasip1/func/fp_control_probe.h"

#include <cfenv>
#include <cstdint>
#include <memory>

#if !defined(__arm64ec__) && !defined(_M_ARM64EC) && (defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1))
# include <xmmintrin.h>
#endif

namespace
{
    namespace fp = ::uwvm2::runtime::lib::details;
    inline constexpr int round_down{::uwvm2test::wasip1_fp_control::hostile_rounding_down};
    inline constexpr int round_up{::uwvm2test::wasip1_fp_control::hostile_rounding_up};

#if !defined(__arm64ec__) && !defined(_M_ARM64EC) && (defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1))
# if defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__)
    inline constexpr unsigned flush_control_mask{(1u << 15u) | (1u << 6u)};  // FTZ + DAZ
# else
    inline constexpr unsigned flush_control_mask{1u << 15u};  // FTZ; DAZ is not available on every SSE1 CPU
# endif

    [[nodiscard]] unsigned read_fp_control() noexcept { return _mm_getcsr(); }
    void write_fp_control(unsigned value) noexcept { _mm_setcsr(value); }
    void enable_flush_controls() noexcept { write_fp_control(read_fp_control() | flush_control_mask); }
    [[nodiscard]] bool flush_controls_are_default() noexcept { return (read_fp_control() & flush_control_mask) == 0u; }
    [[nodiscard]] bool flush_controls_are_enabled() noexcept { return (read_fp_control() & flush_control_mask) == flush_control_mask; }
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    inline constexpr ::std::uint_least64_t flush_control_mask{1ull << 24u};  // FPCR.FZ covers scalar f32/f64 inputs/results

    [[nodiscard]] ::std::uint_least64_t read_fp_control() noexcept
    {
        ::std::uint_least64_t value{};
        __asm__ volatile("mrs %0, fpcr" : "=r"(value));
        return value;
    }

    void write_fp_control(::std::uint_least64_t value) noexcept
    { __asm__ volatile("msr fpcr, %0" : : "r"(value)); }

    void enable_flush_controls() noexcept { write_fp_control(read_fp_control() | flush_control_mask); }
    [[nodiscard]] bool flush_controls_are_default() noexcept { return (read_fp_control() & flush_control_mask) == 0u; }
    [[nodiscard]] bool flush_controls_are_enabled() noexcept { return (read_fp_control() & flush_control_mask) == flush_control_mask; }
#else
    [[nodiscard]] unsigned read_fp_control() noexcept { return 0u; }
    void write_fp_control(unsigned) noexcept {}
    void enable_flush_controls() noexcept {}
    [[nodiscard]] bool flush_controls_are_default() noexcept { return true; }
    [[nodiscard]] bool flush_controls_are_enabled() noexcept { return true; }
#endif

    struct initial_environment_restore
    {
        ::std::fenv_t environment{};
        decltype(read_fp_control()) fp_control{};
        bool valid{};

        initial_environment_restore() noexcept
            : fp_control{read_fp_control()}, valid{::std::fegetenv(::std::addressof(environment)) == 0}
        {}

        ~initial_environment_restore() noexcept
        {
            if(valid) { static_cast<void>(::std::fesetenv(::std::addressof(environment))); }
            write_fp_control(fp_control);
        }
    };

    [[nodiscard]] int test_fp_environment_scope() noexcept
    {
        bool fp_environment_active{};
        initial_environment_restore restore_initial{};
        if(!restore_initial.valid) { return 1; }
        if(::std::fesetround(round_down) != 0) { return 2; }
        if(::std::feclearexcept(FE_ALL_EXCEPT) != 0 || ::std::feraiseexcept(FE_INVALID) != 0) { return 3; }
        enable_flush_controls();
        if(::std::fegetround() != round_down || !flush_controls_are_enabled() || (::std::fetestexcept(FE_ALL_EXCEPT) & FE_INVALID) == 0) { return 4; }

        {
            fp::scoped_llvm_wasm_fp_environment wasm_environment{fp_environment_active};
            if(!wasm_environment.ready()) { return 5; }
            if(!fp::is_llvm_wasm_fp_environment_active(fp_environment_active)) { return 6; }
            if(::std::fegetround() != FE_TONEAREST || !flush_controls_are_default() || ::std::fetestexcept(FE_ALL_EXCEPT) != 0) { return 7; }

            {
                fp::scoped_llvm_wasm_host_fp_environment_restore host_callback_environment{fp_environment_active};
                if(!host_callback_environment.ready()) { return 8; }
                if(::std::fesetround(round_up) != 0 || ::std::feraiseexcept(FE_DIVBYZERO) != 0) { return 9; }
                enable_flush_controls();
                if(::std::fegetround() != round_up || !flush_controls_are_enabled() ||
                   (::std::fetestexcept(FE_ALL_EXCEPT) & FE_DIVBYZERO) == 0)
                {
                    return 10;
                }
            }

            if(::std::fegetround() != FE_TONEAREST || !flush_controls_are_default() || ::std::fetestexcept(FE_ALL_EXCEPT) != 0) { return 11; }

            // A nested raw/LLVM entry installs another default scope but must preserve the outer active marker. Its
            // destructor restores the outer Wasm environment, not the original embedding environment.
            {
                fp::scoped_llvm_wasm_fp_environment nested_wasm_environment{fp_environment_active};
                if(!nested_wasm_environment.ready() || !fp::is_llvm_wasm_fp_environment_active(fp_environment_active)) { return 12; }
                if(::std::fegetround() != FE_TONEAREST || !flush_controls_are_default() || ::std::fetestexcept(FE_ALL_EXCEPT) != 0) { return 13; }
            }
            if(!fp::is_llvm_wasm_fp_environment_active(fp_environment_active)) { return 14; }
            if(::std::fegetround() != FE_TONEAREST || !flush_controls_are_default() || ::std::fetestexcept(FE_ALL_EXCEPT) != 0) { return 15; }
        }

        if(fp::is_llvm_wasm_fp_environment_active(fp_environment_active)) { return 16; }
        if(::std::fegetround() != round_down || !flush_controls_are_enabled() ||
           (::std::fetestexcept(FE_ALL_EXCEPT) & FE_INVALID) == 0 || (::std::fetestexcept(FE_ALL_EXCEPT) & FE_DIVBYZERO) != 0)
        {
            return 17;
        }

        {
            fp::scoped_llvm_wasm_fp_environment disabled_environment{fp_environment_active, false};
            if(!disabled_environment.ready() || fp::is_llvm_wasm_fp_environment_active(fp_environment_active)) { return 18; }
            if(::std::fegetround() != round_down || !flush_controls_are_enabled() || (::std::fetestexcept(FE_ALL_EXCEPT) & FE_INVALID) == 0) { return 19; }
        }

        return 0;
    }
}

int main()
{
    return test_fp_environment_scope();
}
