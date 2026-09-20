// Generate expectations independently with MPFR, then consume endian-neutral
// integer bits on the target. A host long-double oracle can suffer the same
// double rounding as the implementation; do not use it as independent proof.
// Keep producer and consumer free of fast-math assumptions that discard NaNs.
// Build with -DUWVM_FP_ORACLE_GENERATE and MPFR/GMP to generate a deterministic corpus;
// build without MPFR for a native/cross-target consumer. stdin/stdout are endian-neutral hex.
#include <uwvm2/runtime/compiler/shared/strict_float.h>
#include <bit>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cfenv>
#include <iterator>
#ifdef UWVM_FP_TEST_GENERATED
# include "strict_float_generated.h"
#endif
#ifdef UWVM_FP_ORACLE_GENERATE
# include <mpfr.h>
#endif
namespace fp = uwvm2::runtime::compiler::shared::strict_float;
using u64 = std::uint64_t;
using u32 = std::uint32_t;

template <typename F> fp::bits_t<F> actual(unsigned op, u64 a, u64 b)
{
#ifdef UWVM_FP_TEST_GENERATED
    return static_cast<fp::bits_t<F>>(strict_generated(sizeof(F)*8,op,a,b));
#else
    F const x{std::bit_cast<F>(static_cast<fp::bits_t<F>>(a))};
    F const y{std::bit_cast<F>(static_cast<fp::bits_t<F>>(b))};
#ifdef UWVM_FP_TEST_EXTENDED
# define RUN(Op) fp::round_extended<F>(fp::evaluate_extended<fp::operation::Op>(x, y))
#else
# define RUN(Op) std::bit_cast<fp::bits_t<F>>(fp::binary<fp::operation::Op>(x, y))
#endif
    switch(op)
    {
        case 0: return RUN(add);
        case 1: return RUN(sub);
        case 2: return RUN(mul);
        case 3: return RUN(div);
        case 4:
#if defined(UWVM_FP_TEST_EXTENDED)
            return RUN(sqrt);
#else
            if constexpr(fp::needs_extended_rounding) { return std::bit_cast<fp::bits_t<F>>(fp::square_root(x)); }
            else { return std::bit_cast<fp::bits_t<F>>(std::sqrt(x)); }
#endif
        case 5: return std::bit_cast<fp::bits_t<F>>(fp::convert<F>(std::bit_cast<std::int64_t>(a)));
        case 6: return std::bit_cast<fp::bits_t<F>>(fp::convert<F>(a));
        case 7: return std::bit_cast<fp::bits_t<F>>(fp::convert<F>(std::bit_cast<double>(a)));
        default: std::abort();
    }
#undef RUN
#endif
}

#ifdef UWVM_FP_ORACLE_GENERATE
u64 random_bits()
{
    static u64 state{0x9e3779b97f4a7c15ull};
    state += 0x9e3779b97f4a7c15ull;
    u64 z{state}; z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

template <typename F> void generate(unsigned count)
{
    constexpr unsigned p{sizeof(F) == 4 ? 24u : 53u};
    constexpr unsigned eb{sizeof(F) == 4 ? 8u : 11u};
    constexpr u64 sign{u64{1} << (sizeof(F)*8-1)};
    constexpr u64 fraction{(u64{1} << (p-1))-1};
    constexpr u64 inf{((u64{1} << eb)-1) << (p-1)};
    constexpr u64 one{((u64{1} << (eb-1))-1) << (p-1)};
    constexpr u64 edges[]{0, sign, 1, 2, 3, fraction, fraction+1, fraction+2,
        one-1, one, one+1, inf-1, inf, inf+1, inf+(u64{1} << (p-2)), sign|1, sign|one, sign|inf};
    mpfr_t x,y,z;
    mpfr_inits2(64, x, y, z, static_cast<mpfr_ptr>(nullptr));
    mpfr_set_prec(z,p);
    auto emit = [&](unsigned op,u64 a,u64 b)
    {
        mpfr_set_emin(-100000); mpfr_set_emax(100000);
        if(op == 5) { mpfr_set_sj(x,std::bit_cast<std::int64_t>(a),MPFR_RNDN); }
        else if(op == 6) { mpfr_set_uj(x,a,MPFR_RNDN); }
        else if(op == 7) { mpfr_set_d(x,std::bit_cast<double>(a),MPFR_RNDN); }
        else { mpfr_set_d(x,std::bit_cast<F>(static_cast<fp::bits_t<F>>(a)),MPFR_RNDN); }
        mpfr_set_d(y,std::bit_cast<F>(static_cast<fp::bits_t<F>>(b)),MPFR_RNDN);
        // MPFR's documented IEEE emulation propagates the ternary result into subnormalize,
        // so the oracle itself does not double-round at the subnormal boundary.
        mpfr_set_emin(sizeof(F)==4 ? -148 : -1073); mpfr_set_emax(sizeof(F)==4 ? 128 : 1024);
        int ternary{};
        switch(op)
        {
            case 0: ternary=mpfr_add(z,x,y,MPFR_RNDN); break;
            case 1: ternary=mpfr_sub(z,x,y,MPFR_RNDN); break;
            case 2: ternary=mpfr_mul(z,x,y,MPFR_RNDN); break;
            case 3: ternary=mpfr_div(z,x,y,MPFR_RNDN); break;
            case 4: ternary=mpfr_sqrt(z,x,MPFR_RNDN); break;
            default: ternary=mpfr_set(z,x,MPFR_RNDN); break;
        }
        // Conversion operands may start outside the destination exponent range.
        ternary=mpfr_check_range(z,ternary,MPFR_RNDN);
        mpfr_subnormalize(z,ternary,MPFR_RNDN);
        F const result{static_cast<F>(mpfr_get_d(z,MPFR_RNDN))};
        std::printf("%u %u %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n",
            static_cast<unsigned>(sizeof(F)*8),op,a,b,static_cast<u64>(std::bit_cast<fp::bits_t<F>>(result)));
    };
    for(auto a:edges) for(auto b:edges) for(unsigned op{};op!=5;++op) emit(op,a,b);
    for(unsigned exponent{p}; exponent!=64u; ++exponent)
    {
        u64 const base{u64{1} << exponent};
        u64 const half{u64{1} << (exponent-p)};
        for(u64 delta{};delta!=5u;++delta)
        {
            u64 const value{base+half-2u+delta};
            emit(5,value,0); emit(5,u64{}-value,0); emit(6,value,0);
        }
    }
    for(unsigned i{};i!=count;++i)
    {
        u64 a{random_bits()},b{random_bits()};
        if(i%4==0) { a=(a&fraction)|(random_bits()&sign); b=edges[random_bits()%std::size(edges)]; }
        if(i%4==1) { a=one|(a&fraction); b=(one-(static_cast<u64>(p)<< (p-1)))|(b&fraction); }
        for(unsigned op{};op!=5;++op) emit(op,a,b);
        emit(5,random_bits(),0); emit(6,random_bits(),0);
        if constexpr(sizeof(F)==4) { emit(7,random_bits(),0); }
    }
    mpfr_clears(x,y,z,static_cast<mpfr_ptr>(nullptr));
}
#endif

int main(int argc,char** argv)
{
#ifdef UWVM_FP_ORACLE_GENERATE
    unsigned const count{argc>1 ? static_cast<unsigned>(std::strtoul(argv[1],nullptr,10)) : 10000u};
    generate<float>(count); generate<double>(count);
#else
#if (defined(__i386__) || defined(__x86_64__)) && !defined(__arm64ec__) && !defined(_M_ARM64EC)
    if(argc>1)
    {
        unsigned short const control{static_cast<unsigned short>(std::strtoul(argv[1],nullptr,16))};
        __asm__ volatile("fnclex; fldcw %0" : : "m"(control) : "memory");
    }
#elif defined(__m68k__) && defined(__HAVE_68881__)
    if(argc>1)
    {
        unsigned const control{static_cast<unsigned>(std::strtoul(argv[1],nullptr,16))};
        __asm__ volatile("fmove.l %0,%%fpcr" : : "dm"(control) : "memory");
    }
#else
    static_cast<void>(argc); static_cast<void>(argv);
#endif
    unsigned width,op; u64 a,b,want; unsigned long count{},failures{};
    while(std::scanf("%u %u %" SCNx64 " %" SCNx64 " %" SCNx64,&width,&op,&a,&b,&want)==5)
    {
        u64 const got{width==32 ? static_cast<u64>(actual<float>(op,a,b)) : actual<double>(op,a,b)};
        u64 const inf{width==32 ? 0x7f800000ull : 0x7ff0000000000000ull};
        u64 const fraction{width==32 ? 0x7fffffull : 0xfffffffffffffull};
        bool const nan{(want&inf)==inf && (want&fraction)!=0};
        if((!nan && got!=want) || (nan && ((got&inf)!=inf || (got&(fraction^(fraction>>1)))==0)))
        {
            if(failures++<20) std::fprintf(stderr,"%u op=%u a=%016" PRIx64 " b=%016" PRIx64 " want=%016" PRIx64 " got=%016" PRIx64 "\n",width,op,a,b,want,got);
        }
        ++count;
    }
    std::fprintf(stderr,"checked=%lu failures=%lu extended=%u\n",count,failures,fp::needs_extended_rounding);
    return count!=0 && failures==0 ? 0 : 1;
#endif
}
