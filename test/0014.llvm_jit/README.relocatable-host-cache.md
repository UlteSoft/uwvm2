# Relocatable host symbols and persistent JIT caches

The ordinary LLVM JIT cache follows `-Rllvm-cache-path` on every supported
native target. Full compilation, and lazy compilation in uwvm2, do not
silently disable the requested policy on i386, AArch64, or RISC-V64.
The ROS product retains its full-only compilation interface. Cache identities still distinguish target
ABI, CPU features, compiler policy, and source; an object built for one ISA is
not loaded as another ISA's code.

Host data and bridge symbols resolve to the current process during object
materialization. Targets that cannot directly address distant host storage use
a nearby, JIT-owned pointer cell whose initializer is an external symbol
relocation. RISC-V addresses the cell with `lla`, requiring nearby
`PCREL_HI20`/`PCREL_LO12_I` relocations and a full-width pointer relocation to
the host symbol. No process address is stored as the cell's numeric initializer.
The `llvm-host-symbol-address=relocatable-pointer-carrier-v1` ABI identity
invalidates objects emitted with the previous address scheme.

The common policy still rejects persistent objects from debug activations,
private native exception engines, and borrowed source plans that carry
process-owned state. These restrictions follow the same execution protocol on
each ISA. Explicit `-Rllvm-cache-path disable` continues to disable lookup and
store. Signature and source-provenance requirements are unchanged.

`llvm_jit_relocatable_host_symbols.cc` tests the production pointer-cell helper
through MCJIT's named-symbol resolver. Run the probe as three separate processes:

```sh
probe write /private/test/host.object 101
probe read /private/test/host.object 202
probe read /private/test/host.object 203
```

The first process compiles once; the readers must load the object without
compilation. Each reader supplies fresh storage, and the second process binds
the same bridge name to a deliberately different function body. `emit-ir
TARGET FILE` permits inspecting relocations after O1/O2/O3 code generation.
The raw fixture object is test input; it is separate from the signed runtime
cache format.

`run_relocatable_cache.py CONFIG` verifies pinned real CLI build receipts and
dependencies inside the required Linux cgroup. For every listed fixture and
compiler policy, it checks a cold store and two fresh-process cache hits through
both an isolated default `XDG_CACHE_HOME` and an explicit cache directory. Hits
must report `signature_verified=1`, preserve the stored object bytes, and
execute the fixture's Wasm assertions successfully. QEMU prefixes are supported.
A passing report covers only the architectures, profiles, and fixtures listed
in its configuration.
