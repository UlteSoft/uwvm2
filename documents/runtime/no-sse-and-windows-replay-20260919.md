# No-SSE SIMD and native Windows replay, 2026-09-19

This is a bounded follow-up to the platform audit, not an all-platform pass.
The ordinary and ROS repositories contain the same repairs below. The Linux
builds and Windows guest use `uwvm-comprehensive.slice`, with `MemoryMax=32G`,
`MemorySwapMax=0`, and CPUs 16-31. Individual compiler jobs have smaller limits.

## Repairs and reasons

- x86-64 `-mno-sse` does **not** give C++ float/vector-returning helpers a usable
  replacement ABI. The storage-only `wasm_v128` becomes a 16-byte, 16-aligned
  integer aggregate for that configuration only. SSE-enabled and ARM64EC
  representations are unchanged.
- Pure SIMD bit operations discard their unused FP template branches with
  `else`, rather than relying on optimization after `return`. This matters at
  `-O0`, where even unused Float-returning instantiations can fail to compile.
- The no-SSE arithmetic adapter passes integer bits into existing x87 memory
  operations. It retains the RN fast path and binary64 midpoint/subnormal
  repair. Integral rounding and width conversions avoid Float-returning calls.
  It does not enable SSE behind the build configuration or change FP controls
  on the normal SIMD path. Min/max handle arithmetic NaNs and signed zero;
  pseudo-min/max continue selecting the original operand bits.
- The independent rounding oracle still uses `modf`, not the production
  integer rounding implementation. Its input is copied to a local FP object
  so that the oracle itself does not require an unavailable return register.
- Vendored Boost capacity sizing uses its fixed 7/8 load factor exactly in
  integer arithmetic. Growth, rounded capacity, and backing-buffer arithmetic
  reject overflow before allocation. See the vendored README and the new
  `unordered_integer_capacity.cc` regression, including no-exception builds.

These distinctions follow the WebAssembly [numeric semantics](https://webassembly.github.io/spec/core/exec/numerics.html):
arithmetic NaNs have different requirements from bit transport, and pseudo
min/max select an operand rather than applying arithmetic NaN normalization.

## Completed checks

| Check | Observed result | Boundary |
| --- | --- | --- |
| SIMD bit transport and NaN encoding | 16/16 for ordinary; 16/16 for ROS | GCC 15 / Clang 23, O0/O3, normal / no-SSE x86-64 |
| New integer-ABI arithmetic regression | 8/8 for each product | GCC 15 / Clang 22, O0/O3, normal / no-SSE; fixed midpoint, subnormal, zero, and width-conversion results |
| Vendored hash capacity regression | 24/24 | Both products; GCC/Clang; O0/O3, no-SSE, UBSan, and no-exception cases |
| Normal SSE generated instructions | 16/16 comparisons identical | GCC 15 / Clang 22, eight selected functions each; not an application benchmark |
| Actual Windows SIMD bit transport | x86-64 and i686 both exit 0, zero failures | Ordinary-tree header test, O0; real Windows 11 build 26100 under QEMU/KVM, i686 under WOW64; not Wine |
| Actual Windows hash capacity | x86-64 and i686 both exit 0, zero failures | O2; actual 64/32-bit size_t overflow checks and caught allocation exceptions; not JIT unwind coverage |

The selected assembly functions are `f32x4_add`, `f64x2_add`, `f32x4_pmin`,
`f64x2_pmax`, `f32x4_abs`, `f64x2_neg`, `f32x4_ceil`, and `f64x2_nearest`.
The comparison ignores addresses/comments, not instructions or operands.

Windows executables were compiled with the available macOS Clang 23 snapshot
`004ffb73ee4c9b04407eae7c581a872ee328cc84` and the existing MinGW/libc++ SDK.
They test the actual interpreter opfuncs, including tail and non-tail dispatch.
Their source snapshot precedes the final ordinary-ABI constexpr cleanup;
they are not evidence that the final full Windows CLI or ROS build passed.

| Executable | SHA-256 |
| --- | --- |
| x86-64 | `ddc596c771e2c6d795abe95916b695feed349c43870baba8a0f9c83e844189ac` |
| i686 | `40f928d5d8246141b8e7661172041e47cfcd496ddb074019d61604a2df9e9124` |
| capacity x86-64 | `ca4bf449bc7d97ffb44d7356faf01afdb949af68026b62bc8ac0a98a36fa9ade` |
| capacity i686 | `7560cf626c97f716763a3a3e3cab70b44c800f9d77c687a3f3b318d73faf5fdd` |

## Remaining work and environmental failures

This replay does not establish whole no-SSE CLI support, all SIMD conversion
instructions, all interpreter option combinations, named-module builds, or
Windows LLVM-JIT/cache/unwind support. Earlier unsupported MCJIT platform
contracts remain separate from SIMD correctness. ARM64, ARM32, and ARM64EC
Windows execution is still pending: the existing ARM ISO files do not mean
their mostly empty raw system disks contain installed Windows.

A Linux GCC `-m32` capacity build could not start because the installed GCC 15
SDK lacks its 32-bit `bits/c++config.h`; the native Windows i686 run above
does exercise 32-bit capacity arithmetic, but does not replace that Linux build.

Linux-hosted Windows compiler attempts hit their isolated 12 GiB limit at
both O0 and O2; a Clang 22 attempt also exhausted that limit. The cause is not
yet isolated, and this must not be reported as an optimization-only failure
or a Windows execution failure. The macOS compiler produced the two tested
objects. No memory limit was raised beyond the aggregate 32 GiB budget.

The reused Windows image reports an expired evaluation license; it is not a
licensed long-term validation environment. Its base raw disk was never modified.
An initial disposable overlay grew to about 9.8 GiB and filled the host volume;
the guest was stopped and that overlay discarded before any test results were
created. The replacement uses network isolation with an explicit local test
transfer endpoint, plus a guard that pauses the guest below 3 GiB host free
space. The tested guest was subsequently shut down cleanly; its replacement
overlay and scripts remain under Documents for the next run. No activation,
clock, or evaluation-reset workaround was used.

Evidence is retained on SSH host `linux` under
`/home/macromodel/Documents/uwvm-validation-20260918.PwcF7M`:
`main-final-simd/results.json`, `ros-final-simd/results.json`,
`main-integer-abi01/results.json`, `integer-abi01/results.json`,
`capacity-results.json`, `codegen-comparison.json`, and `guest-result.json`.
The earlier `guest-result-no-exitcode.json` is intentionally **not** pass
evidence: the first PowerShell process wrapper lost its exit code. The final
runner owns the process handle and rejects missing exit codes.
