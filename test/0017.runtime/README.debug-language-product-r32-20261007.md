# Fresh full-product language debugger regression — R32, 2026-10-07

Both repositories now have a fresh complete x86_64 Linux CLI/runtime/host product qualified for the finite compiler-backed language expression matrix below. This closes the missing live product check for these cases; it does not establish complete native language/debugger parity. This round adds paired regression fixtures and reusable matrix, lifecycle and replacement runners. No production C++ source was edited by this round.

All compiler, executable and functional Python work ran on SSH Linux inside the original 64 GiB cgroup, swap=0, original CPU set and birth/PIDFD supervisor. The separate full-product role uses 16 GiB owned RSS, 2 GiB total outputs, 512 MiB per file/8 MiB log limits and the original frontend free-space floor. Earlier bounded DATA roles were not expanded. All successful and failed owned trees were retired/reaped; no OOM event changed. The LLVM23 ROS.11 developer provider is separately pinned and historical; the latest complete ROS LLVM SDK source/provider parity is not qualified.

| Check | uwvm2 | uwvm2-ros |
|---|---:|---:|
| Fresh product translation units + link | 3 + 1 | 3 + 1 |
| Genuine new Wasm modules including final runner replay, wasm-tools + LLVM DWARF verification | 40 | 40 |
| Actual -Rdbg matrix sessions including final runner replay | 80 | 80 |
| Target compiler oracle comparisons including final runner replay | 1440 | 1440 |
| Sustained real debugger sessions | 2000 | 2000 |
| Sustained target compiler comparisons | 36000 | 36000 |
| Sustained duration, seconds | 535.085 | 530.620 |
| Hot replacement configurations | 4 | 4 |

The matrix covers C17, C23, C++20, Objective-C/C17 and Objective-C++/C++20, Wasm32/64, DWARF4/5 and instruction/unwind call-stack policies. Source and linker optimization are both 0. Objective-C coverage here is finite C-family expressions, not dynamic method/native debugger parity. Each original compiler module stores 18 volatile oracle results. The debugger reads those genuine guest values at an actual source breakpoint and compares the expression results. Examples:

| Unevaluated expression | C/C23/Objective-C | C++/Objective-C++ |
|---|---:|---:|
| sizeof(1 ? small : small), small is signed char | 4 | 1 |
| sizeof(1 && value) | 4 | 1 |
| sizeof('a') | 4 | 1 |
| sizeof(sizeof(value) + wider), wider is long long | 8 | 8 |
| sizeof(sizeof(value) + wide), wide is long | 4 on Wasm32 / 8 on Wasm64 | 4 on Wasm32 / 8 on Wasm64 |

The live CPP Boolean result also displays as true. The matrix checks explicit frame queries, fabricated and actually retired stop IDs, rejection of assignment/postincrement, unchanged source values, real guest completion without a trap and managed shutdown. The sustained run cycles the same 40 configurations, with a 600-second/2000-session upper bound per repository; the session count limit was reached first. These repeated finite assertions are not distinct debugger features. The packaged lifecycle runner also passed two four-session CLI smoke checks in each repository.

The hot replacement checks run in the actual prepared debugger before executing a Wasm instruction. An invalid void-function body with an extra i32 stack value is rejected; the exact original validated function body (including local declarations) is accepted and advances generation 1 to 2. Reusing expected generation 1 is rejected. After stopping in the replaced function, the original source value/type/locals metadata is refused as stale. The guest then executes to exit 0. This covers C/Wasm32 and CPP/Wasm64 with both call-stack policies: eight configurations across the pair, twenty-four sessions including both packaged runner replays. It does not qualify in-flight state migration, checkpoint restore or replacement with new DWARF metadata.

Final reusable runner review also closes false-success paths: matrix and hot runners require exit 0 plus managed shutdown, hot replacement requires all four unique configurations, and sustained qualification requires actual complete sessions. The exact final drivers passed another 80 real matrix sessions/1440 target compiler comparisons, eight hot sessions and eight sustained CLI smoke sessions across the pair. Four malformed matrix categories (empty input, missing oracle, nonzero shutdown, and duplicate replacement configuration) produced fourteen expected refusals before a guest session. Earlier tested driver bytes and their receipts are retained unchanged. These final followup records and all late logs are in the small sidecar, alongside the immutable main archive.

## Toolchain and retained failures

Twenty guarded jobs passed and twelve earlier guarded failures are retained. Two final-runner startup failures omitted UWVM_TEST_CPUSET in the controller launch; the cgroup guard rejected both before product execution, and the corrected original environment passed in exclusive new output directories. One frozen early runtime cut had an external access to a private checkpoint engine policy enum; another agent had already corrected the current source through the world transaction interface. The paired fresh cut incorporates that correction without overriding its edits. Other retained failures cover the postlink verifier's mistaken main/uwvm basename, help category, launch library path, use of the x86-only native SDK compiler as a Wasm producer, CPP mangled export names, and the Boolean display parser. The corrected producer uses the separate clang22 WebAssembly target and the qualified SDK wasm-ld.

The original default-link DWARF5 fixture failed LLVM's string-offset verifier after debug string suffix merging. LLD's [custom string section merge decision](https://raw.githubusercontent.com/llvm/llvm-project/main/lld/wasm/InputFiles.cpp) disables that merge at linker -O0, while the [LLVM verifier](https://llvm.org/doxygen/DWARFVerifier_8cpp_source.html) checks that indexed offsets follow a NUL byte. The canonical matrix explicitly uses -Wl,-O0 and keeps strict independent verification. The default merged-string fixture and its failure remain in evidence; resolving that toolchain/verifier conflict is still unfinished and is not silently counted as passed DWARF5 coverage.

The ordinary product's four actual compiler/link stages succeeded before its basename assertion failed. A separate guarded, read-only followup verified the original before pins, every actual dependency, product/object bytes and the original retired/reaped tree without recompiling or rewriting the failed receipt. ROS independently compiled all three product objects and linked successfully under the corrected recipe.

## Reusable tests and evidence

- [Matrix runner](run_debug_language_product_size_cli.py) accepts explicit source root, product, fresh build receipt, Wasm clang/wasm-ld, wasm-tools, llvm-dwarfdump and exclusive output paths.
- [Sustained runner](run_debug_language_product_soak.py) accepts the successful matrix JSON and bounded duration/session limits.
- [Hot replacement runner](run_debug_language_product_hot_cli.py) accepts that same actual matrix and checks validation/generation/source boundaries.
- [Machine qualification](debug_language_product_qualification_20261007.json) records source/product IDs, failed and successful guard receipts, resource limits, result paths, archive proof and workspace drift.

Linux-only main archive: /home/macromodel/Documents/uwvm3-implementation/retained-language-product-r32-final/language-product-r32-evidence.tar.xz, 195639760 bytes, SHA-256 a8b0a8cffe5b26c833b81883542d47a1f25db53ba0d21e79b2589719dd01681d. All 8616 members were verified and fsynced; two direct current source cuts plus two nested failed source cuts are reconstructible. Exact earlier transport bytes are retained. External compilers/LLVM libraries/headers remain separately pinned, not bundled. The archive is on the Linux persistent filesystem, keeping large copies out of the bounded testing arena and the local workspace. Earlier own source/transport duplicates were retired only after exact archive/hash/fsync checks; no peer artifact or process was removed.

uwvm2 source ID: sha256:516d640017ff6c57abec6dfec886bf9b8061ecdfbdbaaef570a3e165d7a67868. uwvm2-ros source ID: sha256:c726095fefad6748d1b300220a74c69311d1f18ca15bbede718ed2602a35450a. Concurrent compiler/runtime/checkpoint/native-caller edits and a new WASIp1 checkpoint header are recorded in the final workspace review and machine qualification. This report qualifies the frozen cut and stated routes, not the latest entire workspace.

Full scoped typedef/CV/reference/glvalue/overload grammar, VLA/function sizeof, variable assignment, Rust/standard Go/TinyGo/Zig/AssemblyScript native semantics and fresh live products, default merged DWARF5 verification, full checkpoint/in-flight migration and remaining architecture products are unfinished. ROS full uwvm-int/full LLVM-JIT policy and the Wasm-generated-context-only ASM boundary are unchanged; no host/VM ASM debugging was added.
