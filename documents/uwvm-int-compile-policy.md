# uwvm-int Compile Policy

The default uwvm-int compile policy is:

```text
combine = soft
delay-local = heavy
```

Heavy combine remains available as an explicit interpreter-performance profile. `combine=none`, `delay-local=soft`, and `delay-local=none` are diagnostic or size-experiment settings rather than production defaults.

## Benchmark

The policy was measured on 2026-09-03 using all 207 modules in `u2bench/wasm/corpus`. Each engine/module pair ran three times; the median guest-reported `Time: ... ms` was used before computing cross-module geometric means. Variant order was rotated for every module and repetition. All 3,105 executions succeeded without a timeout.

Host and build controls:

- Intel Core i9-14900HX, x86-64 Linux, 91 GiB RAM
- clang and libc++ 23.0.0git
- Diagnostic benchmark build: Release, `-O3`, `-march=native`, uwvm-int execution (not a distributable portability baseline)
- current builds used `-j1` to bound peak build memory
- original uwvm2 baseline reported `Opcode Conbine: Heavy` and `Local Delay: Heavy`

Execution ratios below are variant/baseline, so lower is faster:

| Variant | Versus original heavy/heavy, all 207 | Versus current heavy/heavy, all 207 | Versus current heavy/heavy, baseline >=20 ms |
| --- | ---: | ---: | ---: |
| current heavy/heavy | 1.0268 | 1.0000 | 1.0000 |
| current soft/heavy | 1.1068 | 1.0779 | 1.0585 |
| current none/heavy | 1.6042 | 1.5623 | 1.5420 |
| current soft/soft | 1.1366 | 1.1069 | 1.0887 |

The direct delay-local comparison is `soft/soft` versus `soft/heavy`: 1.0269 across all modules and 1.0141 for modules whose soft/heavy median was at least 20 ms. The latter group still had a 1.1753 p95 ratio and individual regressions up to 1.7595. Reducing delay-local therefore has a poor size/performance tradeoff.

Current pure-interpreter binary measurements:

| Compile policy | Build seconds | Peak RSS (KiB) | Binary bytes | Size versus heavy/heavy |
| --- | ---: | ---: | ---: | ---: |
| heavy/heavy | 116.76 | 3,850,916 | 15,733,320 | baseline |
| soft/heavy | 113.82 | 3,852,500 | 15,089,288 | -4.09% |
| none/heavy | 112.99 | 3,852,768 | 14,240,776 | -9.49% |
| soft/soft | 112.57 | 3,852,424 | 14,980,776 | -4.78% |

`soft/heavy` removes about 629 KiB from the current heavy/heavy executable while preserving the high-value delay-local paths. Completely disabling combine saves more space but causes unacceptable dispatch-heavy regressions. Lowering delay-local after selecting soft combine saves only about 106 KiB more and is not the default.

A fresh build with both policy options omitted was byte-identical to the explicitly configured `soft/heavy` build, confirming that the measured profile is the actual default.

The reproducible driver is `tools/benchmark_uwvm_int_policy.py`.
