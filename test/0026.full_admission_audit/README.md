# Full graph admission before observable instantiation

Status: **source-only candidate and regression corpus; no official oracle, native build, or runtime result yet.** Neither patch has been applied. Pure wasm1p1/wasm2 validator files are excluded.

Core 3 [instantiation](https://webassembly.github.io/spec/core/exec/modules.html#exec-instantiation) checks validity before allocating the instance, initializing active segments, or executing its start. [Module validation](https://webassembly.github.io/spec/core/valid/modules.html#modules) includes every function, even an uncalled function. The non-defaultable local counterexample follows [local initialization](https://webassembly.github.io/spec/core/appendix/algorithm.html#locals); the packed-field counterexample follows [GC instruction validation](https://webassembly.github.io/spec/core/valid/instructions.html#structure-instructions). These are distinct from valid modules that trap during segment initialization, whose prior effects can be observable.

The existing driver initializes runtime storage with active segments deferred. `run_initialized_module_graph` currently applies active segments before the full entry calls `compile_all_modules_if_needed`. The original source therefore permits an invalid uncalled body to be discovered after its own imported memory/table have already changed. These source predictions remain unconfirmed until the genuine runtime harness executes.

## Candidate contracts

`uwvm2-full-prepare.patch` and `uwvm2-ros-full-prepare.patch` add a cold `full_compile_prepare_host_api`, its public declaration/stub, and a full-graph call immediately after CLI GC policy preparation and before the first active segment. The API checks the actual full mode, supported selected compiler, real registry and existing execution/publication/provider callback depths. It retains a genuine execution generation lease and FP scope. It compiles through the existing fused full compiler, never executes guest code or fabricates a participant/frame. A subsequent compatible run consumes the same `compiled_all` and configuration signature; it does not perform another raw-bytecode traversal. Native EH private-leaf management retains its existing exclusion. The new fatal driver message uses the existing fast_io color formatting.

Preparation changes the timing of LLVM call_indirect table-view creation. Therefore the full-prepare patch **must not ship without** the companion `*-active-element-view.patch`. The companion gives actual runtime storage one optional nullable refresh hook, including LLVM-only builds. Standalone parser/initializer/translator products acquire no hard runtime link dependency. Ordinary uwvm2's interpreter compatibility hook aliases that same storage slot. Actual publication installs it; drained reset detaches it before destroying views. Each successful active element write calls the existing exact-range updater with the resolved table identity, `init`, offset and count, including the materialized-funcref early-continue path. The updater preserves all import aliases and existing atomic writes. It does not clear, resize or repopulate every view. Debug full retains the existing bridge's early return because it resolves the live table.

`candidate_manifest.json` and `targeted_view_candidate_manifest.json` identify the exact original bytes used to derive each unapplied patch. They are compatibility evidence, not test results. `uwvm_runtime.h` declarations are exported by the existing exported namespace when `uwvm_runtime.cppm` includes that header; no second named-module declaration is introduced.

## Genuine runtime probe

`full_graph_effects.cc` uses original official-tool-produced bytes, real parser/loader/import linking, `initialize_runtime(true)`, and the actual production `run_initialized_module_graph`. It performs no preliminary body validation and edits no type/feature metadata. Each run is a fresh process. File loading and diagnostics use fast_io. Actual owners remain alive until the reset guard retires runtime borrows.

Build against the real matching full-runtime SDK/objects with its normal configuration and include paths. Initial Linux component qualification can select interpreter-only (`UWVM_USE_UWVM_INT`, `UWVM_DISABLE_JIT`); LLVM-only and combined/tiered SDK runs follow. After applying a reviewed combined candidate in the isolated Linux source cut, define `UWVM2TEST_FULL_PREPARE_API` to expose the optional direct prepare-twice probe. This macro does not alter production behavior.

Arguments:

```
full_graph_effects int|llvm|tiered consumer.wasm provider.wasm [prepare-twice]
```

`provider.wat` has a real exported memory/table and no start. Both invalid consumers contain an uncalled invalid Core 3 body, active data `a5 5a`, an active element pointing to real local function 1, and a valid empty `_start` at index 2. With original source, the expected before witness is `actual-graph-after-segments-before-entry` showing imported memory bytes `165,90` and table replacement `1`, followed by the real full compiler's fatal body diagnostic. With corrected admission, only `initialized-deferred` may print before that diagnostic; the graph must not enter the callback after applying segments.

Valid GC and try_table consumers actually call the Core 3 probe and the function installed by their active element using ordinary `call_indirect`. After the real guest returns they require memory bytes `165,90`, the replacement function pointer in the provider table, and the actual start marker `158`. A prepare-twice run must additionally observe zero segment/start effects after each successful preparation and then execute the same graph successfully. Source cache inspection is not a substitute for real performance/decode-count qualification.

`provider_started.wat` extends the real provider with its own active element/data, a real start, and an indirect call from that start. It installs seed function 0 before its start; the consumer later replaces the same table with consumer function 1. This exercises the actual resolved-table aliases in both callers, exact writes between preload/main execution, and reuse of the full publication. Expected provider data/start markers are `195` and `113`. It does not claim to exercise a retained guest OS thread; the existing targeted updater's concurrency qualification must be retained separately.

## Official oracle and keeper execution plan

Use the keeper's pinned current official `wasm-tools parse` to encode each original WAT. Do not use `validate` during encoding of the invalid cases. Then run official `wasm-tools validate` on those exact bytes: both provider files and valid consumers must pass, while the invalid consumers must fail for uninitialized non-defaultable local or non-packed struct.get_s. Run each consumer with both provider variants. A nonzero process caused by unrelated parser/native/LLVM/formatter failure is not body-rejection evidence; retain the precise diagnostic and phases. Queue all native runs on SSH Linux inside the shared 64 GiB cgroup. No local native test is authorized by this corpus.

The related original official local-init fixture is pinned to [WebAssembly/spec 2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/local_init.wast). Our two-module active-effect witnesses are new probes, not unchanged upstream tests.
