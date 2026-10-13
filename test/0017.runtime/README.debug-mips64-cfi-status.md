# MIPS64 N64 private CFI and native MCJIT foundation

The paired [component record](native_mips64_n64_cfi.qualification.json) and
[final audit retirement](native_mips64_n64_cfi.retirement.json) qualify the
frozen source447 CFI/MC DATA and compiler/ABI fixtures on Linux, using real
MIPS64R2-generic QEMU in the existing 64 GiB cgroup with swap disabled.
They do not qualify a live Wasm physical caller, `bt` or `finish asm`.

| Actual execution | uwvm2 + uwvm2-ros total |
| --- | ---: |
| Private bounded/sparse CFI evaluator on N64 big/little endian | 4 QEMU sessions |
| Exact MCJIT-emitted C ABI object on N64 big/little endian, O0/O3 | 8 QEMU sessions |
| Genuine LLVM relocated CFI and owned MC decode, O0/O3 | 8 Linux host sessions |
| Restored return rows checked against actual emitted returns | 32 |
| Real Wasm VM sessions | 0 |

The additional [native N64EL-host MCJIT record](native_mips64el_mcjit_cfi.qualification.json)
and [independent audit retirement](native_mips64el_mcjit_cfi.retirement.json)
qualify the genuine MIPS-host SDK and frozen source455 production CFI-fixup
configuration. Both repositories perform code generation, RuntimeDyld loading
and real JIT calls inside a MIPS64R2-generic QEMU process:

| Additional actual N64EL-host execution | Both repositories total |
| --- | ---: |
| Real native MCJIT C ABI/CFI sessions, O0/O3 | 8 |
| Sessions using the production unwind-configuration helper | 4 |
| Actual restored return rows | 32 |
| Real Wasm VM sessions in this component record | 0 |

Frozen source455 used the native-continuation opt-in for the helper. The later
[full VM/CLI baseline](native_mips64el_asm_runtime.qualification.json) freshly
qualifies source471 with the actual owner-table opt-in alone, 12 real Wasm
sessions and four native MCJIT CFI sessions. That helper enables genuine LLVM
CFI-fixup for ordinary Linux N64 R2 while the physical-continuation gate stays
closed. See [current ASM usage and limits](README.debug-mips64el-asm-status.md). Mips16, microMIPS and other ISA revisions
receive no new opt-in here. This option creates no physical caller, native
memory or finish capability. The paired fixture checks the actual target
machine option, restored CFI and exact JIT results 66/0/66. Earlier provisional
receipts with an incorrect source-root field are retained and excluded; the
production configuration was freshly rebuilt and rerun with source identity
verified before admission and after retirement.

The [genuine Ed25519 dependency record](native_mips64el_ed25519.qualification.json)
and [retirement](native_mips64el_ed25519.retirement.json) add two real target
provider sessions. They sign and verify binary test bytes and reject an altered
signature and message. This qualifies the authenticated OpenSSL dependency,
without a Wasm VM or debugger permission.

Each target object executes a real 64-argument call with result 66, an early
return with result 0 and a later framed return with result 66. Every target
executable has a RW GNU stack, without execute permission. The big-endian
oracle links with the actual GNU Scrt1/crti/crtbeginS/crtendS/crtn objects and
LLD; the archived little-endian GNU startup objects require the pinned genuine
GNU cross linker with an explicit non-executable-stack output policy. Neither
startup code nor LLVM libraries are replaced by ABI/signature stubs.
MIPS MCJIT forces static relocation; requesting PIC does not change that fact.

The five host MIPS backend archives are compiled from the pinned
23.1.1-uwvm-ros.11 vendor source. They are genuine x86-64-host compiler
components. The foreign target code executes separately in QEMU. The
standalone metadata owner observes genuine RuntimeDyld object/EH callbacks,
without registering foreign EH with the host unwinder or minting any Wasm
activation, native read, stack borrow or execution event.

The portable async-CFI patch is synchronized with ROS's bundled LLVM. It
records live RA for an entry/frameless leaf, switches CFA from FP to SP at the
actual restore, records each restored preserved register after its actual
load, and restores SP+0 after actual stack adjustment. All conventional N64
async prologue CFI carries FrameSetup, so CFIFixup remembers the completed
prologue before an earlier return and restores the later framed body. The
delay-slot filler cannot move instructions across those CFI boundaries.
Interrupt/EH-return, Mips16 and microMIPS paths do not receive these ordinary
N64 debug frame rules.

The production private CFI parser accepts correct N64 BE/LE metadata and
return column 31. The evaluator recovers s0..s7, gp, fp and ra, with SP derived
from a bounded 16-byte-aligned CFA. It consumes at most eleven explicitly
supplied, aligned eight-byte saved words entirely inside SP..CFA. Duplicate,
overlapping, missing, unaligned and out-of-frame slots, wrapped arithmetic,
unknown CFA/RA and zero/misaligned RA are refused. A real zero-sized leaf
needs a known live RA and performs no saved-word read. Volatile/platform,
FP/MSA and missing columns remain unknown. This is DATA, without native read
or execution authority; preserved GP/FP/RA never become public register data.

Owned MC decoding repairs only the descriptor-proved missing tied input of
standard N64 LDL/LDR. It resolves a unique real GPR64 super-register of the
reported GPR32 alias with the identical hardware encoding. All ordinary
complete-operand and control checks still apply. JR with the real RA operand
is classified as private return DATA; another indirect jump is not a return.
Standard MIPS rejects a PC that is not four-byte aligned. Memory forms and
ABI register rows remain excluded from public disassembly, and these
exceptions do not grant fallthrough stepping or call continuation.

The genuine N64 little-endian MIPS-host SDK and ordinary N64 R2 CFI-fixup
configuration are present. The later [N64EL physical-caller/finish qualification](native_finish_mips64el.qualification.json)
now passes56 real Wasm VM/CLI sessions in both repositories, including28 native
parent returns. Its exact source574 and genuine N64EL SDK scope is separate
from this earlier BE/LE CFI/object foundation.

Big-endian MIPS still needs a genuine target-host SDK and full real Wasm
physical-chain, controller, return/cancellation, numeric, modern-Wasm and CLI
matrix. Its public physical-caller/finish gate remains closed. The current
[BE/LE kernel-return backend extension](native_mips_n64_kernel_return.qualification.json)
now independently passes eight genuine MIPS64R2 QEMU native programs:
both repositories, both endiannesses and O0/O2. They observe16 real parent
return traps,48 recursive wrong-stack traps and48 instruction-successor
rearm traps, plus eight actual running-continuation cancellations. Empty,
wrong-identity, zero-cookie, wrong-descriptor and stale-window return events
are refused. Every owned instruction is restored after the real worker ACK;
GP/SP/FP/RA remain hidden in numeric projection. All eight target ELF stacks
are RW without execute. This test-only issuer executes synthetic functions
and uses its known32-byte child frame: it supplies no genuine Wasm activation
or runtime-CFI authority and qualifies zero Wasm VM/CLI sessions. The original
source574 N64EL VM record is not relabelled as a new current-product result.

The [independent retirement](native_mips_n64_kernel_return.retirement.json)
and [producer retirement](native_mips_n64_kernel_return.runtime_retirement.json)
record four and14 retired PIDFD handles respectively, with zero OOM events.
Four real target-compiler preprocessing runs check all three actual current
controller conditions in both repositories: all12 conditions keep the BE
product gate closed and the previously qualified LE gate enabled, even with
the product macro forced on. This is a scoped condition check, not a full
controller/product build. The current source projection covers the changed
native backend and its exact header/fixture dependency hashes.

All new execution uses the original64GiB Linux cgroup, swap0 and the dedicated
Landlock-enforced tmpfs profile described in [the N64EL notes](README.debug-mips64el-asm-status.md).
No target SDK is replaced by a host library, no public native stack read is
introduced, and no Mips16/microMIPS/R6 path receives the N64 R2 return opt-in.
The [retention record](native_mips_n64_kernel_return.retention.json) and
[retirement](native_mips_n64_kernel_return.retention_retirement.json) archive
all90 completed producer/audit files with their exact bytes. N32/MIPS32, Mips16/microMIPS, R6, native
DAP stepOut, full INT and hardware execution are outside this record.

The independent audit rechecks the exact source, compiler dependency closures,
real SDK/backend archives, package/tool hashes, ELF architecture/endianness,
object and execution logs, success receipts and actual PIDFD retirement.
Earlier failed builds/runs retain failed receipts and contribute no successful
session. The helper's zero public-byte field describes a component that has
no public native interface; it is not a full VM isolation qualification.

The [genuine BE target-host SDK qualification](native_mips64_be_sdk.qualification.json)
and [audit retirement](native_mips64_be_sdk.retirement.json) now establish64
real big-endian N64/R2 LLVM archives with1639 source objects and exact archive
member roundtrips. The captured source is ros.12 plus explicitly recorded
working-tree/async-CFI overlays, including manifest mismatches; it is not
qualified as an unmodified current bundled-LLVM verify_source build.
The [BE native foundation qualification](native_mips64_be_native_foundation.qualification.json)
and [retirement](native_mips64_be_native_foundation.retirement.json) independently
verify eight real MIPS-host MCJIT C-ABI/production-CFI sessions (paired O0/O3)
and two real signed-source OpenSSL Ed25519 providers. Actual C++ compiler
dependency bytes are recorded in addition to the frozen logical source623 map.
The GNU gold consumer keeps genuine startup objects and non-executable stacks;
only the CMake contract's LLD-specific path/color arguments are removed,
with every genuine target LLVM archive token preserved. Failed LLD links
and resource-guard batches remain false. These component records add zero
Wasm VM/CLI sessions or public BE physical-caller/finish qualification.
The test-only BE gate candidate is awaiting a separate full Wasm matrix;
local production gates remain closed. All executions/audits use the original
64GiB/swap0 cgroup and unchanged own-process/resource guards.

The [BE debug-generation qualification](native_mips64_be_debug_generation.qualification.json)
and [independent retirement](native_mips64_be_debug_generation.retirement.json)
now establish four actual ordinary-repository full Wasm physical-caller sessions:
instruction/unwind policies, shallow/deep recursion, exact Wasm-parent joins,
complete depth4 traces and explicit depth32 bounded prefixes. Host frames,
stale proofs, counterfeit aliases, reentry and failed wake transactions are
refused; public native stack bytes remain zero. The pre-fix actual instruction
physical-caller run fails its positive coverage assertion and stays failed.
Both local generators now admit N64/R2 BE debug Async CFI and owned short-edge
emission through the same two guarded paths; no typed-tail ABI gate is changed.
The [local hunk provenance](native_mips64_be_debug_generation.projection.json)
records only those changes and preserves concurrent work. Producer675 retires
six PIDFD handles and independent audit678 retires one, with no OOM events.
This frozen candidate includes the prior test-only BE caller gates; public
production gates are still closed. ROS physical-caller, finish, modern-feature,
numeric-register and CLI qualification are pending this record, which qualifies
zero finish sessions. The [earlier NI-only record](native_mips64_be_recursive_ni.qualification.json)
contains four ordinary-repository sessions before this generation repair;
it supplies no post-repair or ROS coverage.

The [ordinary BE VM prefix](native_mips64_be_ordinary_vm.qualification.json)
and [independent retirement](native_mips64_be_ordinary_vm.retirement.json)
qualify26 ordinary-repository actual Wasm sessions: the independently qualified
four NI and four physical-caller sessions plus18 new finish/modern/numeric runs.
Ten genuine finish sessions complete14 authenticated parent-return hops;
recursive/deep-stack escapes reject wrong SP before continuing real native
instructions. Numeric register runs include actual SI/NI and i64/f32/f64
values, masked upper bits, hidden runtime code and forged/retired identity
rejection. This prefix adds zero CLI or ROS executions. The successful CLI
link has one FastIO install-path header at an older physical path with exact
frozen logical bytes; both copies are materialized for subsequent compilation.
Whole producer681 remains failed on that conservative path guard, and its
completed positive prefix alone is independently qualified by audit682.
The record does not qualify every Wasm3 feature, MSA native v128 registers,
physical hardware, or a normal current bundled verify_source product build.
Public BE caller/finish gates remain closed pending current-product adoption.

The [ordinary BE CLI and ROS object prefix](native_mips64_be_ordinary_cli_ros_objects.qualification.json)
and [retirement](native_mips64_be_ordinary_cli_ros_objects.retirement.json)
extend the independently qualified ordinary result to28 actual VM/CLI sessions,
including two real Rdbg CLI executions. The two ROS runtime/host objects and
private stack-slot component are independently byte-qualified; they alone
supply no ROS VM coverage. Both FastIO install-path leaves now use physical
materialization of the exact frozen logical bytes, with no FastIO source change.
The [ROS NI prefix](native_mips64_be_ros_ni.qualification.json)
and [retirement](native_mips64_be_ros_ni.retirement.json) add four genuine ROS
recursive NI runs across instruction/unwind and normal/cancel policies.
This establishes32 source-identical frozen VM/CLI sessions in total.
Failed whole producers684/687 and incomplete689 remain failed; only separately
audited positive prefixes count. Remaining ROS physical-caller/finish/modern/
numeric/CLI sessions and current-product BE gate adoption are still pending.

The [full BE VM/CLI frozen candidate qualification](native_mips64_be_asm_runtime.qualification.json),
[producer retirement](native_mips64_be_asm_runtime.runtime_retirement.json) and
[independent audit retirement](native_mips64_be_asm_runtime.retirement.json)
now establish56 actual executions,28 per repository. The full source-identical
matrix comprises8 recursive NI,8 physical-caller,20 finish,8 modern-Wasm and8
numeric-register VM sessions, plus4 real Rdbg CLI executions. Finish completes28
authenticated parent-return hops. Both private stack-slot components are also
qualified. This supersedes the earlier remaining-ROS pending statements for
this frozen candidate alone; the first32 qualified sessions are reused without
repetition and producer699 adds the remaining24 ROS executions.

Independent audit700 verifies all12 exact compiled executables one at a time
inside the original cgroup, including big-endian N64/R2 ELF identity, unchanged
archive/member bytes, non-executable stack flags, actual C++ dependencies,
all8139 frozen source files and real positive/negative runtime counters.
Host/VM frames, pointer carriers, forged/stale identities, failed wake transactions
and wrong-SP continuations remain refused; public native stack bytes remain zero.
Producer699 retires17 owned PIDFD handles and independent audit700 retires one,
with unchanged OOM counters and resource guards. The normal O1 SDK C++ profile
is retained: failed reduced-inlining links and storage-guard attempts remain
failed and contribute no successful session.

This is a frozen test-only gate candidate after the paired debug Async-CFI and
owned short-edge generation repair. Local public BE caller/finish gates remain
closed; current controller changes from concurrent work and normal current
bundled verify_source product builds are not qualified here. Native MSA/v128,
every Wasm3 feature, exact EH-triggered NI abandonment, physical hardware and
complete remaining-architecture/product parity are still unqualified.

The [current-source BE application qualification](native_mips64_be_current_product.qualification.json)
and [independent retirement](native_mips64_be_current_product.retirement.json)
now qualify the paired public N64/R2 BE gates against a fresh physical
8224-file source snapshot, including the then-current concurrent controller
changes. All four runtime/host objects and all twelve VM/CLI executables are
freshly compiled; no earlier session or stale object is reused. Both repositories
complete28 actual Wasm VM/CLI sessions:56 total, with8 recursive NI,8 physical
caller,20 finish,8 modern-Wasm and8 numeric-register runs plus4 CLI executions.
Finish completes28 authenticated parent-return hops. Two private stack-slot
components additionally reject wrong widths and stale owners.

The paired production guards now admit BE while retaining pointer width,
N64/R2, mips16/microMIPS exclusions and every owner/epoch/stop/lease boundary.
The typed-tail ABI gate remains unchanged. The actual compiler target chooses
the BE or LE GNU gold basename, ahead of LLVM target and architecture aliases;
all release-family N64 native-debug profiles keep LTO disabled and the final
stack non-executable. The [actual build-profile callbacks](native_mips64_be_current_product.profiles.qualification.json)
cover210 platform cases,168 release-rule cases and12 product-definition cases
in the original cgroup. Unrelated valid release profiles retain their requested
LTO behavior.

Independent audit713 checks source/dependency hashes, positive and negative
runtime counts, all twelve exact executable archives and all four completed
object bytes inside the original cgroup. Host/VM frames, unproved pointer
carriers, forged/stale identities, wrong-SP continuations and failed wake
transactions remain refused; public native stack bytes remain zero. The
ordinary O1 C++ profile and private test witness instrumentation are retained.
This qualifies the current application source gates using the genuine declared
overlay LLVM23.1.1-uwvm-ros.12 SDK. It does not qualify a whole xmake invocation,
normal latest bundled verify_source packaging, native MSA/v128, all Wasm3
features, exact EH-triggered abandonment, physical hardware or every other
architecture against this same current source snapshot.

The earlier frozen candidate and its closed-public-gate statements above are
historical; their receipts remain unchanged. Completed historical source656
and objects684 have exact recoverable cold mappings in owned retirement
profiles703 and701, respectively. Fresh source702 and completed current product
bytes are retained separately. Linux test products remain confined to the
bounded owned tmpfs; executed raw products are retired only after exact
recoverable archive verification.


MIPS MSA frame reader and vector aliases (2026-10-08)

The paired native Linux adapter now recognizes the N64 kernel MSA record only
when USED_FP, USED_FR1 and USED_EXTCONTEXT are set, the hybrid layout is absent,
the record is exactly272 bytes and its fixed END marker is present. The record
starts after the kernel's16-byte signal mask at ucontext+656, rather than after
libc's128-byte sigset reservation. It reads no uc_link, saved SP, record chain,
unknown-sized payload, CSR or padding into the public register projection.
Each architectural64-bit half is normalized separately so byte lanes retain
the same order on BE and LE.

Wasm-v128 locations select the separately captured w0..w31 slots, while
f32/f64 locations retain f0..f31. LLVM MIPS shares DWARF32+n for these aliases;
conflicting scalar and vector roles suppress the W alias independent of order.
The scalar width and its zeroed residual bytes remain the narrower authority.
All new C++ fixture output uses fast_io.

The [component qualification](native_mips_msa_context.qualification.json) and
[independent audit retirement](native_mips_msa_context.audit_retirement.json)
record20 actual target runs in the original64GiB cgroup:16 paired BE/LE C++
ABI DATA/projection runs, plus4 paired BE/LE freestanding R6/MSA assembly
observations. Each of the40 compiler/QEMU command roots has a recorded birth,
cgroup/UID and retired PIDFD. The C++ fixtures test32 vector slots,28 rejected
layouts and10 guarded reads per target/repository. The final ELF images are
independently checked for target endianness, N64 architecture and non-executable
stack. The assembly images are genuine generated R6/NaN2008 ELFs with no libc,
SDK or ELF interpreter; their instruction witnesses prove ld.b/copy_s.d lane
order and their real SIGTRAP handlers observe QEMU's scalar-only USED_FP flag.
Those observations are distinct from the synthetic saved-MSA ABI records.

The attempted R6 C++ provider build failed because the genuine NaN2008 libc
stubs header is absent. Its failed receipt is retained unchanged. The assembly
probe does not bypass or qualify that libc provider. QEMU's observed signal
frame omits the saved MSA upper halves, so it grants no public W-register state.
A real Linux saved-MSA frame at an authenticated Wasm JIT stop, a complete
current VM/CLI regression with MSA enabled, normal latest SDK packaging and
remaining all-architecture/all-Wasm3 coverage are still unqualified. The prior
56-session N64/R2 VM result above remains evidence for its earlier source cut.

Exact source, raw ELF images, logs and positive/failed profiles were archived
and independently transported before any raw retirement. See the
[cold retention record](native_mips_msa_context.retention.json).
