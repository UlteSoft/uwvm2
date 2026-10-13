# RISC-V guaranteed tail ABI probe

This directory tests the target-host LLVM package itself under QEMU. It is
separate from full VM validation and uses only a private per-engine mapping
for two fixed host callbacks. It never reuses the emitted objects. The caller
selects `baseline` to record known .8 defects or `patched` to require every
valid fixture to execute correctly. Expected baseline defects are not a
successful Tail ABI implementation.

The [LLVM call rules](https://llvm.org/docs/LangRef.html#call-instruction)
require a musttail transfer to immediately precede its return, with matching
calling conventions and return types. C convention also requires matching
prototypes. Tail convention permits different fixed prototypes; simply
bitcasting a C callee cannot provide this guarantee. Tail varargs and
unsupported ABI attributes must still be rejected by the verifier.

Run only in the configured SSH Linux cgroup:

```sh
python3 test/0014.llvm_jit/riscv_tail_abi/run.py \
  --out /dev/shm/uwvm-builds/riscv-tail-abi-new \
  --llvm-root /dev/shm/uwvm-llvm-riscv64-ros8/build \
  --llvm-source-root /dev/shm/uwvm-llvm-ros8/source/llvm \
  --mode baseline
```

The runner compiles the driver for `riscv64-linux-gnu` with libstdc++/libgcc_s,
`-fno-rtti`, C++ exceptions and unwind tables. MCJIT explicitly uses
`generic-rv64,+m,+a,+f,+d,+c,-v`, so the vector fixtures also exercise the
non-RVV ABI. A patched run records actual ELF disassembly and FDEs. The input
manifest hashes the real target archives, generated configuration, source and
link response file before and after execution.

The current generated set has eighteen valid modules and five invalid
musttail modules. Positive cases include equal signatures, 0→10→2 and
0→18→2 arguments, indirect target selection, register/stack argument reversal,
2↔12 and 2↔20 tail recursion, 10 and 24 f64 arguments, computed and permuted
v128 values, four v128 arguments, and a typed C++ exception leaving a tail
callee through a normal JIT caller. The exception callback performs an actual
`_Unwind_Backtrace` before throwing; the native outer caller catches only the
exact probe exception type. Tail recursion performs 10,000 iterations with
at least 20,001 SP observations and must stay within a 4 KiB observed range.
This bounds stack growth; it is not a performance benchmark.

The greater arities are necessary: RISC-V FastCC has twelve integer argument
registers and twenty floating argument registers. A Tail ABI reusing FastCC
would not overflow with only ten/twelve integers or ten floats. The four-v128
case forces additional scalarized argument locations too.

## Baseline findings

The genuine .8 package accepts all valid IR and rejects all five invalid
forms with verifier errors, rather than parse failures. It cannot lower
Tail convention, reporting `Unsupported calling convention`.

It also miscompiles an existing C-convention musttail with the same twelve
integer parameters. Reversing 1..12 and computing a position-weighted sum
must return 364, but returns 710. The actual object shows `middle` allocating
112 bytes, writing outgoing stack parameters at the current SP, then restoring
SP before the tail jump. The callee consequently reads the old 9,10,11,12
instead of 4,3,2,1. Some writes also overlap saved-register slots. A later unchanged-backend
probe build aborts with `free(): invalid size` after the same corrupted transfer,
so baseline capture requires nonzero failure rather than assuming the
corrupted caller can always print a deterministic diagnostic. Register-only
C musttail, twelve-argument ordinary call, and the same twelve-argument tail
without a local frame all return the correct values. These controls separate
the stack-slot failure from fixture or driver errors.

Two real FastCC vector controls pass. Without RVV, `<4 x i32>` is scalarized
into four XLEN register arguments, not two: the generated single-vector call
uses a0..a3 and the two-vector permutation uses a0..a7. This observation applies
to these fixed-vector types and target options; it does not qualify scalable
vectors or every vector configuration.

## Backend changes that need separate qualification

Relevant bundled source files are under `third-parties/llvm/llvm/lib/Target`:

- `RISCV/RISCVCallingConv.cpp`: `CC_RISCV` currently dispatches FastCC only for
  `CallingConv::Fast`; introducing Tail requires an explicit ABI choice.
- `RISCV/RISCVISelLowering.cpp`: `LowerFormalArguments` rejects Tail. Current
  musttail stack stores use the local SP, and its indirect-argument path
  assumes a corresponding incoming formal. Neither assumption establishes
  different-arity Tail correctness.
- `RISCV/RISCVMachineFunctionInfo.h`: incoming argument-area size, callee-pop
  size and reserved tail-growth area need explicit bookkeeping.
- `RISCV/RISCVInstrInfo.td` and frame lowering: tail pseudos currently carry
  no stack delta. Normal calls to a callee-pop Tail function, ordinary returns,
  guaranteed tail epilogues and DWARF CFA updates must agree.
- `RISCV/RISCVFrameLowering.cpp`: call-frame elimination currently handles
  the ordinary stack amount without callee-pop accounting.

AArch64 provides concrete counterparts: `DoesCalleeRestoreStack`,
`getArgumentStackToRestore`, `getTailCallReservedStack`, the `FPDiff` incoming
minus outgoing argument-area calculation, fixed outgoing stack objects, and
`addTokenForArgument` to protect overlapping loads before stores. Its
`CALLSEQ_END` communicates `CalleePopBytes`; `TCRETURN` carries the per-tail
stack adjustment. These are design references, not proof that copying them
unchanged is correct for RISC-V.

The backend patch must preserve incoming values before overlapping stores,
keep stack alignment, avoid passing pointers into a retired frame, and retain
valid unwind CFA through both growth and shrinkage. Fixed-vector lowering,
byval, scalable vectors, reserved registers, large frames and alternate RISC-V
ABIs require explicit bounds on any claimed support. This test set covers the
RV64 LP64D fixed numeric path only; it does not qualify RV32, RVE, RVV or byval.

The baseline evidence archive in the ordinary repository is
`build/wasm3-evidence/riscv-tail-abi-baseline-r1-r5.tar.gz`, SHA-256
`58f4d1ff1ce46abda335791ff098630fb0ae12f1c83dff019b2b08ed459a6c35`.
Its 434 verified files preserve source revisions, actual target drivers, all
23 final IR fixtures, command/provider hashes, prior assertion failures, actual
ELF objects, disassembly, FDEs and both backend failures. Identical members are
represented as archive hardlinks after SHA comparison. Every expanded member
and remote original was verified before pruning only backed-up old binaries.
