# Debug-full patchable call routing prerequisite

The compiler and runtime now route eligible LLVM-full debug calls through stable
typed slots and resolve indirect calls from the live table. This is **not**
end-to-end hot replacement: same-ABI replacement compilation/publication,
generation-aware diagnostics, code/CFI retirement and a working replacement
command remain missing. The host controller returns `unsupported` for
`replace_function`.

## Compiler interface and unchanged default

`compile_all_from_uwvm::compile_option` has two default-zero fields:

- `debug_full_patchable_typed_target_base_address`
- `debug_full_patchable_typed_target_count`

Either nonzero requests routing. Preparation requires both a nonzero aligned base
and exactly one slot per local function, checked multiplication/address extent,
`compilation_mode == full`, and `emit_debug_safe_points == true`. Existing debug
mode checks also reject raw-bridge routing, lazy target metadata and tiered OSR.
Invalid requests clear emitted module storage; they cannot publish an empty
successful result. Base/count zero keeps ordinary full lowering unchanged.
Debug-full, including a module that cannot use typed slots, uses the live-table
indirect resolver described below. Memory instructions receive no new checks,
loads or locks.

The external symbol is `uwvm_m_<module hash>_debug_full_typed_targets`. LLVM sees
an exact `[local_count x intptr]` allocation. Every bounded direct-call slot load
is volatile and acquire. The helper independently checks the requested LLVM
function type against the selected module-owned signature before emitting a GEP.
The compiler never dereferences slot contents or bakes a current target into IR.
The existing host-object symbol registration handles relocation binding. All
debug-full object-cache reads and writes are disabled: generated code also
embeds process-local observer, pause-domain and slot addresses, so a persistent
object would not be safe to load in another process. Ordinary full cache
behavior is unchanged. The slot extent/address remains immutable while any
compiled generation uses it.

`single_func_emit.h` routes normal local calls and same-module resolved import
aliases through these typed slots. `return_call` uses the same checked slot;
self-tail calls deliberately stop using the old-body backedge. The original
calling convention, multi-result output pointer and immediate `musttail`/`ret`
sequence remain. Imported targets in another module retain the existing raw
runtime bridge or typed-tail resolver. `call_indirect` in debug-full copies a
target record from the current actual table under the same host lock as table
mutation and active-element initialization, then releases the lock before
invoking the target. It retains the full-width bounds, null and canonical-type
checks, typed/raw branches and host tail adapter. This matters because debug
preparation compiles before ordered active segments may populate an imported
table. Ordinary full retains its original compact-view lowering.

## Runtime contract for replacement publication

The host must validate the **complete** original and replacement Wasm parameter
and result sequence, native calling convention, argument ABI and hidden result
buffer ABI. Equal byte sizes alone are insufficient. Provider/module identities,
reference codecs, memory/table/global/tag bindings and exception compatibility
must remain valid. This compiler option is host-only; guest memory is never a
source of executable pointers.

The current debug-full runtime allocates slots before compiling and publishes
nonzero typed targets only after native code/CFI registration, before admitting
guest execution. A future replacement must install every new typed target, raw
wrapper, CFI registration and diagnostic mapping before publication. A
replacement transaction must own an authorized pause covering all participating
host executions and must seal new admission during publication. Prepare and
validate the replacement completely before that transaction. Publish the new
owner/CFI/maps first, then raw public-entry targets, direct typed slots and all
indirect/import aliases; release stores pair with generated acquire loads. Resume
only after the entire transaction completes. Independent release stores alone do
not make a multi-slot replacement atomic to unpaused guests.

Existing `publish_llvm_jit_call_indirect_defined_entry_targets` is not sufficient
unchanged for hot replacement: it matches the stored context or the **new** raw
address. An already-full entry with context zero still contains the **old** raw
address. The replacement implementation must use stable function identity or
explicit old-address matching to update every alias. Imported-call and public raw
entry caches need the same audit. Do not change guest function-reference identity
when changing its code implementation.

Retain old executable owners, FDE registrations, diagnostic maps and metadata
until no activation/return PC can refer to them. Keeping all versions until a
fully drained runtime reset is a conservative first implementation; pausing does
not remove old activations. An already-entered function may finish its old body;
subsequent normal or tail calls consult the updated slot.

Debug metadata needs an additional decision before this is safe to expose. The
current safe-point bridge carries module/function/offset/local bytes, while its
code generation comes from runtime context. Resumed old activations may have old
local layouts and offsets. The first runtime implementation must reject replacement while the target appears
in any cooperatively stopped active frame, including suspended host reentry. A
future implementation may instead carry and retain per-code-generation metadata
for both old and new activations. A single global generation increment cannot
identify old frames.

## Focused qualification

`test/0014.llvm_jit/llvm_jit_debug_patchable_calls_ir.cc` covers scalar/vector
parameters, multiple results, local/direct/indirect/self tail calls, unchanged
mmap accesses, exact signature/index rejection, invalid mode/table alignment/
extent and incompatible routing options. It inspects actual translated Wasm IR,
including exact array bounds, acquire/volatile loads, calling conventions and
adjacent `musttail` returns. It never executes a fake replacement target.

`run_debug_patchable_calls_ir.py` requires a candidate probe with its matching
runtime object and either a baseline probe or retained qualified baseline
objects. It runs both stack policies and LLVM verification/O3. It compares
**all** nonempty executable sections and complete relocations for ordinary
full mode. Debug-full intentionally differs: both its typed-slot and fallback
objects must contain the live-table resolver relocation and no compact-view
read, while typed-slot objects additionally retain acquire loads and actual
indirect tail jumps, including self tail. This test has no timing, concurrent
replacement publication or retirement claim.

The matching frozen r247 O3 builds passed in the Linux 64 GiB cgroup for both
repositories, compared with matching frozen r243 baseline probes. Ordinary used
the genuine LLVM provider revision .8; ROS used its paired revision .9. Both
`instruction` and `unwind` policies passed LLVM verification and x86-64 object
checks. Eight before/after object comparisons retain identical complete
executable sections and relocations, including the mmap load/store function.
The 20 generated objects contain nonempty `.ltext`; no `.text`-only shortcut is
used. Enabled routing keeps five bounded acquire/volatile slot loads and five
`musttail` calls, removes the old self-tail backedge, and produces actual indirect
tail jumps in all four selected tail functions. Fourteen invalid configurations
per policy are rejected in ordinary, twelve in ROS.

The first probe had a test-only assertion that incorrectly required a GEP
instruction. LLVM legitimately folds constant GEPs into constant expressions or
the base global for index zero. The r2 test accepts and verifies all these forms;
no production change was needed. The original failed probe and diagnostics are
preserved in `r247-debug-patchable-failed-probe.tar.gz`, SHA-256
`c5a0bf998608acf74242c28c8b07e03bd223856fbd7a0d57c8de8cbea3ab7744`.
The independently recorded test overlay SHA-256 is
`53d5a8d7235ec4983ed385bab3ffc653da1b0aaaa67e34c27a0e18e2b7632e83`.
The complete compiler qualification archive is
`r247-debug-patchable-compiler-qualification.tar.gz`, SHA-256
`aa31652e9dbe00ac80ca0619cb0fa69ccd5c6b61b6cc185a2bb194482821ec60`;
all 487 archived files were individually checked against the manifest. It
includes matching probes, commands, IR, objects, disassembly and source
fingerprints; runtime objects and CLI binaries remain in the separate integrated
qualification evidence.

Frozen production source IDs are
`sha256:75c8528be908555515410a52fc232a609a2c2588a5b7a0db4d3d224f8c51fa51`
(ordinary) and
`sha256:0e50d182fe617a2fcf47a370ea00c206529d71e70d4f7f7c861a2a7f8b410e83`
(ROS). Neither these checks nor the source IDs qualify execution of replacement
code, publication under concurrent guests, other target architectures, or timing.

The later r252 qualification connected slot routing to both runtimes and tested
it under actual O3 CLIs. In each product, debug console and host-owned Unix
server tests passed, and both `instruction` and `unwind` probes found the same
live resolver in debug-full ELF relocations. Ordinary full executable sections
and complete relocations stayed identical to r247. Four actual mmap memory
leaves per product were byte-identical to r243 and r247; unwind leaves kept
zero helper calls or synchronization instructions. The independently checked
archive is `build/wasm3-evidence/r252-debug-full-qualified.tar.gz` (SHA-256
`5315f0a8e293eadccc34f6ba38bb382c157d10949ee67fe21931577963e226ee`).
The updated IR runner used in that frozen-source qualification is a staged
test overlay (SHA-256
`9479d421cbf44fa7b852b0f383a9880e95e5f9cbed82438d58261429c391e755`).
General `call_ref`, retained exception references, and full GC remain outside
the supported replacement ABI. No code generation/CFI lifetime transaction or
same-ABI replacement execution has been accepted.

LLVM contracts consulted: [call and musttail](https://llvm.org/docs/LangRef.html#call-instruction),
[atomic ordering](https://llvm.org/docs/LangRef.html#atomic-memory-ordering-constraints).
