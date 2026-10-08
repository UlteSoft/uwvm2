# LLVM precise-root insertion review, 2026-09-28

This is a static review of the actual development emitter. It is not a compiled root-map qualification, collector qualification, or performance result. No product source was changed for this review. The r2 O3 ELF files do not contain the proposed root-frame integration.

Reviewed ordinary headers, SHA-256:

- `single_func_emit.h`: `4f610552968c3d0255a1383bab73d660e443f1c75b6e2cf03b34ae60831bfb8e`
- `single_func_gc_emit.h`: `5a66be2421db89f4a7bbbf125c366bf3173efc7bbfd9bb56d5f9d060b546ff57`
- `single_func_exception_emit.h`: `915c079304915230bc96d9246b46a0073ff5bde985823e1c5f1f9847aae4c75f`
- `native_exception_landingpad.h`: `0aee7f5c8dafdcf90fe0040dbdee3c7de21d5630061152c0084e649b64ca8000`

Reviewed ROS headers, SHA-256:

- `single_func_emit.h`: `802c6a921ce6af6a2c110e947fec40004dce8d6387d97c12667590bf07470e74`
- `single_func_gc_emit.h`: the same `5a66be...ff57` header
- `single_func_exception_emit.h`: `c0171447c1e4eef795dea010a68d70b9e6b5d7079f883bd611da50d4afcf8a9a`
- `native_exception_landingpad.h`: the same `0aee7f...8000` header

Line numbers below refer to these exact files. ROS intentionally removes lazy/tiered/OSR. Its exception header differs only in logical-frame ownership and its subsequent line numbers are one less.

## Required root inventory

At a collecting or parking point, retain complete typed reference carriers from current reference locals, the live operand-stack prefix, and any reference operands already moved into temporary vectors/slots. Do not infer reference status from `i128` alone: `v128` and reference carriers are both integer-shaped IR. Use the Wasm type descriptor (including lowered GC references, externref and exnref); skip proven null/i31/function-only values only with an explicit representation proof.

`local_types` and `local_pointers` supply the local type/location inventory (ordinary 6561/6589, ROS 6505/6522). Load each relevant local at the actual snapshot insertion point. Entry initialization values are not current loop/call values after local.set or mem2reg. A nonmoving collector can copy the full carrier into a precisely typed published record; its correctness still requires all guest threads to obey the pause protocol.

`llvm_jit_prepared_wasm_call_operands_t` (ordinary 103–116) stores bare `llvm::Value*` arguments, ABI layout and result types, but no parameter type vector. Retain the callee parameter signature or add typed extra-root descriptors. Neither raw host-buffer addresses nor hidden multi-result-buffer addresses are guest references.

## GC operations

The following positions are identical in both repositories' GC emitter:

| Function / position | Actual lifetime | Required action |
| --- | --- | --- |
| `try_emit_runtime_local_func_llvm_jit_gc_aggregate` 387, input_base 437 | All inputs remain in state.operand_stack. | Snapshot all typed reference locals, prefix and reference inputs before a collecting aggregate bridge. |
| Heap-input allocation 451 | Input slots have not yet been written; stack descriptors still own all SSA inputs. | If this malloc-only helper remains explicitly noncollecting, no extra GC snapshot is needed for it. If it becomes a pause/collection point, publish original SSA inputs before calling it. |
| Input slot stores 474–486; fixed5 call 506 / generic7 call 558 | Inputs are still in the operand model during both bridge forms. | Keep the same root contract for specialization and fallback, including struct.new / array.new_fixed counts greater than eight. |
| Scratch free 565; status guards 570–580 | A successful returned reference may exist only in gc.output. | Keep free and validation/trap helpers explicitly noncollecting, or add the initialized output as an extra root on successful paths. Never scan the output before bridge success. |
| Stack shrink 604; result load/push 609–611 | References move from bridge output into operand SSA only here. | Update/retire argument snapshot only after successful result handoff; do not keep dead arguments indefinitely. |
| `try_emit_runtime_local_func_llvm_jit_gc_convert` 328, bridge 366 | input remains on stack until pop 381. | Snapshot input plus prefix/locals before a collecting conversion. Output is initialized only on success and loaded at 376. |

Publication of caller roots alone does not cover temporary native objects created after allocation inside a bridge. Native helpers need their own scoped roots, or a proven noncollecting interval from object creation through output handoff.

## Wasm calls, host calls and materialization

| Function | Ordinary start | ROS start |
| --- | ---: | ---: |
| `prepare_runtime_local_func_llvm_jit_wasm_call_operands` | 8510 | 7974 |
| `try_emit_runtime_local_func_llvm_jit_call` | 9676 | 8890 |
| `try_emit_runtime_local_func_llvm_jit_call_indirect` | 9820 | 8977 |
| `try_emit_runtime_local_func_llvm_jit_call_ref` | 10148 | 9307 |
| `emit_runtime_local_func_llvm_jit_runtime_raw_host_bridge_call` | 8686 | 8150 |

Argument preparation pops values at ordinary 8526. Ordinary call invokes preparation at 9694; call_indirect pops its numeric selector at 9853 and parameters at 9857; dynamic call_ref pops its reference at 10165 and parameters at 10189. Afterward the current state.operand_stack contains only the prefix.

Before a normal callee, runtime raw bridge, lazy materializer or host re-entry, publish:

1. Current reference locals and prefix references.
2. Typed reference parameters in prepared.arguments.
3. Dynamic call_ref's separate reference.value / initialized ref_slot while its resolver still consumes it (ordinary resolver 10218, lazy tail resolver 10263).
4. Any initialized outgoing payload that a nested callback will still use.

The known ref.func shortcut at 10177–10186 names a pinned module-owned function record; it still inherits normal parameter/prefix rooting from call/return_call. An executable entry/context address is not itself a GC reference.

Caller roots must stay published while the callee or reentrant host callback runs. Snapshotting only immediately before the allocating callee's own helper misses its suspended callers' operand prefixes. Each entered guest activation must be independently visible to the collector.

For multi-results, `emit_llvm_jit_typed_wasm_call` 2359 creates a caller-owned result buffer and passes its address at 2378. The buffer is not initialized reference storage before the call; do not mark all result bytes as roots during execution. Typed result reconstruction is at 2380, raw reconstruction at 8703, and normal operand push at 8542/8580. Preserve result carriers before any collecting cleanup/poll inserted between return and reconstruction/push. A noncollecting return handoff is a simpler contract.

A root hook placed only in `emit_runtime_local_func_llvm_jit_may_throw_call` cannot reconstruct popped Wasm arguments from its generic address/int arguments. Pass typed extra-root metadata into the call path, or establish the snapshot before argument preparation and keep it alive through all relevant fast/slow and exceptional edges.

## Return and tail transfer

- `try_emit_runtime_local_func_llvm_jit_return` (ordinary 8491, ROS 7955) branches to the common return PHIs; final cleanup belongs to `finalize_runtime_local_func_llvm_jit_emit_state` (ordinary 7215, ROS 7043). The ordinary return PHIs are packed at 7257 and multi-result bytes are stored at 7267. Reference results must survive any proposed cleanup between PHI selection and caller handoff.
- `try_emit_runtime_local_func_llvm_jit_return_call_self` (9421 / 8679) prepares/pops arguments, captures them all before overwriting locals at 9439, then branches to body at 9441. Reuse one native root frame; refresh the snapshot using the new local values at body entry. Do not publish pre-reset locals as a new activation and do not allocate one root frame per self-tail iteration.
- Direct/import/indirect/ref tail paths must root reference arguments during resolver/materialization (ordinary 9540, 9640, 10033, 10263). Retire the outgoing root record only after all such helpers succeed.
- Genuine musttail sites are ordinary 9551/9560/9665/10058/10067/10282/10292 and the tiered public wrapper at 7401. LLVM's immediate-return constraint leaves no room for root cleanup after the transfer.
- A record pointing into the retiring native frame cannot remain in TLS. Detach it before musttail, and ensure the transfer-to-callee interval cannot pause/collect before the callee has published arguments. If that cannot be guaranteed, use a bounded thread-owned handoff record; never forward an address into the retired frame.
- `create_llvm_jit_host_tail_adapter` (9450 / 8708) owns its own packed buffers, calls the host at 9484/9495, and loads the result at 9502. Its typed arguments and reference results need their own root lifetime and unwind cleanup when host re-entry can collect. Rooting only the outgoing Wasm frame is insufficient because that frame was retired.

The GC-frame ownership predicate must be independent of emit_call_stack_frames. That flag is diagnostic instruction-trace maintenance; unwind mode intentionally omits those push/pop operations. Reusing the flag would omit GC cleanup/rooting in the faster unwind mode.

## Exception payloads and catch continuations

Exception coordinates here use the ordinary header; ROS positions after line 47 are one less.

- `try_emit_runtime_local_func_llvm_jit_throw_tuple` 304 writes payload operands without popping them (first 328, copy 330–345), then invokes the throwing bridge at 357. A snapshot before this invocation can see stack input/prefix, but it must use the declared tag payload types and current locals.
- `try_emit_runtime_local_func_llvm_jit_throw_ref` 371 retains exnref on state.operand_stack and stores it at 394, then calls the bridge at 400. Root the complete exnref and preserve its payload ownership through native propagation.
- `emit_runtime_local_func_llvm_jit_may_throw_call` 190 currently builds cleanup only when lexical handlers or the diagnostic owns_frame predicate require it (205–206). A published GC frame needs cleanup even in unwind mode with no lexical catch. Add the independent GC ownership condition.
- Successful lexical catch continues inside the same LLVM function. Its root frame must remain linked, unlike foreign/unmatched propagation that exits the function.
- `emit_llvm_jit_caught_tuple` 100 loads payload references into a separate local values vector at 144–158. With catch_ref, make_ref at 176 can allocate while these references are absent from state.operand_stack. Pass these values as additional roots for that helper; an active-exception anchor protects the original exception owner but must not be mistaken for the later continuation's complete root map.
- make_ref writes a fresh reference slot, loaded at 180. At 267–274, payload/new-exnref ownership must transfer to the target PHIs before end_catch at 270 drops the native catch owner. If end_catch and the edge-to-target interval are guaranteed noncollecting, publish the selected values on entry to the target block before its first collecting operation. If either can collect, publish them before end_catch.
- Handler lookup and payload copy currently declare no-throw. No-throw does not imply noncollecting; give these cold helpers an explicit collection contract.
- `native_exception_landingpad::emit_typed_entry` foreign edge 127 and `emit_caught_exit_cleanup` 160–162 need exactly one GC-frame exit cleanup, while selected handler continuations must not pop it.
- Unmatched rethrow is an invoke at landingpad 173, through a cleanup that balances end_catch and resumes the new landingpad record. Pop the GC frame there, not before a rethrow helper can still access payload roots.
- GC root cleanup helpers used while unwinding must neither allocate nor throw. Fatal native trap paths also need a documented frame-lease retirement boundary if a trap can be caught/recovered by an embedder.

## Loop snapshots and OSR

Ordinary tiered functions use one internal LLVM core with multiple logical entries: hidden entry id/local-base arguments at 7040–7068, entry switch 7309, normal-init target 7082, and OSR load blocks at 7319. Each OSR branch restores locals into the same local allocas at 7344 and then enters its recorded block/loop at 7346. Thus normal-init-only GC frame publication is incomplete.

Initialize empty frame storage in a common LLVM entry if desired, but publish typed roots after normal local stores and, independently, after each OSR local restore. An OSR parameter null constant is not the live value; the serialized local snapshot is. Keep the interpreter's suspended roots and snapshot pinned until the native core's roots are installed. Root-frame ownership must not copy the diagnostic push/pop borrowing protocol without a separate proof.

`try_record_runtime_local_func_llvm_jit_tiered_reentry` 7894 only records empty operand-stack points (7897). That limits this OSR ABI to locals, but does not prove that roots in the suspended interpreter/host/caller have disappeared.

A separate loop bug risk exists even outside OSR: `try_emit_runtime_local_func_llvm_jit_loop` (8019 / 7485) inserts the existing debug hook at ordinary 8075 before updating the operand model. Only at 8085–8090 does it replace incoming descriptors with current loop PHIs. A collecting hook that scans state at the debug-hook location can publish predecessor/entry values on later backedges. Put a GC snapshot after that update or explicitly pass current loop_param_phis plus the prefix. Do not reuse a debug snapshot as a root map.

Self-tail body entry (7143–7148) is another repeated entry that must refresh changed local roots, while maintaining constant stack usage.

## Performance boundary and required qualification

Do not put root publication into the generic runtime-bridge template at 8666 (ROS 8130) for every call: it also serves normal memory/trap helpers and loses semantic argument types. Instead tag actual collecting, reentrant and parking operations and supply their typed extra roots. No root loads/stores/calls should appear in a reference-free numeric/memory function merely because the build supports Core3.

Reference-bearing suspended callers still need a published snapshot before calling a numeric callee: the callee may transitively allocate or invoke a host callback. Skipping by immediate callee signature alone is unsafe. Truly noncollecting calls can omit snapshot updates only with a maintained transitive-effect/ABI contract.

For memory.atomic.wait, numeric operands are popped at ordinary 12805/12808/12812 before the blocking bridge 12823/12829; relevant roots are remaining prefix plus locals and suspended ancestors. Root publication/enrollment must precede parking. A CPU-bound thread also needs a collector pause protocol at eligible loop/call points; skipping memory-access instrumentation does not prove an arbitrary running guest is paused.

Minimum tests before claiming product collection/root correctness:

1. Actual LLVM verifier + object disassembly for full/lazy/verified/tiered and both diagnostics; all root calls absent from reference-free scalar mmap32/64 loops.
2. Forced collection with refs held only in locals, operand prefix below numeric call arguments, popped reference parameters, dynamic call_ref value, and a nested host callback.
3. Loop-ref PHIs changing on each backedge; self-tail argument permutation and bounded frame count; cross-function musttail with references and no retired-frame addresses.
4. Both normal and OSR paths, including the interpreter-to-native handoff and result buffers.
5. Reference throw payload, catch_ref allocation with previously loaded payload refs, selected same-frame continuation, mismatched outer handler, throw_ref, rethrow, foreign exception cleanup and uncaught propagation.
6. A parked wait thread and an allocating peer, with root-frame visibility and complete admitted-store cohort; ASan/UBSan/LSan and stale token checks.
7. collection_count greater than zero, reclaimed counts and bounded 1024-root ring RSS. Passing the current monotonic VM's semantic fixtures is not collector qualification.

The source-bound 16fcee explicit native collector proof (7 actual collections and six objects reclaimed in the main fixture, separately qualified destructor-gap checks) is useful component evidence, but it does not satisfy these LLVM/VM root or automatic-trigger tests.
