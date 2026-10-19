# CPython stock CLI and standard-library operations: r2

The real CPython 3.14.8 stock WASIp1 command passed all 16 probes in native CPython, Wasmtime 42.0.1 and the three isolated fd_readdir-fixed UWVM products: **80/80 operations, five normal exits**. This is ordinary functional qualification; it supplies no performance ratio or Rdbg result. The original failed r1 evidence and [diagnosis](stock-stdlib-results.md) remain unchanged.

The tested command uses `-S -B -X utf8 -c SOURCE` with arguments `π`, `你好`, an empty argument and `space value`. Its only preopen is the task-owned standard-library bundle at guest `/python`; there is no host-root preopen. The bundle contains 2,434 files / 48,729,996 bytes from the pinned source Lib and WASI-generated sysconfig. Native CPython reads the same pure-Python bytes together with its own native extension/sysconfig directory, and does not load WASI sysconfig. `PYTHONHOME=/python` and `PYTHONPATH=/python/sysconfig` are guest settings. The other three explicit guest environment entries are `UWVM_PY_AUX=two-你好`, `UWVM_PY_EMPTY=` and `UWVM_PY_TEST=python-π`. System environment inheritance is disabled for UWVM.

The actual operations cover dataclasses, annotationlib, inspect, typing, weak references/GC, collections, a single-thread nonblocking queue operation, traceback, JSON, math, struct/bytes, enum, Unicode, arguments/environment and real Python `sys.settrace` line/frame-local observations. The trace observes an absent `counter`, then 7, then 12, then return 12. This is independent language tracing, not an Rdbg Python-frame/locals bridge. Native thread creation and complete asyncio support were not tested. Distributing the complete Lib tree does not establish complete standard-library or CPython regression-suite compatibility.

| Engine | Source identity | Binary SHA-256 | Operations |
| --- | --- | --- | --- |
| native CPython 3.14.8 | pinned producer native build | `f36bef8c90257efac58a0ebc1e5c0f5231ab0f5c00246fdac9c014de123d837c` | 16/16 |
| Wasmtime 42.0.1 | pinned official release | `21f8e8f994a96d2267afe4a4c06a6302e78aca20e9438afbf01dd443fe32108b` | 16/16 |
| uwvm2 | `sha256:b3457d8dd19e130395dd59c50ce180b75e9ad3b7f89b91faadb4a0353bf4a5af` | `31ad5edb0149b6c0315614416bd491a612234860cfefad9fb3a2d3a7adb06f09` | 16/16 |
| uwvm2-stable | `sha256:334611ea03a3b8bfea8cc94a1f0d8528a2d615dd4abbb72695e0c16831cdfb3e` | `4d200c723504904f8206d75498f6ba0dbc562926edbd8b78e611e7a9419321c2` | 16/16 |
| uwvm2-ros | `sha256:8165592f78db1d7acf302a575a50f61c8f2f9514df537de1dfd14216a4edd7a0` | `6e18792a93074734976b76277c1fd44bb0a00678110ab2dca0a4cd86df7cb9dc` | 16/16 |

UWVM products are `/tmp/high-level-20261017/fd-readdir-candidate-r2/products/{repository}/uwvm`; their source snapshots and build receipts are independent of the preserved old products. The scoped runtime change is the reviewed 32/64-bit fd_readdir truncated-entry serialization correction. Build receipt `9851c8bb329783b7cbcd5161ac8fe6e76a304598183e063dc29aa8187d5ec706` and [candidate qualification](fd-readdir-candidate/qualification.md) bind unchanged compiler/LLVM inputs and the exact six-header delta.

The ROS invocation additionally grants the three named capabilities `random`, `monotonic-clock` and `realtime-clock`. Main/stable receive no extra load arguments. This explicit selected policy is not a proof of formal permission minimality. Random seeding was left normal; no deterministic-hash-seed workaround or extra directory preopen was used.

Every engine produced the identical complete 2,519-byte stdout, SHA-256 `d223584f129fed4c0b65cd211033e31515c215ef2d86f001624e140e9dae3fa1`, with strict UTF-8, canonical JSON plus one LF, exact schema, exact 16 probe names, individual PASS status and a 16/0 summary. Native/Wasmtime stderr is empty. Raw UWVM stderr is retained; only complete LF-terminated lines with the exact known engine prefixes `[llvm-jit-full] `, `[llvm-jit-cache] ` or `[gc-managed] ` are separated into engine diagnostics. The remaining guest stderr is empty for all three. The fixed guest source contains none of those prefixes.

The original stock module was unchanged: `259d031c351311f87cbd0d0adf3e40d2aba7f501723ced37b2836eb086d2b09b`. The actual `-c` UTF-8 bytes and saved probe source both hash to `cc1ed7faeb9b2a9eb56420f3deafe7db5fae7fda19b24047427e9fd54465b6ce`. Standard-library and native-generated asset inventories hash to `1d255fae1d7c4bda5e36c1fbd23cc3a8022cccb3a2084dd8ccce153f6e6d6411` and `a4c0284ca77c14a62f5c27922a7c4c896d05883079158259a26e3b1b6eead143`; both inventories are unchanged after execution. Repository policy hash before/snapshot/after is `38bb55cd06ee91e4eb0cb480a189b6853457a6b21110cc157c8ba29b6bd9607e`. No bound input mutation was found.

Execution was serial on CPU 4 in the original SSH Linux Docker cgroup, memory.max 64 GiB, swap.max 0, core size 0, boot `33464c92-7dae-4410-b9f7-92e3edf249b9`. The outer guard capped owned RSS at 8 GiB, gave parent jobs priority at 50 GiB shared anonymous memory and kept a 2 GiB disk floor. Actual outer peak owned RSS was 996,868,096 bytes; no guard stop or OOM event occurred. All five target groups and the outer controller are retired, ownership records are closed, and all recorded pidfds were released. No performance sampling ran.

Small original evidence is preserved under [evidence/stock-stdlib-r2](evidence/stock-stdlib-r2/): all raw stdout/stderr, leaf/outer lifecycle receipts, exact source/controller, policy snapshot and asset inventories, without copying the 48 MB bundle again. Main report SHA-256 is `72de00619a12eda929c2b351d668adb4f138c3aadb6fda9f1cdf01a3315259fa`; summary SHA-256 is `7b65dc909614a839c8e7e3351ac3dcad5818d595c6ed53786026a0ca6ad700cf`. The actual runner is `stock_stdlib.py` SHA-256 `634d591afe6d843cc6c9756c014f9dd439f0123ad71a70d2acc533ab42885cd2`; actual outer controller is `stock_priority_launch_r2.py` SHA-256 `6ca2c07d7687d376c73d9936f186d2767f82e0b4bd5e15e47d735a001bd227da`.

The executed command, inside the same approved cgroup, was:

```sh
python3 /tmp/high-level-20261017/uwvm2-ros/test/0050.high_level_languages/python/stock_stdlib.py \
  --build /tmp/high-level-20261017/python/work-r4 \
  --out /tmp/high-level-20261017/python/stock-stdlib-r2 \
  --wasmtime /home/macromodel/.wasmtime/bin/wasmtime \
  --products /tmp/high-level-20261017/fd-readdir-candidate-r2/products \
  --load-args-policy /tmp/high-level-20261017/uwvm2-ros/test/0050.high_level_languages/engines/r4-preparation/stock-load-args-policy.json \
  --timeout 180
```

The qualified output directory is immutable evidence; a reproduction must select a new output directory and retain the same outer lifecycle/resource guards. Runtime mounting rights were not advertised as read-only.
