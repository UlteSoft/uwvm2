// Independent LLVM backend regression input, with no UWVM dependencies.
// A scalar-condition v128 select followed by f64x2 pseudo-minimum can expose
// an illegal GPR -> FGR64CC copy in MIPS r6 code generation. No UWVM headers,
// library calls, pointer arithmetic or NaN canonicalization are involved.
//
// Reproduced with Clang 22.1.8 and the tested LLVM 23 toolchain. Example:
// clang++ --target=mipsel-linux-gnu -march=mips32r6 -mabi=32 -mnan=2008 \
//   -std=c++20 -O3 -ffp-contract=off -c mipsr6_fp_select_backend.cpp
//
// Isolating an oracle/opfunc does NOT repair this LLVM backend defect. The
// tested backend repair is documents/toolchain/patches/llvm-mips-r6-condition-registers.patch.
// check_mipsr6_backend.py requires patched codegen AND failing stock negative
// controls, then checks exact bits in QEMU. Keep this source independent: a
// noinline wrapper, soft-float flag or rewritten select would hide the bug.
// Merely updating UWVM does not patch an installed Clang or JIT libLLVM.
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
