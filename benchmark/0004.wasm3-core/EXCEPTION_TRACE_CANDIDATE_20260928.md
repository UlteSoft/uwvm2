# Compact exception diagnostics candidate

The escaping-throw benchmark currently pays for a physical diagnostic walk
before native C++ propagation. It also allocates a second vector of named
frames and separate object/control-block allocations for the exception value
and trace. The measured 4.4–4.7 second historical runs do not qualify this
candidate or establish its speedup.

The reviewed candidate preserves `diagnostic_trace::frames()` and immutable
exception values. A runtime-owned symbol snapshot copies only actual optional
name records before execution admission. Each throw retains compact module and
function indices together with an independent strong symbol owner. Names are
copied once on first display, using `call_once` to publish the complete cache to
concurrent readers. `frame_count()` reads only immutable indices. A retained
exception can outlive module/parser teardown, and `throw_ref` keeps its original
trace. The exception value and trace factories use `make_shared` so each owner
and control block require one allocation. Reference payload roots still move
with their exact fields.

This candidate still performs the real diagnostic walk and native C++ unwind.
It does not skip destructors, replace `_Unwind_Resume`, add instruction frames
to ordinary calls, or use `longjmp` across host activations. LLVM documents
different native exception models and mandatory cleanup behavior. Simply
recording frames during a personality search is insufficient: search stops at
the first native handler, while our typed handler still has to discriminate
the actual Wasm tag. It would not by itself preserve the complete original
trace of a subsequently retained `exnref`.

[LLVM exception handling](https://llvm.org/docs/ExceptionHandling.html),
[Itanium C++ exception ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html),
[Core 3 exception instances](https://webassembly.github.io/spec/core/exec/runtime.html#exception-instances)

The native component runner uses an explicit private copy of the entire
exception-header folder and verifies its actual dependency files. It links no
old VM runtime object. The new fixture checks real allocator calls, concurrent
name publication, independent symbol lifetime, invalid identities, native
throw/rethrow, and the original span interface. Existing value/root ownership
and interpreter throw-capture tests are also run. All commands require the
established remote Linux 64 GiB, swap-free cgroup and E16 CPU; each command has a
2 GiB resident-memory budget and owned process identities/pidfds.

Status: candidate only; native C++ compilation, sanitizers and whole-VM
performance are pending. The original source preimages and candidate byte
manifest are preserved in the development directory
`/tmp/uwvm-exception-compact-trace-candidate-20260928`. Integration requires
unchanged preimages, a new complete product build, real Wasm catch/catch_ref/
throw_ref and uncaught diagnostics tests, and quiet paired P-core timings.
Tests must cover frequent caught throws, no-throw loops, nested/native cleanup,
concurrent retained exceptions and source/Wasm/native debugger stops.
