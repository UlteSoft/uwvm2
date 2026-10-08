# Private numeric leaf specialization for native exception handling

This is a source design dated 2026-10-02. It does not authorize or implement a production change. r11 GC and the separate Windows COFF include fix remain frozen. No compiler, VM, WAT assembler or SSH command was run for this audit. Actual qualification belongs to the Linux/platform keeper.

The recommended next experiment retains real native C++ exception search and cleanup. It removes eager diagnostic capture only from a private copy of a numeric leaf whose direct caller is proved to consume every possible guest exception without retaining its instance. The public function and every unknown route keep the existing complete original trace. The default pending-numeric island is already a different, faster protocol; a forced `native-unwind` run must not silently select that protocol.

## Evidence and the actual call path

The completed ordinary 4096-depth hardware report supplied to this audit attributes visible head samples of 40.45% to backtrace and 50.98% to raise/resume, with a 41.63% truncated-branch weight. These are incomplete visible attributions, not complete phase proportions, mutually exclusive totals or a forecast speedup. The ROS counterpart was interrupted. Removing capture cannot remove the remaining native raise/resume work. No improvement is claimed here.

Current source locations in the ordinary repository, before this proposed experiment:

| Location | Actual behavior |
| --- | --- |
| `src/uwvm2/runtime/lib/uwvm_runtime.default.cpp:20004` | `llvm_jit_throw_numeric_abi_bridge` authenticates the actual module/tag, preflights the exact tuple width, copies numeric bits and roots the tag. |
| `uwvm_runtime.default.cpp:20032` | Numeric throw eagerly calls `capture_runtime_exception_trace()` before creating/publishing the owned immutable value and calling `throw_value`. |
| `uwvm_runtime.default.cpp:20138` | The general tuple bridge does the same after preserving every reference payload root. |
| `uwvm_runtime.default.cpp:3208` | In qualified diagnostic-unwind mode, capture calls the actual `_Unwind_Backtrace`, authenticates JIT PC/function ranges and merges genuine native/T0 boundary records. Other policies retain their existing logical-stack path. |
| `uwvm_runtime.default.cpp:2550` | The bounded physical backtrace capacity is currently 64 frames. Workload depth 1024/4096 and profile truncation must not be described as the trace-buffer capacity. |
| `uwvm_runtime.default.cpp:20219` | `throw_ref` retains and throws the same immutable exception value. It does not overwrite its original trace. |
| `src/uwvm2/runtime/exception/value.h:482`, `activation.h:117` | A normal C++ throw creates a fresh native activation/header for the owned value; the native-roots and external-handles variants retain their existing owner protocols. |
| `translate/single_func_exception_emit.h:195` | A potentially throwing guest call becomes an `invoke` when lexical handlers or logical/GC cleanup require it. |
| `single_func_exception_emit.h:239-304` | The typed C++ guest landingpad subsequently chooses the actual Wasm tag/ordered handler. An unmatched Wasm tag calls native `__cxa_rethrow`. |
| `translate/opcode/branch_cases.h:124` | The existing first matching same-function non-reference handler turns a lexical throw into a tuple branch. This shortcut already avoids allocation, tracing and native unwinding and must remain unchanged. |

The relevant references explain different layers. Core 3 routes a thrown exception through the first matching handler and permits retaining its exception instance. These identities and clause order constrain specialization. [Core 3 exception execution](https://webassembly.github.io/spec/core/exec/instructions.html#exec-throw).

The Itanium ABI searches until a native handler is found, then performs cleanup. A handler that rethrows starts another search, and unwinder-private header fields are not an application cursor cache. Thus the first native handler need not be the consuming Wasm handler. [Itanium exception ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html#base-abi).

LLVM preserves normal and exceptional continuations through `invoke`, typed landingpads and cleanup/resume. A private copy must retain those constructs and the real platform ABI. [LLVM exception handling](https://llvm.org/docs/ExceptionHandling.html#try-catch).

Wasmtime 49 uses a separate validated exception-table search, dynamically resolves imported tags, and transfers to a matching handler through its own exception ABI. This supports separating guest effects from native unwind effects, but does not make its transfer sequence a compatible replacement for UWVM C++ unwinding. [Pinned Wasmtime handler search](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/wasmtime/src/runtime/vm/throw.rs).

## Why personality trace reuse is deferred

A wrapper around `__gxx_personality_v0` could observe actual phase-one contexts, but that alone does not replace the existing original trace:

1. Native search ends at a typed C++ guest catch. Current Wasm tag matching happens later; that landingpad can rethrow. Older original callers above this catch have not all been visited.
2. A leaf or cleanup-free function can have CFI without a personality call. In the pinned LLVM 23 sources, `DwarfCFIException.cpp:74-91` and `EHPersonalities.h:97-107` show that an unknown personality plus an unwind-table request could force its emission; merely wrapping existing personality mappings is insufficient. Making every function participate also changes metadata, code-generation classification and object/cache contracts.
3. The current immutable value and diagnostic owner are published before native propagation. A trace builder that continues to mutate a shared published value would break the existing lifetime contract, especially for captured references, external handles and concurrent `throw_ref` activations.
4. A generic C++ exception class is not a guest-value membership proof. No private C++ exception-header offset, unwinder `private_1/private_2` field, raw TLS flag or return-PC guess can grant authority to write an original trace. The LLVM libc++abi throw implementation initializes its own activation and raises it; its internal layout is not a new public callback contract. [LLVM C++ throw implementation](https://github.com/llvm/llvm-project/blob/main/libcxxabi/src/cxa_exception.cpp).
5. GNU Win64 SEH has a different personality entry ABI and dispatch context. The existing per-object executable wrapper and authenticated `.pdata/.xdata` ownership must remain intact.

A future segmented phase-one collector would need a private activation-bound buffer, complete physical-frame coverage, original outer/T0 boundary completion before any reference publication, exact CFA deduplication across native rethrows, and a once-sealed original trace. That is a broader lifecycle/ABI project. It is not the proposed next patch.

## First implementation scope and the proof

The first scope is ordinary/ROS LLVM full, a real initialized and retained owning source, actual completed fused validation/translation, native guest EH enabled, and no debugger-management admission. Lazy, tiered, interpreter, raw-only ABI and cross-module calls remain on their current paths. A missing optimization proof is a decline, never a validation rejection or a reason to reparse the body.

For the first experiment, keep the containing module closed: no imported function, tag, memory, table or global and no user host callback. This deliberately makes imported-tag aliases and host reentry ordinary complete-trace cases. Do not relax the existing whole-module pending-plan publisher to admit a partial leaf; the new proof is a distinct compiler-private native-specialization record, not pending ABI admission.

The callee eligibility record is produced during its one existing successful fused pass. It carries source/module/function identity, completion, exact numeric signature/local eligibility, and a conservative set of escaping linked tag instances. Collect effects only from actual validated instructions and their resolved operands. No whole-body validator prepass or second Wasm-byte parse is allowed. Compiler-created IR may be inspected/cloned cold after the complete module traversal.

Initial leaf restrictions:

- Numeric i32/i64/f32/f64 signature and locals; numeric tag payloads only. Do not infer this from flattened reference carriers or the exported function ABI alone.
- No outgoing direct call, self recursion, `call_ref`, `call_indirect`, `return_call*`, host call or unknown target. A caller's tail transfer also declines because its lexical handlers do not survive the transfer.
- No `throw_ref`, reference-valued operation/local/global, `catch_ref`, `catch_all_ref`, GC allocation/field access, vector payload or captured old exception. Even a statically null `throw_ref` can simply decline this first experiment.
- Initially allow only the needed scalar numeric/control instructions and own escaping numeric throws. Memory/global/table/atomic/GC and other helper-producing instructions can decline until their precise helper-effect contract is included. This adds no checks to ordinary memory accesses.
- A throw already consumed by the existing same-function non-ref branch adds no escaping tag. If a summary is incomplete, the emitting body fell back, or any unsupported instruction occurs, decline. Cold proof must not guess purity from omitted/dead IR.

At an actual ordinary direct call instruction in the caller, preserve the validator-created lexical handler order: inner frames before outer frames, and clauses in their original order. For every callee escaping tag, resolve the first matching actual instance or `catch_all`. It must be non-reference. A previous matching reference clause defeats the optimization even if a later outer non-reference catch exists. A missing/unowned identity, empty/incomplete candidate proof or unsupported call route declines.

Tag identity is the authenticated linked instance, not tag index, type index, payload shape, signature, name or function subtype. The existing shared resolver in `compiler/shared/wasm_exception_control.h:49-90` already resolves imports to provider records; if a later version admits imported tags, it must prove the actual provider record/root ownership and preserve aliases before comparing identities. Distinct same-signature local tags must remain distinct. The initial no-import scope must still run the existing alias fixtures through the complete-trace path.

Numeric-only payload plus first consuming non-ref catch means the new instance cannot be exposed as an exnref along this private edge. A preexisting exnref elsewhere in the caller retains its immutable value and original trace; the specialization cannot modify or replace it. Catch continuation may throw a new instance later, which uses its own ordinary trace rules. No clone can be entered from a Wasm export, table, `ref.func`, raw entry, host callback or another unproved callsite.

## The code change that the proof permits

Keep the original complete typed function. Create a private machine function copy only after complete successful fused emission and cold proof. Redirect only the proved ordinary direct `invoke`/call to that copy. Keep the original LLVM function type, calling convention, argument attributes, exact multi-result buffer layout and normal/unwind destinations. Preserve `NoInline`, `NoMerge`, `nooutline`, stack probes and UWTable kind. The clone is not a Wasm function index or a new canonical function type.

Inside this copy, replace only authenticated escaping numeric-throw calls with a separately named host bridge that runs the same module/tag/signature preflight, owned payload publication, tag/root ownership and `throw_value`, with an empty diagnostic trace. It still allocates a real immutable guest exception and performs genuine native raise, cleanup and catch. It does not set `nounwind`, remove `invoke`, bypass C++ cleanup, substitute a pending return or modify the standard validator.

Prefer a common numeric throw builder with a compile-time trace policy so the existing complete bridge and the private consumed bridge cannot diverge in payload/root checks. The original tuple bridge and `throw_ref` remain unchanged. Preserve `source_exception_guest_bridge::try_publish_owned` and the registered/native source lease paths: skipping trace does not permit an unregistered owner or a different census policy.

The private bridge declaration is engine-scoped and exact-ABI bound before finalization. Verify that its only IR users are the proved copies; it must not become a process-global symbol or guest import capability. Cached objects must undergo the same current-source proof and binding. A flag named `safe`, a `use_count`, an LLVM metadata string, or a caller-supplied address is not proof. Actual function/owner membership and completed traversal remain authoritative.

## Trap, source and native range mapping

The copy remains a physical activation of the original Wasm callee. Trap output from a divide, unreachable or stack probe must report that original function/module and its real caller. It must not report a private synthetic Wasm function or lose the frame.

`pending_llvm_jit_code_ranges` already records actual loaded text and exact function extents transactionally, including private physical functions. `refresh_llvm_jit_unwind_entry_bounds` uses a matching loaded function start rather than borrowing the interval before a nearby public entry. Extend full materialization's currently public/raw-only identity registration (`uwvm_runtime.default.cpp:13797-13831`) with bounded private-copy records:

1. Resolve every required copy inside its actual still-private engine. Authenticate the actual loaded executable function extent and its exact original source mapping. A declaration, zero/missing address, alien engine address or absent/torn extent declines before any runtime publication.
2. Retain the copy's code/context/source and CFI with the existing complete module publication. Record its actual code address under the original `(module_id, function_index)` with `wrapper_entry=false`. It represents the guest callee and must not be hidden as a raw host wrapper.
3. Commit ranges and identities before callable publication. On failure, roll back the private candidate/module consistently; never publish a partly unmapped copy. Do not infer an address or upper bound from symbol-name resemblance, allocation padding or the next typed entry.

Use a module-qualified deterministic named hidden definition where necessary for cached/partitioned symbol lookup. Such a native name does not expose a Wasm export. It must remain distinct with the same common no-merge attributes. `-Rct1` partitions must preserve the existing local Win64 personality wrapper, and each object must retain its authenticated CFI/table ownership.

Instruction call-stack mode copies the original callee's logical push/pop and EH cleanup, with the original Wasm identity. Unwind mode copies its CFI and adds no per-call instruction frames or handler stack. `none` and `unwind-uncheck` retain their existing diagnostic-policy behavior; native EH qualification is still independently required. Do not implement the private call using a raw host adapter that inserts or removes a guest frame.

## Debugging, replacement and caches

The initial implementation declines whenever debug management, a debug observer, safe-point emission, a patchable typed-target slot, a lazy target slot or a runtime call bridge is configured. This is a publication condition, not a command-name check.

Current debug configuration (`uwvm_runtime.default.cpp:18516-18543`) only succeeds before runtime/compilation publication. An armed server is configured before compiling its bodies; later authenticated attach changes the session/stop state, not the already frozen eligibility. Those bodies therefore never acquire private copies. Non-armed normal full cannot create management afterward through this API. If that admission model changes, this optimization must first gain a real stopped-set invalidation/recompilation protocol; ABI equality does not prove that a replacement has the old escaping-tag set.

Debug replacement already reloads actual addresses through host-owned typed slots and requires real generation/source proof. Do not cache a private copy's executable address in function-reference/type-projection caches, tables, import caches or debugger metadata. The original public/raw entries and their ABI remain unchanged. With initial debug decline, no copy can bypass a later function replacement or stale session generation.

Add a distinct code-generation policy key, for example `native-trace-specialization=private-leaf-v1`, and an explicit off/declined identity for old objects. Run proof and cloning before `module_bitcode_hash` / the partitioned-cache lookup (`uwvm_runtime.default.cpp:13246-13280`) so the complete IR hash covers private bodies, redirects and bridge declarations. Reconstruct the current authenticated clone/source binding on each cache load; a cached metadata record alone never admits a copy.

If imported-tag specialization is later admitted, the effective alias/handler decision must also close cache identity. Prefer a stable authenticated decision digest derived from the actual topology/ordered selection, never persisted raw tag pointers. The first no-import version avoids that additional dependency. Changes in feature configuration, call-stack policy, source bytes, native ABI, debug admission and specialization version must independently prevent inappropriate cache reuse.

## Minimal proposed patch locations

| Proposed location | Responsibility |
| --- | --- |
| Fused LLVM instruction emission plus `single_func_exception_emit.h` | Populate per-function completed leaf/tag effects and per-direct-call ordered handler candidates during successful original emission. The int/pure validators and Wasm1p1/Wasm2 validators are untouched. |
| A new compiler-private leaf-EH finalizer beside `single_func_pending_numeric_effects.h` | Verify actual source/function/call membership after complete fused translation; inspect trusted IR, copy eligible leaves and redirect only proved direct ordinary calls. No body decoding, global authority flag or whole-module numeric admission relaxation. |
| `uwvm_runtime.default.cpp` numeric builder and narrowly scoped native binding | Share complete payload/root checks, add the engine-local consumed-numeric bridge and bind it only after current complete proof. Existing actual full-validation recording can be used without a second traversal; normal full currently records that source epoch only in its pending-plan branch, so a new private candidate must explicitly obtain its own completed normal-full proof. |
| Full materializer/publication and private-copy identity records | Resolve/authenticate all copied functions; retain their source/code/CFI and publish exact original guest identity for trap/unwind lookup transactionally. |
| Full code-generation cache policy | Separate the optimization version; hash transformed complete IR before every full object/cache path; recreate source/binder proof for cache hits. |

A worker's successful local summary does not seal whole-module admission. Parallel task summaries must merge by authenticated module/function identity after every task has succeeded. Pre-link optimization may drop or rewrite candidate IR; missing/unmatched metadata declines. Root/source epoch and executable publication must be established after the complete fused pass, never because a request or compile shape exists.

## Qualification and rejection witnesses

No production or test runner was changed by this document. The following are required before enabling the experiment:

| Witness | Required result and admission |
| --- | --- |
| Existing cross-call non-ref numeric fixture | Exit 0; private copy selected only for the actual proved edge. Native full proof, real C++ raise/catch/cleanup and correct numeric payload/count remain visible. |
| Same exported leaf called once under non-ref catch and once escaping | First call uses the copy; escaping public call reports the original leaf and caller trace with the complete expected payload. |
| Same leaf through table or `ref.func` under a reference catch | Original public path only; preserved exnref rethrow reports the original leaf, not the later throw_ref wrapper. |
| Leaf with two different numeric tags; caller catches only one | Entire callsite declines. The other tag reaches the outer catcher/fatal path with its original trace. |
| Inner matching catch_ref/catch_all_ref before outer non-ref | Decline; exact original instance/trace survives. Imported alias version must use ordinary native path and preserve the alias behavior. |
| Earlier unrelated same-signature tag plus later non-ref matching tag | Actual instance comparison selects correctly; signature/index equality cannot silently consume the wrong instance. |
| Existing GC payload and old-exn-payload fixtures | Original complete trace/root paths; no specialized reference tuple or overwrite of the older immutable trace. |
| Tail call or outgoing/ref/indirect/imported/unknown callee | Decline without changing validation or musttail behavior; an outer caller's expired handler is never treated as active. |
| Trap inside eligible physical copy | Same original callee/caller stack identities in instruction and unwind policies; no synthetic/missing frame. Include divide/unreachable and qualified stack-overflow cases. |
| Armed debugger, attach during execution, and same-ABI replacement with a new escaping tag | No private copy at initial compilation; fresh target generation and original trace remain correct. Attempting post-publication debug reconfiguration retains its existing rejection. |
| Uncached, cache store/load, `-Rct0`/`-Rct1` and changed call-stack/feature/source/debug policy | Correct current-source proof and exact loaded copy mapping on every selected path; stale objects cannot enable the optimization. |
| Host reentry/imported provider or concurrent throw_ref of an older shared value | Ordinary complete native path; independent C++ activations and unchanged original diagnostic owner. |

Official Wasm validation/comparator execution must precede product checks. Compare semantic results in pure/int full/lazy and LLVM full/lazy as supported, while only qualified full has this optimization. Linux, macOS (at most 4 GiB), the actual Windows VM and qualified Linux QEMU platforms each need their own native EH, trap, object-cache and retirement witnesses. A Linux success is not a Windows/other-architecture pass.

For performance, retain the exact source-bound checksum workload and forced `native-unwind`, pin it to the assigned P core inside the 64 GiB cgroup, and compare capture-on baseline with the private-copy variant using the same input/CFI/call-stack policy. Record actual selected-copy count and separate normal-call throughput, payload/allocation/retirement, backtrace and raise/resume hardware evidence. Keep the existing auto pending result as a separately named comparator, never as evidence that native unwind became faster.

The first patch is acceptable only if every omitted-capture edge has the complete static consumption proof, every public/unknown/ref route remains complete, and exact trap/source mapping survives real object loading. Broader tag-set propagation, SCC specialization or partial pending islands are subsequent designs rather than implicit extensions of this proof.
