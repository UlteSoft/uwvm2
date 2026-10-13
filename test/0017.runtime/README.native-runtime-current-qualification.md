The MIPS64EL full LLVM [VM/CLI baseline](native_mips64el_asm_runtime.qualification.json)
now qualifies 12 real Wasm sessions across both repositories: eight numeric
runtime groups and four real `-Rdbg` CLI processes. Native SI/NI and typed
numeric values are positive; ABI/VM code, pointer registers and stack bytes
remain hidden. Its [usage and exact limits](README.debug-mips64el-asm-status.md)
keep physical caller/finish separate from this baseline. All runs use the
original SSH Linux 64 GiB cgroup with swap disabled and zero OOM events.

MIPS64EL's [full source574 qualification](native_finish_mips64el.qualification.json)
now passes 56 actual VM/CLI sessions across both repositories, with 28 real native
parent returns: eight physical caller, twenty finish/deep-finish, eight recursive
NI, eight modern Wasm NI, eight numeric and four real `-Rdbg` CLI sessions.
Private stack 2/target MC 6/parser 1 components are counted separately. Public
native stack bytes remain 0; VM code, pointer registers, forged callers and stale
stops retain their refusals. Independent 575 verifies 21 completed 574 +1 completed 591
 + 34 successful 596 executions while preserving every failed aggregate receipt.
The source 503 audit 567's 24 sessions remain a separate earlier qualification.

The current [build wiring](native_mips64el_build_wiring.qualification.json)
fixes MIPS64EL target recognition, the LoongArch `loong64` continuation alias and
explicit target-gold selection; four Lua syntax checks/fourteen configuration
cases/two actual compiler-driver selections add zero VM sessions and do not
claim a full current-workspace xmake build. Ordinary BFD link594 still fails
GOT_PAGE; the verified remaining 34 links use genuine gold 2.40, explicit real
N64EL libatomic.a and noexecstack. Runtime source 574 and new build-config hashes
stay distinct. Original64 GiB/swap 0 cgroup tests have zero OOM events; the authorized
future shared-folder ceiling is40 GiB and every other bound remains unchanged.
Completed raw objects are recoverably retired only after the full independent
byte audit; detailed scope is in [the MIPS execution notes](README.debug-mips64el-asm-status.md).

MIPS64EL now also has a genuine MIPS-host SDK and production CFI-fixup
[MCJIT component record](native_mips64el_mcjit_cfi.qualification.json): eight
real target-host JIT C ABI/CFI sessions, four using the production configuration,
and 32 actual return rows. Its [Ed25519 dependency](native_mips64el_ed25519.qualification.json)
has two real target provider sessions. These records qualify zero Wasm VM
sessions; executable physical caller/finish progress is recorded separately. Their scope and excluded
provisional receipts are documented in [the MIPS notes](README.debug-mips64-cfi-status.md).

MIPS64 N64 BE/LE has a separate paired
[CFI/MC and real target-object foundation](native_mips64_n64_cfi.qualification.json)
and [scope/boundary notes](README.debug-mips64-cfi-status.md): twelve QEMU
components, eight genuine relocated-CFI sessions and 32 restored return rows.
This is zero actual Wasm VM sessions; it does not qualify the later N64EL
physical-return implementation or a big-endian production return gate.

The later [current N64 BE/LE kernel return component](native_mips_n64_kernel_return.qualification.json)
passes eight genuine target QEMU programs across both repositories and O0/O2:
16 actual parent traps,48 wrong-stack recursive traps,48 successor rearm traps
and eight running-continuation cancellations. Its issuer is test-only, the
functions are synthetic and its32-byte frame formula is not production CFI.
It qualifies zero Wasm VM sessions and no genuine BE target-host SDK or full
BE product gate. Four real target preprocessing runs independently check12
current controller conditions and retain the public BE gate closed.
Both [producer](native_mips_n64_kernel_return.runtime_retirement.json) and
[independent audit](native_mips_n64_kernel_return.retirement.json) retire their
recorded handles with zero OOM events. Exact source scope and recoverable
byte archives are described in [the MIPS component notes](README.debug-mips64-cfi-status.md).

The separate [MIPS64EL release-LTO regression](native_mips64el_release_lto.qualification.json)
closes the default release-rule/gold mismatch in both repositories:156 actual
rule callbacks, two real old-default LTO failures, six genuine target links
and six QEMU native C++ FastIO executions. It adds zero Wasm VM sessions.
These dedicated tests enforce read-only persistent inputs with actual kernel
Landlock and write test outputs only to owned tmpfs; they do not qualify the
original writable-batch disk-floor/shared-folder ceiling. All tests retain
the original64GiB/swap0 cgroup and52GiB memory/6GiB own-RSS/512MiB tmpfs bounds.
They do not claim a complete current-workspace xmake product build.

Current LoongArch64 full LLVM physical-caller and software-finish closure: see
[native_finish_loongarch64.qualification.json](native_finish_loongarch64.qualification.json),
[usage and boundary notes](README.debug-loongarch64-finish-status.md) and the
[final audit retirement receipt](native_finish_loongarch64.retirement.json).
Both repositories pass 52 actual QEMU Wasm sessions: 20 finish fixtures,
28 real return hops, eight physical-caller sessions and four actual `-Rdbg`
CLI sessions. Twelve components are counted separately. Real recursive NI,
i32/i64/f32/f64/v128 registers, modern Wasm return/exception and mixed
cancellation paths pass under both call-stack policies. Native stack public
bytes remain zero. The record preserves every failed resource admission,
exact successful-execution reuse and the final aggregate folder-policy change;
it qualifies source 393 and its genuine repaired LoongArch LLVM SDK only.

Current i386 full LLVM physical-caller and software-finish closure: see
[native_finish_i386.qualification.json](native_finish_i386.qualification.json)
and [its usage and boundary notes](README.debug-i386-finish-status.md).
Both repositories pass 52 actual QEMU Wasm sessions, including 20 finish
fixtures, 28 real return hops, eight physical-caller sessions and four actual
-Rdbg CLI sessions. Numeric GPR/SSE2 i64/f32/f64/v128 and modern Wasm NI
return/exception paths are separately exercised. Ten components are counted
separately. The frozen production and test-only assertion overlay have distinct
hashes; concurrent workspace changes are recorded separately.

The earlier [i386 CFI/private-stack foundation](native_i386_cfi_stack.qualification.json)
retains its 24 component/regression executions, exact four-byte reads and
high-address pthread-stack checks. Those historical tests add no live Wasm
session to the later full-product count.

This file preserves the historical R3 qualification plan. Later scoped Linux
software call/step results are recorded in
[native_call_linux_sparc_rv_lifecycle.qualification.json](native_call_linux_sparc_rv_lifecycle.qualification.json)
and [native_call_linux_software.qualification.json](native_call_linux_software.qualification.json).
RISC-V software `finish asm` has its separate current execution record in
[native_finish_riscv64.qualification.json](native_finish_riscv64.qualification.json).
The AArch64 full LLVM physical-caller and software-finish closure is recorded in
[native_finish_aarch64.qualification.json](native_finish_aarch64.qualification.json):
52 actual Wasm sessions in both repositories, 20 finish fixtures, eight physical
caller sessions and four actual `-Rdbg` CLI sessions. Its frozen-source and
QEMU scope is separate from full INT and other targets. CFI/MC DATA runs are
listed separately from runtime execution.
Full INT startup isolation has its own AArch64 record in
[native_interpreter_boundary_aarch64.qualification.json](native_interpreter_boundary_aarch64.qualification.json).
Both products actually execute the scalar/memory fixture normally and reject
the LLVM-only `-Rdbg` entry with exit126. This verifies the startup boundary;
it does not qualify a live full INT debugger or native Wasm instruction stops.
These records do not change the historical R3 results or qualify every platform.

The lifecycle record's current RV64 vector-model closure passed 16 NI/modern
Wasm sessions and 16 software finish sessions across both repositories and both
call-stack policies, including the 64-parameter/70-frame recursion case.
Batch 102 independently audited exact source, SDK, logs, process retirement and
archived dependencies. This closes that scoped delta; historical R3 results
and other targets' full-product requirements remain unchanged.

This plan separates the current source/API build from actual native behavior.
It supersedes no historical failure or R3 result and expands no platform gate.
The sole Linux keeper runs all builds and tests inside the existing 64 GiB
cgroup; the standing QEMU owner controls Windows guests in that same cgroup.
Do not run local compilers, guest code, QEMU, VM or native trap tests. Mac native
work remains blocked until the parent accepts actual Linux memory measurements
and a preventive local 4 GiB limit. Cross-object MachO reading is not Mac native.

The root's current R3 immutable cut predates the isolated `native_disassembly.h`
`cstr_len` delta. Preserve the old cut and its actual outcome. The post-cut
delta requires a separately pinned header/module/display build; never overlay
the old source and keep its binary qualification label. Keep ordinary/ROS,
EH/no-EH, target/SDK/provider/runtime ABI and layout macros explicit in every
build record. Current native session contains copied GPR data; observer now has
`on_close`, and generated locals have availability bytes. Fresh producer,
runtime, CLI/main and host consumers must use one coherent ABI source closure.

| Scope | Existing/new actual source | Required evidence |
| --- | --- | --- |
| Linux hardware adapter | `native_step_linux_x86_64.cc` + current platform/native-register headers | Fresh build and real zero/one/second instruction traps; actual thread/owner/PC and 18 GPR copy; forged/first-gate/retired rejection; release then handler drain then clear; signal/race tests. No old R3 binary substitution. |
| Windows hardware adapter | `native_step_windows_raii_x86_64.cc` + QEMU owner's four fresh EH/no-EH product artifacts | Exact current COFF/PE/import/bridge acceptance, pinned runtime DLL closure, suspended owned-job admission, regular output, actual guest TF traps/GPR/RAII/death/race results. One ROS EH COFF or PE is not four-product qualification. |
| Bounded compiler metadata | `native_provenance_metadata.cc`, `native_provenance_sections_metadata.cc` | Actual LLVM verifier, emitted IR/object, target DWARF rows and nonzero section offsets; no debug metadata on normal-full modules. |
| Loaded code ownership | `native_provenance_loaded_rows.cc`, `native_provenance_mcjit_owner.cc`, `debug_native_provenance_runtime.cc` | Actual per-section loaded addresses, owned relocations, function/gen/epoch and stopped private capture; replacement invalidates old map; unknown/ambiguous stays explicit. DI rows never grant guest local/stack values. |
| LLVM MC DATA | `native_owned_instruction_semantics.cc` and existing `native_instruction_semantics.cc` | Actual LLVM target/header/archive closure, real ordinary/call/return/branch/trap decode plus invalid/prefix/system/flag-restoring refusal. Actual AArch64 SoftFail status differs from synthetic generic-status checking. No native instruction executes in these components. |
| Native controller lifecycle | `debug_controller_closed_domain_runtime.cc/.wat`, `run_debug_controller_closed_domain.py` | Each normal case first proves actual trap/GPR; direct reset/stop happen with the trap retained and no preliminary detach. Closed-domain reads fail. Actual release/drain/clear precedes execution drain; guest joins with Core 3 `return_call` result 13; observer lifetime retires at reset. |
| Recursive host-close negatives | Same controller fixture/runner, `recursive-reset` and `recursive-stop` | Genuine trap proof and exactly one callback marker, expected SIGILL/SIGTRAP/SIGABRT. Timeout, SIGSEGV, recursion, callback return or normal-success text all fail. This deliberately invalid trusted host observer is not a guest capability. |
| Native `ni/nexti` | Wasm owner's `debug_native_next_runtime.cc/.wat` and runner | At least one actually executed ordinary fallthrough and one actual call/return/branch refusal; true PC/GPR/new-stop versus unchanged retained stop. Wrong/stale identity and script rejection. Never substitute DATA classification for execution. This baseline fixture does not qualify returning-call NI or finish; use the later scoped records linked above. |
| Named-module native API | New `debug_native_module_capsule.cppm`, `debug_native_module.cc` | Fresh actual production partitions and import-only consumer, both no-LLVM model and actual LLVM profiles. Header success cannot prove imports/exports. The focused capsule does not qualify the complete primary/controller/runtime or kernel traps. |

Run the existing controller lifecycle runner against the fresh host fixture and
an independently reviewed build record. Its interface is `--source-root`,
`--binary`, `--binary-sha256`, `--qualified-build-record`, `--wasm-tools`, `--out`
and optional bounded `--race-repeats`. It checks the cgroup and official WAT
parse/validate, then separately tests instruction/unwind policies, baseline,
direct reset/stop, recursive reset/stop, and delayed real close races. Keep all
raw commands, logs, return codes, source/binary/build-record hashes and failures.
Exit 77 means unavailable, never passed. Race elapsed time does not prove which
request/arming/trap phase won; do not silently invent exact phase coverage.

For named modules, start a fresh explicit output directory and pin compiler,
standard library, fast_io, SDK/provider and macro inputs. Disable implicit
modules. Build the fresh fast_io PCM first. Under the real product primary
module name, build `native_registers`, `native_instruction_semantics`,
`native_disassembly`, then `native_disassembly_window`,
`native_owned_instruction_semantics` and `native_next_policy`. `native_step`
imports `native_registers`; Windows additionally needs actual
`native_step_windows`, Mac actual `native_step_macos`, before the facade.
Compile the focused capsule against only those explicit name-to-PCM paths;
compile/link the matching PCM objects and import-only consumer using the same
profile. Compile the full production aggregate independently when qualifying
the product. Record actual dependency files; neither a previous PCM nor the
focused capsule can replace current aggregate or runtime ABI evidence.

The capsule checks copied model layout/aliases separately from actual LLVM MC
ordinary/call/display/window DATA. It does not install a signal/VEH/Mach handler,
create any secret stop capability, touch a native context, dereference a display
PC, or execute a machine instruction. Its clear output states these limits.
Linux AArch64/RISC-V/i386 QEMU user components and absent-target/libc closures
remain their own matrix entries; they cannot prove x64 Windows or Mach kernel
debugging. The pending MachO local-relocation candidate is still unapplied and
needs its isolated LLVM object/DWARF test before any vendor application.
