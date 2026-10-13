This is a new **source candidate**, with no native or platform qualification
implied by the files. The sole Linux keeper must perform actual compilation and
execution inside the existing 64GiB cgroup. No local compilation or execution
was performed while preparing the candidate.

`ni [THREAD]` and `nexti [THREAD]` select the current genuine native top-frame
trap. THREAD is an optional participant identifier, not a repeat count. A
cooperative Wasm safe point must first use `step asm THREAD` to establish the
real kernel trap. The current implementation executes only an ordinary native
instruction whose actual LLVM MC descriptor passes the conservative owned-byte
policy. A direct branch also uses the same complete current-owner proof as SI:
its decoded target and, for a conditional branch, its fallthrough must BOTH be
exact instruction boundaries inside that authentic Wasm function. Calls,
returns, indirect or cross-owner branches, traps, system instructions, unmodelled side
effects, prefixes, incomplete decoding and two-decoder length disagreement are
unavailable; rejection retains the actual PC, GPRs and stop identifier. Full
call-skipping `nexti` and caller-based `finish` remain pending actual continuation
and unwind ownership. The console help and diagnostics expose this limitation.

Admission reuses the existing authenticated step operation. It adds no wire
operation or launch permission. The private capture factory authenticates the
actual participant, native trap, code owner, runtime epoch and generation while
copying under one stopped-domain/publication transaction. Both decoders consume
the same bounded owned bytes. Every refusal precedes external unpark and backend
release. No ordinary non-debug JIT instruction, guard or lock is added.

Build `debug_native_next_runtime.cc` as an **own-main host fixture**, using the
keeper's exact fresh full production compiler/runtime/CLI consumer macro and
LLVM/library closure, excluding the product main object. Do not reuse an old
runtime object or ABI-compatible-looking binary: `controller_reply` now appends
`native_next_reason`, and all producer/runtime/consumer TUs must consume one
current source cut. Keep ordinary/ROS and instruction/unwind records separate.
Named-module and header closures also need actual qualification of the newly
exported owned-MC and native-next-policy partitions. The separate
`debug_native_next_command.cc` and `native_next_policy.cc` components exercise
only grammar or real LLVM DATA decoding; neither qualifies runtime execution.

The runtime fixture uses the real owning initializer, debug-full fused compiler,
publication, private capture, controller observer and OS single-step backend.
It first proves cooperative-stop rejection, establishes a genuine native trap,
and tests wrong-participant and script-preparse refusals. Then at least one
ordinary instruction AND a real conditional direct-branch NI must each execute,
with an actual call/return/unproved branch refused. Each successful conditional
NI requires a fresh runtime-owned copy at the actual successor, identical true
owner/generation/epoch, and a separately submitted old-stop NI that leaves ALL
current kernel GPRs/PC/stop unchanged. Ordinary stale-stop proof is counted
separately; neither an ADD nor an unconditional JMP can satisfy conditional
coverage. Rejected host transfers are crossed only via genuine Wasm stepping
with TF clear, then a new authenticated kernel trap. The scan is bounded to
1024 NI attempts, 128 real Wasm steps and 45 seconds inside the cgroup. Missing coverage is FAIL, not SKIP or PASS. Refusal must preserve
every captured GPR and the actual stop. A retained trap is finally retired by
the actual reset observer before guest join and input-dependent result 125.
The WAT deliberately contains a nondefaultable GC reference local and
`return_call`, with explicit feature gates. The first cooperative breakpoint also uses the
same finite genuine Wasm recovery search when its bootstrap instruction is
conservatively unavailable; the first-attempt `step asm` need not succeed.
Branch admission and complete-image boundary-walk headers are both hashed in
the runner's source-drift receipts. An input-dependent negative path
executes `unreachable`; this preserves a semantically real conditional edge
instead of hoping an optimizable arithmetic selection remains a branch; availability producer changes must
come from the same fresh build. Actual code generation may require adjusting
the fixture to reach both witnesses, never inventing bytes or trap phases.

`run_debug_native_next.py` does not compile. It requires a qualified fixture
binary SHA and an independently reviewed nonempty build-provenance record,
checks the existing cgroup, parses and validates WAT with official wasm-tools,
runs both call-stack policies with bounded waits, checks positive/refusal counts,
and records source/binary identities. An input digest is identification, not
build proof. Exit 77 is unavailable and never a platform pass. Windows/macOS
owners must retain their actual kernel qualification gates; QEMU user-mode
components alone cannot prove those kernel paths.

The distinction between an instruction step, call-skipping next and finish
follows the [GDB stepping manual](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Continuing-and-Stepping.html).
The fixture's tail-call and reference-local syntax follows the
[WebAssembly 3.0 instructions](https://webassembly.github.io/spec/core/syntax/instructions.html#control-instructions).
Backend continuation/CFI requirements are documented in
`src/uwvm2/uwvm/debugger/native_step_next_finish.md`; this limited ordinary
implementation does not turn that proposal into completed call/finish support.
