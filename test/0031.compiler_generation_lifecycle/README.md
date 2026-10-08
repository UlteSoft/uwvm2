# Actual compiler generation and selected source lifecycle

Status: held test-only source; no oracle/native/assembly/performance result. This component depends only on the separately reviewed cold scalar API and current production file loader/initializer/runtime. It does not require or install the retained int lazy candidate.

The exact original WAT includes a GC struct type, an unused nondefaultable `(ref $box)` local, memory64 and table64 declarations. The official parser and all-feature validator must accept it. The fixture loads that exact binary into a canonical nonmoving `full_source_instance`, checks actual imports, initializes with active segments deferred and seals the actual initialized registry. It never submits synthetic code/types, ready booleans, epoch values or alternate reset callbacks.

After genuine standalone reset returns, the actual nonwrapping runtime counter must increase while the exact selected source/control block and original code/type metadata remain owned. A second reset must repeat the property. The existing drained native retirement helper then removes all selected source views; the last source owner drops and its real weak pointer must expire. An empty-source reset must still advance the same actual counter. Metadata is observed only while its real source pin remains; no borrowed pointer is read after retirement/final release. This is externally serialized native administration, not concurrent-reset lease proof or executable admission.

Compile the own-main fixture with exactly the current keeper source/macro/SDK layout and link the same newly built runtime.o and host-api/support objects from the coherent scalar-source cut. Do not link product main.o. ROS and ordinary each need an independently matching build. The record must identify source-before/source-after closures and real object/executable binds; merely handing the runner a nonempty JSON file cannot qualify a fresh build. Use the same loader-supported backend build recipe as the existing genuine source-retention fixtures, with int enabled or an actual compatible LLVM-only recipe. The test neither compiles nor executes a guest function, and does not qualify a compiled-cache/lazy/LLVM lowering path, all configuration matrix or performance.

Execute only through the sole SSH Linux keeper in the enforced ancestor memory.max <=64 GiB cgroup:

```
python3 test/0031.compiler_generation_lifecycle/run_compiler_generation_lifecycle.py \
  --repo uwvm2 --source-root FRESH_SOURCE_ROOT \
  --scalar-manifest CURRENT_SCALAR_MANIFEST_JSON \
  --fixture-manifest THIS_IMMUTABLE_MANIFEST_JSON \
  --binary FRESH_OWN_MAIN_BINARY --wasm-tools PINNED_OFFICIAL_WASM_TOOLS \
  --build-record ACTUAL_MATCHING_FULL_BUILD_CLOSURE --output RESULT_DIRECTORY
```

The runner enforces exact after source hashes, actual cgroup ancestry limit and mandatory real reset/ownership coverage. Missing coverage is failure. Build closure qualification remains the keeper's responsibility. Original R1/R2 retained-plan packages remain held and are not applied or qualified by this test.

Normative context: [Core3 module validation](https://webassembly.github.io/spec/core/valid/modules.html#functions) and [local initialization](https://webassembly.github.io/spec/core/appendix/algorithm.html). The original WAT's nondefaultable local is never read; this component observes parser/initializer metadata lifetime, not body execution.
