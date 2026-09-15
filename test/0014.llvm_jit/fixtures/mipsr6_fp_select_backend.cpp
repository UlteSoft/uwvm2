// Independent LLVM backend diagnostic, not part of the passing runtime suite.
// A scalar-condition v128 select followed by f64x2 pseudo-minimum can expose
// an illegal GPR -> FGR64CC copy in MIPS r6 code generation. No UWVM headers,
// library calls, pointer arithmetic or NaN canonicalization are involved.
//
// Reproduced with Clang 22.1.8 and the tested LLVM 23 toolchain. Example:
// clang++ --target=mipsel-linux-gnu -march=mips32r6 -mabi=32 -mnan=2008 \
//   -std=c++20 -O3 -ffp-contract=off -c mipsr6_fp_select_backend.cpp
//
// Isolating an oracle/opfunc in the C++ test driver does NOT repair this LLVM
// backend defect. Keep this reproducer when investigating complete MIPS JIT
// coverage; do not infer that all valid Wasm select/SIMD combinations are safe
// to compile just because the separate opfunc regression passes.
using mips_repro_f64x2 = double __attribute__((vector_size(16)));
using mips_repro_u64x2 = unsigned long long __attribute__((vector_size(16)));

extern "C" mips_repro_f64x2 mipsr6_pmin_after_select(mips_repro_f64x2 a, mips_repro_f64x2 b, bool condition)
{
    auto selected = condition ? b : a;
    auto take_a = __builtin_bit_cast(mips_repro_u64x2, a < selected);
    return __builtin_bit_cast(mips_repro_f64x2,
        (__builtin_bit_cast(mips_repro_u64x2, a) & take_a) |
        (__builtin_bit_cast(mips_repro_u64x2, selected) & ~take_a));
}
