# Loaded native provenance runtime candidate r1

This is source only. It has not been compiled or executed on the author host.
Old R3c/R3f native binaries, old target receipts and successful formatter/source
checks do not qualify this candidate. Use a fresh composite runtime/main/host
TU closure in both repositories. The independent compiler DI r1 source remains
immutable; current compiler leaves may additionally contain reviewed checkpoint
hooks and fused Core 3 fixes and must be snapshotted by the keeper as a whole.

Only the private full/replacement MCJIT listener opts in to line-table capture.
It processes the real object with `DWARFContext::create(Process, LoadedObjectInfo)`
and owns only scalar loaded text ranges, row intervals and copied opaque file
identities. It does not use ELF-only `getObjectForDebug`, a global image slide,
guest `.debug_*` addresses, a raw requested native PC or borrowed LLVM rows.
Object/LLVM borrow lifetimes end during `notifyObjectLoaded`. Object errors,
unsupported relocation, row/file/section budgets and ambiguous ownership remain
unavailable; metadata failure does not publish a partial address map.

The initial full image is moved only after symbol resolution, engine ownership,
actual range/CFI commit and private listener detachment. A replacement keeps its
map private with its new engine, then joins the existing actual stopped-set
commit. Each image binds exactly once to the actual nonzero runtime epoch.
Reset destroys its owner. The private actual restore/drain generation-change
hook invalidates both initial and retained maps; old maps cannot be rebound to
a new epoch. Even before that hook is invoked, a query for a changed epoch fails.

The new host query accepts a canonical private activation capture and an
optional actual native session identity. It has no requested-PC argument. Its
lock order follows existing code-copy operations: execution lease, one real
parked-participant transaction, publication, native transition. It checks the
actual trap/participant/thread, exact function code interval, unwind entry,
module/function, retained generation and runtime epoch before consulting rows.
Copied result IDs/PCs/row positions are data and cannot mint fresh authority.
Only exact rows expose a Wasm offset. Unknown and ambiguous rows stay explicit.
Several same-PC DWARF origins are deliberately retained conservatively; this
can reduce exact coverage compared with LLVM's last-row lookup. Legal zero-width
ordinary rows at HighPC are checked and ignored because they cover no byte.
No DI row implies exact VM locals, GC roots, operands or native frame values.
Ordinary full/lazy/tiered/interpreter execution gets no mapping callback or
new normal-mode hot-path poll.

## Actual keeper qualification

All execution/builds remain in the existing 64 GiB/no-swap Linux cgroup;
E-cores build, P-cores measure. Native guest/platform runs remain serialized
with the sole keeper/platform agent and retain the actual source, tool/library,
command, process and output identities. No author-host native run is authorized
by this document.

1. Run the immutable `native_provenance_metadata.cc` LLVM verifier test first.
   Build the new `native_provenance_sections_metadata.cc` against the same real
   LLVM closure. It takes `OUTPUT.ll elf|coff|macho`, uses fast_io RAII output,
   and emits two separate text sections. A preceding exported padding function
   forces each mapped function to have a nonzero section-relative address.
2. For each available closure, use official tools independently, for example:

   ```sh
   FRESH_SECTIONS OUTPUT.ll elf
   ACTUAL_LLC -O2 -filetype=obj -mtriple=x86_64-pc-linux-gnu OUTPUT.ll -o OUTPUT.o
   ACTUAL_DWARFDUMP --verify OUTPUT.o
   ACTUAL_DWARFDUMP --debug-line OUTPUT.o
   FRESH_LOADED_ROWS OUTPUT.o
   ```

   Repeat ELF AArch64/RISC-V64 and available remaining architectures; repeat
   COFF x86-64/AArch64 with `coff` section spelling and actual Windows triple;
   repeat Mach-O x86-64/AArch64 with `macho` spelling and actual Apple triple.
   The loaded-rows component requires two distinct real text section indices,
   distinct synthetic load addresses, nonzero function offsets, correct opaque
   module/function/generation identities, owner-contained exact rows, LLVM
   object borrow retirement, one-past rejection and epoch revocation. This
   proves relocation/data behavior only, not runtime trap ownership.
3. Run `native_provenance_mcjit_owner.cc` with actual target/MCJIT/asm-printer,
   Object, RuntimeDyld and DebugInfoDWARF libraries. It genuinely finalizes two
   separate native engines, observes the real loaded-object callback, commits
   actual function ranges, checks independent function generations, ordinary
   opt-out and epoch invalidation. It never calls generated machine code.
   Clang/GNU x86-64/AArch64 fixture admission is explicit; other fixtures return
   77 and are not counted as passed.
4. Officially parse/validate `debug_activation_runtime.wat`, then fresh-build
   `debug_native_provenance_runtime.cc` with the actual CLI/runtime macros and
   run `FIXTURE.wasm instruction` and `FIXTURE.wasm unwind`. Use the separate
   GC-root/tail-call fixture with its existing `gc-roots` argument as well.
   This is actual VM execution: canonical before-park captures, recursive and
   typed-tail identities, full/replacement code generations and owned native
   position lookup. It requires some exact original opcode provenance but
   permits legitimate unknown/ambiguous optimized scaffolding, especially a
   constant replacement. Forged native session, foreign alias, unreadable
   capture, stale stop, resume and reset must clear the entire output.
5. Fresh native CLI stepping on Linux/Windows/macOS still needs actual trap
   tests. The new cold query must be checked after real `si`/trap capture, not
   by treating the last cooperative safe point as the current native operation.
   Existing native one-owner admission is unchanged: `ni`, `finish` and calls
   escaping that owner still need real physical-frame/return-owner plans.

## Known LLVM 23 target dependency

The inspected vendored LLVM 23 `RelocationResolver.cpp` lacks a Mach-O AArch64
resolver. Its Mach-O x86-64 unsigned resolver returns S and discards LocData;
the real ObjectWriter uses local section relocation with an inplace nonzero
section offset. These are target dependencies requiring real patched-object
qualification. Do not suppress warnings, add a guessed global slide, count
unsupported targets as passed, or use the old single-section/offset-zero
fixture as proof. Keep actual ELF/COFF results separate from Mach-O pending.
The normal product should receive a reviewed patch; ROS should use its paired
vendored source correction once authorized and tested.

References:

- [LLVM JIT object debugging](https://llvm.org/docs/DebuggingJITedCode.html)
- [LLVM source-level metadata](https://llvm.org/docs/SourceLevelDebugging.html)
- [LLVM DWARFContext API](https://llvm.org/doxygen/classllvm_1_1DWARFContext.html)
- [GDB machine-code commands](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Machine-Code.html)
- [LLDB command map](https://lldb.llvm.org/use/map.html)
