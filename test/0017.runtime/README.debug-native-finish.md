# Native Wasm finish

The controller implements "finish asm [THREAD]" and "fin asm [THREAD]" from a current authenticated native top-frame stop. It obtains a sealed return plan from the actual kernel trap, private registered CFI and current Wasm owners. The normal parent return gets a fresh native-only capture and stop identifier. The original cooperative pause anchor stays unchanged. Child locals, source frames, stack bytes and frame pointers cannot become parent inspection data.

"step asm THREAD" establishes the native stop. "bt THREAD" shows authenticated Wasm parents. "finish asm THREAD" returns to the nearest proved Wasm parent; omitting THREAD selects only the controller's actual current native participant. The Wasm root and unknown/host callers are refused before execution. No caller address, frame ordinal or native memory input is accepted.

The DAP adapter maps stepOut at native statement/instruction granularity to the same command. Old frame, register and instruction references expire before the broker request. Wasm and source step-out policies keep their existing routing.

Cancellation, timeout and unavailable normal-return evidence drain actual descriptors and providers through the worker release ACK and preserve the cooperative Wasm pause request. All successful return events remain strongly owned until that drain. Tail and exception paths are not reported as completed native returns.

The executable return backends require the Linux native continuation product
and the genuine LLVM owner-table configuration. Scoped console/controller
qualification exists for x86-64 LP64, little-endian RV64/AArch64/LoongArch64
LP64 and i386.
See [RV64](native_finish_riscv64.qualification.json),
[AArch64](native_finish_aarch64.qualification.json) and
[i386](native_finish_i386.qualification.json) and
[LoongArch64](native_finish_loongarch64.qualification.json) for their independent frozen-source,
SDK, QEMU and call-stack-policy scope. Other targets refuse physical native
finish. Windows, macOS, FreeBSD, alternate libc/signal ABIs and native DAP
stepOut on the non-x86-64 targets require separate qualification.

debug_native_finish_command.cc checks bounded grammar and script refusal. debug_native_finish_console.cc with debug_native_finish_console.wat exercises genuine recursive returns, current parent inspection, stale-stop refusal, root refusal, pre-wake interruption and cancellation of a nonreturning callee. The cancellation fixture verifies real Wasm execution and exits by mutating only a Wasm global.

2026-10-06 R35 acceptance after reboot: both full-interpreter plus full-LLVM runtime products compiled and linked against the freshly rebuilt vendored 23.1.1-uwvm-ros.11 SDK. Both grammar components and all 239 DAP unit cases per repository passed. Twelve genuine console scenarios passed across both repositories and instruction/unwind call-stack policies: recursive returns, nonreturning cancellation, and a real retained NI call before return cancellation. They prove 12 actual parent returns, four root refusals, eight post-wake cancellations and four mixed call/return event drains. Pre-wake interruption preserves the real native trap.

The fixture now retains the original immutable Wasm image, selects the production value-observation profile before compilation, and waits for the actual native wake before sending its cancellation. A genuine typed Wasm call in the loop supplies the mixed-event witness. Every retained call, return and bootstrap event is disabled before the one shared worker retirement ACK; descriptor/plan ownership survives until actual backend release and clear.

All tests ran in the restored 64 GiB Linux cgroup with swap disabled. Source/SDK/tool inputs and eight compiler dependency closures were SHA-256 verified; the final native and verification task owners actually retired. The complete current receipt is kept in asm-dbg-r35/primary-evidence-r35.tar.gz on the Linux persistent recovery directory. This qualifies the console/controller route on Linux x86_64 LP64; a positive real DAP stepOut matrix and other-platform native return backends remain unqualified.


2026-10-06 R36/R37 follow-up: genuine broker/DAP native stepOut is now qualified
on the same Linux x86_64 product. R37 corrected deadline cleanup in both native
finish and instruction/next paths: event/TLS release and a real Wasm stop share
a new bounded drain deadline after the execution deadline expires. Only a fully
parked capture publishes a fresh stop ID. DAP forwards the actual timeout
diagnostic and observes complete console status/wait replies immediately, so a
real guest-exit reply emits exited before terminated. Current native register
watches are readonly; expired-frame watch denials do not mean current watches
are unsupported. SP/FP remain unavailable and no native memory capability is
introduced.

Both full products passed twelve console cases, eight actual DAP cases and
246 DAP unit tests per repository (one skipped each). Four real nonreturning
stepOut deadlines retained actual cooperative pauses and all four guests
naturally exited with code zero after a Wasm-only global mutation. The R37
main/console TUs were rebuilt against the immutable R35 cut with the controller
repair; unchanged qualified runtime/host-api objects and the one SDK were reused.
Compiler dependency closures prove those reused TUs do not include the changed
controller. Concurrent Python edits were captured and tested separately; later
parallel C++ edits and other native backends remain outside this source cut.
See ../0018.debugger/README.native-finish-dap.md for the actual DAP fixtures.


R37 qualification binds its immutable controller and Python snapshots. Later
parallel C++ or Python edits, including the subsequent WASIp1 acknowledgement
parser update, are preserved and are outside this acceptance. The native timeout
dispatch and complete-status observer repaired here retain their tested code.
