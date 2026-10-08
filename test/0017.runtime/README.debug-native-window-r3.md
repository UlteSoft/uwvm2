# Owned-function native disassembly r3

This source candidate extends the immutable r2 subset; neither r2 nor R5
receipts qualify this newer runtime API, controller, adapter or helper.
Preparation used source/AST review only. No local native compile, Python
test execution, server, SSH or VM was run.

The standard [DAP request](https://github.com/microsoft/debug-adapter-protocol/blob/main/specification.md#Types_DisassembleArguments)
supports signed byte and instruction offsets and symbol lookup. The current
[VS Code pane](https://github.com/microsoft/vscode/blob/main/src/vs/workbench/contrib/debug/browser/disassemblyView.ts)
initially requests instructionOffset=-200, instructionCount=400; its
[debug session](https://github.com/microsoft/vscode/blob/main/src/vs/workbench/contrib/debug/browser/debugSession.ts)
requests resolveSymbols=true. This candidate accepts count 1..512,
byte offset -65536..65536, instruction offset -8192..8192 and either symbol flag.
It is a bounded subset, not support for all JS-safe integer offsets or arbitrary
native addresses. Actual IDE/platform acceptance remains pending.

The adapter uses at most 32 rows per authenticated console query:
`disassemble-range THREAD STOP_ID COUNT BYTE_OFFSET INSTRUCTION_OFFSET SYMBOLS`.
SYMBOLS is 0/1. Internal instruction offsets have 512 positions of paging
headroom (absolute limit 8704); real decoding remains limited to 8192 instructions.
The original zero-offset <=32 native-stop r2 wire command remains compatible.

The new cold runtime query accepts only the existing private activation capture
and actual active native session identity. It compares control blocks before
dereference and holds execution lease -> ONE real domain ticket/participant
guard -> publication -> actual native-trap ownership. It returns an owned copy
of precisely the current loaded function symbol interval, at most 64KiB, with
actual PC, module/function, runtime epoch and compiled function generation.
No requested address or range enters this API. Missing/oversized owners,
foreign aliases, stale/resumed/reset captures and transitions clear all output.
No publication, capture or TLS layout or normal JIT instruction was added.

A cold MC context is reused within each page. The decoder walks forward from
the actual function entry; it must decode the stopped PC exactly. It never
guesses variable-length x86 boundaries by scanning backwards. A byte offset
into a previously decoded instruction is unavailable. A decoder gap cannot
restart decoding; budget exhaustion rejects the request. Before/after-owner
positions and unavailable suffixes become exact-count invalid fillers with
address "-1", no bytes or invented PC. Bytes from another function/adapter/
section do not enter the decoder.

Only the actual current owner's optional Wasm custom function name is attempted.
A complete name of at most 256 bytes is copied; a longer or absent name is
omitted. The console escapes controls and the adapter retains escaped display
bytes literally. No external symbol loader, host-address resolver, source
reconstruction, source-value permission or expression evaluation is installed.

At attach.stepLevel=native, only the selected top physical cooperative frame
may receive an independent opaque `uwvm-native-code:ID`. The adapter first
requires real readonly private-owner query success and matching fresh complete
stop snapshots. Wasm/source and inline display references never acquire this
permission. Native trap frames retain `uwvm-native-stop:ID`. Neither reference
grants readMemory/writeMemory, instruction-breakpoint, restart or locals access.
Every page checks fresh status before and after; all pages must match exact
owner/PC/epoch/function generation and name attempt. Resume, same-site repark,
replacement, reset, malformed reply and transport failure retire references
and discard the whole partial result.

## Minimal keeper checks

Use a fresh reviewed r3 runtime/main/consumer dependency closure, exact common
TU layout macros, actual LLVM23 MC/native disassembler and DWARF link closure.
The author has not executed these commands. The sole keeper runs serially
inside the existing 64GiB/no-swap Linux cgroup.

1. Guard the shell, then run `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover
   -s test/0018.debugger -p 'test_dap*.py' -v`. New window protocol has 10
   synthetic methods; these prove formatter parsing only, no runtime authority.
2. Build/run `test/0017.runtime/native_disassembly_window.cc` with the normal
   textual header prefix, no LLVM mode macro. Its mock decoder checks finite
   boundaries, explicit fillers, overflow/budget rejection and fast_io grammar.
3. Build/run `test/0017.runtime/native_disassembly_window_mc.cc` with the actual
   native LLVM MC libraries/macro prefix. It tests copied x86/ARM64 instructions,
   real backward selection and rejection of inside-instruction byte offsets.
4. Officially parse/validate `debug_activation_runtime.wat`; fresh-build and run
   `debug_native_function_runtime.cc FIXTURE.wasm instruction` and `... unwind`.
   Success must include genuine whole-owner copy, zero padding outside it,
   generation two, alias/foreign-session rejection, resume and reset.
5. Fresh Linux CLI (both repositories):
   `python3 test/0017.runtime/run_debug_native_window_cli.py --source-root CURRENT_SOURCE
   --uwvm FRESH_CLI --wasm-tools ACTUAL_WASM_TOOLS --out NEW_DIRECTORY [--ros]`.
   The official tiny named loop is validated before execution; both policies
   require actual 400-row bounded pages/symbol, before-owner invalid filler,
   true backward instruction boundary, stale-ID refusal, and bytes matching two
   executed native steps. A previous executed instruction need not be the
   previous linear decoded instruction when control branches.

Windows needs the explicit UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT=1
and fresh reviewed PE/LLVM DLL/DWARF closure under its nonbreakaway owned Job.
macOS needs the current fresh Apple arm64 SDK/MC/MIG/DWARF closure and reviewed
fixed no-fork owner/observed <4GiB/no-swap admission. Linux CLI tests do not
authorize arbitrary Windows/macOS runs or qualify old products. Actual broker/
IDE interaction and all three platform product results are still pending.


## Large native functions — 2026-10-06

The current implementation removes the whole-function 64 KiB and 8192 decoded
instruction ceilings, including the legacy whole-function provenance-mask size
check. Cold function replies own exact-sized byte and provenance buffers.
Allocation occurs before the parked-domain/native gate; the same live Wasm
owner, participant, generation, epoch and trap are reauthenticated before
copying. Allocation failure leaves the stop intact and the reply unavailable.
Raw adapters, private snapshot helpers and VM/host code retain no guest authority.

Forward decoding retains scalar indices and a bounded output page. A second
forward walk supplies negative instruction offsets without backwards scanning or
resynchronizing after unknown bytes. Window count and wire offset limits remain
as stated above; they do not cap the size or instruction count of the owner.
The owner-copy probes verify exact buffer extents instead of unused padding.

Both repositories' 70001-byte mock boundary regression ran on Linux x86_64 and
AArch64 in the existing 64 GiB cgroup. This verifies the finite decoding policy;
actual full-VM SI/NI and Wasm-state regressions are recorded separately in
documents/runtime/wasm-debug-full-audit-20261005.txt.
