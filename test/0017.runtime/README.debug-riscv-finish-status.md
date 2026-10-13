RISC-V native finish status
==========================

Linux RV64 physical `finish asm` now returns to an authenticated Wasm parent.
It uses the selected worker's real kernel trap, registered relocated RV64 CFI,
bounded copies from its registered stack, exact published Wasm code owners and
dynamic activation identities. The software return event seals PC, restored
parent SP, epoch, function generation and trap revision privately. These are
not user-supplied addresses or a guessed logical backtrace.

If recursive calls reach the same return PC at another SP, the private backend
executes the original owned instruction through an authenticated successor
trap and re-arms the return event. It publishes the parent only at the sealed
SP. Running cancellation drains the return event and worker ACK, then exposes
a genuine Wasm pause. A root or unproved caller is refused without native
movement. VM/host code is never a public ASM stop or disassembly capability.

Build and console use
---------------------

Both repositories enable the continuation product bridge for `riscv64` with
`linux-native-debug`; `linux-x64-native-debug` remains x86-64 only. The tested
target-host SDK is LLVM 23.1.1-uwvm-ros.11 with the exact
[split-probe CFA backport](README.debug-riscv-probe-cfi.md), not an unmodified
archive with the same version string. External LLVM builds also need the
qualified TailCC capability `UWVM_LLVM_RISCV_TAILCC_FIXED=1`. The tests retain
full interpreter plus full LLVM; they add no ROS basic, lazy or tiered mode.

For an ordinary full LLVM build:

    uwvm -Rdbg -Rcc jit -Rcm full -Rct 0 -Rllvm-cache-path disable \
      -Rllvm-call-stack instruction --run PROGRAM.wasm

ROS selects full mode with `-Raot`. `unwind` is the other tested call-stack
policy. Inside the debugger, establish an actual native stop first:

    step asm THREAD
    bt THREAD
    finish asm THREAD
    fin asm

`fin asm` uses the selected participant. Cooperative Wasm stops alone cannot
authorize native finish. An unavailable instruction or caller retains the stop;
the user can explicitly use `step wasm THREAD` to seek another Wasm stop.
There is no implicit Wasm-search fallback inside native finish.

Executable qualification on 2026-10-07
--------------------------------------

`recovery111-rv-finish` passed 22 commands on immutable source111, including
fresh full-runtime/host/fixture builds, official Wasm parse/validate and twelve
actual QEMU RV64 VM sessions across both repositories and both stack policies.
Normal tests returned through three genuine Wasm parents; recursive-running
tests returned through two and observed actual wrong-SP/successor traps.
Cancellation tests woke the real loop, observed its changed Wasm counter,
requested a real pause, changed only the Wasm exit global and returned 77.
Returning cases preserved result 14. All completed wait, join, reset and detach.

`recovery116-rv-call-finish` passed four additional actual sessions requiring
a genuine opaque-call NI continuation before running finish cancellation.
No text match, constructed controller reply or refusal substitutes for those
executed calls. Stale stop identifiers and the actual Wasm-root finish are
refused; public native stack bytes remain absent.

`debug_native_finish_console_stack_arguments.wat` adds 64 real Wasm parameters
to the recursive function while preserving its indices and result. It targets
physical caller SP reconstruction after a genuine TailCC stack-argument pop.
`debug_native_finish_console_deep_stack_arguments.wat` raises recursion to 70
while preserving result 14. Its return event must tolerate more than 64 genuine
descendant returns at the same PC before completing at the exact parent SP.
The separate registered-CFI O0/O3 test observes actual emitted metadata only;
it does not by itself qualify a live caller or a return event.

The completed source111 qualification comprises 91 selected successful build,
validation and regression commands, plus the successful final closure audit.
It includes 40 real finish-fixture sessions and 18 real CLI sessions, with
72 authenticated physical return hops. Both 64-parameter variants pass all
eight repository/policy/scenario cases each. The 70-level recursive-running
cases each observe 69 wrong-SP hits and 69 genuine successor traps. The four
return-display tests check actual architecture SP/FP values and known-bit masks,
not only x86 register names. No Wasm-root case enters a host caller.

The fresh full CLI regressions cover 10000-local managed quit, optimized mixed
numeric/SIMD bit patterns, indirect tail-call and return_call_ref frame/operand
previews, and the debug-policy 10000-local workspace. Every CLI close requires
real exit code zero; forced kill and timeout fail. The full proof is
`QUALIFIED127.json` in the owned Linux recovery directory, SHA-256
`619411dbd8623b31aa25d4b4bb03d63f98238d6b87d3f5a62a5fba79054f33c6`.
It rechecks 30154 pinned inputs, 5988 actual dependency inputs and 1648 RV64 ELF
SDK members. See [the compact qualification record](native_finish_riscv64.qualification.json).
The selected owned RSS peak is 7077494784 bytes; the recovery-folder high-water
mark is 16153972736 bytes, below its 16 GiB budget. Memory max/OOM counters remain
zero after the reboot; the final closure audit retired and reaped its own process.

The scoped RV64 QEMU stack-read fallback uses FastIO `/proc/self/mem` only when
`process_vm_readv` returns ENOSYS, and only for already authenticated eight-byte
CFI slots. It rechecks worker lifetime and registered bounds. Other read errors
remain failures. Qualification uses the recorded QEMU prefix/sysroot; alternate
guest-base mappings are not qualified. These cold debugger paths add no emitted
instruction to a Wasm function when debug observation is disabled.

This is scoped RV64 native-finish coverage, not all-ISA, all-Core-3/thread,
language-type or complete instance/external-I/O rollback qualification. Later
concurrent working-tree changes are recorded separately from the tested cut.

Earlier unimplemented/refusal history
-------------------------------------

On the earlier recovery87/recovery89 source cuts, physical `finish asm` was
not implemented on Linux RISC-V. The controller's
native-finish path, physical-caller capture, and return-event continuation are
guarded for Linux x86_64 on those cuts. Their RISC-V software stepping did
not supply the equivalent sealed return event. Correcting LLVM's split-probe
CFI fixes native exception unwinding; it does not implement native finish.

The normal functional scenario in debug_native_finish_console.cc still
requires genuine movement to the physical Wasm caller. Its first RISC-V run
failed in recovery87. That failure is preserved and excluded from successful
physical-finish coverage.

The separate `unsupported-finish` scenario is a refusal test. Run the compiled
fixture as:

    FINISH_FIXTURE recursive.wasm instruction unsupported-finish
    FINISH_FIXTURE recursive.wasm unwind unsupported-finish

Both inputs and the fixture must use the actual target-host full LLVM SDK;
execute the fixture, QEMU, and Wasm parse/validate in the verified test cgroup.
The four paired-repository/policy runs in recovery89 reached a genuine native
instruction stop, then required both interrupt states to reject finish with
`unsupported_command` / `caller_unwind_unavailable`. The stop identifier,
native PC, and Wasm function identity remained unchanged. No native movement
was reported. Continuing the same guest returned 14; wait, physical join,
runtime reset, and controller retirement completed.

Those four passes qualify safe refusal only: `real-native-finish=0` and
`finish-refusal-qualified=1`. They do not qualify physical caller inspection,
successful return stepping, or a functional Wasm-root finish test. The old
tested transcript's `actual-bt` label counts console inspections; the current
source calls it `actual-native-console` to avoid implying a native backtrace.
That label-only edit does not change the tested behavior.

The implementation required proving a physical Wasm caller from registered CFI,
then install an authenticated software return event with cancellation and
owner-retirement handling. It must reject host callers and must not expose a
VM implementation stop. Merely widening the x86_64 preprocessor guards cannot
provide these guarantees.

Exact evidence is retained in the owned Linux recovery directory:

    recovery87-rv-asm-boundary-status.json
    recovery89-rv-asm-finish-refusal-status.json
    RECOVERY-RESULT90.json

See [the independent CFI fix](README.debug-riscv-probe-cfi.md) for the actual
large-frame managed-quit regression and its successful full-VM tests.
