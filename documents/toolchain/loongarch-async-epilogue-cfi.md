# LoongArch asynchronous epilogue CFI

The historical LoongArch qualification below uses the platform C calling
convention. This CFI change alone does not enable TailCC; the separately qualified
[LoongArch64 TailCC repair](llvm-loongarch-tail-abi.md) now supplies that capability. The LLVM 23.1.1 backend emits the prologue
CFI but leaves an FP-based row in force after the epilogue restores FP. A native
stop at the return instruction therefore cannot use that row to prove its CFA.
The strict Wasm-only debugger refuses such rows rather than using an ABI guess.

The paired downstream patch changes only asynchronous unwind tables. Before the
first saved-register load it switches CFA to SP plus the remaining frame size.
Each actual saved-register load is followed by an explicit same-value rule; the
final SP adjustment is followed by CFA=SP. Synchronous call-site tables retain
their existing behavior. Private CFI consumes genuine relocated rows; public
native stack bytes, SP/FP, VM pointers and host frames remain inaccessible.

ROS retains this patch in its bundled LLVM source inventory. An external LLVM
provider for uwvm2 must carry the same backend repair to supply these return
rows. The compiler source hash and SDK libraries must be recorded together;
matching a version string alone does not qualify a provider. Debug object-cache
behavior and live Wasm execution require their own validation.

The LoongArch registered-CFI fixture executes actual generated 64-argument C ABI
functions at O0/O3 and requires a real SP-based zero-offset return row. The
[paired LoongArch64 full LLVM caller/finish record](../../test/0017.runtime/native_finish_loongarch64.qualification.json)
now verifies 52 actual Wasm sessions under both policies; see the
[usage and boundary notes](../../test/0017.runtime/README.debug-loongarch64-finish-status.md).
Other targets and actual hardware retain separate qualification requirements.

The CIE also explicitly records initial same-value for RA/r1 and callee-saved
r22..r31. Generated spill and restore rows replace these ABI entry rules at
the corresponding native PCs. Missing rules remain unknown in the runtime;
no rule is inferred there, and volatile/TP/FP-vector state is not promoted.

MCJIT defaults CFI fixup off. The opt-in Linux LoongArch native-continuation
build enables it before selecting the real TargetMachine, including replacement
engines. LLVM then restores the framed CFI state when a live block follows an
epilogue in object layout. The registered-CFI fixture includes such a block,
checks its saved RA and FP-based CFA, and executes both branches. Its explicit
legacy-layout case keeps fixup disabled to demonstrate the malformed row. The
private live caller also refuses an identical-PC/SP physical-frame cycle.

The owned-instruction decoder admits only the compiler's bounded DBAR ordering
hints (0, 0x10, 0x12, 0x14, 0x700), including the acquire fence before a genuine
patchable Wasm call. Each still needs its next instruction inside the same
authenticated function. Ordering operands stay hidden; IBAR, system/trap
instructions, other hints and an out-of-owner successor remain refused.
