# Numeric native exceptions in the LLVM compiler

This records the implemented numeric slice, not complete Core 3 exception or GC
support. `throw_ref`, `catch_ref` and reference payload reconstruction still need
backend integration. A canonical reference identity in `runtime/exception/value`
is not a packed Wasm reference ABI value.

The control-flow rules follow the [Core execution specification](https://webassembly.github.io/spec/core/exec/instructions.html),
LLVM [exception handling](https://llvm.org/docs/ExceptionHandling.html), and the
[`invoke`](https://llvm.org/docs/LangRef.html#invoke-instruction) /
[`landingpad`](https://llvm.org/docs/LangRef.html#landingpad-instruction) contracts.

## Emission and ownership

`compile_all_from_uwvm/translate/single_func.h` accepts a borrowed
`compile_option::native_exception_target_machine`. The runtime owns that target
until all compilation tasks and lazy materialization workers have finished.
Module preparation records the exact target triple/layout and module-owned native
EH declarations. Qualification uses the matching host C++ ABI and an actual LLVM
target with ELF/Mach-O DWARF CFI; it is not a processor-name whitelist.

CFI and exception propagation are part of the VM call ABI. They remain enabled
for a syntax-disabled module when an imported function can throw. The validator
continues to reject that module's disabled exception instructions.

`translate/single_func_exception_emit.h` contains the execution lowering:

- `try_record_runtime_local_func_llvm_jit_exception_handlers` copies ordered
  lexical handlers, live outer target blocks/PHIs and the target control-frame
  index. No address into a growing compiler vector escapes.
- `emit_runtime_local_func_llvm_jit_may_throw_call` emits an `invoke` when an
  active handler or owned instruction-trace frame needs exceptional control.
  It searches handlers from inner to outer and preserves clause order. The
  landingpad catches only the real `guest_exception` type; foreign native
  exceptions run frame cleanup and resume unwinding.
- `emit_llvm_jit_caught_numeric_tuple` calls the cold no-throw payload copier
  while `__cxa_begin_catch` owns the actual object. It rebuilds i32/i64/f32/f64/
  v128 SSA carriers from checked private-frame byte storage. Float carriers use
  integer bit patterns. It ends the native catch before adding label PHI edges
  and branching to the Wasm continuation. EH-only forward labels therefore get
  real incoming edges; loop labels receive their parameter tuples.
- `try_emit_runtime_local_func_llvm_jit_throw_numeric` materializes the complete
  validated numeric tuple and calls the runtime throw bridge with module ID,
  tag index, buffer address and exact bytes. Runtime module/tag metadata stay
  pinned. The bridge creates the existing owning exception value and a distinct
  native activation; it never substitutes a trap or TLS singleton.

The existing local matching `throw` to label optimization remains a direct
branch. The shared callback in `emit_llvm_jit_typed_wasm_call` puts multi-result
loads after the invoke's normal successor. Full, lazy typed/raw fast/slow calls,
imports and indirect calls use the same exceptional-call hook. Ordinary memory,
SIMD, trap and snapshot helpers are not converted into guest exception calls.

## Frame and tail-call rules

Instruction mode emits a cold pop when an exception leaves an owned Wasm frame,
including frames without local handlers. A handled exception that stays inside
the function does not pop that frame. Native-unwind and disabled-diagnostic modes
emit no logical instruction push/pop. All necessary generated wrappers carry
CFI; diagnostic unwind retains asynchronous records, while EH-only records use
synchronous tables.

Musttail transfers stay `CallInst` plus immediate return; they do not retain the
retired function's handlers. The ordinary non-tail tiered public wrapper cleans
its owned frame on an exceptional exit. A TailCC OSR wrapper restores the
inherited interpreter frame after the native core consumes it, on either normal
or exceptional exit, so its suspended C++ owner performs exactly one final pop.
This wrapper path has IR verification; that alone is not tiered execution
qualification.

## Focused evidence

`llvm_jit_exception_cross_ir.cc` verifies actual Wasm translation, typed
landingpad clauses, complete PHIs, CFI and zero instruction maintenance for
unwind/none. Its final remote matrix has 534 positive module verifications and
six explicit rejections of synthetic raw-only direct-tail configurations with
no typed tail target. The fixtures include a separately configured,
syntax-disabled imported transit module. The IR-only executable used aborting
link stubs for the three new runtime bridges; it does not claim guest execution.
The separate CLI suite tests actual exception execution.

`check_exception_cross_codegen.py` inspects real cached full-JIT objects. It
compares every nonempty executable ELF section and all relocations, validates
CFI/LSDA and exact bridge symbols, checks signed cache replay and native tail
jumps, and compares ordinary mmap load/store code with exceptions disabled and
enabled. A provably redundant protected-normal catch may be optimized away only
when complete code and relocations match the equivalent ordinary program.
Runtime build optimization is recorded separately from generated `pb-o3` code;
this checker makes no timing-performance claim.

The first IR and ordinary codegen evidence archive is
`build/wasm3-evidence/llvm-native-exception-compiler-ir-r212.tar.gz`, SHA-256
`a9517ed378e3e517c74dff1bba3977ff85084f094f0356f950ade7787f8a266b`.
It records the exact frozen LLVM/header inputs, including the ROS .7 header
probe; it is not a claim that that probe used the later .8 runtime build.

The actual ROS r213 runtime built against the genuine paired LLVM .8 package
passes eight cross-exception codegen checks across ten nonempty native objects,
and four local-throw/branch executable-section and complete-relocation pairs.
Its runtime was built at O1; generated code uses `pb-o3`.

`check_exception_memory_revision_codegen.py` also compares the identical mmap
loop using ordinary r211 and r212 runtimes both built at O3. Native-unwind mode
has identical whole executable images (338 bytes) and complete relocations.
Instruction mode keeps the exact memory leaf and normal caller instructions;
its whole executable image grows from 402 to 434 bytes for cold exception-frame
cleanup, with the relocation differences explicitly retained. This is static
code evidence, not a timing measurement or a claim about every memory program.

These actual ROS and before/after objects, commands, full disassemblies and
manifests are archived in
`build/wasm3-evidence/llvm-native-exception-codegen-r213.tar.gz`, SHA-256
`9b6da31856425c5653b1278345654e03f2ec7e4cbba5a0866c4f2aeb03d012bb`.

## Following publication and helper-contract changes

The logical instruction-frame push/pop helpers are `noexcept` in both host
prototypes and definitions. Their generated calls and external declarations now
carry LLVM `nounwind`, with compile-time checks against the host declarations.
The generic bridge and every potentially throwing Wasm call keep their original
exception contracts. `llvm_jit_call_stack_nounwind.cc` and its runner check that
LLVM may remove a redundant normal-call cleanup while retaining an actual guest
call's `invoke`, landingpad and resume. This focused test emits IR with address
stubs and does not execute guest code.

Ordinary lazy compilation additionally uses
`lazy_compile_options::prepare_materialized_group` before releasing any member's
`ready` flag. The synchronous callback receives the complete, exclusively owned
materialized records after native CFI registration and owner installation. It
prepares every member/core/reentry address mapping without publishing targets or
using a ready-gated accessor. Only after it returns can acquire readers observe
a callable entry; a direct call from that entry therefore cannot outrun another
member's diagnostic mapping. The retained single-function materializer uses the
same preparation path. ROS does not contain this lazy compiler.

`llvm_jit_lazy_group_publication.cc` exercises the ordering gate with concurrent
acquire readers and noncontiguous groups; native concurrent exception-trace
execution is a separate runtime qualification. These changes follow the frozen
r212/r213 objects above; those earlier archives do not include them.

The focused no-throw helper test passed for both products: LLVM O3 removes the
normal caller's redundant `invoke`/cleanup and preserves the actual guest-call
exceptional edge. The ordinary publication gate passed 64 rounds with 256
concurrent acquire readers at O3, and another 64/256 under ASan/UBSan. The
concurrent real-runtime trace host source passed a separate syntax check; its
execution still requires the following complete runtime build. Evidence:
`build/wasm3-evidence/llvm-trace-nounwind-group-publication-r214.tar.gz`, SHA-256
`6dddca42a0a4390cdecf2e9aba2c69f7e3c54c8525d21c388dbdaec90c6f7f7b`.

The r216 O3 runtime now passes the actual ordinary-lazy concurrent trace test:
16 processes rotate one throwing chain among eight concurrently executing native
chains, and one additional process checks all normal returns. Every exception
has the exact three Wasm function frames and payload, without truncation. The
test uses a void thread callable with an aborting check; an earlier test-only
return-int assertion accidentally created undefined successful fallthrough and
is retained as a failed attempt in the evidence. No production fix was needed
for that test issue.

Both r216 products also pass eight actual full-JIT codegen checks over ten
nonempty objects each. With the corrected helper no-throw contracts, the
instruction-mode memory loop and redundant protected-normal case both occupy
402 executable bytes; their full code and relocations match. The unwind versions
both remain 338 bytes with zero logical instruction maintenance. Comparing the
ordinary r211 baseline objects against r216 now gives identical **all executable
sections and complete relocations in both diagnostic modes**. These concrete
programs establish unchanged generated memory/call code, not universal timing
performance or all-platform qualification.

The new actual-runtime traces, objects, commands, baseline objects, source
provenance and retained failed test attempt are archived in
`build/wasm3-evidence/llvm-native-exception-concurrent-trace-codegen-r216.tar.gz`,
SHA-256 `95a98472d21a9e85d782558ae156066d250e6eade3ecfb858828ec95edd713c9`.
