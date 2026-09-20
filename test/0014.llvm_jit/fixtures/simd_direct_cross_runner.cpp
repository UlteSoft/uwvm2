// Compile the generated C oracle with -Dmain=simd_cross_main, then link this
// file with its object and the generated LLVM object. A plain C process does
// not establish Wasm's FP controls: for example, PPC libc can enter with VSCR.NJ
// set, making subnormal comparisons fail even when the generated code is right.
// Use the production entry guard instead of embedding an approximate fenv fix
// in the oracle. This remains a generated-code test, not whole-CLI coverage.
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
extern "C" int simd_cross_main();
int main()
{
    bool active{};
    uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment environment{active};
    if(!environment.ready()) { return 125; }
    return simd_cross_main();
}
