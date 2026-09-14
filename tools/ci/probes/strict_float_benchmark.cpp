// Native timings only: QEMU timings are not hardware-performance evidence.
#include <uwvm2/runtime/compiler/shared/strict_float.h>
#include <bit>
#include <chrono>
#include <cstdio>
namespace fp=uwvm2::runtime::compiler::shared::strict_float;
template <typename F,unsigned Mode> void run(char const* label)
{
    F value{1.0},increment{0.00001};
    auto const start{std::chrono::steady_clock::now()};
    for(unsigned i{};i!=1000000u;++i)
    {
        if constexpr(Mode==0) { volatile F rounded{value+increment}; value=rounded; }
        else if constexpr(Mode==1) { value=std::bit_cast<F>(fp::round_extended<F>(fp::evaluate_extended<fp::operation::add>(value,increment))); }
        else { value=fp::binary<fp::operation::add>(value,increment); }
    }
    auto const elapsed{std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()};
    std::printf("%s,f%zu,%.9f,%llx\n",label,sizeof(F)*8,elapsed,
        static_cast<unsigned long long>(std::bit_cast<fp::bits_t<F>>(value)));
}
int main()
{
    static_assert(fp::needs_extended_rounding,"build this probe for x87/68881, not SSE2");
    for(unsigned i{};i!=7u;++i)
    {
        run<float,0>("native-unfixed"); run<float,1>("strict-always"); run<float,2>("strict-fast");
        run<double,0>("native-unfixed"); run<double,1>("strict-always"); run<double,2>("strict-fast");
    }
}
