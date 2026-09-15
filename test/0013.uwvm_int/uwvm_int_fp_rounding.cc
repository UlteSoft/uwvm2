// Cover ordinary/fused tail/byref evaluators AND the integer JIT bridge: a fix
// only in one dispatch path is incomplete. Include O0/O3 and SSE2 without SSE4.1;
// host rounding may preserve sNaNs, whereas Wasm rounding must quiet them.
// Independent rounding oracle: split the exact input into integral/fractional
// parts. Never use the tested bridge (or ceil/floor/trunc/nearbyint) as expected.
#define UWVM_ENABLE_UWVM_INT_COMBINE_OPS 1
#define UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS 1
#include <uwvm2/runtime/compiler/uwvm_int/optable/conbine_heavy.h>
#include <uwvm2/runtime/compiler/shared/strict_float_bits.h>
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <cstdio>
#include "../0014.llvm_jit/fixtures/fp_rounding_oracle.h"

namespace o = uwvm2::runtime::compiler::uwvm_int::optable;
namespace n = o::numeric_details;
namespace bridge = uwvm2::runtime::compiler::shared::strict_float_jit;
template <class Float>
using bits_t = std::conditional_t<sizeof(Float) == 4, std::uint32_t, std::uint64_t>;

UWVM_INTERPRETER_OPFUNC_HOT_MACRO void finish(std::byte const*, std::byte*, std::byte*) noexcept {}

template <class Float, n::float_unop Op, unsigned Mode>
UWVM_NOINLINE bits_t<Float> evaluate(bits_t<Float> input)
{
    if constexpr(Mode == 0) { return std::bit_cast<bits_t<Float>>(n::eval_float_unop<Op>(std::bit_cast<Float>(input))); }
    else if constexpr(Mode == 1)
    {
        constexpr unsigned code = Op == n::float_unop::ceil ? 9 : Op == n::float_unop::floor ? 10 : Op == n::float_unop::trunc ? 11 : 12;
        return static_cast<bits_t<Float>>(bridge::bridge(input, 0, code | (sizeof(Float) == 8 ? 16u : 0u)));
    }
    else
    {
        constexpr bool tail = (Mode & 1) == 0;
        constexpr bool fused = Mode >= 4;
        constexpr o::uwvm_interpreter_translate_option_t option{.is_tail_call = tail};
        std::byte code[64]{}, values[16]{}, local[16]{};
        std::memcpy(values, &input, sizeof(input));
        std::memcpy(local, &input, sizeof(input));
        auto immediate = code + sizeof(void*);
        if constexpr(fused)
        {
            std::size_t offset{};
            std::memcpy(immediate, &offset, sizeof(offset));
            immediate += sizeof(offset);
        }
        auto end = &finish;
        std::memcpy(immediate, &end, sizeof(end));
        std::byte const* ip = code;
        std::byte* sp = values + (fused ? 0 : sizeof(input));
        std::byte* lp = local;
#define RUN(NAME)                                                                                                                                              \
    if constexpr(tail)                                                                                                                                         \
        o::NAME<option, Op, SIZE_MAX>(ip, sp, lp);                                                                                                             \
    else                                                                                                                                                       \
        o::NAME<option, Op>(ip, sp, lp)
        if constexpr(sizeof(Float) == 4)
        {
            if constexpr(fused) { RUN(uwvmint_f32_unop_localget); }
            else
            {
                RUN(uwvmint_f32_unop);
            }
        }
        else
        {
            if constexpr(fused) { RUN(uwvmint_f64_unop_localget); }
            else
            {
                RUN(uwvmint_f64_unop);
            }
        }
#undef RUN
        bits_t<Float> result;
        std::memcpy(&result, values, sizeof(result));
        return result;
    }
}

template <class Float, unsigned Mode>
unsigned check()
{
    using bits = bits_t<Float>;
    using fn = bits (*)(bits);
    fn functions[]{evaluate<Float, n::float_unop::ceil, Mode>,
                   evaluate<Float, n::float_unop::floor, Mode>,
                   evaluate<Float, n::float_unop::trunc, Mode>,
                   evaluate<Float, n::float_unop::nearest, Mode>};
    constexpr unsigned fraction_bits = sizeof(Float) == 4 ? 23 : 52;
    constexpr unsigned exponent_count = sizeof(Float) == 4 ? 256 : 2048;
    constexpr bits sign = bits{1} << (sizeof(bits) * 8 - 1);
    constexpr bits quiet = bits{1} << (fraction_bits - 1);
    unsigned errors{};
    auto sample = [&](bits input)
    {
        for(unsigned op{}; op != 4; ++op)
        {
            auto actual = functions[op](input), expected = fp_rounding_oracle::expected<Float>(input, op);
            bool okay = fp_rounding_oracle::matches<Float>(input, actual, op);
            if(!okay)
            {
                if(errors < 8)
                {
                    std::fprintf(stderr,
                                 "round f%zu mode=%u op=%u in=%llx expected=%llx actual=%llx\n",
                                 sizeof(Float) * 8,
                                 Mode,
                                 op,
                                 (unsigned long long)input,
                                 (unsigned long long)expected,
                                 (unsigned long long)actual);
                }
                ++errors;
            }
        }
    };
    // Both sides of every binade, including subnormals, infinities and NaNs.
    for(unsigned e{}; e != exponent_count; ++e)
    {
        bits base = bits{e} << fraction_bits;
        for(bits v: {base, bits(base + 1), bits(base - (e != 0)), bits(base | quiet), bits(base | quiet | 1)})
        {
            sample(v);
            sample(v | sign);
        }
    }
    // Exact ties and their adjacent representable values; deterministic random payloads.
    for(unsigned i{}; i != 32; ++i)
    {
        bits base = std::bit_cast<bits>(static_cast<Float>(i + 0.5));
        for(bits v: {bits(base - 1), base, bits(base + 1)})
        {
            sample(v);
            sample(v | sign);
        }
    }
    std::uint64_t state{0x6a09e667f3bcc909ull};
    for(unsigned i{}; i != 4096; ++i)
    {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        sample(static_cast<bits>(state));
    }
    return errors;
}

int main()
{
    bool active{};
    uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment environment{active};
    if(!environment.ready()) { return 2; }
    unsigned errors{};
#define CHECK(M) errors += check<n::wasm_f32, M>() + check<n::wasm_f64, M>()
    CHECK(0);
    CHECK(1);
    CHECK(2);
    CHECK(3);
    CHECK(4);
    CHECK(5);
#undef CHECK
    std::printf("FP rounding: %u failures\n", errors);
    return errors != 0;
}
