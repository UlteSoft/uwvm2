# MIPS64EL Linux ASM debugger execution record

The [full VM/CLI baseline](native_mips64el_asm_runtime.qualification.json)
qualifies frozen source471 with a genuine N64 little-endian LLVM SDK on
MIPS64R2-generic Linux QEMU. Both repositories were rebuilt with the actual
`linux-native-debug` owner-table opt-in
(`UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2=1`), without the physical-continuation
product macro. Its independent retirement records remain separate from later
source cuts.

| Baseline execution across both repositories | Sessions |
| --- | ---: |
| Real scalar/numeric Wasm VM, instruction/unwind policies | 8 |
| Real `-Rdbg` console processes, instruction/unwind policies | 4 |
| Genuine native-host MCJIT CFI, O0/O3 | 4 |
| Genuine target Ed25519 provider | 2 |

The baseline proves native SI/NI, copied instruction bytes and positive
i32/i64/f32/f64 values, including wide i64 values. Numeric availability follows
the typed Wasm value and actual kernel trap. ABI pointers, unknown values and
residual high bits remain hidden. Wasm SIMD executes through scalarized code;
this R2 model has no qualified physical v128 register class.

Raw-address disassembly, stale stop identities and invalid threads are refused.
Memory/address forms and VM scaffolding expose no instruction bytes.
`zero`, `k0`, `k1`, `gp`, `sp`, `fp`, `ra`, `status`, `hi` and `lo`
remain hidden. No public native stack-byte, raw-memory or VM-register reader is
provided. An unproved instruction retains its exact stop. CLI sessions must
close with `quit` and actual exit0.

For a qualified full LLVM build, start ordinary uwvm2 with:

```sh
uwvm -Rdbg -Rcc jit -Rcm full -Rct 0 -Rllvm-call-stack instruction -Rllvm-cache-path disable --run program.wasm
```

For uwvm2-ros, use its full LLVM selector:

```sh
uwvm -Rdbg -Raot -Rct 0 -Rllvm-call-stack instruction -Rllvm-cache-path disable --run program.wasm
```

Use the actual thread and stop IDs printed by that process:

```text
break 0 0 0
continue
wait
step asm THREAD
ni THREAD
info all-registers
disassemble THREAD STOP_ID 1
```

When the current native instruction cannot be contained, `step wasm THREAD`
continues through a real Wasm safe point. `quit` retires the session.
Executable physical finish additionally requires the physical-continuation
product build macro; a baseline build cannot establish that feature.

The earlier source 503 physical-caller/finish matrix has completed an independent
audit in batch 567: 24 actual VM sessions across both repositories, eight physical
caller cases, sixteen finish cases and twenty genuine parent return hops.
Two private eight-byte stack-slot components are counted separately. This audit
combines sixteen completed 555 rows with eight actual 563 ROS finish runs;
the original whole 555 memory-limit failure remains false. These older sessions
are not added to the current source 574 count.

The synchronized source 574 changes address the genuine recursive-NI failure.
LLVM's typed-entry acquire load emits N64 `SYNC 0`. It is now admitted only as a
bounded memory-order operation and remains hidden from public disassembly.
Nonzero SYNC, SYNCI, WAIT, SYSCALL, BREAK, ERET and DI remain refused.
The NI fixture tries NI first at each genuine native stop and uses a private
return-event counter to distinguish real calls. It still requires actual
recursive descendants and equal owned-escape counts.

Six genuine target MC component runs in571 reproduced the preimage refusal,
checked the candidate and passed the full positive/negative instruction suite
in both repositories. They execute zero Wasm VM sessions. The corrected modern
NI output parser also passes a separate 554 data component over eight
authenticated historical LoongArch64 logs, retaining all ten required counters,
legacy compatibility and missing-counter refusal; that contributes zero new
VM sessions.

The [full source574 qualification](native_finish_mips64el.qualification.json)
and [progress record](native_mips64el_physical_return.progress.json) now establish
the complete paired matrix on MIPS64R2-generic Linux QEMU, N64 little endian:

| Actual execution across both repositories | VM/CLI sessions |
| --- | ---: |
| Physical caller, shallow/deep, both call-stack policies | 8 |
| Finish normal/recursive/cancel/mixed-cancel, both policies | 16 |
| Finish with 64 arguments and 70 recursive frames, both policies | 4 |
| Modern Wasm NI return/exception: GC, memory64, return_call, try_table | 8 |
| Recursive NI normal/cancel, both policies | 8 |
| Scalar/all-numeric native SI/NI/registers, both policies | 8 |
| Real ordinary andROS `-Rdbg` CLI processes, both policies | 4 |
| Total | 56 |

The finish cases complete 28 genuine native parent returns. Both repositories'
normal recursive-NI policies prove real returning calls and equal descendant/
owned-escape counts. Private stack-slot components 2, genuine target MC components 6
and the parser data regression 1 are separate from the 56 VM/CLI count. Every
private read remains width/owner bounded; public native stack bytes remain 0.
Host-boundary, forged-alias, stale-stop and closed-domain refusals stay asserted.
The [runtime retirement](native_finish_mips64el.runtime_retirement.json) and
[independent audit retirement](native_finish_mips64el.retirement.json) preserve
successful original-cgroup execution and zero OOM/oom-kill events.

Independent audit 575 verifies the actual 21 completed 574 rows, one completed 591
modern row and 34 subsequent 596 executions, without adding duplicate VM sessions.
Both runtime/host objects in each repository were genuinely freshly compiled
against this frozen source. The original 574 shared38 GiB failure and 591 shared
52 GiB memory-guard failure remain false. Separate completed-build/modern audits
586/592 authenticated their successful partial rows before reuse. The older24
source 503 sessions are not added to this source 574 matrix.

Section flags alone do not solve the large N64 consumer's local GOT: the actual
ordinary numeric BFD link 594 still overflows `R_MIPS_GOT_PAGE`. The remaining 34
executions succeed with genuine Debian target gold 2.40 and the same source 574/
Clang compile profile. The first 22 BFD-linked sessions and 34 gold-linked sessions
retain distinct link records. All twelve ELF archives have independently checked
N64EL identities and `PT_GNU_STACK` flags6 (read/write, no execute).

Current build wiring is synchronized in both repositories. MIPS64EL debug
targets select `-fuse-ld=gold --ld-path=mips64el-linux-gnuabi64-ld.gold`, preserve
`-z noexecstack` and collect sections. The target gold binary must be installed
or available through Clang's search path; selecting only the linker flavour is
insufficient. The current release-family rules automatically disable LTO
for this MIPS64EL native-debug gold profile, while retaining other targets'
existing LTO settings. The separate [release-rule qualification](native_mips64el_release_lto.qualification.json)
and [independent retirement](native_mips64el_release_lto.retirement.json)
now verify six full Lua syntax files, 156 actual release-rule callbacks, two
real old-default LTO link failures and six target-gold links followed by six
actual MIPS64R2 QEMU native C++ executions using FastIO. All six ELF images
have RW, non-executable stacks. These are zero new Wasm VM sessions and no
complete current-workspace xmake product build.
The missing genuine compiler-side LLVMgold.so explains the old failure; the
new policy is checked for all three release modes and preserves unrelated
target/LTO settings. Source 574's VM qualification remains separate.

Failed admission607 created no child below the original32GiB disk reserve;
dedicated tmpfs preparation608 likewise created no child above the existing
persistent40GiB shared-folder ceiling. Successful replacement609 uses actual
kernel Landlock ABI7 to permit test-file writes only in its owned tmpfs and
/dev/null. It proves persistent input write-open/create refusals, child-exec
inheritance and positive tmpfs write/read/delete, all in the original64GiB
cgroup with swap0. This distinct read-only-persistent-input profile does not
qualify the original disk reserve/shared-folder ceiling; existing persistent
occupancy is recorded and it adds no persistent test products. The52GiB group
memory,6GiB own RSS, single-compiler and512MiB combined tmpfs bounds remain
enforced. The [producer retirement](native_mips64el_release_lto.runtime_retirement.json)
records12 retired PIDFD handles and zero OOM events. Initial independent610
failed on an audit field-name mismatch, with both handles retired; corrected
independent611 passes without new compile/QEMU/VM execution. The earlier
build-wiring record covers the pre-change files only. The [retention record](native_mips64el_release_lto.retention.json)
and [retirement](native_mips64el_release_lto.retention_retirement.json)
preserve all106 completed and failed evidence files byte-exactly. Use a complete genuine target sysroot. The isolated test
sysroot has a pre-existing dangling `libatomic.so`; these verified links
explicitly use its genuine N64EL GCC13 `libatomic.a`, with no shared sysroot edits.
MIPS64EL `target`/`llvm-target` now enables physical continuation even when the
configured architecture is `mips64`; the existing LoongArch `loong64` alias is
also recognized by the continuation gate.

The [build-wiring record](native_mips64el_build_wiring.qualification.json)
covers four complete Lua syntax checks, fourteen production-platform/scoped-gate
configuration cases and two actual Clang target-gold selections, all in the
original cgroup. It adds zero VM sessions and does not claim a complete xmake
product build. Its current workspace file hashes are separate from source 574's
runtime qualification.

Future admissions use the human-authorized 40 GiB shared-folder ceiling.
The original 64 GiB cgroup, swap 0, own 4 GiB cap, disk 32 GiB reserve, shared-memory
52 GiB guard, own-RSS 6 GiB guard, single compiler and combined 512 MiB tmpfs bound
remain enforced. The historical cache-reclaim record 593 changes none of these
limits or peer processes. Failed admission 603 created no child when disk reserve
briefly fell below 32 GiB; successful replacement 604 kept every original bound.

The [retention record](native_finish_mips64el.retention.json) records177,275,565
raw object/dependency/private-slot bytes retired after the full audit and exact
archive-member verification. All twelve actual VM/CLI ELF images, both pairs of
runtime/host objects and historical logs/metadata remain recoverable with exact
restoration maps. The certified 64 LLVM libraries, include headers and immutable
source 574 remain live. A 229,897,342-byte inactive local archive moved back to the
bounded Linux retained folder only after whole-byte verification, relieving Mac
disk pressure. The failed local save left the previous source bytes intact.
No peer files or processes were removed; rerunning historical audits may require
restoring certified raw paths from the recorded archives.

Big-endian full VM/CLI, full INT native debugging, native DAP, R6/compressed
MIPS and real hardware remain outside these scoped records. Exact EH-caused NI
abandonment is not qualified by an exception-fixture observation alone. See also
[the CFI/component scope](README.debug-mips64-cfi-status.md).

The [paired Linux MIPS alias qualification](native_linux_mips_alias.qualification.json)
and [independent retirement](native_linux_mips_alias.retirement.json) cover the
exact current Linux platform script after adding `mipsel` and `mips64el` to
allowed architectures and to the LLVM MIPS tail-call/GOT/section flag path.
Both repositories pass192 actual Lua5.4 callbacks across eight architectures,
LLVM/non-LLVM, native-debug on/off and three release modes, plus four actual
pre-fix alias controls. The N64EL native-debug gold/NX/GC policy is preserved.
This is platform configuration coverage: it adds no whole xmake product,
compiler/QEMU/Wasm session or big-endian public continuation qualification.
The producer and independent audit run in the original64GiB/swap0 cgroup
with inherited Landlock, one retired PIDFD handle each and zero OOM events.
Earlier release-rule/build-wiring records retain their original source hashes.
