# Wasm and WASIp1 joint preparation

The host-only `llvm_jit_checkpoint_prepare_instance_host_api(ticket, captures, request)` now accepts `request.include_wasip1 = true`. The genuine manager authenticates the complete current parked cohort, holds the same execution lease, closed host admission, exclusive GC access and publication guard, and copies the Wasm graph and every distinct visible WASIp1 environment at that stop. Sharing a recording label alone never proves this relationship.

```cpp
uwvm2::runtime::lib::llvm_jit_checkpoint_prepare_request request{};
request.recording_label[0] = std::byte{1};
request.include_wasip1 = true;
// Default: require_managed_wasip1_resources == true.
auto result = uwvm2::runtime::lib::llvm_jit_checkpoint_prepare_instance_host_api(
    current_ticket, genuine_current_captures, request);
```

The runtime must first be configured with a finite observation budget, for example `llvm_jit_configure_debug_value_observation_host_api({})`, before any engine is published. The default unlimited observation profile cannot be upgraded to resumable preparation; it returns `engine_preparation_declined` without changing the original world.

Preparation privately clones managed anonymous files, binary contents, cursors and flags; reconstructs shared descriptor aliases, rights, free-list order, reserved cells, argument strings and environment strings; and keeps those candidates alive through Wasm resource reconstruction and actual LLVM engine preparation. Every candidate is destroyed before the proof scope ends, including on resource or engine failure. No live FD table is swapped, no original execution is retired and no new world is published.

The default strict policy refuses retained external resources, including normal stdio and mount-backed directories, with `wasip1_preparation_declined` and `wasip1_status == unsupported_resource`. A host may explicitly set `require_managed_wasip1_resources = false` for a binding-only rehearsal: retained owned external resources stay pinned, while external file contents, kernel offsets and previous I/O effects remain outside rollback. This option does not make an observer resource restorable.

The Wasm resource preparer now recognizes actual factory-issued WASIp1 builtins. It authenticates the original source/module/import ordinal, compares the newly initialized native adapter identity, checks the exact signature and the complete canonical 108-byte interface descriptor, and relocates the function to the new unpublished import record. Chained imports must resolve to that exact terminal object. Matching names, signatures, labels or wire IDs do not authorize arbitrary providers. This binding grants no host execution permission and does not widen ASM debug access.

`prepared_and_discarded` and `wasip1_prepared_together` report a completed rehearsal only. `wasip1_environments` counts distinct environments after whole preparation succeeds. `wasip1_checkpoint_required` also identifies visible WASIp1 state when `include_wasip1` is false. A WASIp1 capture or preparation failure returns no successful pair; a later Wasm failure leaves the original instance and environments intact. Existing Wasm-native and original-source budgets remain unchanged, and WASIp1 uses its separate descriptor/text/managed-file quotas.

This is a prerequisite for complete joint restore, not a complete-instance restore or replay API. Private frame/root planning now follows engine preparation; its copied carriers never become live GC roots or execute a resume entry. Installing the private WASIp1 candidates into a new world, old-world retirement/join, actual startup worker enrollment, live root installation and non-fallible publication of the whole world remain separate requirements. Portable WASIp1 import remains metadata-only and requires destination mount rebinding; it does not contain external file contents. Wasm checkpoint capture still reminds callers to checkpoint WASIp1 at the same cooperative stop.

The extended `debug_wasip1_checkpoint_runtime.cc` uses genuine native guest pauses and checks default strict refusal, explicit binding-only preparation, managed-only success, capsule-registry exhaustion, early Wasm resource failure, late frame/root-budget failure, repeated candidate disposal, forged capture refusal, and continued guest WASI reads. The builtin-alias fixture adds globals, an active table and a passive element segment sharing WASIp1 funcrefs. Execution qualification is recorded separately in the joint-preparation test report.

The R27 native execution matrix, exact frozen source scope and remaining restore requirements are in [the joint-preparation report](../0018.debugger/wasip1_joint_preparation_r27_test_report.md).
