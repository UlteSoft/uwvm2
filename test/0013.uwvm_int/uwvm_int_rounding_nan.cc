// Optimized scalar interpreter rounding must quiet sNaNs and preserve signed zero.
#include <uwvm2/runtime/compiler/uwvm_int/optable/numeric.h>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <type_traits>

namespace detail = uwvm2::runtime::compiler::uwvm_int::optable::numeric_details;
template <typename Float> using bits_t = std::conditional_t<sizeof(Float) == 4, std::uint32_t, std::uint64_t>;

template <typename Float, detail::float_unop Op>
[[gnu::noinline]] bits_t<Float> evaluate(bits_t<Float> bits)
{
    return std::bit_cast<bits_t<Float>>(detail::eval_float_unop<Op>(std::bit_cast<Float>(bits)));
}

template <typename Float>
bool check()
{
    using bits = bits_t<Float>;
    bits (*functions[])(bits){evaluate<Float, detail::float_unop::ceil>, evaluate<Float, detail::float_unop::floor>,
                              evaluate<Float, detail::float_unop::trunc>, evaluate<Float, detail::float_unop::nearest>};
    constexpr bits sign{bits{1} << (sizeof(Float) * 8u - 1u)};
    constexpr bits quiet{bits{1} << (sizeof(Float) == 4 ? 22u : 51u)};
    constexpr bits inf{sizeof(Float) == 4 ? bits{0x7f800000u} : static_cast<bits>(0x7ff0000000000000ull)};
    for(unsigned op{}; op != 4; ++op)
    {
        for(bits pattern : {inf | bits{1}, sign | inf | bits{1}, inf | quiet})
        {
            volatile bits input{pattern};
            if((functions[op](input) & (inf | quiet)) != (inf | quiet)) { std::printf("NaN width=%zu op=%u\n", sizeof(Float)*8, op); return false; }
        }
        for(bits pattern : {bits{}, sign, inf, inf | sign})
        {
            volatile bits input{pattern};
            if(functions[op](input) != pattern) { return false; }
        }
        volatile bits positive{std::bit_cast<bits>(Float{1.5})};
        volatile bits negative{std::bit_cast<bits>(Float{-1.5})};
        if(functions[op](positive) != std::bit_cast<bits>(op == 0 || op == 3 ? Float{2} : Float{1})) { return false; }
        if(functions[op](negative) != std::bit_cast<bits>(op == 1 || op == 3 ? Float{-2} : Float{-1})) { return false; }
    }
    return true;
}
int main()
{
    if(!check<float>() || !check<double>()) { return 1; }
    std::puts("PASS 72 scalar interpreter rounding checks");
}
