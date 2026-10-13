# Debug-full native PC provenance, compiler foundation

This is a source candidate. No new native IR/object/product receipt has yet
qualified it. Its compiler base includes the separately frozen Core 3 fused
block-type resolver v3-r2. It does not change pure validation, add a bytecode
validation traversal, emit a per-instruction runtime mapping callback, or
extend native-step platform admission.

Only admitted debug-full compilation attaches LLVM-owned line-table metadata
to the actual typed Wasm function. The synthetic filename is
`uwvm-mMODULE-fFUNCTION-gGENERATION.wasm-native-v1`, producer is
`uwvm debug-jit native Wasm provenance v1`, and directory is
`uwvm-native-provenance-v1`. The original function/module/generation comes from
the real compiler's full-only request. LLVM owns those strings immediately.
Guest custom `.debug_*` sections cannot supply this native metadata.

DWARF line N>0 means expression byte offset N-1. Line zero means unknown native
scaffolding. Fused dispatch sets a location before inline and generic opcode
lowering; structural and exceptional code can legitimately have multiple
native ranges for one Wasm opcode. Optimizations can remove, combine, duplicate
or reorder instructions, so this is execution provenance, not a promise that
every byte has a native instruction or that a native register is a guest local.
Source-language lookup is a separate guest-DWARF query at a resolved Wasm
offset. Live locals remain unavailable unless their independent capture and
generation checks succeed.

## Required real qualification

The sole native keeper runs in the existing 64 GiB cgroup, using E-cores for
builds. Retain before/after source identities and actual LLVM tool/library,
output, process and command hashes.

1. Build/run `native_provenance_metadata.cc`. It uses actual LLVM DIBuilder,
   verifies the resulting module, refuses invalid/one-past offsets and a
   conflicting existing DWARF version, and verifies zero-line cleanup and
   immutable compiler file identity. Optional output IR uses fast_io RAII.
2. Use official `llc -O2 -filetype=obj` on its verified IR, then actual
   `llvm-dwarfdump --verify` and `--debug-line`. Use available x86-64, AArch64,
   RISC-V64, COFF and Mach-O target closures independently. Require matching
   producer/file identity, bounded native rows and only known Wasm offsets;
   optimization may legitimately eliminate an input row. Object generation
   does not prove that the VM retained or relocated those rows at execution.
3. Fresh-build all runtime, main and host-component TUs in both products.
   Exercise debug-full ordinary opcodes and Core 3 GC, modern `try_table`,
   exception paths, tail calls, memory64/table64, and hot replacement.
   LLVM verification must pass before object publication. Ordinary full, lazy,
   tiered and interpreter modes must have no native-provenance CU or generated
   mapping callback. Compare normal-mode generated memory-access instructions.
4. The runtime still needs live object/line-table publication. It must bind
   relocated `[native_begin,native_end)` rows to the actual loaded code owner,
   generation and runtime epoch under the existing private execution lease,
   pause-ticket, publication and real native-trap guards. A file-relative
   object address, user-supplied PC, guest DWARF row or copied display ID must
   not mint read authority. Resumed/reset/replaced/stale captures must fail.
5. Only after that runtime query is implemented and tested may the controller
   display native-PC-to-Wasm/source provenance. It must preserve unavailable
   gaps and multiple matching provenance results rather than invent a source
   line from the previous cooperative safe point.

## Native execution-plan follow-up

LLVM MC instruction descriptors provide call, return and branch semantics;
formatted disassembly text is display data and is unsuitable as an execution
plan's instruction classifier. `ni` and `finish` additionally require real
current and parent physical-frame identities, bounded admitted callee/return
owners, and safe cancellation/retirement across exceptions and replacements.
The existing one-owner `si` cannot silently authorize an escaped host PC.

References:

- [LLVM source-level debug metadata](https://llvm.org/docs/SourceLevelDebugging.html)
- [LLVM JIT object debugging support](https://llvm.org/docs/DebuggingJITedCode.html)
- [LLVM MC instruction descriptor semantics](https://github.com/llvm/llvm-project/blob/main/llvm/include/llvm/MC/MCInstrDesc.h)
- [LLDB native instruction execution plan](https://github.com/llvm/llvm-project/blob/main/lldb/source/Target/ThreadPlanStepInstruction.cpp)
