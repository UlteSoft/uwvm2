# Numeric exception execution in uwvm-int

The r211 implementation connects numeric Core 3 exceptions to the actual interpreter compiler and runtime in both products. Ordinary uwvm2 full/lazy/lazy-with-verification and ROS full pass the 30-case new-syntax CLI suite in O1 and O3 builds. This is a bounded numeric EH result: reference payloads, `catch_ref`, `catch_all_ref`, `throw_ref`, full GC, native LLVM EH and mixed-tier EH are not qualified by these results.

The implementation follows the [Core 3 execution rules](https://webassembly.github.io/spec/core/exec/instructions.html) and [validation rules](https://webassembly.github.io/spec/core/valid/instructions.html). Guest exceptions propagate independently of traps; catches preserve lexical order and branch to outer labels, including loop parameters. Tail calls retire the outgoing activation's handlers. Old wasm1p1/wasm2 pure validator directories remain unchanged.

## Escaping throw and ownership

`translate/opcode/branch_cases.h` retains the existing same-function non-reference throw-to-branch optimization. A reachable escaping throw spills delayed producers and cached values before emitting the real throwing opcode. An unreachable throw still validates its complete signature.

`optable/exception_throw.h` owns an immutable descriptor containing the tag-instance root, copied numeric kinds and checked tuple byte count. Its cold opcode copies i32/i64/f32/f64/v128 bits into an immutable `runtime::exception::value`; no frame or parser pointer survives propagation. Raw copying preserves signaling NaNs and vector bits. Every real throw creates a distinct C++ activation; values and original diagnostic traces can be shared without sharing unwinder headers.

Tags compare actual instance identity. Equal function signatures or equal module-local tag indices are insufficient. Linked import aliases retain the provider's same identity token. Reference values require a rooted backend codec and must never be approximated by copying pointer-width bytes.

## Protected calls and exception continuations

Only calls with active handlers select the protected direct/indirect opfunc. Those calls disable fusion and register fast paths that would otherwise consume a following instruction or retain stale caller values. Calls without handlers keep their existing emission. `return_call` and `return_call_indirect` do not install the retired caller's catches.

`translate/single_func_exception.h` flattens clauses from inner to outer frames, preserving order and duplicates, stopping after a catch-all. Metadata contains stable label indices while compilation is mutable. It records the caller's complete spilled byte count, including an indirect selector, and the target's byte prefix and payload layout. It accounts for exception-only tuple high-water marks even when the callee itself has zero parameters/results.

`optable/exception_metadata.h` uses the original pre-call operand top to recover the caller base. It validates the complete destination tuple before writing any byte, copies the selected payload and returns a cold continuation. Foreign C++ exceptions and Wasm traps cannot match a guest catch-all. Dispatch does not allocate and does not increment metadata ownership on normal calls.

The cold thunk reconstructs the target's actual register-ring layout from memory and branches to its fixed label. Existing register arguments at the exceptional entry are stale and are never reused as payload. Loop targets use the already-fixed loop-header parameter layout. Ordinary forward joins preserve their established layout.

An EH-only forward end required a specific correction: its label and saved entry state are empty-cache, while following numeric instructions require canonical cached operands. The compiler captures the empty state **before** emitting `stacktop_fill_to_canonical` at that label. The cold thunk jumps to this fill; following instructions are compiled against the resulting canonical cache. Choosing an empty snapshot without that fill failed the real ring1 test. No extra fill is inserted into an ordinary join solely because EH is enabled.

Only after all thunks and bytecode fixups are complete does the compiler construct `exception_function_metadata`, resolve target offsets and patch stable site addresses. The owning metadata moves with the compiled local function, including lazy publication. No compiler lambda or vector-element pointer is published into executable bytecode.

## Runtime propagation and diagnostics

Actual interpreter execution helpers use `UWVM_THROWS`; compiler-only and trap callbacks keep their existing contracts. Scratch marks, tail argument storage and logical frames have scoped cleanup. The public entry remains noexcept and catches the exact `guest_exception` type to print the distinct colored `uwvm: [fatal] Uncaught WebAssembly exception` report through `fast_io::io::print`.

The report includes entry identity, typed payloads with raw floating/vector bits and the original throw-site stack. The cold throw callback snapshots frames before unwinding and copies display names into immutable ownership. Guest-controlled names are escaped, and one output lock covers the whole report. Source/opcode locations are explicitly described as unavailable. Rethrowing an immutable value retains the original snapshot.

The in-progress native integration reuses the same value and report. Diagnostic native unwind must replace generated instruction push/pop, and must capture native frames before propagation; a deliberately absent logical JIT stack is not a fallback. Its bounded native capture also marks truncated traces. These changes require their own native CLI and code-generation qualification.

## Qualification and limits

All compilation and execution used the SSH Linux container, its 64-GiB memory cgroup and allowed `0,2,4,6,16-31` CPUs. Tests did not run on the development Mac.

- `test/0013.uwvm_int/wasm3/exception_cross.cc`: actual parser/initer/compiler execution, 360 cases per product across byref/ring1/ring2, four combine levels and both delay settings; O3 and ASan/UBSan passed. The later r212 source merely separates a malformed include-line trailing token from its intended `#undef`; r211 Clang logged a warning and executed the tests successfully.
- `test/0014.llvm_jit/run_exception_cross_cli.py`: 30 new Core 3 cases cover direct/indirect propagation, aliases, ordered mismatch and duplicate tags, EH-only large/mixed tuples and loops, tail bypass, repeated scratch cleanup, uncatchable traps and exact uncaught stacks. Wasmtime independently validates and executes the fixtures.
- `bench_exception_cross_cli.py`: four comparisons with nine paired samples, followed by a separate CPU0-affinity repeat. The same unused-EH Wasm binary is compared with the feature disabled/enabled; protected-normal, static local throw and real depth1/depth8 propagation are separate workloads.

The fixed-core unused-feature and protected-normal medians were near baseline in both products. Depth1/depth8 totals were approximately 8.5–9.0/38–39 microseconds per throw. These include value allocation, the default diagnostic frame/name snapshot, propagation and cleanup, plus separately recorded process startup/compilation contributions. They are not a measurement of unwinding alone and are not overall performance acceptance.

The report archive `core3-interpreter-eh-r211-reports.tar.gz` contains 1,660 individually verified files, SHA256 `5887a1e5c86f5603e23f4e68a134aa3f1c6e878616f24114e97393f6515df411`. Compiler unit artifacts remain in the corresponding r211 build directories. See the main implementation status for exact frozen source IDs and broader unfinished work.
