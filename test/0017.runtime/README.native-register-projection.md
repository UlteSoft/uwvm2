# Wasm-only native debugger views

The raw platform register snapshot and complete owned function image are private execution-proof data. Public controller replies carry code identity only, qualified decoded rows and a separate numeric register projection. Native numbers and register values never become memory references or write capabilities.

The compiler emits synthetic numeric variable records for actual validated operand SSA values of i32, i64, f32, f64 and v128. The loaded-object collector accepts only exact private producer/file identities, actual relocated function and lexical ranges, bounded register-only locations, the current function generation and execution epoch. Frame-relative, dereference, pointer/reference, unknown and conflicting locations remain unavailable. Only proved low bits are copied. SP/FP/LR, platform registers, flags, x87 and FP control state remain unavailable. Private snapshots retain those values for authenticated continuation proofs.

Code position and public code qualification are independent: nonzero lines retain Wasm offset + 1; reserved synthetic column 1 marks pure numeric definitions reached from actual numeric operands. Column 0 grants no code display permission. Safe-point packet/poll instructions and unqualified restoration/bridge instructions do not become public rows. Every byte of a displayed instruction must qualify; unknown gaps suppress the remainder of a page. This conservative view can report unavailable native instructions.

The private target description uses the architectural fifteen-byte bound for the actual engine's X86 triple. LLVM23's X86 MCAsmInfo leaves the inline-assembly length field at its four-byte default, which cannot bound a variable-length X86 decoder. Other target metadata retains the existing SDK checks. Every decode still reads only the authenticated owned image and its remaining extent.

CLI spelling is preserved:

```
info registers
info registers $rax
info registers $xmm0
info registers st(0)
info all-registers
info registers all
```

A partial i32 register can appear as `rax=0x????????12345678`; a register with no qualified numeric bits appears as `rsp=unavailable`. This is current physical register data only when the authentic trap and compiler location agree. It is not an operand-stack value presented under an invented register name.

DAP native frames expose a read-only `Registers` scope. Native watch/hover accepts one register selector, including `$pc`, `sp`, `fp`, flag aliases and x87 aliases, and returns the same qualified or unavailable value. Dereference/arithmetic expressions are rejected. Scope IDs are session-unique and bound to the current participant/stop; status is refreshed before and after copying. `ni` and `nexti` retire old references before dispatch, including transport-error paths. No native `memoryReference`, register assignment or native-memory API is provided.

Validation sources:

- `native_register_projection.cc`: poisoned unqualified GPR/FP bits, partial widths, hidden controls/platform registers and raw-snapshot preservation (owned DATA component).
- `native_numeric_identity_origin.cc`: exact compiler identity origin, foreign assembly/pointer exclusion and real LLVM IR verification (compiler DATA component).
- `native_provenance_numeric_mcjit.cc`: actual emitted/loaded DWARF locations for all five numeric types, independent generations and epoch revocation (MCJIT component; does not claim a genuine debugger trap).
- `test_dap_native_registers.py`: read-only scope, aliases, pagination, stale stops, NI aliases/lost replies, invalid expressions and malformed/host-control payload refusal (protocol component).
- Actual runtime/CLI trap tests and fresh runtime/main/host/module consumers are additionally required before claiming complete product qualification. Components and fake protocol packets are not substitutes for those tests.

The Linux X86-64 product build enables this contract with the Xmake option
`--linux-x64-native-debug=y` on an existing LLVM/Clang JIT configuration. It
sets the matching runtime and CLI owner/continuation flags. The SDK must really
emit native-owner-table-v2 records; stock LLVM cannot substitute symbol sizes.
ROS uses its pinned `23.1.1-uwvm-ros.11` producer. This opt-in preserves existing
builds with other SDKs/platforms. The validation build uses those exact flags.

For instruction observations, numeric records are emitted both before and after
a successfully lowered opcode; post-op records share the actual instruction's
lexical scope. The Linux debug-only shutdown path reads the same qualified
process-lifetime lock-free atomic byte as a relaxed hint. It consumes no
published request state and avoids unnecessary acquire barriers on other
architectures. When clear it skips the no-op host poll; when requested the real
host poll rechecks with acquire semantics and authenticates ownership under
its mutex. It retains the original throwing
Invoke and cleanup before a Wasm effect. The private flag/address and every
poll instruction remain unqualified scaffolding. Normal execution emits none.
Actual parked-loop/host-block shutdown tests across none/observe/resumable
profiles are required alongside native positive tests.

Linux debug builds also materialize validated numeric operands in supported
target-specific physical register classes after cooperative host calls. A compiler-owned empty tied-output
inline-assembly identity preserves the original numeric bits; register
allocation inserts any required real copy/reload. Only numeric Wasm types enter
this path. The original pure numeric definitions and this exact compiler-created numeric
identity share numeric code provenance. Other inline assembly has no such
origin. A zero-byte identity must not insert a conflicting column-0 record at
the following numeric ALU address. All native loads/stores and SP/FP operands
remain hidden by the independent public MC filter; no spill location grants a
read capability. Neither guest
scripts nor debugger requests can supply assembly, a register constraint or a
native address. Normal execution does not emit these debug identities.

`debug_native_public_registers_runtime.cc` requires real kernel traps, visible
numeric code, hidden runtime bytes, authentic numeric bit patterns, retained
refusals and expired/foreign-stop rejection. Its `all-numeric` case uses
`debug_native_public_numeric_runtime.wat` and requires actual positive physical
i64/f32/f64/v128 values as well as i32. Both instruction and unwind stack
policies must pass. The nearest FP/next boundary tests require real NI/nexti
execution, qualified executed bytes and SI call/caller refusal; unavailable
scaffolding can no longer be decoded by borrowing the removed raw reply fields.

A line-zero allocator spill can still have a genuine typed numeric DWARF
register location. Register projection uses the separate private numeric owner/lexical/location
ranges even when source position rows are zero or overlap after allocation.
Every numeric record must match the actual owner and exact private file;
foreign/missing locations and expired epochs remain unavailable.
This does not qualify a single byte of scaffold code for public display.

The same boundary covers stack and memory. A native stop publishes no raw stack,
CFA, return address, saved caller register set or spill-area bytes. Its old
cooperative trace/local packet is retired; DAP shows one authenticated current
JIT instruction label rather than inventing a caller stack. Register values,
PCs and code references cannot be passed to readMemory/writeMemory or converted
into native stack addresses. Register-only DWARF locations grant no spill/frame
read authority; unproved stack content remains unavailable.

The ordinary memory command is a separate Wasm linear-memory operation: module,
memory index and logical byte offset, followed by current extent checks. It
never accepts a native address, SP/FP/CFA offset or process-memory reference.
JIT ownership of a whole physical frame is insufficient to prove every byte:
VM spills, padding, return addresses and host data may share it, so the frame is
never dumped wholesale. Any future stack/native-data view must qualify each
field's Wasm type, storage extent and live stop/owner/generation before copying.
The physical-stop test rejects attached old stack/memory data; DAP tests reject
native stack/code/register memory references and cached cooperative caller rows.

Linux native adapters also cover AArch64, i686, PowerPC (32/64, both PPC64 byte
orders), MIPS64 n64 (both byte orders), SPARC64, LoongArch64, RV64, SystemZ and
ARM-state ARMhf. Non-X86-64 adapters execute one actual instruction between
software breakpoints and copy registers from the selected thread's real
kernel signal frame. A sealed runtime plan must prove the current owned body,
trap revision and every possible successor before executable bytes change.
Calls, returns, unsupported indirect/delayed branches and self-loops are refused
when their complete continuation is not proved. SPARC V9 direct delayed branches
also authenticate the real kernel PC/NPC pair: both possible successors, the delay
slot and its pending continuation must belong to the same exact loaded Wasm body.
Annulled paths are proved separately; indirect/nested control transfers remain
refused. A real kernel trap before a delay slot does not fabricate an already
executed instruction. Cancelling restores the exact original bytes and waits for
the actual worker to retire its signal-frame borrow.

Use the opt-in xmake option `--linux-native-debug=y` with a Linux LLVM/Clang
build and the qualified native-owner-table-v2 SDK. The previous
`--linux-x64-native-debug=y` spelling remains limited to X86-64. Signal ABI
support does not enable an incomplete MCJIT object loader. PPC32 and SPARC V9
additionally require the revision-11 patched library's actual capability query,
qualified relocation handling and a native execution test. Linux PPC64 ELFv1
and ELFv2 also require the PowerPC CodeGen archive's compiled TOC-tail ABI query.
Their private C entries force normal calls through the global-entry sequence
with an actual caller TOC restore; exact nonvariadic musttail transfers reuse the
incoming argument area without overwriting that original save slot. Unmarked
LLVM functions retain the original TOC eligibility checks. Stock SDKs remain
rejected on those paths. ARM software breakpoints require ARM-state generated
code; Thumb code is refused before patching.

The compiler uses target-specific tied register identities for actual numeric
operands where the target has a supported physical register class. The target
assembler parser is registered and linked explicitly, including static LLVM
consumers, so the real consuming marker can be encoded. PPC64 widens an i32
numeric witness to a zero-extended full X register before the consuming marker;
the public location still qualifies only the low 32 bits. This avoids accepting
DWARF bit-piece or native spill expressions as register authority. Stack spills,
frame expressions, split/bit-piece locations and absent vector classes still
have no public read capability. `native_step_linux_software.cc` verifies actual
instruction effects and kernel GPR/FP capture under QEMU, including stale plans,
forged session identities, ready cancellation and genuine worker retirement.
Its private test issuer owns an isolated executable fixture page; passing that
fixture is explicitly separate from `debug_native_linux_runtime.cc`, which uses
the real Wasm validator, compiler, loaded owners, controller and runtime issuer.
`native_linux_register_projection.cc` checks bounded multi-byte DWARF register
numbers and poisoned unqualified bits. Cross-target qualification must record
both scopes separately; a backend fixture cannot certify a whole JIT product.

The [Linux qualification record from 2026-10-05](../0022.qemu_platforms/linux_native_step_qualification_20261005.json) records the exact source and binary cuts, genuine backend tests, complete Wasm runtime integration tests, original cgroup receipts and execution replays. It also lists loader and product qualification gaps; the two scopes must remain separate.

The revision-11 PPC32/SPARC V9 loader's real ELF relocation/remapping DATA
checks and Release overflow negative controls are recorded in
[the loader component qualification](../0022.qemu_platforms/linux_native_loader_data_qualification_20261005.json).
They run inside the original Linux cgroup and do not replace QEMU target-native
MCJIT execution, real Wasm owner/issuer/controller/register projection or CLI
qualification. `native_mcjit_elf_loader.cc` is the separate target execution
oracle; `native_elf_relocations.cc` checks remote mapping and relocated bytes.

The additional linked-library capability ABI positive checks and stale archive
link rejection, along with the Release ELF DATA regressions, are recorded in
[the loader capability qualification](../0022.qemu_platforms/linux_native_loader_capability_qualification_20261005.json).
The compiled RuntimeDyld library must supply the capability query; header macros
alone cannot enable either target. This remains a component qualification.

The internal resumable entry is retained in `llvm.compiler.used` after exact
function verification. `NoInline`/`OptimizeNone` alone do not prevent GlobalOpt
from changing its private C calling convention. Retention keeps the authenticated
typed continuation ABI intact; it creates no public register, address or memory
permission. Actual none/observe/resumable parked-loop and host-block retirement
checks are part of product qualification for both stack policies.
