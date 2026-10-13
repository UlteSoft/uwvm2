"use strict";
/* Actual ExtensionDevelopmentHost tests. No vscode stub, VM/broker spawn,
 * native compiler, arbitrary host memory request or fabricated paused ID.
 * The sole keeper starts this host and its original broker in the existing
 * controlled Linux test group and retires that whole owned tree afterwards.
 */
const assert = require("assert");
const crypto = require("crypto");
const fs = require("fs");
const path = require("path");
const {execFileSync} = require("child_process");
const {TextDecoder} = require("util");
const vscode = require("vscode");

const TYPE = "uwvm-llvm-full";
const EXPECTED_SCOPE = "actual Linux broker/stdin DAP only; not actual VS Code, Windows or Mac acceptance";
const decoder = new TextDecoder("utf-8", {fatal: true});
const MAX_LOG = 8 * 1024 * 1024;
const MAX_MESSAGES = 16384;
const records = [];
let failure;
let raw;
let bytes = 0;
let output;
let input;
const sessions = new Map();
const terminated = new Set();
let qualifiedProduction;

function requireValue(value, label) { assert.ok(value, label); }
function sha(file) {
    const digest = crypto.createHash("sha256"), buffer = Buffer.alloc(65536);
    const fd = fs.openSync(file, "r");
    try {
        for (;;) {
            const count = fs.readSync(fd, buffer, 0, buffer.length, null);
            if (count === 0) break;
            digest.update(buffer.subarray(0, count));
        }
    } finally { fs.closeSync(fd); }
    return digest.digest("hex");
}
function boundedJson(file, budget) {
    const size = fs.statSync(file).size;
    requireValue(size > 0 && size <= budget, "bounded actual JSON input");
    return JSON.parse(decoder.decode(fs.readFileSync(file)));
}
function exactPath(file) {
    requireValue(typeof file === "string" && path.isAbsolute(file), "absolute actual source path");
    return fs.realpathSync(file);
}
function productionFileMap() {
    const production = {};
    function walk(directory) {
        for (const entry of fs.readdirSync(directory, {withFileTypes: true}).sort((a, b) => a.name.localeCompare(b.name))) {
            const file = path.join(directory, entry.name);
            if (entry.isDirectory()) walk(file);
            else if (fs.statSync(file).isFile() && entry.name !== ".DS_Store" && !entry.name.startsWith("._")) {
                production[path.relative(input.source_root, file)] = sha(file);
            }
        }
    }
    walk(path.join(input.source_root, "src"));
    return production;
}
function sameFileHashes(actual, expected) {
    return Object.keys(actual).length === Object.keys(expected).length &&
        Object.keys(actual).every(file => actual[file] === expected[file]);
}
function sourceDigest(production) {
    const entries = Object.entries(production).sort((a, b) => a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0);
    return {files: entries.length, sha256: crypto.createHash("sha256").update(JSON.stringify(entries)).digest("hex")};
}
function unsigned(value, label, nonzero = false) {
    requireValue(Number.isSafeInteger(value) && value >= (nonzero ? 1 : 0), label);
    return value;
}
function log(direction, session, message) {
    try {
        requireValue(records.length < MAX_MESSAGES, "actual tracker message budget");
        const value = {direction, session: session.id, message};
        const wire = Buffer.from(JSON.stringify(value) + "\n");
        requireValue(bytes + wire.length <= MAX_LOG, "actual tracker byte budget");
        fs.writeSync(raw, wire); bytes += wire.length; records.push(value);
    } catch (error) { failure = error; }
}
async function until(predicate, label, milliseconds = 25000) {
    const deadline = Date.now() + milliseconds;
    for (;;) {
        if (failure) throw failure;
        const value = predicate();
        if (value) return value;
        requireValue(Date.now() < deadline, label);
        await new Promise(resolve => setTimeout(resolve, 10));
    }
}
async function bounded(promise, label) {
    let timer;
    try {
        return await Promise.race([promise, new Promise((_, reject) => {
            timer = setTimeout(() => reject(new Error(label)), 25000);
        })]);
    } finally { clearTimeout(timer); }
}
function cgroup() {
    const guard = path.join(input.source_root, "tools/ci/require_wasm3_test_cgroup.sh");
    requireValue(sha(guard) === input.guard_sha256, "reviewed original guard bytes");
    // Only the already reviewed fixed Bash guard is spawned. It observes this
    // inherited cgroup; neither this test nor that guard creates a new harness.
    const result = execFileSync(input.bash, [guard], {encoding: "utf8", timeout: 10000});
    requireValue(Buffer.byteLength(result) <= 65536, "bounded actual guard output");
}
function u32(data, at, end) {
    let value = 0;
    for (let shift = 0; shift <= 28; shift += 7) {
        requireValue(at < end, "bounded fixture ULEB borrow");
        // [at,end) bounded owned Buffer; borrow byte then advance once.
        const byte = data[at++];
        requireValue(shift !== 28 || (byte & 0xf0) === 0, "fixture ULEB32 width");
        value += (byte & 0x7f) * 2 ** shift;
        if ((byte & 0x80) === 0) return [value, at];
    }
    throw new Error("overlong fixture ULEB");
}
function exportsOf(file) {
    const data = fs.readFileSync(file);
    requireValue(data.length >= 8 && data.length <= 262144 &&
        data.subarray(0, 8).equals(Buffer.from([0, 97, 115, 109, 1, 0, 0, 0])),
        "fixed verified fixture module/header budget");
    const functions = new Map();
    let at = 8;
    while (at < data.length) {
        const kind = data[at++];
        let size; [size, at] = u32(data, at, data.length);
        requireValue(size <= data.length - at, "bounded owned section range");
        const end = at + size;
        if (kind === 2) {
            const [count] = u32(data, at, end);
            requireValue(count === 0, "fixed IDE fixtures cannot contain imports");
        }
        if (kind === 7) {
            let count; [count, at] = u32(data, at, end);
            requireValue(count <= 4096, "fixed export count budget");
            for (let index = 0; index < count; ++index) {
                let length; [length, at] = u32(data, at, end);
                requireValue(length <= end - at && length <= 256, "bounded fixture export name");
                const name = decoder.decode(data.subarray(at, at + length)); at += length;
                requireValue(at < end, "bounded fixture export tag borrow");
                const tag = data[at++];
                let functionIndex; [functionIndex, at] = u32(data, at, end);
                if (tag === 0) {
                    requireValue(!functions.has(name), "duplicate fixture function export");
                    functions.set(name, functionIndex);
                }
            }
            requireValue(at === end, "exact fixture export section end");
        }
        // The owned section was bounds checked before moving to its next byte.
        at = end;
    }
    return functions;
}
function stopped(text) {
    const labels = [...text.matchAll(/^stop-id ([0-9]+)$/gm)];
    const positions = [...text.matchAll(/^thread ([0-9]+) module=([0-9]+) function=([0-9]+) byte-offset=([0-9]+) generation=([0-9]+)$/gm)];
    requireValue(text.startsWith("stopped: ") && labels.length === 1 && positions.length === 1,
                 "complete actual one-participant stop");
    const stop = BigInt(labels[0][1]);
    requireValue(stop > 0n && stop < (1n << 64n), "actual stop label range");
    const values = positions[0].slice(1).map(value => {
        const result = Number(value);
        requireValue(Number.isSafeInteger(result) && result >= 0, "safe actual DAP coordinate");
        return result;
    });
    requireValue(values[0] > 0 && values[1] === 0 && values[4] > 0, "actual selected fixture publication");
    return {thread: values[0], function: values[2], offset: values[3],
            generation: values[4], stop: stop.toString(), text};
}
async function request(session, command, arguments_) {
    return bounded(session.customRequest(command, arguments_), "actual IDE " + command + " timeout");
}
async function consoleCommand(session, text) {
    const reply = await request(session, "evaluate", {expression: text, context: "repl"});
    requireValue(reply && typeof reply.result === "string" && Buffer.byteLength(reply.result) <= 65536,
                 "bounded genuine console response");
    return reply.result + "\n";
}
async function rejectRequest(session, command, arguments_) {
    let rejected = false;
    try { await request(session, command, arguments_); }
    catch (error) { rejected = true; }
    requireValue(rejected, "actual IDE stale/read request unexpectedly succeeded: " + command);
}
async function begin(session, functionIndex) {
    await consoleCommand(session, "pause");
    const point = await consoleCommand(session, "break 0 " + functionIndex + " 0");
    const match = /^breakpoint ([0-9]+)\b/m.exec(point);
    requireValue(match !== null, "actual executable fixture breakpoint");
    await consoleCommand(session, "continue");
    let state;
    for (let index = 0; index < 20; ++index) {
        const text = await consoleCommand(session, "wait");
        if (text.startsWith("stopped: breakpoint")) {
            state = stopped(await consoleCommand(session, "status")); break;
        }
    }
    requireValue(state && state.function === functionIndex && state.offset === 0,
                 "actual fixture entry participant from current stop");
    await consoleCommand(session, "delete " + match[1]);
    return state;
}
async function frame(session, state) {
    const reply = await request(session, "stackTrace", {threadId: state.thread});
    requireValue(reply && Array.isArray(reply.stackFrames), "actual IDE frames");
    const physical = reply.stackFrames.filter(item =>
        item.name.startsWith("#0 ") || item.name.startsWith("Native JIT instruction"));
    requireValue(physical.length === 1, "one actual top physical IDE frame");
    unsigned(physical[0].id, "actual frame ID", true);
    return physical[0];
}
async function start(level) {
    const name = "UWVM Current " + level + " " + sessions.size;
    const mark = records.length;
    const configuration = {type: TYPE, request: "attach", name, socketDir: input.broker_directory,
                           moduleId: 0, pythonPath: input.python};
    if (level !== "source") configuration.stepLevel = level; // real schema/default selection
    const begun = await bounded(vscode.debug.startDebugging(undefined, configuration), "actual IDE attach timeout");
    requireValue(begun, "actual IDE refused attach");
    const session = await until(() => [...sessions.values()].find(value => value.name === name),
                                "actual IDE session start event");
    await until(() => vscode.debug.activeDebugSession && vscode.debug.activeDebugSession.id === session.id,
                "one actual focused debug session");
    await until(() => records.slice(mark).find(row => row.session === session.id &&
        row.direction === "adapter" && row.message.type === "response" &&
        row.message.command === "configurationDone" && row.message.success === true),
        "actual IDE configurationDone was not acknowledged");
    return session;
}
async function end(session) {
    await bounded(vscode.debug.stopDebugging(session), "actual IDE stopDebugging timeout");
    await until(() => terminated.has(session.id), "actual IDE session did not terminate");
    requireValue(records.some(row => row.session === session.id && row.direction === "IDE" &&
        row.message.type === "request" && row.message.command === "disconnect"),
        "actual IDE did not send disconnect");
}
async function uiStep(session, command, dapCommand, state) {
    const mark = records.length;
    // Built-in command IDs come from current official debugCommands.ts. The
    // tracker proves which actual participant/granularity the UI selected.
    await bounded(vscode.commands.executeCommand(command), "actual IDE step action timeout");
    const response = await until(() => records.slice(mark).find(row => row.session === session.id &&
        row.direction === "adapter" && row.message.type === "response" &&
        row.message.command === dapCommand), "actual UI action did not send " + dapCommand);
    requireValue(response.message.success === true, "actual UI step failed");
    const sent = records.slice(mark).filter(row => row.session === session.id && row.direction === "IDE" &&
        row.message.type === "request" && row.message.command === dapCommand);
    requireValue(sent.length === 1 && sent[0].message.seq === response.message.request_seq &&
        sent[0].message.arguments.threadId === state.thread, "actual UI selected another participant");
    return {request: sent[0].message, response: response.message, mark};
}
async function sourceFinish(report) {
    const row = input.case_row;
    const exports = exportsOf(input.fixture);
    const leaf = exports.get("source_step_leaf"), outer = exports.get("source_step_outer");
    unsigned(leaf, "official source leaf"); unsigned(outer, "official source caller");
    const source = fs.readFileSync(input.fixture_source, "utf8").split(/\r?\n/);
    const markers = source.map((line, index) => line.includes("STEP_LEAF_ENTRY") ? index + 1 : 0).filter(Boolean);
    requireValue(markers.length === 1, "official source marker");
    const priorOrigin = row.positions.filter(p => p.function === leaf && p.line === markers[0] && p.is_statement);
    const priorAfter = row.positions.at(-1);
    requireValue(priorOrigin.length > 0 && priorAfter.function === outer && priorAfter.is_statement &&
        priorAfter.inline.length === 0, "previous officially mapped fixture goldens");
    const origin = priorOrigin.at(-1);
    const session = await start("source");
    try {
        let state = await begin(session, leaf);
        let found = false;
        for (let index = 0; index < 512; ++index) {
            const trace = await consoleCommand(session, "bt " + state.thread);
            state = stopped(trace);
            if (state.function === leaf && state.offset === origin.offset) { found = true; break; }
            await consoleCommand(session, "step wasm " + state.thread);
        }
        requireValue(found, "actual IDE could not reach the verified source origin");
        const current = await frame(session, state);
        requireValue(current.source && path.basename(current.source.path) === path.basename(input.fixture_source) &&
            current.line === origin.line && current.column === origin.column, "IDE origin differs from official mapped row");
        const step = await uiStep(session, "workbench.action.debug.stepOut", "stepOut", state);
        const granularity = step.request.arguments.granularity;
        requireValue(granularity === undefined || granularity === "statement" || granularity === "line",
                     "source UI unexpectedly selected native instructions");
        const next = stopped(await consoleCommand(session, "bt " + state.thread));
        requireValue(BigInt(next.stop) > BigInt(state.stop) && next.function === outer &&
            next.offset === priorAfter.offset, "real source finish did not reach verified caller PC");
        const destination = await frame(session, next);
        requireValue(destination.source &&
            path.basename(destination.source.path) === path.basename(input.fixture_source) &&
            destination.line === priorAfter.line && destination.column === priorAfter.column,
            "IDE destination differs from official mapped statement row");
        await rejectRequest(session, "scopes", {frameId: current.id});
        report.case_result = {actual_origin: state, actual_destination: next,
            official_origin: origin, official_destination: priorAfter,
            actual_ui_step: step, source_default_omitted: true};
    } finally { await end(session); }
}
async function wasmNative(report) {
    const functions = exportsOf(input.fixture), startIndex = functions.get("_start");
    unsigned(startIndex, "official numeric loop entry");
    const wasm = await start("wasm");
    let state;
    try {
        state = await begin(wasm, startIndex);
        const step = await uiStep(wasm, "workbench.action.debug.stepInto", "stepIn", state);
        requireValue(step.request.arguments.granularity === undefined || step.request.arguments.granularity === "statement",
                     "Wasm UI selected a different level");
        const next = stopped(await consoleCommand(wasm, "status"));
        requireValue(next.function === startIndex && state.offset === 0 && next.offset === 2 &&
            BigInt(next.stop) > BigInt(state.stop), "IDE one Wasm opcode differs from verified loop");
        report.wasm_step = {before: state, after: next, actual_ui_step: step}; state = next;
    } finally { await end(wasm); }
    const native = await start("native");
    try {
        state = stopped(await consoleCommand(native, "status"));
        const current = await frame(native, state), reference = current.instructionPointerReference;
        requireValue(typeof reference === "string" && reference.startsWith("uwvm-native-code:"),
                     "actual cooperative readonly native owner did not qualify");
        const window = await request(native, "disassemble", {memoryReference: reference,
            instructionOffset: -200, instructionCount: 400, resolveSymbols: true});
        requireValue(window.instructions.length === 400 &&
            window.instructions[200].presentationHint === "normal" &&
            window.instructions[200].instructionBytes && window.instructions[200].symbol === "window",
            "actual IDE native request lacks verified owner window/name");
        requireValue(window.instructions.every(row => row.presentationHint !== "invalid" ||
            row.address === "-1" && row.instructionBytes === undefined), "invalid filler fabricated a PC");
        const first = (await request(native, "disassemble", {memoryReference: reference, instructionCount: 1})).instructions[0];
        const step = await uiStep(native, "workbench.action.debug.stepInto", "stepIn", state);
        const granularity = step.request.arguments.granularity;
        requireValue(granularity === undefined || granularity === "statement" || granularity === "instruction",
                     "native UI selected another level");
        const next = stopped(await consoleCommand(native, "status"));
        const outputs = records.slice(step.mark).filter(row => row.session === native.id &&
            row.direction === "adapter" && row.message.type === "event" && row.message.event === "output")
            .map(row => row.message.body.output);
        const executed = outputs.map(text => /native instruction 0x([0-9a-fA-F]+) bytes=([0-9a-fA-F ]+)  ([^\r\n]+?) -> 0x([0-9a-fA-F]+)/.exec(text)).filter(Boolean);
        requireValue(executed.length === 1 && BigInt(first.address) === BigInt("0x" + executed[0][1]) &&
            first.instructionBytes === executed[0][2] && BigInt(next.stop) > BigInt(state.stop),
            "actual UI native execution differs from copied instruction");
        const pcs = [...next.text.matchAll(/^  native-pc=0x([0-9a-fA-F]+)$/gm)];
        requireValue(pcs.length === 1 && BigInt("0x" + pcs[0][1]) === BigInt("0x" + executed[0][4]),
                     "actual native TO differs from stopped PC");
        await rejectRequest(native, "disassemble", {memoryReference: reference, instructionCount: 1});
        const trap = await frame(native, next);
        requireValue(trap.source === undefined && trap.instructionPointerReference.startsWith("uwvm-native-stop:"),
                     "native trap reused source permission");
        requireValue((await request(native, "scopes", {frameId: trap.id})).scopes.length === 0,
                     "native trap gained local scope");
        await rejectRequest(native, "readMemory", {memoryReference: trap.instructionPointerReference, count: 1});
        report.native_step = {before: state, after: next, actual_ui_step: step,
            copied_instruction: first, actual_execution: executed[0].slice(1), window_count: 400,
            pane_rendering_qualified: false};
    } finally { await end(native); }
}
function verifyInput() {
    requireValue(process.platform === "linux" && process.arch === "x64", "keeper Linux x86-64 only");
    requireValue(input.schema === 1 && ["source-finish", "wasm-native"].includes(input.case) &&
        ["instruction", "unwind"].includes(input.policy), "fixed current IDE test schema/case");
    input.source_root = exactPath(input.source_root);
    requireValue(exactPath(__filename) === path.join(input.source_root, "test/0018.debugger/vscode_current_broker.cjs"),
                 "actual extension test source path");
    input.python = exactPath(input.python); input.bash = exactPath(input.bash);
    cgroup();
    const extension = vscode.extensions.getExtension("uwvm.uwvm-llvm-full-debug");
    requireValue(extension && exactPath(extension.extensionPath) === path.join(input.source_root, "tools/debug"),
                 "real production extension discovery/path");
    for (const [name, digest] of Object.entries(input.extension_sha256)) {
        requireValue(["extension.js", "package.json", "dap_adapter.py"].includes(name) &&
            sha(path.join(extension.extensionPath, name)) === digest, "actual frozen extension source " + name);
    }
    requireValue(Object.keys(input.extension_sha256).length === 3, "complete actual extension entry closure");
    input.evidence_summary = exactPath(input.evidence_summary);
    const prior = boundedJson(input.evidence_summary, 8 * 1024 * 1024);
    requireValue(prior.passed === true && prior.scope === EXPECTED_SCOPE, "real current broker acceptance prerequisite");
    const rows = prior.cases.filter(row => row.case === input.case && row.policy === input.policy && row.passed);
    requireValue(rows.length === 1, "actual selected named case/policy qualification");
    input.case_row = rows[0];
    requireValue(JSON.stringify(prior.inputs_before) === JSON.stringify(prior.inputs_after) &&
        JSON.stringify(prior.production_before) === JSON.stringify(prior.production_after),
        "prior actual source/tool/product closure remained unchanged");
    const production = productionFileMap();
    requireValue(sameFileHashes(production, prior.production_after),
        "actual complete source root differs from original fresh product");
    qualifiedProduction = production;
    const directory = path.dirname(input.evidence_summary);
    const selected = input.case === "source-finish" ? "c5-loop.wasm" : "native-loop.wasm";
    input.fixture = exactPath(path.join(directory, selected));
    requireValue(sha(input.fixture) === input.case_row.fixture_sha256, "actual fixed fixture qualification");
    input.fixture_source = path.join(directory, "debug_source_step_c.c");
    const artifacts = new Map();
    for (const record of prior.artifact_receipts) {
        requireValue(typeof record.path === "string" && !path.isAbsolute(record.path) &&
            !record.path.split(/[\\/]/).includes("..") && !artifacts.has(record.path), "owned prior artifact path");
        artifacts.set(record.path, record.sha256);
    }
    for (const file of input.case === "source-finish" ?
        [selected, "debug_source_step_c.c", "source-oracle.log", "verify-source-dwarf.log"] : [selected]) {
        const actual = exactPath(path.join(directory, file));
        requireValue(path.dirname(actual) === directory && artifacts.has(file) &&
            sha(actual) === artifacts.get(file), "actual official fixture/oracle artifact " + file);
    }
    const buildPath = Object.keys(prior.inputs_before).filter(file => path.isAbsolute(file) &&
        file !== input.evidence_summary && file.endsWith(".json")).filter(file => {
        try {
            const candidate = boundedJson(file, 8 * 1024 * 1024);
            return candidate.binary_path && candidate.binary_sha256 && candidate.link_argv;
        } catch { return false; }
    });
    requireValue(buildPath.length === 1, "one original qualified product link receipt");
    const build = boundedJson(buildPath[0], 8 * 1024 * 1024);
    requireValue(sha(build.source_before_file) === sha(build.source_after_file) &&
        sha(buildPath[0]) === prior.inputs_before[buildPath[0]] &&
        build.link_returncode === 0 && build.binary_sha256 === prior.product_sha256 &&
        sha(exactPath(build.binary_path)) === prior.product_sha256,
        "current actual binary/original link qualification");
    requireValue(Array.isArray(input.broker_actual_argv) && Array.isArray(input.case_row.cleanup.actual_argv),
                 "original owned broker argv required");
    const original = input.case_row.cleanup.actual_argv.slice();
    const index = original.indexOf("--socket-dir");
    requireValue(index >= 0 && original.indexOf("--socket-dir", index + 1) < 0, "one original broker directory");
    input.broker_directory = exactPath(input.broker_directory);
    original[index + 1] = input.broker_directory;
    requireValue(sha(input.python) === sha(exactPath(original[0])), "same actually qualified host Python");
    requireValue(JSON.stringify(original) === JSON.stringify(input.broker_actual_argv),
                 "fresh original broker must use exact qualified product/fixture/policy argv");
    const owner = fs.statSync(input.broker_directory);
    requireValue(owner.isDirectory() && owner.uid === process.getuid() && (owner.mode & 0o077) === 0,
                 "private owner broker directory");
    const socket = fs.lstatSync(path.join(input.broker_directory, "control.sock"));
    const capability = fs.lstatSync(path.join(input.broker_directory, "capability"));
    requireValue(socket.isSocket() && socket.uid === process.getuid() && (socket.mode & 0o077) === 0 &&
        capability.isFile() && capability.uid === process.getuid() && (capability.mode & 0o077) === 0,
        "real private broker files; capability is never read or logged by this test");
    return {original_build_receipt: buildPath[0], original_build_receipt_sha256: sha(buildPath[0]),
        actual_binary_path: exactPath(build.binary_path), product_sha256: prior.product_sha256,
        original_source_before_file: exactPath(build.source_before_file),
        original_source_after_file: exactPath(build.source_after_file),
        original_source_before_sha256: sha(build.source_before_file),
        original_source_after_sha256: sha(build.source_after_file),
        actual_source_before: sourceDigest(production), actual_guard_sha256: sha(path.join(input.source_root, "tools/ci/require_wasm3_test_cgroup.sh")),
        actual_fixture_sha256: sha(input.fixture), prior_evidence_sha256: sha(input.evidence_summary),
        actual_python_sha256: sha(input.python), actual_bash_sha256: sha(input.bash),
        actual_extension: Object.fromEntries(Object.keys(input.extension_sha256).map(name =>
            [name, sha(path.join(extension.extensionPath, name))])),
        broker_argv_scope: "keeper's actual original Popen required; JSON is not identity/launch authority"};
}
async function run() {
    const filename = process.env.UWVM2_VSCODE_TEST_INPUT;
    requireValue(filename, "keeper fixed UWVM2_VSCODE_TEST_INPUT required");
    const inputFile = exactPath(filename);
    input = boundedJson(inputFile, 65536);
    output = path.resolve(input.output_directory);
    requireValue(path.isAbsolute(input.output_directory) && !fs.existsSync(output), "fresh actual IDE evidence directory");
    fs.mkdirSync(output, {mode: 0o700});
    raw = fs.openSync(path.join(output, "vscode-dap.raw.jsonl"), "wx", 0o600);
    const report = {passed: false, scope: "actual Linux ExtensionDevelopmentHost + standard UI stepping; not disassembly pane rendering",
                    case: input.case, policy: input.policy, actual_input_sha256: sha(inputFile),
                    actual_vscode_version: vscode.version, actual_ide_node_executable: process.execPath,
                    actual_ide_node_executable_sha256: sha(process.execPath), actual_test_source_sha256: sha(__filename)};
    const disposables = [];
    try {
        report.qualification = verifyInput();
        requireValue(!vscode.debug.activeDebugSession, "no other debug session may be mixed");
        disposables.push(vscode.debug.onDidStartDebugSession(session => {
            if (session.type === TYPE) { sessions.set(session.id, session); }
        }));
        disposables.push(vscode.debug.onDidTerminateDebugSession(session => {
            if (session.type === TYPE) { terminated.add(session.id); }
        }));
        disposables.push(vscode.debug.registerDebugAdapterTrackerFactory(TYPE, {
            createDebugAdapterTracker(session) {
                return {
                    onWillReceiveMessage(message) { log("IDE", session, message); },
                    onDidSendMessage(message) { log("adapter", session, message); },
                    onError(error) { failure = error; log("tracker-error", session, {message: String(error)}); },
                    onExit(code, signal) { log("adapter-exit", session, {code, signal}); }
                };
            }
        }));
        if (input.case === "source-finish") await sourceFinish(report);
        else await wasmNative(report);
        if (failure) throw failure;
        cgroup();
        requireValue([...sessions.keys()].every(id => terminated.has(id)), "every actual IDE session retired");
        requireValue(report.qualification.actual_python_sha256 === sha(input.python) &&
            report.qualification.actual_bash_sha256 === sha(input.bash), "actual host tools changed");
        requireValue(report.actual_input_sha256 === sha(inputFile), "IDE input changed during actual test");
        requireValue(report.qualification.prior_evidence_sha256 === sha(input.evidence_summary), "original evidence changed");
        requireValue(report.qualification.actual_fixture_sha256 === sha(input.fixture), "fixture changed during IDE test");
        requireValue(report.actual_test_source_sha256 === sha(__filename), "IDE test source changed");
        requireValue(report.actual_ide_node_executable_sha256 === sha(process.execPath), "actual Electron executable changed");
        requireValue(report.qualification.actual_guard_sha256 === sha(path.join(input.source_root, "tools/ci/require_wasm3_test_cgroup.sh")),
                     "actual reviewed guard changed");
        requireValue(report.qualification.product_sha256 === sha(report.qualification.actual_binary_path),
                     "actual product binary changed during IDE test");
        requireValue(report.qualification.original_build_receipt_sha256 === sha(report.qualification.original_build_receipt) &&
            report.qualification.original_source_before_sha256 === sha(report.qualification.original_source_before_file) &&
            report.qualification.original_source_after_sha256 === sha(report.qualification.original_source_after_file),
            "actual original link/source receipts changed");
        const productionAfter = productionFileMap();
        requireValue(sameFileHashes(productionAfter, qualifiedProduction), "complete production source changed during IDE test");
        report.qualification.actual_source_after = sourceDigest(productionAfter);
        for (const [name, digest] of Object.entries(input.extension_sha256)) {
            requireValue(sha(path.join(input.source_root, "tools/debug", name)) === digest,
                         "extension changed during actual session");
        }
        report.passed = true;
    } catch (error) { report.error = String(error && error.stack || error); throw error; }
    finally {
        for (const session of sessions.values()) {
            if (!terminated.has(session.id)) {
                try { await end(session); }
                catch (error) { report.passed = false; report.cleanup_error = String(error); }
            }
        }
        for (const disposable of disposables.reverse()) disposable.dispose();
        fs.closeSync(raw);
        report.actual_tracker_messages = records.length; report.actual_tracker_bytes = bytes;
        report.raw_sha256 = sha(path.join(output, "vscode-dap.raw.jsonl"));
        report.retirement_scope = "actual DebugSession/disconnect only; keeper separately proves original broker/VM/Electron tree retirement";
        fs.writeFileSync(path.join(output, "vscode-summary.json"), JSON.stringify(report, null, 2) + "\n", {flag: "wx", mode: 0o600});
    }
    requireValue(report.passed, "actual IDE test or cleanup failed");
    console.log("PASS actual current ExtensionDevelopmentHost fixed case; no pane/platform/global debugger claim");
}
module.exports = {run};
