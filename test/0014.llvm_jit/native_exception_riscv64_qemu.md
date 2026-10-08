# RISC-V64 native EH execution qualification (r216)

This stage executes the complete VM target ELF under `qemu-riscv64`, using
frozen r216 sources and a genuine independent LLVM `23.1.1-uwvm-ros.8` package.
The host runtime is cross-compiled with Clang at O1, `-fno-rtti`, C++ exceptions
and asynchronous unwind tables. The actual target provider is libstdc++ with
libgcc_s, confirmed by the target loader; this does not qualify LLVM libunwind
or establish a performance result from QEMU.

Fresh RISC-V CMake configuration matched all seven checked generated
ABI/config/target files, apart from the expected version change. The target
compile graph and changed-private-header include closure selected sixteen
translation units, including the complete RuntimeDyld component. Seven
archives were updated; the other 55 archives remain hash-identical to their
original provider. No source or original provider was modified by the build.
The real target llvm-config and an LLVMSupport version printer both report .8
under QEMU. Product version validation rejects the old .7 headers.

The package includes the RuntimeDyld distant CALL/CALL_PLT repair described in
`documents/toolchain/riscv-rtdyld-call-range.md`. The default MCJIT code model
passes the standalone landingpad qualification: 18,313 checks and 1,152 real
JIT executions. This is a dependency check, separate from VM qualification.
No Large-code-model workaround is enabled for the VM runs.

| Actual frozen r216 VM profile | Qualified fixtures | Stack policies | Outstanding failure |
| --- | ---: | --- | --- |
| uwvm2-ros LLVM full | 30 of 31 | instruction, unwind, none | indirect tail call |
| uwvm2 LLVM full | 30 of 31 | instruction, unwind, none | indirect tail call |
| uwvm2 LLVM lazy and lazy+verification | 30 of 31 each | instruction, unwind, none | indirect tail call |

The passing portion represents 360 actual target-VM fixture executions, plus
the ROS six-fixture development checks. It covers cross-function numeric
payloads, the 680-byte EH-only tuple, non-tail indirect calls, direct tail-call
handler bypass, catch ordering, imported tag aliases, loop labels, precise
uncaught payload bits and original throw stacks, and native traps that must
not become catchable Wasm exceptions. This is numeric `catch`/`catch_all`
coverage, not complete exception-reference or reference-payload coverage.

The indirect-tail fixture fails in all twelve requested product/mode/policy
combinations. Full mode rejects its emitted module before optimization or
native materialization: the RISC-V typed ABI uses the C calling convention,
while the indirect-tail path requires LLVM Tail calling convention. The
same-prototype direct-tail fixture passes. Lazy failures have separate logs
(instruction/none report an illegal throw opcode; unwind terminates without
that diagnostic), so they must be requalified after the ABI fix rather than
assumed solved. The rejected fixture is explicitly excluded from the later
30-fixture runs and is never counted as a successful test.

Full-module cross-process caching remains disabled by the existing RISC-V
safety policy: generated code can contain process-local bridge/storage
addresses. An attempted actual-cache memory-leaf inspection correctly produced
no cache object and its failed assertion is retained. No cache gate was
removed, and this stage makes no signed-cache or memory-assembly success
claim for the actual VM. The RV64 disassembly matcher added to
`check_wasm3_native_frame_codegen.py` uses unaliased `jal/jalr ra` and compressed
`c.jalr`, excluding return/jump transfers through `zero`; it still needs an
actual observed VM object on this target.

Dependency evidence in the ordinary repository:
`build/wasm3-evidence/llvm-ros8-riscv64-incremental-r216.tar.gz`, SHA-256
`ef3ad7d0e905236cfb7842a66f29e080739eb0554c47607d5f52e7a496d11b9b`.
Its 1,294 verified files include fresh configuration, rebuild closure, target
objects/archives, version and landingpad qualification. The package manifest
SHA-256 is `889420833124aa9355c13589415f7aa5470f9189ae71389afb305ab375064319`.
The first loader check used the wrong loader filename; the corrected check
uses `ld-linux-riscv64-lp64d.so.1`. Both attempts are preserved; no library
change was required.

Actual VM baseline evidence:
`build/wasm3-evidence/native-eh-riscv64-cli-r216.tar.gz`, SHA-256
`4948196d8e89f0d68ca8e03632dcdd7439eba3dfef782761cba476d22935fbce`.
Its 1,163 verified files include both real target binaries/runtime objects,
commands, emulator/provider hashes, source fingerprints, fixtures, successful
runs and all known failures. Complete matching frozen r216 source trees are
already retained in `native-eh-aarch64-cli-r216.tar.gz`, SHA-256
`f64daca7de1219252979afccb7882e646fd7a8743b3dd54a658678cec12b2417`;
the accompanying provenance file records this shared source archive instead
of duplicating it. Every archive member was checked against its SHA manifest
and the remote original.
