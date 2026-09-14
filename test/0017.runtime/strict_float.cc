#include <uwvm2/runtime/compiler/shared/strict_float.h>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdio>

namespace fp = ::uwvm2::runtime::compiler::shared::strict_float;

template <typename Float>
int check(fp::operation op, fp::bits_t<Float> lhs, fp::bits_t<Float> rhs, fp::bits_t<Float> expected)
{
    Float const left{::std::bit_cast<Float>(lhs)}, right{::std::bit_cast<Float>(rhs)};
    Float actual{};
    switch(op)
    {
        case fp::operation::add: actual = fp::binary<fp::operation::add>(left, right); break;
        case fp::operation::sub: actual = fp::binary<fp::operation::sub>(left, right); break;
        case fp::operation::mul: actual = fp::binary<fp::operation::mul>(left, right); break;
        case fp::operation::div: actual = fp::binary<fp::operation::div>(left, right); break;
        case fp::operation::sqrt:
            if constexpr(fp::needs_extended_rounding) { actual = fp::square_root(left); }
            else { actual = ::std::sqrt(left); }
            break;
    }
    if(::std::bit_cast<fp::bits_t<Float>>(actual) != expected)
    {
        ::std::fprintf(stderr, "dispatch op=%u width=%zu actual=%llx expected=%llx\n", static_cast<unsigned>(op), sizeof(Float)*8,
            static_cast<unsigned long long>(::std::bit_cast<fp::bits_t<Float>>(actual)), static_cast<unsigned long long>(expected));
        return 1;
    }
#if (((defined(__i386__) || defined(__x86_64__)) && !defined(__arm64ec__) && !defined(_M_ARM64EC) && !defined(_SOFT_FLOAT)) || \
     (defined(__m68k__) && defined(__HAVE_68881__)))
    fp::extended_value extended{};
    switch(op)
    {
        case fp::operation::add: extended = fp::evaluate_extended<fp::operation::add>(left, right); break;
        case fp::operation::sub: extended = fp::evaluate_extended<fp::operation::sub>(left, right); break;
        case fp::operation::mul: extended = fp::evaluate_extended<fp::operation::mul>(left, right); break;
        case fp::operation::div: extended = fp::evaluate_extended<fp::operation::div>(left, right); break;
        case fp::operation::sqrt: extended = fp::evaluate_extended<fp::operation::sqrt>(left); break;
    }
    auto const result{fp::round_extended<Float>(extended)};
    if(result != expected)
    {
        ::std::fprintf(stderr, "strict_float: op=%u lhs=%llx rhs=%llx actual=%llx expected=%llx ext=%x:%llx\n",
            static_cast<unsigned>(op), static_cast<unsigned long long>(lhs), static_cast<unsigned long long>(rhs),
            static_cast<unsigned long long>(result), static_cast<unsigned long long>(expected), extended.sign_exponent,
            static_cast<unsigned long long>(extended.significand));
        return 1;
    }
#endif
    return 0;
}

int main()
{
    ::std::fenv_t original{};
    if(::std::fegetenv(&original) != 0 || ::std::fesetenv(FE_DFL_ENV) != 0) { return 1; }
    int errors{};
    errors += check<double>(fp::operation::add, 0x3ff0000000000000ull, 0x3ca0000000000001ull, 0x3ff0000000000001ull);
    errors += check<double>(fp::operation::sub, 0x3ff0000000000000ull, 0x3c90000000000001ull, 0x3fefffffffffffffull);
    errors += check<double>(fp::operation::mul, 0x0010000000000000ull, 0x3fe0000000000000ull, 0x0008000000000000ull);
    errors += check<double>(fp::operation::mul, 0x0000000000000001ull, 0x4000000000000000ull, 2ull);
    errors += check<double>(fp::operation::div, 0x0000000000000001ull, 0x4000000000000000ull, 0ull);
    errors += check<double>(fp::operation::div, 0x0000000000000003ull, 0x4000000000000000ull, 2ull);
    errors += check<double>(fp::operation::sqrt, 0x4000000000000000ull, 0ull, 0x3ff6a09e667f3bcdull);
    errors += check<double>(fp::operation::sqrt, 0x8000000000000000ull, 0ull, 0x8000000000000000ull);
    errors += check<double>(fp::operation::mul, 0x7fefffffffffffffull, 0x4000000000000000ull, 0x7ff0000000000000ull);
    errors += check<float>(fp::operation::add, 0x3f800000u, 0x33800000u, 0x3f800000u);
    errors += check<float>(fp::operation::add, 0x3f800000u, 0x33800001u, 0x3f800001u);
    errors += check<float>(fp::operation::mul, 0x00800000u, 0x3f000000u, 0x00400000u);
    errors += check<float>(fp::operation::sqrt, 0x40000000u, 0u, 0x3fb504f3u);
    volatile fp::u64 integer{(fp::u64{1} << 63u) + 1025u};
    auto converted_double{::std::bit_cast<fp::u64>(fp::convert<double>(integer))};
    if(converted_double != 0x43e0000000000001ull) { ::std::fprintf(stderr,"u64->f64=%llx\n",static_cast<unsigned long long>(converted_double)); ++errors; }
    integer = (fp::u64{1} << 63u) + (fp::u64{1} << 39u) + 1u;
    auto converted_float{::std::bit_cast<fp::u32>(fp::convert<float>(integer))};
    if(converted_float != 0x5f000001u) { ::std::fprintf(stderr,"u64->f32=%x\n",static_cast<unsigned>(converted_float)); ++errors; }
    volatile double demote{::std::bit_cast<double>(fp::u64{0x3ff0000010000001ull})};
    auto demoted{::std::bit_cast<fp::u32>(fp::convert<float>(demote))};
    if(demoted != 0x3f800001u) { ::std::fprintf(stderr,"f64->f32=%x\n",static_cast<unsigned>(demoted)); ++errors; }
#if defined(FE_DOWNWARD) && !defined(UWVM_ASSUME_FIXED_WASM_FP_ENVIRONMENT)
    if constexpr(fp::needs_extended_rounding)
    {
        // Correctly rounded operations must restore every precision/rounding control, including hostile callers.
        if(::std::fesetround(FE_DOWNWARD) != 0) { return 1; }
        volatile double one{1.0}, half{::std::bit_cast<double>(fp::u64{0x3ca0000000000001ull})};
        errors += ::std::bit_cast<fp::u64>(fp::binary<fp::operation::add>(one, half)) != 0x3ff0000000000001ull;
        errors += ::std::fegetround() != FE_DOWNWARD;
    }
#endif
    errors += ::std::fesetenv(&original) != 0;
    return errors != 0;
}
