// Compile without AltiVec enabled: a generic PPC runtime can still host native
// JIT vector code. Optional VSCR access must follow OS HWCAP, not just compiler
// feature macros, and must preserve the scalar libc environment as well.
// Build without -maltivec as well as with it: a JIT may use more ISA features than its host executable.
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>

#if defined(__linux__) && (defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__))
# include <sys/auxv.h>
namespace fp = ::uwvm2::runtime::lib::details;
unsigned vscr_non_java(bool poison) noexcept
{
    alignas(16) unsigned value[4], scratch[4];
    __asm__ volatile(".machine push\n\t.machine altivec\n\tstvx 0,0,%0\n\tmfvscr 0\n\tstvx 0,0,%1\n\tlvx 0,0,%0\n\t.machine pop"
                     : : "r"(scratch), "r"(value) : "memory");
    if(poison)
    {
        for(auto& lane: value) { lane |= 0x10000u; }
        __asm__ volatile(".machine push\n\t.machine altivec\n\tstvx 0,0,%0\n\tlvx 0,0,%1\n\tmtvscr 0\n\tlvx 0,0,%0\n\t.machine pop"
                         : : "r"(scratch), "r"(value) : "memory");
    }
    return (value[0] | value[1] | value[2] | value[3]) & 0x10000u;
}
bool gradual_vector_add() noexcept
{
    alignas(16) unsigned const tiny[4]{1u, 1u, 1u, 1u};
    alignas(16) unsigned sum[4], scratch[4];
    __asm__ volatile(".machine push\n\t.machine altivec\n\tstvx 0,0,%0\n\tlvx 0,0,%1\n\tvaddfp 0,0,0\n\tstvx 0,0,%2\n\tlvx 0,0,%0\n\t.machine pop"
                     : : "r"(scratch), "r"(tiny), "r"(sum) : "memory");
    return sum[0] == 2u && sum[1] == 2u && sum[2] == 2u && sum[3] == 2u;
}
int main()
{
    if((::getauxval(AT_HWCAP) & 0x10000000ul) == 0ul) { return 0; }
    fp::scoped_wasm_auxiliary_fp_environment original{true};
    vscr_non_java(true);
    bool active{};
    {
        fp::scoped_llvm_wasm_fp_environment entry{active};
        if(!entry.ready() || vscr_non_java(false) != 0u || !gradual_vector_add()) { return 1; }
        { fp::scoped_wasm_host_fp_control_restore callback{}; vscr_non_java(true); }
        if(vscr_non_java(false) != 0u || !gradual_vector_add()) { return 2; }
    }
    return vscr_non_java(false) == 0x10000u ? 0 : 3;
}
#else
int main() { return 0; }
#endif
