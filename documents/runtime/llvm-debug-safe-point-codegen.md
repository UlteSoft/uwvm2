# LLVM full cooperative debug-point code generation

This is optional compiler/runtime infrastructure, not a completed interactive debugger,
source-language local-variable inspector, or function hot-replacement implementation.
It is shared by the ordinary and ROS full LLVM emitters. Lazy and tiered execution
must reject an enabled request; the ordinary compiler additionally rejects lazy target
metadata, atomic lazy targets, raw bridge routing, and OSR reentry metadata.

`compile_option::emit_debug_safe_points` defaults to false. Enabling it requires explicit
`llvm_jit_compilation_mode::full` provenance. T2 can otherwise resemble full compilation,
so the presence or absence of OSR entries alone is not a sufficient mode check.
The entry/loop implementation emits a `noexcept` host bridge with three register-width
scalars: module identity, function index, and expression-relative byte offset. The runtime
supplies the pinned code generation and authorized participant context. No native pointer
or arbitrary host-memory operation is exposed to Wasm.

The source span is the already validated, pinned function expression. Offset zero is the
first expression byte, not the code-section/local-declaration offset. The compiler rejects
null or empty spans, one-past offsets, and `SIZE_MAX`. Entry observations are placed in the
body block reached again by optimized self-tail transfers; loop observations follow the
header PHIs so both first entry and backedges visit them. Actual musttail calls retain an
immediately following return. The host bridge and call are nounwind but deliberately retain
synchronization and memory side effects: a host may cooperatively wait there.

The shared bridge-symbol helper registers the process address through LLVM `AddSymbol`
while constructing IR. Full materialization performs both ordinary and parallel object-cache
lookups only after that translation, so a cache hit also has its bridge binding available.
The runtime cache policy separates enabled points with `debug-safe-points=entry-loop-v1`;
the complete preoptimization IR hash additionally participates in the cache identity.

## Qualified entry/loop scope

The r218 runtime integration is tracked separately from the compiler probes. The following
compiler evidence uses genuine paired LLVM 23 `.8`, SSH Linux, and the required 64 GiB
cgroup. Probe/host runtime optimization is not a timing result; generated object evidence
uses LLVM `default<O3>` plus `llc -O3`.

Both products and both instruction/unwind diagnostic policies passed actual Wasm translation
of four functions containing scalar memory accesses in a loop, a cross-function `return_call`,
and a self-tail transfer. The enabled IR contains exactly four entry points and one loop
point at expression offset two. The probes check source bounds, exact host symbol resolution,
noexcept side effects, true musttail adjacency, and the self-tail backedge. The ordinary
product rejects six invalid mode/routing/target combinations per policy; ROS rejects four.

For the same fixture, default-off and explicit-full/off objects are identical, and each also
matches a probe built against the genuine frozen r216 pre-change emitter. Every nonempty
`SHF_EXECINSTR` section, its identity/attributes/alignment, actual bytes, and all object
relocations are compared. Only the existing `uwvm_m_<hash>` namespace is normalized.
The unchanged executable totals are 1063 bytes with instruction diagnostics and 827 with
unwind diagnostics; enabled entry/loop totals are 1209 and 1081 bytes respectively, in both
products. This proves absence of added generated instructions for that complete fixture;
it is not a universal performance or platform claim.

A negative test found that r218 per-function rejection could leave an empty LLVM module
which aggregate finalization reported as emitted. No function body or debug bridge was
published in that case. The r219 guard-only correction clears the aggregate storage before
returning failure, and the negative tests now require `emitted == false`. The runtime's
independent full-mode authorization check remains in place.

Evidence archives:

- `build/wasm3-evidence/debug-safe-point-ir-r218-rejected-state-discovery.tar.gz`, SHA-256
  `69352a6b4d937f4c82984a55aa62daa4eebac432c3091d69a120af747387da12`;
  retained discovery binary/logs and positive IR, not a passed negative suite.
- `build/wasm3-evidence/debug-safe-point-ir-r219.tar.gz`, SHA-256
  `c2db2c2c4b4e88bbc7302edb2e2a93dac203c822343523afd420cb86676823e3`;
  335 regular files, 97,422,198 compressed bytes, both products' guard-only qualification,
  frozen r216 baseline probes/objects, commands, source hashes, and full relocation dumps.
  Every archived regular file was hash-verified. Only four verified probe executables were
  subsequently removed from remote scratch storage, releasing 301,982,896 bytes.

## Qualified per-instruction extension

`llvm_jit_debug_safe_point_granularity::{entry_loop,instruction}` is an additional explicit
compiler choice; entry/loop remains the default. Instruction mode is intended for an
interactive debug session and carries substantial expected overhead: external observer calls
can spill registers and inhibit load merging or other optimizations. Disabled code must retain
the same complete executable-section and relocation evidence.

Ordinary reachable operations observe their checked opcode offset before consuming operands.
Loop, else, and structural-end points are deferred to their correct post-PHI target blocks;
this covers branch and exception-handler arrivals without incorrectly stopping the then arm
at an else location. A lexical function end is observed only on fallthrough, not by adding a
synthetic stop to a shared return block. The first opcode replaces the extra entry-zero point,
so self-tail reentry sees it without duplicate stops. Prefix opcodes have one observation for
the complete Wasm instruction, and deduplication between inline and generic emitters is only
compiler state, never generated execution state.

The separate r220/r220b qualification passed in both products using genuine paired LLVM 23
`.8`, frozen matching runtime headers, and the required SSH Linux cgroup. The four-function
fixture has exactly 36 distinct reachable opcode points. With instruction diagnostics its
instruction-mode executable sections total 1961 bytes; with unwind diagnostics they total
1817 bytes. Default-off objects retain the exact r216 executable bytes and full relocations
(1063 and 827 bytes respectively); entry/loop totals also remain 1209 and 1081 bytes.
Source bounds, structured merge positions, implicit versus early returns, self-tail reentry,
true musttail adjacency, invalid mode/state rejection, and bridge symbol/side effects passed.

Additionally, 31 actual Core3 EH fixtures (including imported tag identity) and a synthetic
syntax-disabled import transit module passed actual translation and LLVM verification under
all three diagnostic policies: 96 translations per product, 192 total. Instruction offsets
are bounded by the actual function expression and musttail calls remain adjacent to their
returns. These probes inspect generated IR/native objects; actual guest pause/resume and
observer execution are separate runtime tests, and these figures are not timing results.

The archive `build/wasm3-evidence/debug-instruction-ir-r220.tar.gz` has SHA-256
`188446096ff7174a12f933518cfb469e52738a0b383e8d5be3a7363dfd16491f`,
455 regular files and 97,079,757 compressed bytes. Every member was independently hashed;
four probe executables were then freshly rechecked and removed from remote scratch,
releasing 301,885,096 bytes. Commands, fixture/source fingerprints, initial test-only compiler
diagnostic, corrected successful builds, complete IR/objects/relocations, and exact reused
r216 baseline objects are retained.
