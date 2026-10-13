# Real private loaded-object failure and four HotReplace entries

All native qualification is false. Only the Linux keeper may compile/run this
component in the original 64 GiB cgroup. Build it with the actual LLVM backend
and C++ exception prerequisites, current product includes and bundled FastIO;
run little/big endian native/QEMU jobs where MCJIT itself is available. The test
uses real MCJIT definitions, emitted objects, relocation callbacks and listener
registration. It never executes an unpublished VM generation or creates a fake
ExecutionEngine/ObjectFile/LoadedObjectInfo. Its replacement `operator new` is
TEST ONLY and is armed only around a synchronous real pending notification.
The first-allocation and one-successful-allocation controls must show sticky
failure, zero surviving ranges/functions and zero commit callbacks, while the
unarmed positive proves actual loaded function/text ownership. A callback-thrown
sentinel additionally verifies the real helper no longer terminates before the
pending receiver's boundary. This does not prove recovery from LLVM fatal-error,
FastIO allocator termination, failed global LLVM allocation or no-exceptions
builds. The ordinary process-global listener keeps its original noexcept policy.

`hotreplace-four-entries.wat` is modern Core3 source: initialized nondefaultable
GC local across typed exception payload/catch, memory64/table64 and typed active
reference/indirect call. Parse it with the pinned official parser, and run both
instruction/unwind LLVM-full debug configurations. Its worker returns 42 from
35+7. A same-source same-ABI complete body replacing marker7 with8 returns43;
update the actual `expected` global as part of the authorized stopped state. With
actual resumable profile17/nativeABI2, verify typed/raw/resume-typed/resume-raw
all originate from definitions in the actual replacement engine and have exact
loaded function/text proof before prepared diagnostic maps or stopped-set
commit. Reject missing/declaration-only/foreign/global same-name targets and
invalid changed i64 result bodies; no old generation should be published on a
failure. The saved complete body/type/tag/source identities must remain authentic.

Four addresses and diagnostic ranges are DATA, not execution, checkpoint,
ASM/stop, source, epoch, CFI or complete world-restoration permissions. Preserve
actual current transaction predicates. Original FastIO prepared-map helper and
append callback remain noexcept; their native allocator policy was not changed.
Runtime manager full/staged/hot paths must reject sticky observation failures
both after finalizeObject and after possible demand lookup, before owner seal,
adoption or metadata publication. The optional explicit-ELF observation and
native provenance candidate are revoked if this outer failure occurs. Missing
DWARF alone remains unavailable mapping and is not treated as an OOM failure.

Official source references used for callback/Function ownership review:
- https://github.com/llvm/llvm-project/blob/main/llvm/lib/ExecutionEngine/MCJIT/MCJIT.cpp
- https://github.com/llvm/llvm-project/blob/main/llvm/lib/Object/SymbolSize.cpp

Production matcher/typed validation, original traces and uncaught FastIO
exception diagnostics, hot memory/scalar IR and musttail emission are unchanged.
Header/module/native/official-parser/LLVM/CFI/restore/ASM/performance results are
all unrun; this packet is PRIVATE SOURCE pending integration and real tests.
