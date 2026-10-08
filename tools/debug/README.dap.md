# LLVM-full IDE attachment

`dap_adapter.py` exposes the host-owned debugger broker through the standard
Debug Adapter Protocol (DAP) over stdin/stdout. The VM must have been launched
through `secure_server.py` (Linux), `secure_server_macos.py` (macOS), or
`uwvm-debug-server.exe serve` (Windows). The adapter attaches to the broker;
it cannot enable debug instrumentation inside an unprepared VM. The guest gets
neither the broker listener nor the authorization token.

Source and Wasm instruction breakpoints accept the finite DAP hit conditions
`>=N` and `>N`, with an unsigned 64-bit `N`. `>18446744073709551615` is
rejected because the saturating counter cannot represent a greater hit count.
For example, `>=4` ignores the
first three matching executable safe-point visits and stops on every later
visit. The adapter sends one `break-source ... ignore 3` or `break ... ignore 3`
command; the controller installs the breakpoint and initial policy under one
lock. Explicit reconfiguration of an existing target resets its hit counter,
reenables it and installs the new ignore count. Empty fields preserve an
ordinary breakpoint. A nonempty language `condition` is sent in the same
atomic registration command, for example `break ... ignore 3 if counter == 4`.
Conditions must be printable single-line expressions of at most 256 ASCII
bytes; the controller validates and evaluates them against the selected source
context. Unsupported expressions produce an unverified breakpoint.
Exact-count/modulo hit conditions and nonempty `logMessage` are rejected with
`verified: false`. The adapter advertises conditional and finite hit-condition
support; logpoints remain unavailable.

The interactive CLI also supports `display EXPR`, `info display`,
`undisplay [ID]` and `enable|disable|delete display [ID]`. It stores at most 32
printable read-only expressions, each up to 256 bytes, then reevaluates enabled entries
through the normal authenticated source-value query at each newly observed
sole-thread stop, explicitly selecting frame 0. A new entry evaluates immediately
when such a stop exists. Scope/metadata/native-stop failures retain the expression
and report unavailable; no old guest value, pointer or frame is reused. Empty
IDs omitted from a command select all registered entries. These are CLI session settings, not DAP watch
objects, native memory access or guest execution hooks. Tab completes current
source variables and members in a `display` expression.

The wire format and step granularity follow the [DAP specification](https://microsoft.github.io/debug-adapter-protocol/specification);
the VS Code entry point follows its [debugger extension API](https://code.visualstudio.com/api/extension-guides/debugger-extension).

For VS Code, load this `tools/debug` directory as a local extension, or package
it as a VSIX. It contributes the `uwvm-llvm-full` debug type. A Unix
`launch.json` entry is:

```json
{
  "version": "0.2.0",
  "configurations": [{
    "type": "uwvm-llvm-full",
    "request": "attach",
    "name": "UWVM LLVM Full",
    "socketDir": "/absolute/private/broker-directory",
    "moduleId": 0,
    "stepLevel": "source"
  }]
}
```

On Windows use `pipeName` from the broker's startup output and ask VS Code for
the capability at session start. Do not save the capability in `launch.json`.

```json
{
  "version": "0.2.0",
  "configurations": [{
    "type": "uwvm-llvm-full",
    "request": "attach",
    "name": "UWVM LLVM Full",
    "pipeName": "\\\\.\\pipe\\uwvm-debug-host-...",
    "capability": "${input:uwvmDebugCapability}",
    "stepLevel": "source"
  }],
  "inputs": [{
    "id": "uwvmDebugCapability",
    "type": "promptString",
    "description": "UWVM host debugger capability",
    "password": true
  }]
}
```

The `attach.stepLevel` setting is `source` by default. With this level,
ordinary DAP `stepIn`, `next`, and `stepOut` (omitted or `statement`
granularity) map to the existing source `into`, `over`, and `out` policies.
Explicit `line` granularity selects the same source policies at every level.
Source stepping requires live embedded `-g` metadata; its absence returns the
controller's unavailable error without falling back to a Wasm step. The [DAP
stepping contract](https://github.com/microsoft/debug-adapter-protocol/blob/main/specification.md#Types_SteppingGranularity)
defines statement and line stepping in terms of source execution.

Select `stepLevel: "wasm"` for opcode `stepIn`, `next` and `stepOut`.
They map to Wasm `into`, `over` and `out`; `next` follows real call and tail-call
continuations, and `stepOut` follows return or exception unwind. With this level,
omitted/statement/instruction granularity uses Wasm instructions. Explicit `line`
granularity still selects source stepping. Select `stepLevel: "native"` for native
instruction `stepIn` and `next`. Native `next` routes to `ni THREAD` at the
controller's current genuine top native trap; the controller retains the
existing owner, continuation, timeout/cancellation and worker ACK checks.
Explicit instruction `next` at source level uses the same native policy.
Unsupported or cooperative stops return the controller error without fallback.
Old frame, register, value and code references retire before the broker request,
including cancellation and lost replies. Native `stepOut` requires its separate qualified
implementation and may return unavailable. No source operation is substituted.
Ordinary **Wasm locals** supports indexed `variables` pages using `start`
and `count`, including indices beyond the diagnostic capture prefix. Omitted
or zero `count` requests all remaining locals within the 1024-row response
budget; larger scopes require explicit bounded pages. Each actual producer
copy is <=64 rows, with fresh matching complete stop checks. Named filtering
returns no indexed locals; retired scopes and GC references fail.
See [R41 pagination and Linux startup acceptance](../../test/0018.debugger/README.wasm-local-pages.md)
for examples and actual test coverage.

Wasm instruction breakpoints use
the `wasm:MODULE:FUNCTION:BYTE_OFFSET` reference in stack frames. Source breakpoints require
embedded `-g` DWARF and exact path matching. Stack traces, Wasm locals,
bounded `readMemory` requests using `wasm-memory:MODULE:MEMORY:BYTE_OFFSET`,
pause/continue, and an authenticated debug console are exposed. `readMemory`
accepts `count` 0..65536 and a signed byte `offset`, with a decimal numeric
guest byte offset in the response `address`. Memory32, memory64 and multiple
memories use their actual module/memory index spaces. Each actual console copy
is <=256 bytes and surrounded by matching complete identified cooperative
stop checks. A changed stop, malformed/lost reply or unavailable range refuses
the entire request; no copied prefix is returned. A zero-byte request still
checks the actual guest range. Native traps, legacy unidentified stops and
host/code/stack references are refused. The selector identifies a logical guest
range freshly on each request; it is not a host address or checkpoint handle.
See [R45 memory pages and Linux acceptance](../../test/0018.debugger/README.wasm-memory-pages.md).

`writeMemory` accepts the same guest selector, a signed byte offset and
canonical base64 for 0..256 bytes. Nonempty data uses one complete native
`set wasm memory` commit, after current identified cooperative cohort checks;
the runtime validates the entire actual writable range before its copy.
`allowPartial=true` and larger writes are explicitly refused before I/O.
Empty data checks the real guest range without a mutation. Cached frame, value,
code and scope references retire before any nonempty attempt. Only the exact
applied v3 acknowledgement confirms `bytesWritten`; a lost/malformed reply
is unconfirmed and never retried. A known committed write stays successful
even if another host resumes afterward. Memory and stack/variable invalidation
events follow successful commits only when the client negotiated those events.
The logical selector acquires the current native transaction; it is not an
old-frame or optimistic `if-stop` write capability. Native/code/stack/host
references and ASM traps are refused.
See [R46 bounded memory writes and Linux acceptance](../../test/0018.debugger/README.wasm-memory-writes.md). The console
accepts `replace MODULE FUNCTION GENERATION BODY_FILE` for same-ABI function
hot replacement while fully stopped. The Wasm fallback source is currently a
function label, not a disassembled WAT listing;
native instruction text appears in the Debug Console after native stepping.
At a native trap, `stackTrace` now returns one instruction-position label from
the authenticated controller's stopped PC. It compares a fresh complete
`status` with the selected `bt THREAD` reply, then gives the label a
session-unique `uwvm-native-stop:ID` display reference. It exposes no old Wasm
offset, source location, caller frames or local-variable scope. Resume,
transport/protocol failure, a changed stop, detach and exit retire those IDs.
The display reference grants no native memory read/write or breakpoint access.
A current identified native stop supports the r2 zero-offset <=32-row subset.
The r3 source candidate also accepts bounded signed byte/instruction offsets,
instructionCount 1..512 and resolveSymbols=true. A 400-row initial IDE window
is paged into <=32-row console queries with fresh complete stop checks around
every page. One private-owned current function image (<=64KiB) is decoded
forward from its real entry, limited to 8192 instructions; no guessed backwards
x86 boundaries or neighboring function bytes are read. Out-of-owner rows use
invalid filler address "-1". Symbol lookup attempts only the actual current
owner's optional full bounded Wasm custom name, safely escaped.

Only attach.stepLevel=native may give the selected top physical cooperative
frame a separate uwvm-native-code:ID, after genuine readonly owner-copy success
and fresh matching complete status. Source/Wasm/inline references remain
without code access. Native traps retain uwvm-native-stop:ID. Both are opaque
display/query references, not memory/write/breakpoint/restart/locals authority.
The new console grammar is
disassemble-range THREAD STOP_ID COUNT BYTE_OFFSET INSTRUCTION_OFFSET SYMBOLS;
numeric stop labels only reject stale requests. The runtime-private capture,
actual control block/ticket/participant, epoch/function generation and live
native session still authorize the exact owner. Missing owner/backend, changed
stop or generation, malformed/oversized replies and transport failures discard
the whole result. The original <=32-row zero-offset native-stop command remains
compatible.

See [r3 limits and keeper qualification](../../test/0017.runtime/README.debug-native-window-r3.md).
This source extension has only AST/source review, not actual VS Code pane,
fresh JIT binary, Windows or macOS native qualification. The [DAP StackFrame contract](https://github.com/microsoft/debug-adapter-protocol/blob/main/specification.md#Types_StackFrame)
permits a position label without source, with zero line/column; [reference
lifetimes](https://github.com/microsoft/debug-adapter-protocol/blob/main/overview.md#lifetime-of-objects-references)
end when execution resumes.

A matched complete `status` and selected `bt THREAD` with a real public
`stop-id` now display the current physical frame's embedded inline chain as
separate inner-to-outer **Inlined** labels before `#0`. The call-site remains
explicitly display metadata, not a reconstructed caller Wasm PC or a current
source location. These session-unique labels have no source reference,
instruction reference, variable scope, restart or address capability. The
adapter never unescapes metadata into paths or commands. Ambiguous unquoted
call-site delimiters, late/out-of-thread rows, malformed values and budgets
fail closed; unidentifiable stops show only their existing physical frames.
Native traps cannot retain an inline chain. Source variables below remain a
limited independently authenticated current-physical-frame scope.
This is also a normal DAP stdio process for other DAP-capable IDEs; no IDE
process may connect directly to the guest's private control endpoint.

`python3 test/0018.debugger/test_dap_adapter.py` exercises framing and the
three-level command mapping without launching a VM. Its native-position
regressions cover the controller's real text grammar, selected-thread and PC
binding, stale-reference retirement and refusal of arbitrary address access.
The 2026-10-02 adapter update has only been statically reviewed locally; its
tests require a separate execution receipt in the Linux 64 GiB cgroup. It does
not qualify a fresh JIT binary or the Windows native-step backend.
`node test/0018.debugger/test_vscode_extension.cjs` checks the VS Code
descriptor and verifies that the capability does not enter process arguments.
`test/0018.debugger/run_dap_macos.py --source-step --source-language c|cpp|rust`
is a historical runner for DWARF C/C++/Rust, source/Wasm breakpoints and native
stepping. It does not qualify the current joint producer/source/runtime closure.
Current macOS execution needs the reviewed fixed-fixture no-fork/RSS owner and
actual fresh arm64 dependency qualification; old RSS sampling scripts are not
aggregate kernel hard caps. Linux execution stays in the 64GiB cgroup, and
Windows attachment requires its own fresh PE/owned Job/guest-VM qualification.


The current source candidate adds a separate **Source variables** scope for
the current physical frame when its embedded line location and actual
`stop-id` label are present. It displays only the controller's finite copied
numeric variables, including parameters, boolean/integer values, preserved
float bits and unavailable reasons. The Wasm locals scope is independent.
Pointer/reference values, arbitrary location expressions, members, caller
locals and native traps do not acquire value or address access. Every returned
source variable is a read-only display leaf, with no evaluate or memory
reference. This is a limited source-value candidate, not full C++/Rust object
inspection.

The controller increments its public stop label on each actual pause,
completed native step and successful replacement. It does not expose the
private pause ticket; exhaustion disables labels rather than reusing one.
The source-value reply carries `source-stop` from the same controller mutex
as its genuine copied-value query. The adapter rechecks complete status,
requires the source reply to match the original label and retires references
on changes, resume or failure. This distinguishes separate loop pauses at
the same function, PC and code generation. The actual runtime still checks
the captured ticket, source owner, publication and function generation. A
label or a metadata name is never authority to read a frame or memory.
Ambiguous delimiter-bearing metadata is rejected, and escaped label text is
never turned into a path or command.

`test/0018.debugger/test_dap_source_values.py` covers this bounded text protocol
and same-position stop retirement using a test broker only. The extended
controller unit exercises actual pause-domain transitions with its existing
synthetic trace provider. Neither is product source/DWARF/native acceptance.
These changes have only source/AST review; the keeper must run them and the
fresh matching Stage2 runtime/CLI source-value fixtures in the Linux cgroup.
Old Windows products and source snapshots remain separate evidence.


The 2026-10-02 step-level/inline candidate changes only this adapter,
VS Code configuration, documentation and protocol test sources. Its author
has performed AST/static review only. `test_dap_step_level_inline.py` checks
source default policy spelling, explicit Wasm/native instruction requests,
missing-DWARF refusal, current-stop inline labels and retirement/no-authority
negatives using formatter-shaped test replies. This does not qualify real
DWARF, JIT code generation or source next/finish. The existing real macOS and
Windows DAP fixture runners explicitly select `wasm` to preserve their first
single-opcode request; old binaries and receipts remain historical evidence.
The keeper must run these small protocol tests in the Linux 64 GiB cgroup,
then combine the reviewed adapter with a fresh matching R5 product and
official fixtures. Current Windows/Mac product acceptance remains pending.


The replacement-reference descendant retires every stopped frame, scope,
source/inline label and native display/query ID **before** sending the exact
console 'replace' spelling, including failed body validation or transport
failure. After a non-error replacement reply it synchronously reads actual
'status' to establish the new stop; the generation printed by 'replace' alone
cannot establish a stopped position. A failed status refresh keeps the old IDs
retired even when replacement itself committed. It never restores an old ID,
and continuous queued requests cannot bypass this by avoiding idle polling.

'test_dap_replacement_lifetime.py' covers formatter-shaped success, bad-body
failure, same-PC new stop/publication generation, native/cooperative references,
failed status refresh and the absence of a 'replace_function' alias. These are
protocol controls only, not real JIT replacement or IDE acceptance. This
descendant has AST/source review only; the immutable r3c/R5 packets remain
separate, and the keeper must execute its tests against the matching adapter.

When a replacement is acknowledged but the subsequent complete status refresh
fails, the adapter explicitly reports `replacement acknowledged; stop refresh
failed` and keeps every prior debugger reference retired. The replacement may
already be committed; inspect the real function generation before sending
another replacement. This diagnostic does not grant any new frame or read
permission and does not turn a failed body validation into an acknowledgement.

Replacement refresh accepts only a complete current stopped reply with a
nonzero shared stop identifier, nonzero participant/runtime generations and
unique participants. Unknown text, prepared/running/closed replies or missing
and ambiguous identities fail before a stopped event is emitted. The general
historical status parser keeps its prior compatibility; only this post-ack
refresh is stricter. An acknowledged replacement can therefore report a failed
refresh if another host action resumed it before the status query; its old IDs
stay retired and the acknowledgement must not be mistaken for an uncommitted
replacement.


The language-frame candidate requests `frames THREAD STOP_ID` from the VM and
builds opaque DAP frame references from its jointly authenticated current
physical/inline scope list and actual caller activation identities. Inline
frames are innermost first. The current Code offset remains attached to the
current physical instruction reference; older caller PCs are not inferred.
Source locations are shown only where the runtime has a real current row.

Source scopes bind the original complete stop and source-frame ordinal.
`locals source THREAD STOP_ID FRAME` and
`print-frame THREAD STOP_ID FRAME EXPR` perform scope selection and
read-only lookup in one VM command; they do not mutate the persistent console
cursor, so concurrent authorized clients cannot interleave select/read.
The explicit command separates all three metadata fields from every expression
prefix (`*`, `-`, `!`, `@as`, characters and numeric literals). DAP requires
a matching VM implementing this command; rejected requests do not fall back
to the ambiguous legacy positional `print` form.

Watch/hover accepts producer roots, members, Rust numeric members, signed
64-bit indices, unary `*`, Zig postfix `.*`, `->` and parentheses. It also
accepts the controller's finite integer/f32/f64 arithmetic, comparisons,
bit operations, short-circuit `&&`/`||`, `sizeof` and builtin numeric
conversions: C casts, `static_cast<double>(value)`, Rust `value as i64`,
Go `int32(value)`, and bounded Zig `@as(i64, value)`. These conversions
are syntax for the existing numeric evaluator, never guest function calls.
Producer types and the current guest ABI determine availability and values;
this does not implement each language's complete expression semantics.

The adapter validates the entire 256-byte printable ASCII input before
contacting the broker. Reference leaves have at most 32 selector steps;
scalar syntax has 128 nodes and 32 combined nesting levels. Scalar spaces
are retained for numeric type names, Rust `as` and character literals.
Selector-only expressions retain their established normalization and
canonical decimal-index rule. Calls, assignment, contiguous increment or
decrement, pointer casts, address-of, numeric dereference and injected commands
fail even in an unselected short-circuit operand. Arithmetic cannot manufacture
a pointer/read plan; pointer-typed arithmetic remains unavailable in the
controller. Every actual guest target requires the controller's coherent
source-memory transaction and its type/address-class/guest bounds checks.
The adapter checks the complete real stop before and after a query and retires
every reference on resume, replacement, changed stop or failure. Source names
that match console aliases never cause a resume observation. No caller-local
or older-frame read authority is accepted from a label.

Caller scopes display that the caller PC and locals were not captured.
Current source locals lists retain the existing direct numeric/constant subset;
unreadable frame-relative storage remains unavailable. Individual `print` may
use the product's existing authenticated single-unshared-memory whole-object
copy. The language-frame patch has AST/source review only; actual DAP, producer
and fresh product acceptance must be performed by the Linux cgroup keeper.
GDB/LLDB parity and selected outer-frame next/finish are not qualified.

## Standard GC member pages

GC object variables include exact `indexedVariables` and `namedVariables: 0`
when the member count fits DAP int32. Clients can use these display hints to
request pages. Standard `variables` accepts `start` and `count` up to 1024 rows
per reply, borrowing the original Wasm root/path in chunks of at most 64.
Omitted/zero `count` returns the remaining members within that response budget;
larger scopes require explicit bounded pages. Type/cardinality/module/epoch or
stop changes reject the whole response. Custom `uwvm/wasmMembers` retains its
64-row per-query limit. See [R42 member paging acceptance](../../test/0018.debugger/README.wasm-member-pages.md).

## Standard typed Wasm scope pages

Typed locals, operands, saved if parameters, controls, handlers, globals and
Table 0 honor standard `variables` pagination. Omitted/zero count reads all
remaining entries within a 1024-entry response budget; each actual borrow stays
at most 64. Original module/frame/selector/table and indices survive paging.
A changed stop, epoch, cardinality, snapshot notice or short page rejects the
whole response. The named filter refreshes the stop without borrowing indexed
state. Paged GC roots retain their original selectors for member expansion.
See [implementation and examples](../../test/0018.debugger/README.wasm-scope-pages.md).

## Deep Wasm activation pages

At a current cooperative Wasm stop, `stepLevel: "wasm"` requests canonical
`frames wasm THREAD STOP_ID FIRST COUNT` pages instead of the diagnostic `bt`
array. Pages have at most 128 rows and carry the full `totalFrames`; frame zero
is current and larger ordinals select actual callers. `stackTrace.startFrame`
can select a deep page, and its scopes route typed locals/operands to the same
Wasm frame ordinal. If the complete Wasm query fails, `stackTrace` reports an
error; a bounded diagnostic backtrace cannot stand for a complete chain. For example:

```text
frames wasm 1 41 4096 16
operands 1 4096 0 64
```

The activation ledger grows in stable 64-frame blocks; 64 is a block size,
not a call-depth limit. Its default identity-storage budget is 256 MiB and the
cold host API `llvm_jit_configure_debug_activation_storage_host_api(bytes)` can
configure it before publication. Resource failure closes precise capture instead
of publishing a partial chain. Observation has no fixed frame count; resumable
checkpoint profiles retain a separate finite default budget of 4096 frames.

Operand scopes explicitly say `last safepoint; may differ from native state`.
Console and `uwvm/wasmState.snapshotNote` replies include:

```text
Note: Last Wasm safepoint snapshot; may differ from current native state.
```

A native instruction pause cannot provide current typed Wasm values or a
cooperative hot-replacement transaction. Return to a current complete Wasm stop
before querying those values or replacing functions. Source and Wasm frame pages
are checked against the complete stopped cohort after copying; resumed, changed
or retired stops cannot publish opaque frame IDs. The bounded diagnostic `bt`
array and the DWARF inline-scope limit remain separate from canonical Wasm depth.

Source watch/hover/variables evaluation requires the `print-frame` reply's
unique actual formatter identity: either `source-stop ID` for copied numeric
locals/constants, or `source-value stop=ID name=...` for object/scalar-expression
displays. The latter requires its complete `source-value end` trailer.
The identity must match the requested genuine source frame, together with
unchanged pre/post status. Missing, duplicate, mixed, stale, empty, oversized
or control-bearing packets retire frame labels and fail the DAP response.
Copied values, objects, pointer displays and origin receipts remain text:
they create no variables/memory reference or read access. Optional origin
receipts must be unique and agree with the requested stop and participant.

## WASIp1 operation acknowledgement and refresh

`uwvm/wasip1Edit` distinguishes the complete native operation acknowledgement
from its subsequent status observation. A confirmed commit or nonapplied
refusal survives a failed or malformed refresh, with `stopCurrent:false`,
`stopObservationAvailable:false` and a bounded `stopObservationDiagnostic`.
Valid refreshes carry `stopObservationAvailable:true`; only the original
complete cooperative stop can still be current. Copied labels retire before
the edit. A lost acknowledgement remains unconfirmed, with no automatic retry.
Confirmed applied operations send an `invalidated` stacks/variables event
after the response when the client negotiated `supportsInvalidatedEvent`.
Checkpoint replies continue to require matching metadata and explicitly
report `wasmCheckpointRequired:true` and `externalIORollback:false`.
See [R47 checkpoint observation and actual rollback checks](../../test/0018.debugger/README.wasip1-commit-observation.md).

## Standard WASIp1 environment scopes

At the current cooperative physical Wasm frame, source and Wasm views expose
**WASIp1 arguments**, **WASIp1 environment**, **WASIp1 descriptors** and
**WASIp1 preopens**. Each expensive scope is read on demand. Its standard DAP
`variables.start` is a dense list ordinal, including sparse descriptor tables;
the displayed `fd N` remains the original guest descriptor. Omitted or zero
`count` reads the remaining entries within the 1024-row and response byte
budgets. Each producer request copies at most 64 rows.

Original complete stop, module, runtime epoch, shared-environment flag and
cardinality are checked across pages. Stale references and inconsistent pages
fail without publishing a prefix. Native frames grant no WASIp1 scope. These
panels expose read-only guest metadata, without host memory/handle/path
authority. See [limits and examples](../../test/0018.debugger/README.wasip1-scopes.md).


### R36 finite TinyGo collection acceptance (2026-10-07)

The Linux launch broker permits 120 seconds for its first authenticated VM command to accommodate full JIT compilation. This launch-owned allowance cannot be renewed by reconnecting; subsequent commands retain the 6-second VM timeout. UnixBroker authentication/ordinary requests retain 8 seconds, with 125 seconds for the first Linux request. VM transport loss retires the original launch rather than reusing an unsequenced late reply.

After the actual `stackTrace` source frame is obtained, Watch/Hover/Variables evaluation of qualified TinyGo map/channel `len`/`cap` returns a read-only scalar (`3`, type `int`, variablesReference `0`), instead of the console packet. The adapter advertises Hover support and the new evaluate type field honors the client's `supportsVariableType`. REPL keeps console text; unknown/compound copied objects remain opaque. No display projection creates memory, expansion, expression or host-VM authority.

`test/0018.debugger/run_dap_tinygo_collections.py` tests the original O0 no-scheduler Wasm using an authenticated Host-prepaused launch, actual broker and separate stdio adapter. It verifies actual source frames/scopes, compiler argument values, pagination, queued stale IDs, natural guest exit and original OS wait. Run only through the Linux cgroup supervisor. This finite acceptance does not qualify ordinary unprepared fast-program attach, actual VS Code UI, collection expansion, gc Go/goroutines, full native language parity or full cross-architecture VM/JIT. See `test/0017.runtime/README.debug-go-dap-r36-20261007.md` and its machine record.

### R37 copied source object views (2026-10-07)

Explicit source Watch/Hover/Variables evaluation of a complete bounded copied object now returns an expandable read-only variablesReference. For the original TinyGo fixture use *box to inspect CollectionHolder fields. Expansion serves the existing copied snapshot, brackets pagination with real stop checks, and derives no selector, memoryReference or pointer chasing from labels. Unknown/extended/truncated packets stay opaque. Source variables still uses numeric root enumeration and box remains unavailable there. New-stop, step/resume/replacement/error retirement clears views and quotas without reusing IDs.

The real Linux cgroup runner test/0018.debugger/run_dap_tinygo_objects.py qualifies both original R35 fresh full products with instruction/unwind, natural guest exit, OS wait and EOF; each repository passes 96 protocol regressions and over 10 minutes of real DAP sessions. Nested array protocol DATA is separate from real producer evidence. Full IDE/native/all-language/cross-VM and map entries remain unqualified. See test/0017.runtime/README.debug-source-objects-dap-r37-20261007.md and its machine record.

### R38 current source-root variables (2026-10-07)

Source variables may enrich an unsupported numeric root with its actual copied source object display. Only unique unescaped bounded ASCII current-frame identifiers are reselected through the existing print-frame API; hidden pages, duplicate roots, labels, pointer bits and native/caller frames grant no selector. A nonzero root pointer returns a lazy read-only variablesReference; expanding it reselects *ROOT once and then serves copied snapshot pages. Nil and copied member pointers remain leaves. Each read has real pre/post stop checks; stop/error retirement clears selectors, frame bindings, objects and quotas without reusing IDs. Roots are bounded to 32 reads per page and existing per-stop object retention limits.

The original TinyGo box -> CollectionHolder scope expansion is qualified on both original R35 full products with instruction/unwind, actual guest exit 0, OS wait and EOF. Each repository passes 110 protocol regressions and more than 10 minutes of real DAP sessions. Other languages/layouts, map entries, IDE/native/cross-VM parity remain unqualified. See test/0017.runtime/README.debug-source-scope-dap-r38-20261007.md and its machine record. R37 numeric-only scope statements document the previous control version.

### R39 copied array and complete text displays (2026-10-07)

Complete bounded canonical text from the existing VM source object formatter now displays as quoted escaped text, with a read-only variablesReference only for its already copied children. For the original TinyGo array fixture, evaluate box.Message in a current source frame to inspect the original UTF-8 bytes and ptr/len carriers. Text, pointer bits and labels never become query selectors; copied member pointers stay leaves. Truncated/omitted/ambiguous text keeps its whole opaque packet. Unknown scalar decorations, malformed escapes/extent and retired stop checks cannot publish a partial tree.

The new real Linux cgroup runner test/0018.debugger/run_dap_tinygo_array_objects.py qualifies ordinary/named/matrix/zero-length/zero-size arrays through Source variables and Watch/Hover/Variables evaluation, plus explicit complete escaped string text, on both original R35 fresh full products and original R34 producer Wasm. Each repository passes 118 protocol regressions and more than 10 minutes of real DAP sessions with original guest exit 0, OS wait and EOF. Source nested string fields do not automatically read payload. Other languages/ABIs, map entries and full IDE/native/cross-VM parity remain unqualified. See test/0017.runtime/README.debug-array-text-dap-r39-20261007.md and its machine record.

### R40 finite integer-enum displays (2026-10-07)

Complete canonical integer enum annotations now retain their numeric value and escaped enumerator name as read-only leaf displays, so a containing copied aggregate keeps its children and pagination. Enumerator names and guest bits grant no query/address/setter authority. Truncated names and unsupported variant/omission grammars remain whole opaque packets. The real enum runner qualifies C/C++ Wasm32/64 DWARF4/5 Source variables, Watch/Hover/Variables evaluation on original R35 full VMs; each repository passes 127 protocol regressions and more than 10 minutes of genuine guest sessions. The Objective-C/Objective-C++ default array-bound source fix has fresh LLVM metadata-component evidence only; updated full VM/DAP, Rust variants and all-language/native/cross-VM parity remain pending. See test/0017.runtime/README.debug-enum-dap-r40-20261007.md.


R41 后续：Objective-C / Objective-C++ 默认数组下界修复已在 R35 frozen full source + 修复的新完整 VM 上通过实际 Wasm32/64、DWARF4/5 DAP 和长测。当前全部工作区及最新 ROS provider 仍待资格化。参见 ../../test/0017.runtime/README.debug-objc-full-dap-r41-20261007.md 和 ../../test/0017.runtime/debug_objc_full_dap_qualification_20261007.json。


### R42 copied active variant displays (2026-10-07)

Complete canonical variant-part and VM-selected active-variant rows, including default selections, now retain read-only children and pages. The adapter validates copied owner extents and a unique active branch; it never selects a payload or turns labels into selectors, pointer reads or mutation permission. Unknown/type-only/truncated extensions remain whole opaque packets. Actual official Rust Wasm32 O0 DWARF4/5 Source variables/Watch/Hover/Variables, genuine-step descendant retirement, paired 10-minute sessions and existing C-family/TinyGo regressions pass on original R41 scoped full products. General Rust/native/Wasm64/full cross-architecture qualification remains pending. See [the R42 report](../../test/0017.runtime/README.debug-rust-variants-dap-r42-20261007.md).


### R43 Rust 活动变体摘要

折叠变量和直接 object.FIELD 求值现在显示已复制活动 case/完整有限载荷，例如 Negative { signed: -3 }、Some { __0.__0.__0: 13 }、None；原类型、只读属性及全部子树保留。超过有限摘要预算时仅显示已证实的 case，不增加查询或标签派生 selector。真实 Rust Wasm32 O0 DWARF4/5 两栈策略、持续会话和最终整合回归见 [R43 报告](../../test/0017.runtime/README.debug-rust-variant-summaries-dap-r43-20261007.md)。完整 Rust pretty-printer/native 等价、优化布局、Wasm64 和全架构 VM 仍未资格化。


### R44 Rust 嵌套活动变体

有限嵌套enum/Option复制摘要已支持两层/三层实际活动case，例如 Carry { payload: Negative { signed: -3 } }，数组载荷显示items[0]。每层都验证实际复制selection，整个外层共享节点/深度/值/UTF-8预算；完整子树保留，不生成查询或指针权限。真实编译器样例、两栈策略、持续会话与回归见 [R44报告](../../test/0017.runtime/README.debug-rust-nested-variants-dap-r44-20261007.md)。完整Rust pretty-printer/native、其他layout、Wasm64和全架构VM仍未资格化。

R45: 实际 Rust tuple/tuple-struct/嵌套tuple及array tuple的只读字段、子对象分页与stale引用已资格化；修复真实 C/C++ computed unsigned int 显示为raw packet的问题，严格四bytes/UINT32范围，保留未知类型fallback。详见[有限范围与实际证据](../../test/0017.runtime/README.debug-rust-tuples-unsigned-dap-r45-20261007.md)。

R46: C、C++、Objective-C、Objective-C++ 的八种 canonical copied 整数primitive现可显示标量/type；真实Wasm32/64、DWARF4/5、两栈策略及持续测试通过。long使用packet明确的4/8字节guest width，保持只读/零展开。未知类型与plain char/bool/float显示未新增资格。参见[R46报告](../../test/0017.runtime/README.debug-c-integer-dap-r46-20261007.md).

R47: VM已计算的canonical bool/float/double表达式现在显示标量及type，保留-0/Inf/NaN原拼写，按明确1/4/8字节宽度验证，保持只读与零展开。真实C系Wasm32/64、DWARF4/5、两栈策略和至少600秒持续测试通过。参见[R47报告](../../test/0017.runtime/README.debug-c-scalar-dap-r47-20261007.md).

R48 已补 Rust/Go/TinyGo/Zig 的有限 primitive bool 谓词、严格逻辑操作数、Rust整数 !、Go/Rust优先级，以及DAP的Zig and/or语法。两仓原始类型组件及genuine Rust/TinyGo会话已验证；完整native语言/类型推断、其他producer和full QEMU仍未完成。见 [README.debug-native-predicate-r48-20261008.md](../../test/0017.runtime/README.debug-native-predicate-r48-20261008.md)。

R49 最新primitive谓词在16个Linux QEMU profile的组件覆盖及公共SDK/linker入口修复见[跨架构报告](../../test/0017.runtime/README.debug-native-predicate-cross-r49-20261008.md)；完整VM/JIT/DAP与native体验仍未资格化。
