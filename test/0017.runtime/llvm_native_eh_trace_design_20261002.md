# Native EH trace design audit — 2026-10-02

Production r10 stays frozen. This document and its seven fixtures are source-only preparation for remote oracle, semantic, and profiling work; they have not been compiled or run locally.

## Current finding

The proposed same-function optimization has no eligible normal path left to optimize. In LLVM opcode/branch_cases.h, a checked throw first calls shared wasm_exception_control::find_handler. A first matching non-ref handler turns that throw into a tuple branch. It creates no exception value, captures no trace, and calls no native unwinder. Preserve this faster path.

Only escaping throws or throws first matched by a ref handler reach single_func_exception_emit.h::try_emit_runtime_local_func_llvm_jit_throw_tuple. Both llvm_jit_throw_numeric_abi_bridge and llvm_jit_throw_tuple_abi_bridge then call capture_runtime_exception_trace before throw_value. In unwind mode, capture_runtime_exception_trace calls the bounded physical backtrace helper; the subsequent C++ throw starts native EH propagation independently. This is a source hypothesis about repeated work, not measured hotspot attribution.

The existing run_wasm_exception_performance.py workload really throws in $step and catches in _start. The callee's own lexical stack therefore cannot prove the caller's non-ref consumption. Generated Wasm functions carry NoInline/NoMerge/nooutline, so the call boundary survives ordinary LLVM optimization.

## Exact local proof and its limits

A correct source proof traverses live lexical frames from innermost to outermost and each frame's catches in declaration order. Resolve the thrown and caught tags to their actual initialized instance identity. The first matching tagged catch or catch_all decides the result. Its with_reference flag must be false. A matching catch_ref/catch_all_ref stops the search and declines the optimization; it must not be skipped to reach an outer non-ref catch. Unknown or unpinned metadata also declines.

Current initializer code creates a distinct owning identity per local tag and resolves every imported/reexported tag to its actual provider-defined record after checking alias cycles and signatures. The shared local matcher compares those records; native landingpad matching compares exception_identity.get(). A numeric module-local tag index, function signature, or export name alone is insufficient. In particular, imported X and Y may alias one tag even though their indices differ.

Core 3 checks handlers in this order and gives ref catches an exception reference that may survive the handler. See the [official throw/throw_ref execution rules](https://webassembly.github.io/spec/core/exec/instructions.html#exec-throw). A same-function non-ref catch discards the new exception identity but can still return an OLD exnref contained in its payload. That older immutable value and its original trace must remain owned.

No inference from a callee's lexical stack applies to callers, indirect targets, public exports, host reentry, tail-retired handlers, or separately instantiated modules. Function hot replacement preserves tag ABI, but cannot strengthen a callee's call-site proof. Existing cached object identity hashes the freshly generated IR, covering decisions that change branch/native lowering; lazy ObjectCache also hashes IR when cache_key_is_complete is false.

## Native propagation and diagnostic lifetime

Keep throw_ref on its current path: it retains and throws the existing immutable value instead of recapturing the stack. Ref catches issue an owning exnref before __cxa_end_catch. Tuple catches copy/root complete reference carriers before releasing the native activation, and may enter another throwing continuation only afterwards. Any trace optimization must preserve these ownership steps and precise GC-root snapshots.

Do not move trace capture into the selected catch block as a simple replacement. The original callee may already have unwound by then; an escaped saved exnref would lose its throw origin. The [Itanium EH ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html#cxx-throw) separates search and cleanup and can restart search after rethrow. Its typed C++ handler selection does not decide our later Wasm tag dispatch. LLVM's [EH model](https://llvm.org/docs/ExceptionHandling.html) likewise transfers control to landingpads through personality-driven invoke edges.

Possible future work requires a separate proof and measured benefit: an explicitly private callee specialization with a closed set of verified consuming call sites, or collecting diagnostic identities during actual native EH search before any frame disappears. The latter needs independent per-activation storage, immutable publication at the correct boundary, nested rethrow handling, and platform-specific ABI qualification. Neither is a safe one-line change to the current immutable value or personality bridge. No new TLS handler stack, call ABI, cache ABI, CLI, or whole-body prepass is proposed here.

## New remote witnesses

All are valid Core 3 modules. Expected runtime failure cases must report a genuine uncaught WebAssembly exception, the expected payload, and a nonempty untruncated original throw stack; a parser, validator, materialization, or unrelated trap failure is not success.

| Fixture | Expected result and purpose |
| --- | --- |
| eh-native-trace-cross-call-nonref.wat | Success, exactly 32 checked payloads/catches; real callee -> caller native EH when native-unwind dispatch is selected. |
| eh-native-trace-cross-call-gc-payload.wat | Success; struct payload 73 remains live across unwind and later allocations. |
| eh-native-trace-tag-alias-provider.wat | Provider exports X/Y aliases plus raise; preload as EHOwner for the two consumers. |
| eh-native-trace-tag-alias-local-ref.wat | Uncaught payload 41; inner alias ref catch shadows outer non-ref catch. Original $probe frame remains in the saved value's trace. |
| eh-native-trace-tag-alias-callee-ref.wat | Uncaught payload 41; same shadowing across a provider call. Original provider raise frame remains. |
| eh-native-trace-old-exn-payload.wat | Uncaught ORIGINAL payload 43; wrapper is caught without a ref, yet its payload returns the older exception. Trace contains $initial, not the later $rewrap origin. |
| eh-native-trace-catch-all-ref-shadow.wat | Uncaught payload 47; inner catch_all_ref cannot be bypassed in favor of outer non-ref catch. Original $probe trace survives. |

Run wasm-tools parse/validate and a qualified Wasmtime comparator first, then the two products. For actual native-cost measurements explicitly select -Rllvm-exception-dispatch native-unwind, cache disabled, and compare instruction/unwind policies on the assigned P core in the 64 GiB cgroup. Do not let the pending numeric island replace the path being profiled. Test ordinary full/lazy and ROS full as appropriate, and verify emitted code/native backend evidence. Include -Rct1/cache store/load for the alias consumers when qualifying object reuse.

For alias consumers the product preload arguments are --wasm-preload-library PROVIDER.wasm EHOwner, with a distinct main module name. First validate the successful payload/root paths; bounded uncaught logs then check saved-value origin and trace retention. Native full Linux, Windows VM, macOS limits, and qualified QEMU hosts need their own evidence.

Profile the existing checksum workload before extending it. Attribute actual samples separately to capture_runtime_exception_trace/_Unwind_Backtrace, native raise/personality/cleanup, payload/value allocation, reference roots, and end-catch source retirement. Whole-process uncaught timings do not isolate per-throw cost. No performance improvement is claimed until the actual hotspot data and repeated source-bound runs support it.

## r10 actual Linux witnesses and supplementary proof correction

The first actual full-only run completed 44 checks per product. All six runtime cases had the intended semantic outcomes and original trace/payload evidence. Four JIT rows per product were recorded as failures because the supplementary runner incorrectly demanded `owning-source=yes pending-plan=native body-fallback=no` for the two preloaded tag-alias consumers. Those raw failures remain preserved; they are not converted into a fresh execution pass.

The optional full-source owner pin is a single-source optimization admission. Preloaded modules use normal full-native materialization without that optional pin. Their actual logs show full-only JIT translation, disabled interpreter translation, completed optimize/finalize/materialization for BOTH TraceConsumer and EHOwner, followed by full compilation and the correct escaping exception. Requiring the optional source-pin log conflated that optimization with standard native EH.

The corrected runner SHA-256 is `4fed4762d1d12835d416d9b8cb053afabfbe29f56d92bb1b5f62ee5345ba770d`. It requires the real full-only configuration and finished native materialization for every expected module. It rejects pending r2 dispatch or numeric-body fallback; the optional single-source log is recorded separately. Independent static reanalysis of the SHA-verified 24 JIT-full raw rows across both products passed this corrected proof, and removal of either required finalization, injection of r2 dispatch or body fallback was rejected. This is raw-log reanalysis, not a new VM run. The corrected runner still needs fresh remote execution in a new evidence directory.

[Original logs and failure receipts](../../build/wasm3-evidence/platform-diagnostics-20261002-r10-r1/mirror-manifest.json) are mirrored identically between repositories. Fresh Windows compilation separately found SDK `IMAGE_*` macro collisions while parsing LLVM COFF enums. The source fix temporarily shields/restores those names and retains the original unwind-object safety checks; its target build and VM results remain pending.

### Fresh corrected execution

The corrected runner was subsequently executed in fresh remote r2 directories against both unchanged r10 ELF binaries. Each product passed 44/44 checks. Official parsing/validation and Wasmtime outcomes, interpreter-full outcomes, 12 actual JIT-full rows per product across instruction/unwind policies, expected native module finalizations, and original immutable payload/caller chains all passed. The parent independently checked raw-log SHA/length, exact runner and executable identities, source-ID receipts, before/after equality and the actual 64 GiB/swap-zero/CPU cgroup fields.

[Fresh r2 raw evidence](../../build/wasm3-evidence/native-eh-trace-20261002-r10-r2/mirror-manifest.json) is synchronized to both repositories. The original r1 failures remain available separately. Fresh full-only Linux success does not qualify lazy/tiered, Windows/macOS/QEMU or a per-throw performance improvement.

### Ordinary full/lazy execution follow-up

Fresh r3 ordinary execution with the unchanged runner and r10 binary passed
80/80 checks. It adds interpreter lazy/lazy+verification and LLVM lazy/
lazy+verification to the full-only witnesses. The 24 lazy JIT runtime rows
record actual successful lazy compilation; both instruction/unwind policies
retain the original exception payload and caller chain. No pending numeric
fallback or alternate body dispatch was accepted. Root independently verified
raw log SHA/length, before/after inputs, source/executable identity, and the
64 GiB/swap-zero cgroup receipt.

The [ordinary all-mode r3 mirror](../../build/wasm3-evidence/native-eh-trace-20261002-r10-r3-ordinary-all-modes/mirror-manifest.json)
has 169 payload files, 714,801 bytes, manifest SHA-256
`c8d90091eb5678587c2597d487083e627949a8cee8b567fcbfea01f8a0f0ff53`.
ROS supports the two full backends covered by its preceding 44/44 fresh r2
run. These witnesses do not cover tier promotion, native-EH latency, Windows,
macOS or QEMU, and do not qualify the separate private-leaf optimization.
