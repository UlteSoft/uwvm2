# SystemZ load-and-trap motion

The new multi-memory interpreter cross test exposed an LLVM 23.1.1
`SystemZElimCompare` failure. A load of an instruction operand was sunk past
cursor stores and an address-register kill. Machine verification rejected the
result; independently, sinking a load past an aliasing store can change the
loaded value and introduce a trap.

The [downstream patch](patches/llvm-systemz-load-trap-motion.patch) checks memory
effects and base/index definitions before sinking, clears obsolete address kills,
and retains the original memory operands. It preserves adjacent `lgat` fusion.
The patch is based on LLVM 23.1.1, also retained in uwvm2-ros's `.7` package.
It is a local repair, not a claim that LLVM upstream has accepted it.

Apply from an LLVM project source root with `git apply --check` first, then
`git apply`. Rebuild the SystemZ backend and invalidate old native-object caches
through the toolchain's package identity. Do not disable `-verify-machineinstrs`
or remove interpreter `musttail` annotations to work around this failure.

`test/0014.llvm_jit/fixtures/llvm23-systemz-load-trap.ll` retains the reduced real
interpreter case, an aliasing-store case, and a positive adjacent-fusion case.
The original real case fails machine verification with the baseline compiler;
the patched compiler passes LLVM FileCheck and machine verification. The C
oracle executes 999 distinct values under QEMU SystemZ and checks both memory
contents and cursor/result updates:

```sh
python3 test/0014.llvm_jit/run_llvm23_systemz_load_trap.py OUTPUT \
  --llvm PATCHED_LLVM_BIN --clang CLANG --deps CROSS_DEPENDENCIES
```

Run inside the prescribed Linux cgroup. This is a compiler regression, not proof
of complete SystemZ JIT, unwinding, or WebAssembly 3 conformance.
