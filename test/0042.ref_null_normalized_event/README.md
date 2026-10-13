# Ref.null normalized event development probes

These original inputs and command templates are unexecuted. They cover Core3
abstract GC bottoms, explicitly indexed function/struct heaps, exception heaps
without GC, signed-33 index130, legacy ABI carriers, dead-region admission,
adjacent null throw and two invalid typing boundaries. They accompany the
PRIVATE R3 source candidate; no native/oracle/ASM/performance PASS is asserted.

First oracle parse/validate and run only the first three cases on matched intfull
and jitfull products. Then extend to the remaining cases and ordinary lazy modes.
The legal dynamic `_start` probes trap unless every null check equals1. The dead
case only proves dead typed/emitter admission and is explicitly distinguished.
The unused nondefaultable local is invalid even when the body is never called.
It is function31 of32, after30 valid fillers: explicit zero workers and
`--runtime-scheduling-policy func_count 1` avoid interpreter CU batching and
LLVM adjacency warmup of16 functions hiding the missing allbody admission.
Plainlazy acceptance must be reported as a separate unresolved admission bug.
The adjacent null throw must have the existing readable null-reference diagnostic;
SIGSEGV, timeout and unrelated compiler errors are never expected outcomes.

The official Core3 [ref.null typing](https://webassembly.github.io/spec/core/valid/instructions.html#reference-instructions)
assigns the exact nullable heap, and its [binary encoding](https://webassembly.github.io/spec/core/binary/instructions.html#reference-instructions)
uses a heap immediate. [Heap types](https://webassembly.github.io/spec/core/syntax/types.html#heap-types)
distinguish GC, external, function and exception hierarchies and their bottoms.
The [local.get rules](https://webassembly.github.io/spec/core/valid/instructions.html#variable-instructions)
require an initialized local; stack polymorphism does not permit an incorrect
concrete value type. The [official ref_null corpus](https://github.com/WebAssembly/spec/blob/main/test/core/ref_null.wast)
is a reference, not copied fixture content or a pinned executed oracle.

For the130 input verify the actual emitted opcode/immediate offset and bytes;
the explicit WAT name alone does not prove the binary index was preserved.
Only official validation establishes legal/illegal syntax for these new inputs.
Record pinned official tool/version and immutable product/source artifacts.
No native build or executable is permitted on the local Mac; the sole Linux
keeper owns the managed <=64GiB cgroup and all native execution. R12 BEFORE and
new R13 AFTER must each use coherent freshly matched headers/main/runtime/host.
Plan modes and CLI spelling come from the actual source, not execution evidence.
