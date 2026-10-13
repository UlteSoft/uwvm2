# AArch64 native EH execution qualification (r216)

This qualification runs the complete target VM executable under `qemu-aarch64`.
The earlier standalone landingpad probe is a separate LLVM dependency check.
The tested runtime sources are frozen r216; the host executable is compiled
with Clang at O1, `-fno-rtti`, C++ exceptions and asynchronous unwind tables.
The actual target C++ provider is libstdc++ with libgcc_s, as recorded by the
AArch64 dynamic loader. This is not LLVM libunwind qualification or a QEMU
performance measurement.

The target LLVM package is a genuine independent
`23.1.1-uwvm-ros.8` incremental build. Fresh AArch64 CMake configuration matched
all seven checked ABI/config/target headers with only the expected version
change. Its own compile-command graph and private-header dependency closure
selected nineteen translation units. Nine archives were updated; the other
103 supplied archives are hash-identical to the original target package.
QEMU executed both the newly built target llvm-config and an LLVMSupport
version printer; both report .8. The .8 product guard rejects old .7 headers.
The provider's original archives were not modified.

| Actual VM profile | Fixtures | Requested stack policies | Commands including encoding and Wasmtime reference |
| --- | ---: | --- | ---: |
| uwvm2-ros LLVM full | 31 | instruction, unwind, none | 188 |
| uwvm2 LLVM full | 31 | instruction, unwind, none | 188 |
| uwvm2 LLVM lazy and lazy+verification | 31 each | instruction, unwind, none | 281 |

All profiles passed. This represents 372 actual target-VM fixture executions,
in addition to the small six-fixture development checks. Cases cover numeric
cross-function payloads, a 680-byte EH-only tuple, direct/indirect calls,
parameter/result preservation, catch order, imported tag aliases, loop labels,
tail-call bypass, uncaught payload bit patterns, original throw stacks, and
traps that must not be caught as Wasm exceptions. These fixtures qualify
numeric `catch`/`catch_all`; they do not claim complete exception-reference or
reference-payload coverage.

Both target VMs also executed the mixed-tuple fixture with a fresh private
signed object cache, then reused it under each of instruction/unwind/none.
Every warm run records `object-cache-hit` with `signature_verified=1`; the
object bytes remain unchanged. Objects decoded from these actual caches are
AArch64 ELF with LSDA, FDE, exact guest typeinfo/personality and catch-runtime
relocations. Even `none` retains EH unwind metadata, independently of optional
diagnostic stack recording.

This is historical r216 evidence, not the current AArch64 full-module cache
contract. In r299, a Core 3 shared-memory atomic object materialized its
process-local memory base in AArch64 `movk` immediates. Two processes produced
the same Wasm hash but different IR hashes, so persistent replay is neither
safe under a weaker key nor useful under the complete key. Current AArch64
tests require the full-module cache to stay disabled and inspect a transient,
test-only MCJIT object for execution, EH/CFI and instruction versus unwind
assembly checks. They do not require a signed cache hit.

The actual multi-memory fixture's cached memory leaf was disassembled for both
repositories. Instruction tracing contains two frame-maintenance calls.
Unwind contains no calls and retains CFI; this constant-offset leaf is just
address materialization, `ldr`, and `ret`. This is a precise check of that
fixture's generated hot path, not a claim about every memory access pattern.

The build and execution manifests record the real target ELF hash, emulator
hash, dynamically resolved target libraries, frozen source fingerprint,
LLVM package manifest, commands, and input stability. The Python CLI runner
uses a QEMU wrapper; that wrapper's hash is supplemented by the actual target
binary/provider hashes rather than being treated as the executable identity.
The default product memory mode is used; mmap was not disabled for this run.

Dependency evidence is stored in the ordinary repository under
`build/wasm3-evidence/llvm-ros8-aarch64-incremental-r216.tar.gz`, SHA-256
`ade0ce79b8d590121e3f205ea768504058e36d56270199aaf276d00866350451`.
It includes 1,298 verified files, the target build closure, generated config
comparison, version checks, and 18,313 landingpad checks / 1,152 real JIT
executions. The dependency package manifest SHA-256 is
`bc016238fd99985ee89943e861a9962d788e5bd38f6ca4486111d72d3ea8c9c6`.
The first version-check attempt incorrectly expected an unnormalized host
triple; LLVM correctly returned `aarch64-unknown-linux-gnu`. Its failure and
the corrected assertion are retained; no library change was needed.

The complete VM qualification archive is
`build/wasm3-evidence/native-eh-aarch64-cli-r216.tar.gz`, SHA-256
`f64daca7de1219252979afccb7882e646fd7a8743b3dd54a658678cec12b2417`.
Its 4,950 verified files include both actual target executables/runtime objects,
frozen r216 production source trees, fixture sources, commands, reference and
VM logs, signed cached objects, disassembly, and dependency/source manifests.
Every member was checked against its manifest and the remote originals before
removing backed-up large executables and runtime objects to free test space.
