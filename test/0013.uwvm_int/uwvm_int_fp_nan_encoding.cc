// Wasm's quiet bit is independent of the host's historical NaN convention.
#include <uwvm2/runtime/compiler/uwvm_int/optable/numeric.h>
#include <uwvm2/runtime/compiler/shared/strict_float_bits.h>
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <cstdio>
namespace n = uwvm2::runtime::compiler::uwvm_int::optable::numeric_details;
namespace fp = uwvm2::runtime::compiler::shared::strict_float;
namespace jit = uwvm2::runtime::compiler::shared::strict_float_jit;
template <class Float>
using bits_t = fp::bits_t<Float>;

// Match the production bridge's externally registered, dynamic-operand ABI.
UWVM_NOINLINE std::uint64_t native_bridge(std::uint64_t a, std::uint64_t b, unsigned op) { return jit::bridge(a, b, op); }

auto (*volatile registered_bridge)(std::uint64_t, std::uint64_t, unsigned) = native_bridge;

template <class Float, unsigned Op>
UWVM_NOINLINE bits_t<Float> evaluate(bits_t<Float> a, bits_t<Float> b)
{
    auto x = std::bit_cast<Float>(a), y = std::bit_cast<Float>(b);
    Float result{};
    if constexpr(Op == 0) { result = n::eval_float_binop<n::float_binop::add>(x, y); }
    if constexpr(Op == 1) { result = n::eval_float_binop<n::float_binop::sub>(x, y); }
    if constexpr(Op == 2) { result = n::eval_float_binop<n::float_binop::mul>(x, y); }
    if constexpr(Op == 3) { result = n::eval_float_binop<n::float_binop::div>(x, y); }
    if constexpr(Op == 4) { result = n::eval_float_binop<n::float_binop::min>(x, y); }
    if constexpr(Op == 5) { result = n::eval_float_binop<n::float_binop::max>(x, y); }
    if constexpr(Op == 6) { result = n::eval_float_unop<n::float_unop::sqrt>(x); }
    if constexpr(Op == 7) { result = fp::binary<fp::operation::add>(fp::binary<fp::operation::mul>(x, y), Float{1}); }
    return std::bit_cast<bits_t<Float>>(result);
}

template <class Float>
unsigned check()
{
    using bits = bits_t<Float>;
    constexpr bits sign = bits{1} << (sizeof(bits) * 8 - 1);
    constexpr bits inf = sizeof(bits) == 4 ? bits{0x7f800000} : static_cast<bits>(0x7ff0000000000000ull);
    constexpr bits quiet = bits{1} << (sizeof(bits) == 4 ? 22 : 51);
    constexpr bits one = sizeof(bits) == 4 ? bits{0x3f800000} : static_cast<bits>(0x3ff0000000000000ull);
    bits (*functions[])(bits, bits){evaluate<Float, 0>,
                                    evaluate<Float, 1>,
                                    evaluate<Float, 2>,
                                    evaluate<Float, 3>,
                                    evaluate<Float, 4>,
                                    evaluate<Float, 5>,
                                    evaluate<Float, 6>,
                                    evaluate<Float, 7>};
    unsigned errors{};
    auto require_nan = [&](bits actual, bool canonical, unsigned op)
    {
        bool valid = canonical ? (actual & ~sign) == (inf | quiet) : (actual & (inf | quiet)) == (inf | quiet);
        if(!valid)
        {
            if(errors < 12) { std::fprintf(stderr, "NaN f%zu op=%u canonical=%u got=%llx\n", sizeof(bits) * 8, op, canonical, (unsigned long long)actual); }
            ++errors;
        }
    };
    for(bits nan: {bits(inf | 1), bits(inf | quiet), bits(inf | quiet | 123), bits(sign | inf | 1), bits(sign | inf | quiet), bits(sign | inf | quiet | 123)})
    {
        bool canonical = (nan & ~sign) == (inf | quiet);
        for(unsigned op{}; op != 8; ++op)
        {
            require_nan(functions[op](nan, one), canonical, op);
            if(op != 6) { require_nan(functions[op](one, nan), canonical, op); }
        }
        for(unsigned op{}; op != 5; ++op) { require_nan(static_cast<bits>(registered_bridge(nan, one, op | (sizeof(bits) == 8 ? 16 : 0))), canonical, 8 + op); }
        using Other = std::conditional_t<sizeof(bits) == 4, n::wasm_f64, n::wasm_f32>;
        using OtherBits = bits_t<Other>;
        constexpr OtherBits other_sign = OtherBits{1} << (sizeof(OtherBits) * 8 - 1);
        constexpr OtherBits other_nan = sizeof(OtherBits) == 4 ? OtherBits{0x7fc00000} : static_cast<OtherBits>(0x7ff8000000000000ull);
        volatile bits input = nan;
        auto converted = std::bit_cast<OtherBits>(fp::convert<Other>(std::bit_cast<Float>(input)));
        auto bridged = jit::bridge(nan, 0, sizeof(bits) == 4 ? 24u : 7u);
        for(auto result: {converted, static_cast<OtherBits>(bridged)})
        {
            if(canonical ? (result & ~other_sign) != other_nan : (result & other_nan) != other_nan)
            {
                ++errors;
                std::fprintf(stderr, "NaN conversion f%zu canonical=%u got=%llx\n", sizeof(bits) * 8, canonical, (unsigned long long)result);
            }
        }
    }
    // Invalid operations with no NaN operands must also produce canonical NaNs.
    require_nan(functions[0](inf, sign | inf), true, 0);
    require_nan(functions[1](inf, inf), true, 1);
    require_nan(functions[2](0, inf), true, 2);
    require_nan(functions[3](0, 0), true, 3);
    require_nan(functions[3](inf, inf), true, 3);
    require_nan(functions[6](sign | one, 0), true, 6);
    return errors;
}

int main()
{
    bool active{};
    uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment environment{active};
    if(!environment.ready()) { return 2; }
    auto errors = check<n::wasm_f32>() + check<n::wasm_f64>();
    std::printf("FP NaN encoding: %u failures\n", errors);
    return errors != 0;
}
