This new **source-only actual-runtime fixture** has no native compilation,
execution or platform qualification yet. It adds only independent test files;
production command/controller/runtime code remains at the stable NI source cut.
A passing synthetic observer or opcode component cannot qualify these tests.

`debug_wasm_events_runtime.wat` combines Core3 GC `struct.new/struct.get`,
i64-addressed memory64 and table64, SIMD memory/lane instructions, a genuine
cross-function `throw` handled by `try_table`, and a real `return_call`. The host
explicitly enables each corresponding feature, including function references for
`ref.as_non_null`, before the actual owning CLI
initializer. Actual official wasm-tools parse and validation are mandatory;
syntax or lowering failures are FAIL, not silent feature omission.

The own-main host fixture uses the actual full fused compiler/publication,
host-only console authority and controller observer, plus the same production
memory reader. It first rejects an execution-containing script before earlier
trace policy children can run. A valid four-child script independently admits
trace and GC/throw catch policies through the existing authenticated stream.
Actual GC and throw safe points must park the real guest. The event must match
the actual participant/function/byte offset/runtime epoch/generation, and the
formatter must clearly describe **before-instruction** observations. The first
GC stop sees initial zero memory; the later callee throw stop sees the preceding
memory64 store of 31. These checks do not claim the initial struct allocation,
throw, catch or collection has already committed at its pre-instruction stop.

After resuming, the actual exception payload 42 must be caught, the live GC
object must still contain 31 across the throwing call, and the tail call must
return 49. Actual memory64 load, table64 non-nullness and SIMD lane values are
also checked by Wasm traps on mismatch. Three-record trace pages then require contiguous real sequences,
complete generation labels, at least two pages and actual opcode witnesses for
GC construction/read, memory64 load/store, table64 get, SIMD, try_table, throw
and return_call. Missing coverage is FAIL. Trace clearing is exercised; after
actual reset the old instance cannot authorize a memory read. No captured
reference becomes a heap handle, no raw host address is accepted and no debugger
or VM launch authority is created by guest data.

Build the host fixture as **own main** with the keeper's exact current fresh
full production compiler/runtime/CLI consumer macros, LLVM/provider dependencies
and ABI closure, excluding the product main object. Use root's qualified new
complete source cut; an old runtime object or equal binary filename does not
prove the current controller reply or availability/observer ABI. Ordinary and
ROS products plus instruction/unwind stack policies require separate actual
records. A test-only addition does not alter normal non-debug generated code.

`run_debug_wasm_events.py` builds nothing. It requires an identified binary SHA
and independently reviewed build record, checks the existing 64GiB Linux cgroup,
parses/validates with official wasm-tools, enforces bounded real runtime waits,
runs both policies and records actual source/binary/log witnesses. Its source
hash subset and binary digest are identity checks; the external complete build
provenance is still required. Exit 77 stays unavailable. Other OS/architecture
owners need their own fresh actual platform qualification, not QEMU component
success reported as native kernel/runtime success.

The opcode and type requirements follow the official
[WebAssembly 3.0 instructions](https://webassembly.github.io/spec/core/syntax/instructions.html)
and [Core3 types](https://webassembly.github.io/spec/core/syntax/types.html).
This fixture tests only the specified exercised instructions and debugger
policies; it does not claim complete GC heap inspection, managed thread debug,
all-language source support, full native next/finish or executable checkpoint
restore.
