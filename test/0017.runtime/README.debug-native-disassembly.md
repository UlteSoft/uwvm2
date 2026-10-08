# Current-stop readonly native disassembly candidate

This document records the immutable r2 subset. The live r3 extension is
separately described in [README.debug-native-window-r3.md](README.debug-native-window-r3.md);
old r2 receipts do not qualify that new source.

Source candidate only: no local native compile, Python test execution, server,
SSH or VM execution was performed when preparing this packet. Immutable R5
runtime and DAP r1 snapshots remain unchanged. This derivative requires a new
fresh runtime/main/consumer closure; no R5 runtime object or old PE/MachO can be
combined with the new API and reported as current-product acceptance.

The only command is `disassemble THREAD STOP_ID COUNT`, with decimal nonzero
thread/stop and count 1..32. It uses existing authenticated UWC1 backtrace
participant admission. There is no address, byte offset or instruction offset
input. Numeric stop labels reject stale IDE requests and grant no read authority.

The runtime canonicalizes the private activation capture by actual shared-owner
control block before dereference. It acquires an execution lease, exactly ONE
domain callback for the saved actual ticket/participant, then the publication
guard. It verifies actual epoch, every saved function generation/full-or-retained
code owner, the exact currently loaded function symbol range and original native
thread. Native external parks additionally require the REAL active session
identity/phase/PC/owner under native host-transition ownership throughout the
copy. Source queries retain the old strict cooperative-only helper. No old
locals, source positions, caller PCs or source scopes are restored by code copy.

Only a bounded owned array of at most 480 bytes escapes those guards. LLVM MC
`decode_copied` receives that array and a display PC, never a PC cast as a pointer.
Native release/request hold no domain lock while their host-transition guard
lives, preserving lease -> domain -> publication -> native-host ordering.
Unsupported mode/platform, incomplete producer, missing symbol range, aliases,
changed generations, native owner transitions, stale/reset/closed tickets and
allocation failure clear the entire output and return unavailable. The old
native-step live-owner decoder remains separate and bounded.

The DAP adapter accepts only a current `uwvm-native-stop:ID` frame, a real public
stop label and an exact fresh complete status/selected bt. It checks status after
the query too. Inline and Wasm references, raw addresses, nonzero offsets and
symbol requests are unavailable. DAP itself permits negative offsets and
symbol requests; this is deliberately a bounded subset, not full IDE-pane
interaction or an actual VS Code disassembly acceptance claim. Resume, changed
stop, transport error and exit
retire IDs. The decoded bytes are display data; readMemory/writeMemory/restart
and instruction breakpoints gain no native authority. A legacy label without a
stop-id cannot issue the query.

[DAP DisassembleArguments](https://github.com/microsoft/debug-adapter-protocol/blob/main/specification.md#Types_DisassembleArguments)
requires exactly instructionCount entries. At a truncated/unavailable boundary
we return explicit invalid filler with no instruction bytes, staying at the last
actual code boundary. We do not invent a byte length or step across it.
[DisassembledInstruction](https://github.com/microsoft/debug-adapter-protocol/blob/main/specification.md#Types_DisassembledInstruction)
permits the invalid presentation hint. Symbol resolution and line/native-PC
source reconstruction are not claimed.

## Shortest remote checks

The sole keeper runs sequentially in the existing 64GiB/no-swap cgroup after
source/root review. Record original argv/tool SHA, actual fresh runtime/main
source fingerprints and object/ELF or PE/MachO/DLL closure before/after; the
runner's own reported binary SHA is identification, not build provenance.

1. Run `python3 test/0018.debugger/test_dap_native_disassembly.py` (9 protocol
   methods), plus current `test_dap_adapter.py` (19), source values (9), and DAP
   step/inline (10). Fake broker output proves only bounded formatter parsing.
2. Build/run `test/0017.runtime/debug_native_disassembly_command.cc` with the
   current common textual header prefix (no LLVM link required).
3. Build/run `test/0017.runtime/native_disassembly_copied.cc` with actual
   UWVM_USE_LLVM_JIT and native-target LLVM MC/Disassembler libraries. It must
   decode owned bytes with a non-dereferenced display PC, reject truncation and
   pointer overflow. This proves the component, not runtime authority.
4. Fresh-build `test/0017.runtime/debug_native_code_runtime.cc` with EXACT same
   runtime/main layout macros and actual LLVM23/DWARF/runtime dependency prefix
   used by the fresh debug producer closure. Official `wasm-tools parse` and
   `validate` the actual `test/0017.runtime/debug_activation_runtime.wat` before
   running both `instruction` and `unwind`:

   ```text
   WASM_TOOLS parse test/0017.runtime/debug_activation_runtime.wat -o NEW_FIXTURE.wasm
   WASM_TOOLS validate NEW_FIXTURE.wasm
   FRESH_NATIVE_CODE_RUNTIME NEW_FIXTURE.wasm instruction
   FRESH_NATIVE_CODE_RUNTIME NEW_FIXTURE.wasm unwind
   ```

   The fixture
   proves real private code capture, both actual generation owners, aliases,
   unreadable capture/session refusal, resume and reset. It is not a native trap
   or IDE server acceptance test.
5. Fresh Linux x86-64 CLI (both repositories):

   ```text
   python3 test/0017.runtime/run_debug_native_disassembly_cli.py      --source-root CURRENT_SOURCE --uwvm FRESH_CLI --wasm-tools ACTUAL_WASM_TOOLS      --out NEW_OUTPUT_DIRECTORY [--ros]
   ```

   The official tiny loop is parsed/validated before the product is started.
   For both strategies the runner compares real readonly bytes with the next
   two executed native instructions, requires new stop labels, rejects an old
   numeric label and raw-address spelling, and retains source-local unavailability.
   It uses no old PE/current-source claim. Product-Windows/macOS runs still
   require their existing fresh-source qualification and actual owned Job/Mac
   resource launcher; this Linux driver does not authorize either platform.

Normal interpreter/JIT generated IR is unchanged by this candidate. All new
runtime queries/registry/site copies are on the opted-in debugger cold park or
management path; performance/code-generation qualification is still pending,
not inferred from the source projection or protocol units.
