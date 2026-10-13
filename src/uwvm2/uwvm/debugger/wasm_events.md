The native management console now accepts bounded Wasm before-instruction
catchpoints, event traces, and command scripts. Source and machine views share
the same authorized controller; guest code cannot open a management endpoint.

```
catch wasm throw 0 all
catch wasm gc 0 3
trace wasm on memory
continue
wait
trace wasm read
trace wasm read 128 32
info wasm-events
disable wasm-event 1
wasm-script trace wasm clear; trace wasm on gc; info wasm-events
ptype object
info registers $pc
```

`EVENT` accepts `all`, `throw`, `exception`, `gc`, `memory`, `table`, `atomic`,
`call`, `reference`, `simd`, and `control`. Module/function numbers are Wasm
registry indices. These observations occur **before an emitted instruction**:
a throw has not yet thrown, a load/store can still trap, `try_table` is not a
successful catch, and a GC opcode is not a collector event. This distinction
follows the [Core 3.0 binary instruction definitions](https://webassembly.github.io/spec/core/binary/instructions.html).

The design borrows event catchpoints and bounded target trace collection from
[GDB catchpoints](https://sourceware.org/gdb/current/onlinedocs/gdb/Set-Catchpoints.html)
and [GDB tracepoints](https://sourceware.org/gdb/current/onlinedocs/gdb/Set-Tracepoints.html),
and before-opcode filtering from [Whamm's instrumentation rules](https://github.com/ejrgilbert/whamm).
[Wasmtime's guest debugging API](https://docs.wasmtime.dev/api/wasmtime/struct.Config.html#method.guest_debug)
also distinguishes precise instrumented guest state from best-effort native
state. The normal full-JIT path does not acquire this controller or emit these
debug observations.

The callback writes fixed storage only: 512 newest trace records and 256
catchpoints. `trace wasm read [AFTER_SEQUENCE [COUNT]]` returns at most 128
records (also the default), oldest/newest labels, `next-after`, `remaining`, and
`cursor-gap`. Use the returned `next-after` as the next page's cursor. A stale
cursor reports a gap when older records were overwritten; clearing never reuses
a sequence number. Each page reads one coherent ring snapshot while new guest
events may arrive between pages. Sequence cursors confer no execution/read
authority. Replies report overwritten entries. Event sequence numbers do not
wrap; clearing the ring does not reuse them. Trace records contain integer
locations and copied opcodes, never raw reference/native addresses. Classification
uses an immutable expression copy minted from the actual private full-JIT
publication, matching runtime epoch and function generation. Each function copy
is at most 64 KiB and the module copy budget is 8 MiB; unavailable copies are
explicit, and source metadata remains usable. The trusted callback supplies the
instruction boundary. No alternate body validation pass is performed.

An ABI-compatible replacement retires old source/DWARF authority. Wasm event
classification may refresh from the authenticated body only after real
compilation/commit succeeds and the new published safe-point generation matches.
Historical trace records keep their original generation labels. Native register
queries require the real current trapped session and top frame; `info registers`
uses the controller's current native participant, while explicit thread/stop
numbers reject stale IDE requests. Neither spelling grants authority by number.

`wasm-script` pre-parses every command before executing the first one. It accepts
at most 16 policy/inspection commands within the 512-byte console bound. Nested
scripts, resume/step, waits, generic file commands, host expressions, shell
commands and arbitrary calls are rejected. The existing `replace` command is
permitted: it alone may read a sealed owner-controlled body file and independently
checks stopped frames, exact ABI, current generation and real compilation/commit.
Every command uses its own authenticated request and current
state checks. Runtime/state failures stop the remaining list; earlier successful
policy changes are retained. Scripts never execute on the guest callback.
Script replies stay within the 64 KiB management limit, retaining only complete
child replies. An explicit `output-truncated` record gives the displayed count,
actual executed count, and final command's status; omitted output does not mean
omitted execution. Large source object displays likewise report omitted rows.

`ptype NAME` resolves the deepest unambiguous active variable using embedded
DWARF and the actual jointly authenticated stopped source activation. It returns
bounded copied struct/class/union/array/enum/type layout, without guest memory
reads or expression evaluation. Ambiguous multithread stops need
`ptype THREAD STOP_ID NAME`. This is type inspection, not a claim that arbitrary
C++ expressions or complete object values are currently evaluated.

`test/0018.debugger/wasm_events.cc` checks normative opcode categories, truncated
prefixes, oversized offsets, whole-script rejection and trace retirement. The
Core 3.0 WAT fixture additionally includes recursive GC types, arrays, memory64,
table64, typed function references, tail calls, modern exceptions, atomic/shared
memory and relaxed SIMD. Source-only changes must be rebuilt in the authorized
Linux cgroup before runtime qualification; old product binaries do not qualify
these features. Successful catch events, operand-stack materialization, moving
GC object handles, watchpoints and richer scripting remain separate work.

Source values (`print NAME`, `p NAME`, or `print THREAD STOP_ID NAME`) select
only the current scoped variable. Bounded `NAME.member` and `NAME[index]`
selectors may inspect owned aggregates/arrays; calls, assignment, pointer
following, casts, host lookup and arbitrary expressions are rejected. The entire
root object is authenticated and copied first; selection consumes that owned
image, never an independently computed memory address. Numeric direct-local/constant values consume privately authenticated
copied slots. Slot availability comes from the same fused validator's real
initialization state at that emitted site: a nondefaultable local not proven
initialized keeps its original index/type but is neither loaded nor interpreted.
Frame-relative objects require a real captured local frame base,
actual Code PC and function generation; a fresh runtime transaction jointly
checks the private event capture/source binding, holds one all-stopped domain
guard and publication lease, copies at most 64 KiB, and returns its actual
source position. The controller repeats variable selection and offset planning
at that returned position before rendering the owned bytes. Structures/classes,
unions, bounded arrays, enums and supported bit-fields use the finite DWARF
object renderer. Guest pointers are displayed as guest offsets and never
followed.

This initial memory ABI requires exactly one locally defined, **unshared**
linear memory. Its actual memory32/memory64 declaration must match the DWARF
address width. Multiple memories, imported/provider memories, shared memories,
unsupported location expressions, unrecorded frame-base locals, unknown object
sizes, stale stops and replaced functions return unavailable; memory zero is
never guessed from a multi-memory module. Existing producer metadata has no
trusted memory-index mapping, so extending this ABI needs an explicit source
mapping plus coherent ownership protocol. Native traps do not masquerade as a
cooperative local snapshot. These policies run exclusively on the authenticated
management path and add no normal execution memory guard or lock.

The actual runtime witness `debug_source_binding_runtime.cc` accepts an optional
`memory32`/`memory64` argument with the matching `debug_source_memory*_copy.wat`
fixture. The memory64 fixture uses Core 3's `memory i64` declaration and an i64
load address. That witness checks genuine stop-bound copies, mismatched address
width, copy/range caps, alias and unreadable foreign capture owners, stale
resume tickets, replacement retirement and reset. It needs fresh matching
runtime/main/host objects and official Wasm fixture validation; source presence
or a synthetic unit is not a product qualification result.

A qualified native stop may additionally display `native-wasm provenance=exact`
with the actual runtime's loaded LLVM line-table row, module/function and
function/runtime generations. Unknown, ambiguous and unavailable rows stay
explicit. This is generated-code provenance, not proof that a Wasm operation
has committed its effect. Only an exact generation-one row may use the original
embedded guest DWARF for source-line display; replaced code retains no original
source-variable authority. A native row, GPR or display trace never authorizes
cooperative locals, operand-stack recovery, source object memory reads or
checkpoint restoration. The scalar display uses only a runtime-private
capture and the actual active trap session, never a requested native address.

Static/global `DW_OP_addr` locations can use the same single-memory producer
ABI: the selected source CU and variable identity remain actual-bound, its
absolute scalar is treated exclusively as a guest offset, and the identical
stop-bound runtime copy repeats source/variable/offset checks before rendering.
No host relocation, external symbol lookup, guessed frame base or pointer cast
is added. Known metadata constants can be displayed without an absent copied
local being fabricated. Other unsupported expressions remain unavailable.

`catch wasm all` also admits genuine emitted safe points of privately bound
functions whose opcode bytes exceed the cold image-copy budget. Their actual
module epoch, nonzero function generation and expression extent remain checked;
the opcode stays explicitly unavailable. Category-specific catchpoints never
infer GC/EH/memory/etc. from missing bytes and therefore need a bounded opcode
copy. Neither policy admits an unbound or retired generation.
