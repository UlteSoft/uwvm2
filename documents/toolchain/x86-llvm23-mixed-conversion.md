# LLVM 23 X86 mixed-signedness conversion workaround

## Reproducer and cause

The Core 2 `float_exprs.wast` execution audit found two failures in both products'
LLVM paths, while the interpreter passed. LLVM 23.0.0git at
`4c4c1db7c69a6fda6cfa6bc6066bb09a433edc89` also miscompiles this independent IR:

```llvm
define float @mixed(float %x) {
  %i = fptosi float %x to i32
  %f = uitofp i32 %i to float
  ret float %f
}
```

For `x = -1.5`, the integer bit pattern is `0xffffffff`, so the result must be
`4294967296.0f`, not `-1.0f`. The f64 counterpart must return `4294967295.0`.
This follows the integer interpretation and numeric conversion rules in
[Core 2, sections 2.2 and 4.3](https://webassembly.github.io/spec/versions/core/WebAssembly-2.0.pdf).

The backend's
[`lowerFPToIntToFP`](https://github.com/llvm/llvm-project/blob/4c4c1db7c69a6fda6cfa6bc6066bb09a433edc89/llvm/lib/Target/X86/X86ISelLowering.cpp#L18916)
chooses both vector conversions' signedness from the first conversion, losing
the second conversion's independent interpretation. AVX512DQ extends the same
path to i64 and unsigned-to-signed round trips; an AVX2-only negative-i32 test is
therefore insufficient. Standalone LLVM 22.1.8 generated the correct instructions
for the original i32 reproducer. This is separate from the MIPS repair in
[llvm/llvm-project#223905](https://github.com/llvm/llvm-project/pull/223905).

## Why the workaround has this shape

`int_to_float_emit.h` constrains scalar i32/i64-to-f32/f64 conversions only for
LLVM 23 or newer X86 targets with SSE2. Both signedness directions are protected,
even if the producer is not yet a direct FP-to-int cast: later optimization can
expose one through a local, select or PHI. Merely zero-extending to i64 before
conversion does not work: InstCombine recreates the vulnerable conversion.

The i32 x86-64 reproducer still uses two native scalar conversion instructions,
with no helper call, additional branch or spill. Other widths/ABIs retain their
normal target lowering, which can require more instructions. The guard leaves
older LLVM, x87/no-SSE, and non-X86 lowering unchanged. Do not replace it with a
blanket optimization disable, or remove it based only on one passing CPU model.

## Targeted verification

- `llvm_jit_mixed_signedness_conversion.cc` emits production conversion IR,
  applies LLVM O3, and checks 16 families / 134 exact-bit results per product.
- `check_mixed_signedness_codegen.py` uses an independent, precomputed Python
  oracle and separately compiled integer-ABI C callers. It executes O0 and O3
  machine code for SSE2, AVX2, AVX512DQ+VL, AVX512DQ without VL, x86-64 no-SSE,
  i686 SSE2 and i686 x87. AVX-512 runs under Intel SDE; i686 under QEMU.
- Paired run: **28 executions, 3,752 exact-bit checks, zero failures**.
  IR/object hashes and disassembly are retained in
  `/tmp/uwvm-comprehensive.eUFnxH/validation-convert-cross-v6/`.

The finite oracle deliberately avoids int-to-float double-rounding halfway cases;
it is not exhaustive rounding certification. These generated-code runs do not
claim full CLI/OS integration on all targets. Full CLI, cache and broader parser
results are tracked in [the validation audit](validation-policy-parity.md).
