# Current real Linux broker / DAP acceptance

This is a new test-source candidate. Its author has only performed AST/source
review, not Python imports, tests, native compilation, SSH, broker execution or
IDE execution. The immutable R3c/R5 and replacement-reference r1 packets remain
unchanged. Actual product qualification and all executions belong to the sole
keeper under the existing 64GiB/no-swap cgroup; this script adds no resource
harness.

The runner consumes an original fresh R3 product receipt in the existing
source-step schema: binary_path/sha256, successful actual link_argv/link_cwd,
source_before_file/source_after_file, and the complete canonical src/dependency
fingerprint. Runtime objects, identical TU layout flags, LLVM23 MC/DWARF libraries
and signal backend must already have genuine keeper qualification. A binary
hash or an invented source identifier alone does not prove that closure.
Use the reviewed replacement-reference adapter descendant for immediate queued
replacement checks; an old adapter is expected to fail those checks.

The official C5 O1 fixture reuses debug_source_step_c.c bodies and source
markers. The runner records a derived copy that changes only the unique entry
signature to source_step_once, preserving all original line numbers. A separate
tiny C driver repeatedly calls it so the client cannot arrive after a finite
guest has exited. Both objects are officially compiled, hashed around the
Wasm link, validated by wasm-tools and verified/dumped by llvm-dwarfdump.
The independent Code/line oracle rejects actual imports and uses actual emitted
rows. This is a two-CU, single-module embedded source fixture; it is not an old
CLI transcript or an inferred source position.

The actual broker must use -m run: the production -m debug-jit branch is a
local console and does not serve the inherited broker control channel. Each
case pauses an already launcher-authorized VM, installs a genuine executable
breakpoint and derives the real participant from its stopped reply. No thread,
activation, source PC or private ticket is fabricated.

Cases run under both instruction and unwind diagnostics:

- source-finish/source-next/source-into: real default source attach and omitted
  DAP granularity; official line/column/statement and concrete inline display,
  read-only source leaves, disconnect and a fresh adapter reconnect. This does
  not qualify full C++/Rust variables, a Wasm listing or an actual VS Code UI.
- wasm-native: exact loop opcode advancement, genuine top physical readonly
  code reference, VS Code-shaped -200/400/resolveSymbols=true request, invalid
  fillers, two copied versus actually executed native instructions, stale
  references and absence of native source/local/address access.
- replacement: a fixed call/store loop first stores 12; malformed body rejects
  without changing stop/publication and the old callee runs again with result
  12 before a same-ABI inactive leaf replacement commits
  generation two and stores 13. Replace plus old frame/scope/native requests are
  written together on actual adapter stdin; no idle poll is deliberately
  inserted. Numeric coordinates and display IDs grant no new host access.

The two additional replacement-gc-funcref/replacement-gc-tag cases use the
new fixed base/new WAT pair in fixtures/debug_replace_gc_funcref_tag_*.wat.
Official wasm-tools parse/validate-all and print qualify the real modules;
non-Code declarations and all non-target bodies must be identical. A byte
oracle verifies the root stores occur only before the loop, so a generation-one
saved global and GC array retain the original function identity. Every call
uses the explicitly duplicate canonical function type via dynamic call_ref.
The two cases separately replace an inactive numeric leaf or throwing leaf.
Guest-memory results must change [12,12,22] to [13,13,22] or [12,12,23].
The GC payload struct is genuinely allocated and rooted once before the
loop; the replacement changes its field via struct.set and throws that same
rooted reference. Client admission/error waits therefore do not create an
unbounded stream of GC objects. The earlier same-signature wrong-tag catch
is unreachable: only the right instance may return the GC struct payload. This proves payload decoding across
native EH, not survival through an explicit collection, which this test does
not trigger. A valid new-body prefix followed by 0xff must first fail official
validation for that opcode, then fail the actual private replacement. Failed,
committed and stale-generation replacements directly queue old frame/scope/code
requests and require retirement without an idle poll. No guest GC address is
exported, and readMemory uses only the fixed Wasm linear-memory result slots.
Both diagnostic policies and both products use the same fixture namespace.
The author has performed source/AST review only; current native execution is
pending keeper qualification. These do not extend an existing IDE PASS claim.

Every adapter has its original Popen object and bounded raw DAP/JSON/stderr
records. Client disconnect preserves the VM-side authorized endpoint for a new
adapter. Cleanup signals the original broker, whose existing finally waits its
original guest; the test does not permanently quit an infinite guest then kill
a reused numeric PID. Cleanup failures remain failures.

Shortest intended keeper invocation (not executed by the author):

~~~sh
bash "$SOURCE/tools/ci/require_wasm3_test_cgroup.sh"
PYTHONDONTWRITEBYTECODE=1 python3 "$SOURCE/test/0018.debugger/run_dap_current_broker.py" \
  --source-root "$SOURCE" --uwvm "$FRESH_R3_CLI" \
  --build-receipt "$ORIGINAL_FRESH_R3_BUILD_RECEIPT" \
  --wasm-clang "$ACTUAL_WASM_CLANG" --wasm-ld "$ACTUAL_WASM_LD" \
  --wasm-tools "$ACTUAL_WASM_TOOLS" --llvm-dwarfdump "$ACTUAL_DWARFDUMP" \
  --case source-finish --out "$NEW_EVIDENCE_DIRECTORY"
~~~

Add --ros for the ROS product. Omit --case for all seven named cases, or select
a short subset explicitly. This Linux x86-64 runner cannot authorize Windows or
Mac tests. Those require current R3 PE/DLL/Job or arm64 SDK/MIG/Mach-O/owned-RSS
qualification and a separately reviewed fixed transport invocation.

Actual VS Code integration is a later distinct proof: use a real Extension
Development Host with --extensionDevelopmentPath and --extensionTestsPath,
vscode.debug.startDebugging and an actual DebugAdapterTrackerFactory. The
existing Node vscode stub and this live stdio DAP test cannot claim actual
VS Code pane acceptance. See the official
[extension testing entry](https://code.visualstudio.com/api/working-with-extensions/testing-extension).
