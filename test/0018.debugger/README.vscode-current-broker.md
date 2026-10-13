# Actual VS Code ExtensionDevelopmentHost slice

This is source-only test work. No local Node parse, project import, VS Code,
broker, VM, compiler, SSH or native test has been executed by its author.
Historical R3c/R5 and replacement r1/r2/r3 packets remain unchanged.

The new CommonJS test exports run() for the official ExtensionDevelopmentHost
test entry. It uses real vscode APIs, the actual production extension descriptor
factory, and DebugAdapterTrackerFactory requests/responses/events. There is no
vscode stub. Its first case calls the actual workbench stepOut action from a
verified C5 leaf statement with the source level omitted, then checks the real
caller PC and line/column against the prior independently verified official
Code/line goldens. A second fixed case uses the workbench stepInto action for one
Wasm opcode and one native instruction, a -200/400/resolveSymbols window,
copied/executed bytes, retired references and absent native source/local/read
permissions. It does not invoke or claim a rendered DisassemblyView pane.

This module never starts a broker, VM, compiler, QEMU, X server or IDE. It only
spawns the already reviewed fixed Bash cgroup guard and uses the actual IDE APIs.
The sole keeper must first own the actual current R3 product, fixed official
fixture, original broker and IDE processes under the existing 64GiB/no-swap
Linux test group. Broker argv and JSON reports are provenance inputs, not
process identity, launch permission, activation IDs or code-reading credentials.
Every fresh thread/stop/frame/reference comes from a real current broker reply.
The test never reads the capability file.

Run the real current-broker driver first, at least the selected named case under
the selected instruction/unwind policy. Its summary must have passed=true, its
original complete production/source/dependency and actual link qualification,
and its official fixture/oracle artifacts unchanged. Use the reviewed fixed
replacement r3 adapter overlay; this is a tools change and does not alter runtime
layout or authorize an old pre-R3 product.

The keeper then starts another original broker Popen with exactly the successful
case's cleanup.actual_argv, changing only its --socket-dir argument to a fresh
private owner directory. That prior broker has already retired; its PID or
numeric stop labels must never be reused. The new broker must serve the exact
same officially verified fixture and policy. Linux -m run is required for the
inherited authorized channel. The test does not borrow the local -m debug-jit
console or pretend that an old finished process is still attached.

The input is a private schema-1 JSON file. It contains no capability, old thread,
old stop, frame, address or expression credential:

~~~json
{
  "schema": 1,
  "source_root": "/actual/reviewed/source",
  "evidence_summary": "/actual/current-broker-evidence/summary.json",
  "case": "source-finish",
  "policy": "instruction",
  "broker_directory": "/actual/new/private/broker",
  "broker_actual_argv": ["EXACT", "KEEPER", "ORIGINAL", "Popen", "ARGV"],
  "python": "/actual/qualified/python3",
  "bash": "/actual/bash",
  "guard_sha256": "ACTUAL_FROZEN_GUARD_SHA256",
  "extension_sha256": {
    "extension.js": "ACTUAL_FROZEN_EXTENSION_SHA256",
    "package.json": "ACTUAL_FROZEN_PACKAGE_SHA256",
    "dap_adapter.py": "ACTUAL_REVIEWED_RETIREMENT_R3_ADAPTER_SHA256"
  },
  "output_directory": "/actual/new/ide-evidence"
}
~~~

broker_actual_argv must be captured from that original fresh broker Popen,
not reconstructed to masquerade as an actual invocation. The script compares
the qualified fixed arguments, private socket/file metadata and actual binary,
fixture, source tree, guard and extension bytes. The keeper's independent
process ownership, complete TU layout/LLVM23 MC+DWARF closure, original source
and link receipts are still mandatory; labels and hashes alone cannot establish
them. For the second case set case to wasm-native and provide its actual
previous successful fixture/policy broker argv.

Use a pinned actual Linux VS Code installation and its real executable/version
and dependency closure. The existing Node stub is not that installation.
An isolated empty extensions directory and separate private user-data/workspace
avoid mixing another installed extension or debug session. Use an existing
qualified display, or a keeper-owned Xvfb inside the same inherited test group;
this module grants no additional process-spawn or resource permissions.
Do not add --no-sandbox to make an unavailable environment appear qualified.
Workspace Trust must actually admit this known test workspace; the production
extension does not support an untrusted workspace.

Intended official entry, only after keeper qualification (not executed here):

~~~sh
bash "$SOURCE/tools/ci/require_wasm3_test_cgroup.sh"
UWVM2_VSCODE_TEST_INPUT="$PRIVATE_ACTUAL_INPUT" \
  "$ACTUAL_VSCODE_EXECUTABLE" \
  --user-data-dir "$PRIVATE_USER_DATA" --extensions-dir "$EMPTY_EXTENSIONS" \
  --disable-extensions --skip-welcome --skip-release-notes \
  --extensionDevelopmentPath "$SOURCE/tools/debug" \
  --extensionTestsPath "$SOURCE/test/0018.debugger/vscode_current_broker.cjs" \
  "$TRUSTED_FIXED_WORKSPACE"
~~~

Preserve exact IDE argv, executable/version SHA, source hashes, input, standard
streams, raw tracker JSONL and vscode-summary.json. A passing module qualifies
only the actual ExtensionDevelopmentHost/API/action slice. It proves actual
DebugSession disconnect, not Electron/adapter/broker/guest tree retirement.
The sole keeper must wait and check its original owned process tree and cgroup
before declaring the whole case passed. Cleanup failures remain failures.

The r2 source candidate also compares complete src file maps before/after the
real session and retains canonical sorted-map digests and file counts. It checks
the actual binary, original link/source receipts, this test, fixed guard and
Electron executable after execution, alongside existing fixture/extension/tool
pins. These are provenance checks, not a proof of Electron dependency closure,
process identity or resource custody; those remain the keeper's separate duties.
No r1 historical packet is rewritten and neither candidate is claimed executed.

Windows and macOS are explicitly unsupported by this Linux test module.
They require their current R3 PE/DLL/Job or arm64 SDK/MIG/Mach-O/owned-memory
product and transport qualification. The macOS fixed CLI owner does not
authorize spawning an IDE or broker tree.

Primary references:
- [Official extension testing entry](https://code.visualstudio.com/api/working-with-extensions/testing-extension):
  ExtensionDevelopmentHost and --extensionDevelopmentPath/--extensionTestsPath.
- [Current official vscode API declarations](https://raw.githubusercontent.com/microsoft/vscode/main/src/vscode-dts/vscode.d.ts):
  DebugSession.customRequest, startDebugging, stopDebugging and tracker factory.
- [Actual workbench step action identifiers](https://raw.githubusercontent.com/microsoft/vscode/main/src/vs/workbench/contrib/debug/browser/debugCommands.ts):
  workbench.action.debug.stepInto and workbench.action.debug.stepOut.
- [Actual disassembly loading implementation](https://raw.githubusercontent.com/microsoft/vscode/main/src/vs/workbench/contrib/debug/browser/disassemblyView.ts):
  initial -200/400, baseline and scroll requests. Those real pane paths remain
  separate from this API/action test and are not fabricated here.
