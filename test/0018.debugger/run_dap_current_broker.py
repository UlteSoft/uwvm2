#!/usr/bin/env python3
"""Current Linux broker/DAP product acceptance; no formatter test doubles.

Keeper only, inside the existing 64GiB/no-swap cgroup. Fresh original R3
runtime/main/source/link qualification is a prerequisite, not inferred from a
binary hash. Windows/Mac/actual VS Code UI are outside this Linux test driver.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import selectors
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "0017.runtime"))
import run_debug_source_step_cli as source_cli


def sha(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def require(value, message, detail=None):
    if not value:
        raise AssertionError((message, detail))


class Client:
    """Actual DAP stdio with bounded framing and exact queued response IDs."""
    def __init__(self, adapter: Path, directory: Path, folder: Path, level: str):
        self.command = [sys.executable, str(adapter)]
        self.stderr = (folder / (level + "-adapter.stderr")).open("wb")
        self.child = subprocess.Popen(self.command, stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE, stderr=self.stderr, bufsize=0)
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.child.stdout, selectors.EVENT_READ)
        self.pending = bytearray()
        self.raw = bytearray()
        self.events = []
        self.responses = {}
        self.requests = []
        self.sequence = 0
        self.folder, self.level = folder, level
        try:
            self.request("initialize", {"adapterID": "uwvm-llvm-full"})
            attach = {"socketDir": str(directory), "moduleId": 0}
            if level != "source":
                attach["stepLevel"] = level
            self.request("attach", attach)
            self.request("configurationDone")
        except BaseException:
            # Retain the original Popen object even when attach fails; closing
            # stdin asks this owned adapter to retire its broker connection.
            self.child.stdin.close()
            try:
                try:
                    self.child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    self.child.terminate()  # original unreaped adapter Popen, never a guest PID
                    self.child.wait(timeout=5)
                    raise
            finally:
                self.selector.close()
                self.stderr.close()
                self.write_receipt()
            raise

    def queue(self, commands):
        wire, numbers = bytearray(), []
        for command, arguments in commands:
            self.sequence += 1
            require(self.sequence <= 16384, "bounded DAP request count")
            request = {"seq": self.sequence, "type": "request", "command": command,
                       "arguments": arguments}
            encoded = json.dumps(request, separators=(",", ":")).encode()
            require(len(encoded) <= 1048576, "bounded DAP input")
            wire.extend(b"Content-Length: " + str(len(encoded)).encode() + b"\r\n\r\n" + encoded)
            self.requests.append(request)
            numbers.append(self.sequence)
        # A single write/flush queues replacement and stale-ID requests together;
        # no external status request or deliberate idle delay separates them.
        require(self.child.stdin.write(wire) == len(wire), "actual DAP input write was partial")
        self.child.stdin.flush()
        return numbers

    def next_message(self):
        deadline = time.monotonic() + 25
        while True:
            boundary = self.pending.find(b"\r\n\r\n")
            if boundary >= 0:
                require(boundary <= 4096, "bounded DAP output header")
                header = bytes(self.pending[:boundary])
                require(re.fullmatch(rb"Content-Length: [0-9]{1,7}", header) is not None,
                        "exact actual adapter header", header)
                count = int(header[16:])
                require(2 <= count <= 1048576, "bounded DAP output message")
                end = boundary + 4 + count
                if len(self.pending) >= end:
                    value = json.loads(self.pending[boundary + 4:end])
                    del self.pending[:end]
                    require(isinstance(value, dict), "DAP output object")
                    return value
            else:
                require(len(self.pending) <= 4096, "incomplete DAP header budget")
            require(time.monotonic() < deadline, "actual DAP output timeout", self.events[-8:])
            require(self.selector.select(max(0, deadline - time.monotonic())), "actual DAP selector timeout")
            part = os.read(self.child.stdout.fileno(), 65536)
            require(part, "actual adapter closed before reply", self.child.poll())
            self.pending.extend(part)
            self.raw.extend(part)
            require(len(self.raw) <= 8 * 1024 * 1024, "bounded actual DAP transcript")

    def absorb(self, value):
        if value.get("type") == "event":
            self.events.append(value)
            require(len(self.events) <= 16384, "bounded DAP events")
        else:
            require(value.get("type") == "response" and type(value.get("request_seq")) is int,
                    "actual DAP response shape", value)
            request_seq = value["request_seq"]
            require(0 < request_seq <= self.sequence and request_seq not in self.responses,
                    "duplicate/unrequested DAP response", value)
            require(value.get("command") == self.requests[request_seq - 1]["command"],
                    "actual DAP response command mismatch", value)
            self.responses[request_seq] = value

    def response(self, number):
        while number not in self.responses:
            self.absorb(self.next_message())
        return self.responses[number]

    def request(self, command, arguments=None, success=True):
        number = self.queue([(command, arguments or {})])[0]
        reply = self.response(number)
        require(reply.get("success") is success, "actual DAP request result", (command, reply))
        return reply

    def evaluate(self, command):
        reply = self.request("evaluate", {"expression": command, "context": "repl"})
        value = reply.get("body", {}).get("result")
        require(isinstance(value, str) and len(value.encode()) <= 65536,
                "bounded genuine console response")
        return (value + "\n").encode()

    def close(self):
        try:
            if self.child.poll() is None:
                try:
                    self.request("disconnect")
                finally:
                    self.child.stdin.close()
                    try:
                        self.child.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        self.child.terminate()  # original unreaped adapter; fail even if forced retirement succeeds
                        self.child.wait(timeout=5)
                        raise
            require(self.child.returncode == 0, "actual adapter exit", self.child.returncode)
        finally:
            self.selector.close()
            self.stderr.close()
            self.write_receipt()

    def write_receipt(self):
        # Successful retirement permits a real EOF drain, including events
        # emitted after the disconnect response. A live/failed child is never
        # read with a blocking EOF assumption and never reported fully drained.
        drained = self.child.poll() is not None
        if drained:
            tail = self.child.stdout.read(8 * 1024 * 1024 - len(self.raw) + 1)
            self.raw.extend(tail)
            self.pending.extend(tail)
            # Retain raw bytes even if final framing is malformed. The read
            # itself is bounded to the budget plus one diagnostic byte.
            (self.folder / (self.level + "-dap.raw")).write_bytes(self.raw)
            require(len(self.raw) <= 8 * 1024 * 1024, "actual DAP EOF transcript budget")
            while self.pending:
                self.absorb(self.next_message())
        else:
            (self.folder / (self.level + "-dap.raw")).write_bytes(self.raw)
        (self.folder / (self.level + "-dap.json")).write_text(json.dumps({
            "actual_argv": self.command, "requests": self.requests,
            "responses": [self.responses[key] for key in sorted(self.responses)],
            "events": self.events, "returncode": self.child.returncode,
            "actual_eof_drained": drained}, indent=2) + "\n")


class ConsoleProxy:
    def __init__(self, client: Client):
        self.client = client
        self.command = client.command
        self.transcript = bytearray()

    def send(self, command):
        reply = self.client.evaluate(command)
        self.transcript.extend(reply)
        require(len(self.transcript) <= 8 * 1024 * 1024, "bounded copied console transcript")
        return reply


class SourceSession(source_cli.Session):
    """Reuse the official Code/line oracle; real stepping goes through DAP."""
    def __init__(self, client, wasm, sequences, source):
        self.console = ConsoleProxy(client)
        self.client = client
        self.expressions = source_cli.code_expressions(wasm)
        self.sequences, self.source = sequences, source
        self.thread, self.breakpoint, self.last_step_stop = 0, 0, 0
        self.actions, self.positions = [], []

    def begin(self, function):
        self.send("pause")
        point = self.send(f"break 0 {function} 0")
        identifier = re.search(rb"breakpoint ([0-9]+)", point)
        require(identifier is not None, "actual broker executable breakpoint", point)
        self.breakpoint = int(identifier[1])
        self.send("continue")
        for _ in range(20):
            stopped = self.send("wait")
            if b"stopped: breakpoint" in stopped:
                break
        else:
            raise AssertionError("actual broker breakpoint timeout")
        match = re.search(rb"thread ([0-9]+) module=0 function=" + str(function).encode() +
                          rb" byte-offset=0 generation=", stopped)
        require(match is not None, "actual target participant from stopped reply", stopped)
        self.thread = int(match[1])
        self.send(f"delete {self.breakpoint}")

    def check_stack(self, position):
        frames = self.client.request("stackTrace", {"threadId": self.thread})["body"]["stackFrames"]
        reference = f"wasm:0:{position['function']}:{position['offset']}"
        physical = [row for row in frames if row.get("instructionPointerReference") == reference]
        require(len(physical) == 1, "one real current physical DAP frame", frames)
        frame = physical[0]
        source_rows = [row for row in frames if "source" in row and row.get("line", 0) > 0]
        require(len(source_rows) == 1 and
                Path(source_rows[0]["source"].get("path", "")).name == self.source.name and
                (source_rows[0]["line"], source_rows[0]["column"]) == (position["line"], position["column"]),
                "actual DAP source disagrees with independent official line oracle", (position, frames))
        inline = frames[:frames.index(frame)]
        labels_only = bool(inline) and inline[0]["name"].startswith("Inlined ")
        expected = position["inline_display_inner_to_outer"]
        if not labels_only:
            expected = [name.split(" call-site ", 1)[0] for name in expected]
        require([row["name"].removeprefix("Inlined ") for row in inline] == expected,
                "actual DAP inline display chain", (frames, position))
        for row in inline:
            require((not labels_only or "source" not in row) and "instructionPointerReference" not in row and
                    row.get("canRestart") is False, "inline label gained extra permission", row)
        for caller in frames[frames.index(frame) + 1:]:
            if "(saved caller values)" not in caller["name"]:
                continue
            scopes = self.client.request("scopes", {"frameId": caller["id"]})["body"]["scopes"]
            require([scope["name"] for scope in scopes] == ["Source variables"],
                    "saved caller must not receive current Wasm/native scopes", scopes)
            values = self.client.request("variables", {"variablesReference": scopes[0]["variablesReference"]})
            require(all("memoryReference" not in value for value in values["body"]["variables"]),
                    "copied saved caller values gained an address capability", values)
        return frame

    def source_step(self, policy):
        before = self.position()
        request = {"into": "stepIn", "over": "next", "out": "stepOut"}[policy]
        # Omit granularity deliberately: real attach.stepLevel=source must map
        # the IDE's default statement request to the actual controller policy.
        self.client.request(request, {"threadId": self.thread})
        after = self.position()
        require(after["stop_id"] > before["stop_id"] and after["stop_id"] > self.last_step_stop and
                after["is_statement"], "actual default DAP source stop/oracle", (before, after))
        self.last_step_stop = after["stop_id"]
        self.check_stack(after)
        return after


class BrokerSession:
    def __init__(self, product, broker, adapter, vm_args, folder):
        self.folder, self.adapter = folder, adapter
        # Socket metadata needs a short pathname even when TMPDIR points to
        # a deep persistent build root. Large logs/products stay in folder.
        self.directory = Path(tempfile.mkdtemp(prefix="uwvm-dap-current-", dir="/tmp"))
        self.log_path = folder / "broker.raw"
        self.log = self.log_path.open("wb")
        self.command = [sys.executable, str(broker), "serve", "--uwvm", str(product),
                        "--socket-dir", str(self.directory), "--", *vm_args]
        self.child = subprocess.Popen(self.command, stdout=self.log, stderr=subprocess.STDOUT)
        self.clients = []
        try:
            deadline = time.monotonic() + 25
            while not (self.directory / "control.sock").exists():
                require(self.child.poll() is None and time.monotonic() < deadline,
                        "actual broker listener startup failed", self.child.poll())
                time.sleep(0.01)
        except BaseException:
            # Startup exceptions cannot lose ownership of the actual broker.
            self.close()
            raise

    def connect(self, level):
        # Use a distinct transcript basename for genuine reconnects.
        folder = self.folder / f"session-{len(self.clients)}"
        folder.mkdir()
        client = Client(self.adapter, self.directory, folder, level)
        self.clients.append(client)
        return client

    def close(self):
        errors = []
        for client in self.clients:
            try:
                if client.child.poll() is None:
                    client.close()
            except BaseException as error:
                errors.append(repr(error))
        if self.child.poll() is None:
            # Original broker catches SIGTERM and in its own finally retires
            # its original Popen guest. No numeric guest PID is killed here.
            self.child.terminate()
        try:
            result = self.child.wait(timeout=15)
        finally:
            self.log.close()
        require(result in (0, 130), "actual broker cleanup/guest error", result)
        require(not (self.directory / "control.sock").exists() and
                not (self.directory / "capability").exists(),
                "actual broker listener/capability remained after cleanup")
        self.directory.rmdir()
        require(self.log_path.stat().st_size <= 8 * 1024 * 1024 and
                all((client.folder / (client.level + "-adapter.stderr")).stat().st_size <= 8 * 1024 * 1024
                    for client in self.clients), "actual broker/adapter diagnostic log budget")
        require(not errors, "actual adapter retirement failed", errors)
        return {"actual_argv": self.command, "returncode": result,
                "broker_log_sha256": sha(self.log_path),
                "retirement": "original broker waited its original guest; no arbitrary PID kill"}


def actual_location(client):
    reply = client.evaluate("status")
    stop = re.findall(rb"(?m)^stop-id ([0-9]+)$", reply)
    threads = re.findall(rb"(?m)^thread ([0-9]+) module=([0-9]+) function=([0-9]+) byte-offset=([0-9]+) generation=([0-9]+)$", reply)
    require(len(stop) == 1 and int(stop[0]) > 0 and len(threads) == 1,
            "one actual fixed-fixture participant and nonzero stop", reply)
    thread, module, function, offset, epoch = map(int, threads[0])
    require(module == 0 and epoch > 0, "actual single module publication", reply)
    return {"thread": thread, "function": function, "offset": offset,
            "epoch": epoch, "stop": int(stop[0]), "raw": reply.decode()}


def breakpoint_begin(client, function, offset=0):
    client.evaluate("pause")
    point = client.evaluate(f"break 0 {function} {offset}")
    identifier = re.search(rb"breakpoint ([0-9]+)", point)
    require(identifier is not None, "actual breakpoint registration", point)
    client.evaluate("continue")
    for _ in range(20):
        stopped = client.evaluate("wait")
        if b"stopped: breakpoint" in stopped:
            break
    else:
        raise AssertionError("actual fixed fixture breakpoint timeout")
    current = actual_location(client)
    require((current["function"], current["offset"]) == (function, offset),
            "actual breakpoint function/offset", current)
    return current, int(identifier[1])


def source_case(server, wasm, source, sequences, markers, kind):
    client = server.connect("source")
    session = SourceSession(client, wasm, sequences, source)
    indices = {name: source_cli.metadata_cli.function(wasm, "source_step_" + name)[0]
               for name in ("outer", "leaf")}
    origin = indices["leaf"] if kind == "source-finish" else indices["outer"]
    session.begin(origin)
    if kind == "source-finish":
        session.seek(lambda p: p["function"] == origin and p["line"] == markers["STEP_LEAF_ENTRY"] and p["is_statement"],
                     "actual leaf source statement")
        result = session.source_step("out")
        require(result["function"] == indices["outer"] and not result["inline"],
                "actual DAP stepOut did not return to caller", result)
    elif kind == "source-next":
        session.seek(lambda p: p["function"] == origin and p["line"] == markers["STEP_PHYSICAL_CALL"] and p["is_statement"],
                     "actual physical call source statement")
        for _ in range(24):
            result = session.source_step("over")
            require(result["function"] == indices["outer"] and not result["inline"],
                    "actual DAP next stopped in child", result)
            if result["line"] == markers["STEP_AFTER_CALL"]:
                break
        else:
            raise AssertionError("actual DAP next did not reach after-call statement")
    else:
        session.seek(lambda p: p["function"] == origin and p["line"] == markers["STEP_BEFORE_INLINE"] and p["is_statement"],
                     "actual before-inline source statement")
        for _ in range(24):
            result = session.source_step("into")
            require(result["function"] == indices["outer"], "actual DAP into skipped physical origin", result)
            if len(result["inline"]) >= 2 and "source_step_middle" in result["inline"][0] and "source_step_inner" in result["inline"][-1]:
                break
        else:
            raise AssertionError("actual DAP into did not enter concrete nested inline metadata")
    frame = session.check_stack(result)
    scopes = client.request("scopes", {"frameId": frame["id"]})["body"]["scopes"]
    for scope in scopes:
        if scope["name"] == "Source variables":
            variables = client.request("variables", {"variablesReference": scope["variablesReference"]})["body"]["variables"]
            require(all(row.get("variablesReference") == 0 and "memoryReference" not in row and "evaluateName" not in row
                        for row in variables), "source display leaves gained expression/address access", variables)
    old_frame = frame["id"]
    client.close()
    # EOF disconnect keeps the launcher-authorized VM endpoint, but a fresh
    # adapter has no frame/scopes from the previous session.
    reconnect = server.connect("source")
    reconnect.request("scopes", {"frameId": old_frame}, success=False)
    # Closing a client retires its handles, not the launcher's paused VM.
    # An already stopped participant rejects a second pause request.
    retained = actual_location(reconnect)
    require(retained["stop"] == result["stop_id"],
            "reconnect did not retain the actual launcher-owned stop", retained)
    reconnect.request("threads")
    reconnect.close()
    return {"positions": session.positions, "actions": session.actions,
            "scope": "actual default source policy + official C5 rows/inline display + session disconnect/reconnect",
            "source_value_scope": "bounded display only; no full C++/Rust/variable-value qualification"}


def physical_frame(client, thread):
    frames = client.request("stackTrace", {"threadId": thread})["body"]["stackFrames"]
    physical = [row for row in frames if row["name"].startswith("#0 ") or
                row["name"].startswith("Native JIT instruction")]
    require(len(physical) == 1, "one actual selected physical frame", frames)
    return physical[0]


def native_case(server, wasm):
    start = source_cli.metadata_cli.function(wasm, "_start")[0]
    client = server.connect("wasm")
    # The broker attaches to a running loop. Its initialization prefix is
    # executed once; use the independently checked recurring local.get site.
    _, body = source_cli.metadata_cli.function(wasm, "_start")
    _, begin = source_cli.metadata_cli.u32(body, 0, len(body))
    begin += 2  # one local group: count 1, type i32
    require(body[begin:begin + 10] == b"\x41\x11\x21\x00\x03\x40\x20\x00\x41\x29",
            "fixed native fixture numeric prefix changed", body)
    first, point = breakpoint_begin(client, start, 6)
    client.evaluate(f"delete {point}")
    client.request("stepIn", {"threadId": first["thread"]})  # wasm default exact opcode
    second = actual_location(client)
    require(second["stop"] > first["stop"] and second["function"] == start and
            (first["offset"], second["offset"]) == (6, 8),
            "actual fixed numeric fixture did not advance local.get -> const", (first, second))
    client.close()
    native = server.connect("native")
    current = actual_location(native)
    require(current["stop"] == second["stop"],
            "native reconnect did not retain the launcher-owned stop", current)
    thread = current["thread"]
    frame = physical_frame(native, thread)
    reference = frame.get("instructionPointerReference", "")
    require(reference.startswith("uwvm-native-code:"), "actual readonly cooperative native owner did not qualify", frame)
    window = native.request("disassemble", {"memoryReference": reference, "instructionOffset": -200,
                                             "instructionCount": 400, "resolveSymbols": True})["body"]["instructions"]
    require(len(window) == 400, "actual IDE-sized native window count", len(window))
    require(all(row["address"] == "-1" and "instructionBytes" not in row
                for row in window if row.get("presentationHint") == "invalid"), "invalid native fillers fabricated a PC")
    pattern = re.compile(r"native instruction 0x([0-9a-fA-F]+) bytes=([0-9a-fA-F ]+)  ([^\r\n]+?) -> 0x([0-9a-fA-F]+)")
    executed, hidden, refused, positive_registers = [], 0, 0, []
    for _ in range(512):
        frame = physical_frame(native, thread)
        reference = frame.get("instructionPointerReference", "")
        before = native.request("disassemble", {"memoryReference": reference, "instructionCount": 1,
                                                "resolveSymbols": True})["body"]["instructions"][0]
        public = before.get("presentationHint") != "invalid"
        if public:
            require(before.get("instructionBytes") and before.get("symbol") == "window",
                    "qualified native instruction lacks actual bytes or Wasm owner", before)
        else:
            require(before["address"] == "-1" and "instructionBytes" not in before,
                    "hidden scaffolding gained native byte/address access", before)
            hidden += 1
        old_stop, count = current["stop"], len(native.events)
        number = native.queue([("stepIn", {"threadId": thread})])[0]
        response = native.response(number)
        current = actual_location(native)
        if not response.get("success"):
            require(current["stop"] == old_stop, "refused native step changed actual stop", response)
            refused += 1
            native.evaluate(f"step wasm {thread}")
            current = actual_location(native)
            require(current["stop"] > old_stop, "explicit Wasm fallback did not advance", current)
            continue
        require(current["stop"] > old_stop, "successful native step did not advance", current)
        output = [row.get("body", {}).get("output", "") for row in native.events[count:]
                  if row.get("event") == "output"]
        observed = [match for line in output if (match := pattern.search(line)) is not None]
        require(len(observed) == int(public), "native execution output exceeded copied public code", (before, output))
        if public:
            one = observed[0]
            native_pc = re.findall(r"native-pc=0x([0-9a-fA-F]+)", current["raw"])
            require(int(before["address"], 16) == int(one[1], 16) and before["instructionBytes"] == one[2] and
                    len(native_pc) == 1 and int(native_pc[0], 16) == int(one[4], 16),
                    "copied bytes differ from actual executed instruction or successor", (before, one.groups(), current))
            executed.append(one.groups())
        native.request("disassemble", {"memoryReference": reference, "instructionCount": 1}, success=False)
        trap = physical_frame(native, thread)
        require(trap.get("instructionPointerReference", "").startswith("uwvm-native-stop:") and "source" not in trap,
                "actual native trap reused source/Wasm position", trap)
        scopes = native.request("scopes", {"frameId": trap["id"]})["body"]["scopes"]
        require(len(scopes) == 1 and scopes[0]["name"] == "Registers",
                "native trap must expose only its authenticated register scope", scopes)
        registers = native.request("variables", {"variablesReference": scopes[0]["variablesReference"]})["body"]["variables"]
        require(registers and all(row.get("variablesReference") == 0 and "memoryReference" not in row
                                  for row in registers), "native register display gained memory access", registers)
        require(all(row["value"] == "unavailable" for row in registers
                    if row["name"] in ("rsp", "esp", "sp", "rbp", "ebp", "fp")),
                "native register scope disclosed host stack or frame pointers", registers)
        positive_registers += [row for row in registers if row["name"] not in ("rip", "eip", "pc") and
                               row["value"] != "unavailable"]
        native.request("readMemory", {"memoryReference": trap["instructionPointerReference"], "count": 1}, success=False)
        if len(executed) >= 2 and positive_registers:
            break
    require(len(executed) >= 2 and positive_registers,
            "real native walk must execute numeric instructions and expose actual numeric registers", executed)
    native.close()
    return {"wasm_before": first, "wasm_after": second, "native_first": executed[0],
            "native_second": executed[1], "window_count": len(window), "hidden": hidden, "refused": refused,
            "positive_registers": positive_registers,
            "scope": "actual Wasm one-opcode; native -200/400/true; copied/executed bytes and numeric registers; explicit Wasm fallback; no host reads"}


def replacement_case(server, wasm, replacement_body, bad_body, *, scripted_commit=False):
    start, body = source_cli.metadata_cli.function(wasm, "_start")
    groups, begin = source_cli.metadata_cli.u32(body, 0, len(body))
    leaf = source_cli.metadata_cli.function(wasm, "numeric_leaf")[0]
    require(groups == 0, "fixed replacement caller has no declared locals")
    expected = b"\x03\x40\x41\x00\x41\x05\x10" + source_cli.metadata_cli.leb(leaf) + b"\x36\x02\x00\x0c\x00\x0b\x0b"
    require(body[begin:] == expected, "official fixed replacement call/store loop differs", body[begin:])
    call_offset = 6
    client = server.connect("native")
    current, point = breakpoint_begin(client, start, call_offset)
    import base64
    for _ in range(3):
        old_value = client.request("readMemory", {"memoryReference": "wasm-memory:0:0:0", "count": 4})["body"]["data"]
        if int.from_bytes(base64.b64decode(old_value, validate=True), "little") == 12:
            break
        client.evaluate("continue")
        client.evaluate("wait")
        current = actual_location(client)
    else:
        raise AssertionError("actual pre-replacement result 12 was not stored")
    frame = physical_frame(client, current["thread"])
    old_reference = frame["instructionPointerReference"]
    require(old_reference.startswith("uwvm-native-code:"), "actual replacement caller readonly code was not qualified")
    scopes = client.request("scopes", {"frameId": frame["id"]})["body"]["scopes"]
    require(len(scopes) == 1 and scopes[0]["name"] == "Wasm locals", "fixed numeric caller scope shape", scopes)
    old_scope = scopes[0]["variablesReference"]
    failure = client.request("evaluate", {"expression": f"replace 0 {leaf} 1 {bad_body}", "context": "repl"}, success=False)
    require("replacement body failed WebAssembly validation" in failure.get("message", ""),
            "bad body failed for an unrelated reason", failure)
    unchanged = actual_location(client)
    require(unchanged["stop"] == current["stop"] and unchanged["epoch"] == current["epoch"],
            "actual malformed body changed current stop/publication", (current, unchanged))
    # Failure retires adapter IDs too: re-query genuine new IDs for the success batch.
    client.request("variables", {"variablesReference": old_scope}, success=False)
    failed_stop = unchanged
    client.evaluate("continue")
    client.evaluate("wait")
    unchanged = actual_location(client)
    require(unchanged["stop"] > failed_stop["stop"] and
            (unchanged["function"], unchanged["offset"], unchanged["epoch"]) ==
            (failed_stop["function"], failed_stop["offset"], failed_stop["epoch"]),
            "actual old callee did not return to the same caller after invalid replacement", (failed_stop, unchanged))
    failed_value = client.request("readMemory", {"memoryReference": "wasm-memory:0:0:0", "count": 4})["body"]["data"]
    require(int.from_bytes(base64.b64decode(failed_value, validate=True), "little") == 12,
            "actual old callee changed after rejected replacement", failed_value)
    frame = physical_frame(client, current["thread"])
    scopes = client.request("scopes", {"frameId": frame["id"]})["body"]["scopes"]
    old_scope, old_reference = scopes[0]["variablesReference"], frame["instructionPointerReference"]
    commit_expression = f"replace 0 {leaf} 1 {replacement_body}"
    if scripted_commit:
        # A genuine VM-authenticated batch may publish a replacement in any
        # child. Queue stale IDE IDs directly after the same request, before
        # polling or querying a new status/physical frame can hide retirement.
        commit_expression = "wasm-script info wasm-events; " + commit_expression
    numbers = client.queue([
        ("evaluate", {"expression": commit_expression, "context": "repl"}),
        ("variables", {"variablesReference": old_scope}),
        ("scopes", {"frameId": frame["id"]}),
        ("disassemble", {"memoryReference": old_reference, "instructionCount": 1}),
    ])
    results = [client.response(number) for number in numbers]
    require(results[0].get("success") is True and "function replaced; generation 2" in results[0]["body"]["result"],
            "actual same-ABI replacement was not committed", results)
    require(all(row.get("success") is False for row in results[1:]),
            "immediately queued stale references survived replacement", results)
    after = actual_location(client)
    require(after["stop"] > unchanged["stop"] and
            (after["function"], after["offset"], after["epoch"]) ==
            (unchanged["function"], unchanged["offset"], unchanged["epoch"]),
            "actual replacement stop label/caller changed unexpectedly", (unchanged, after))
    client.evaluate("continue")
    client.evaluate("wait")
    value = client.request("readMemory", {"memoryReference": "wasm-memory:0:0:0", "count": 4})["body"]["data"]
    require(int.from_bytes(base64.b64decode(value, validate=True), "little") == 13,
            "actual replacement did not execute new result 13", value)
    client.evaluate(f"delete {point}")
    client.close()
    return {"before": unchanged, "after": after, "failed_body_stop": failed_stop,
            "old_callee_rerun_after_bad_body": unchanged, "queued_results": results,
            "old_result": 12, "new_result": 13, "scripted_commit": scripted_commit,
            "scope": "real malformed body retains old generation/result; genuine inactive same-ABI function gen2; immediate stale IDs fail"}



def replacement_gc_bodies(wasm):
    """Bounded official module bytes; no imported function index assumptions."""
    source_cli.code_expressions(wasm)  # Fail closed on any actual import.
    rows = source_cli.metadata_cli.sections(wasm)
    codes = [payload for kind, payload in rows if kind == 10]
    require(len(codes) == 1, "exactly one official GC Code section")
    payload = codes[0]
    count, at = source_cli.metadata_cli.u32(payload, 0, len(payload))
    require(0 < count <= 16, "small focused GC local-function budget")
    bodies = []
    for _ in range(count):
        size, at = source_cli.metadata_cli.u32(payload, at, len(payload))
        require(2 <= size <= 65536 and size <= len(payload) - at,
                "bounded complete official GC function body")
        bodies.append(payload[at:at + size]); at += size
    require(at == len(payload), "official GC Code section trailing bytes")
    return rows, bodies


def replacement_gc_expected_start(numeric, throwing, checkpoint, tag_caller):
    # This reviewed fixed encoding proves both root writes precede the loop.
    # Dynamic calls consume the saved global/array values; no ref.func or root
    # mutation occurs on a later iteration. Declared type indices are 0/1/2/3.
    leb = source_cli.metadata_cli.leb
    return (b"\x00\x41\x00\xfb\x00\x02\x24\x02\xd2" + leb(numeric) + b"\x24\x01\xd2" + leb(numeric) +
            b"\xd2" + leb(throwing) + b"\xfb\x08\x03\x02\x24\x00\x03\x40" +
            b"\x41\x00\x41\x00\x23\x01\xd4\x14\x01\x36\x02\x00" +
            b"\x41\x04\x41\x00\x23\x00\xd4\x41\x00\xfb\x0b\x03" +
            b"\x14\x01\x36\x02\x00\x41\x08\x10" + leb(tag_caller) +
            b"\x36\x02\x00\x10" + leb(checkpoint) + b"\x0c\x00\x0b\x0b")


def replacement_gc_prepare(base_wasm, new_wasm, out, official_run):
    rows, bodies = replacement_gc_bodies(base_wasm)
    new_rows, new_bodies = replacement_gc_bodies(new_wasm)
    require([(kind, payload) for kind, payload in rows if kind != 10] ==
            [(kind, payload) for kind, payload in new_rows if kind != 10],
            "original type/tag/global/memory/element/function namespaces changed")
    exports = {name: source_cli.metadata_cli.function(base_wasm, name)[0]
               for name in ("numeric_leaf", "throwing_leaf", "replace_checkpoint", "tag_caller", "_start")}
    require(exports == {name: source_cli.metadata_cli.function(new_wasm, name)[0] for name in exports} and
            exports == {"numeric_leaf": 0, "throwing_leaf": 1, "replace_checkpoint": 2,
                        "tag_caller": 3, "_start": 4} and len(bodies) == len(new_bodies) == 5,
            "actual fixed no-import local/export layout", exports)
    targets = {exports["numeric_leaf"], exports["throwing_leaf"]}
    require(all((old != new) if index in targets else (old == new)
                for index, (old, new) in enumerate(zip(bodies, new_bodies))),
            "only the two reviewed same-ABI target bodies may differ")
    require(bodies[exports["replace_checkpoint"]] == b"\x00\x01\x0b" and
            bodies[exports["_start"]] == replacement_gc_expected_start(
                exports["numeric_leaf"], exports["throwing_leaf"],
                exports["replace_checkpoint"], exports["tag_caller"]),
            "actual nop checkpoint or one-time-root/dynamic-loop encoding differs")
    result = {}
    for kind, name in (("replacement-gc-funcref", "numeric_leaf"),
                       ("replacement-gc-tag", "throwing_leaf")):
        index = exports[name]
        body = new_bodies[index]
        require(body[-1:] == b"\x0b", "actual complete target end")
        replacement = out / (kind + ".body"); replacement.write_bytes(body)
        # Preserve the valid target prefix (including struct.set + throw in the
        # tag case). Even an unreachable suffix must reject its illegal opcode.
        bad = body[:-1] + b"\xff" + body[-1:]
        bad_body = out / (kind + "-late-invalid.body"); bad_body.write_bytes(bad)
        invalid = out / (kind + "-late-invalid.wasm")
        local_bodies = list(bodies); local_bodies[index] = bad
        leb = source_cli.metadata_cli.leb
        code = leb(len(local_bodies)) + b"".join(leb(len(value)) + value for value in local_bodies)
        invalid.write_bytes(b"\0asm\x01\0\0\0" + b"".join(
            bytes([tag]) + leb(len(code if tag == 10 else payload)) +
            (code if tag == 10 else payload) for tag, payload in rows))
        emitted = sha(invalid)
        official_run(invalid, kind + "-late-invalid")
        require(sha(invalid) == emitted, "official invalid module changed")
        check_rows, check_bodies = replacement_gc_bodies(invalid)
        require(check_rows == [(tag, code if tag == 10 else payload) for tag, payload in rows] and
                check_bodies == local_bodies, "invalid derivation changed unrelated body/declaration")
        result[kind] = {"function": index, "checkpoint": exports["replace_checkpoint"],
                        "replacement": replacement, "bad_body": bad_body, "invalid_module": invalid,
                        "official_body_sha256": hashlib.sha256(body).hexdigest()}
    return result


def replacement_gc_case(server, wasm, fixture, kind):
    import base64
    expected_old = [12, 12, 22]
    expected_new = [13, 13, 22] if kind == "replacement-gc-funcref" else [12, 12, 23]
    target = fixture["function"]
    client = server.connect("native")
    current, point = breakpoint_begin(client, fixture["checkpoint"], 0)

    def values():
        reply = client.request("readMemory", {"memoryReference": "wasm-memory:0:0:0", "count": 12})
        data = base64.b64decode(reply["body"]["data"], validate=True)
        require(len(data) == 12, "exact three guest-memory result fields")
        return [int.from_bytes(data[at:at + 4], "little") for at in (0, 4, 8)]

    def references():
        frame = physical_frame(client, current["thread"])
        reference = frame.get("instructionPointerReference", "")
        require(reference.startswith("uwvm-native-code:"), "real checkpoint readonly code qualification")
        scopes = client.request("scopes", {"frameId": frame["id"]})["body"]["scopes"]
        require(len(scopes) == 1 and scopes[0]["name"] == "Wasm locals", "real checkpoint scope", scopes)
        return frame["id"], scopes[0]["variablesReference"], reference

    def replacement_batch(path, refs):
        frame, scope, reference = refs
        numbers = client.queue([
            ("evaluate", {"expression": f"replace 0 {target} 1 {path}", "context": "repl"}),
            ("variables", {"variablesReference": scope}), ("scopes", {"frameId": frame}),
            ("disassemble", {"memoryReference": reference, "instructionCount": 1}),
        ])
        replies = [client.response(number) for number in numbers]
        require(all(row.get("success") is False for row in replies[1:]),
                "immediately queued GC replacement references survived", replies)
        return replies

    require(values() == expected_old, "actual gen1 global/array call and correct GC tag payload", values())
    failed = replacement_batch(fixture["bad_body"], references())
    require(failed[0].get("success") is False and
            "replacement body failed WebAssembly validation" in failed[0].get("message", ""),
            "late illegal opcode rejected for an unrelated reason", failed)
    unchanged = actual_location(client)
    require(unchanged == current, "failed body changed actual stop/publication", (current, unchanged))
    client.evaluate("continue"); client.evaluate("wait")
    current = actual_location(client)
    require(current["stop"] > unchanged["stop"] and
            (current["function"], current["offset"], current["epoch"]) ==
            (unchanged["function"], unchanged["offset"], unchanged["epoch"]),
            "actual old leaves did not return to the same checkpoint", (unchanged, current))
    require(values() == expected_old, "rejected replacement changed rooted old call/tag results", values())
    before = current
    committed = replacement_batch(fixture["replacement"], references())
    require(committed[0].get("success") is True and
            "function replaced; generation 2" in committed[0].get("body", {}).get("result", ""),
            "same-ABI rooted target replacement did not commit", committed)
    after = actual_location(client)
    require(after["stop"] > before["stop"] and
            (after["thread"], after["function"], after["offset"], after["epoch"]) ==
            (before["thread"], before["function"], before["offset"], before["epoch"]),
            "generation2 replacement changed checkpoint/publication unexpectedly", (before, after))
    client.evaluate("continue"); client.evaluate("wait")
    current = actual_location(client)
    require(current["stop"] > after["stop"] and values() == expected_new,
            "gen1 rooted funcref did not execute gen2/correct tag+GC payload", (current, values()))
    # A second same expected-generation request must fail after the commit and
    # retire the just-minted IDs without changing the genuine stopped result.
    stale = replacement_batch(fixture["replacement"], references())
    require(stale[0].get("success") is False and "function generation changed" in stale[0].get("message", ""),
            "stale generation was not rejected", stale)
    require(actual_location(client) == current and values() == expected_new,
            "stale replacement changed stop or rooted results")
    client.evaluate(f"delete {point}"); client.close()
    return {"failed_body_stop": unchanged, "before": before, "after": after, "old_results": expected_old,
            "new_results": expected_new, "late_invalid_batch": failed, "replacement_batch": committed,
            "stale_generation_batch": stale, "official_body_sha256": fixture["official_body_sha256"],
            "scope": "genuine inactive same-ABI gen2; gen1 global/array roots use dynamic canonical call_ref; "
                     "distinct equal-signature tag and GC struct payload; no explicit collection/lifetime-peak claim"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source-root", "uwvm", "build-receipt", "wasm-clang", "wasm-ld", "wasm-tools", "llvm-dwarfdump", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--case", action="append", choices=("source-into", "source-next", "source-finish", "wasm-native", "replacement", "replacement-script", "replacement-gc-funcref", "replacement-gc-tag"))
    args = parser.parse_args()
    require(sys.platform == "linux" and os.uname().machine == "x86_64", "keeper Linux x86-64 actual native backend only")
    root = args.source_root.resolve(strict=True)
    require(Path(__file__).resolve() == root / "test/0018.debugger/run_dap_current_broker.py" and
            Path(source_cli.__file__).resolve() == root / "test/0017.runtime/run_debug_source_step_cli.py" and
            Path(source_cli.metadata_cli.__file__).resolve() == root / "test/0017.runtime/run_debug_source_inline_metadata_cli.py",
            "actual runner/oracle import paths must match reviewed source")
    guard = root / "tools/ci/require_wasm3_test_cgroup.sh"
    subprocess.run(["bash", str(guard)], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    binary, build_path = args.uwvm.resolve(strict=True), args.build_receipt.resolve(strict=True)
    build = json.loads(build_path.read_text())
    require(Path(build["binary_path"]).resolve(strict=True) == binary and build["binary_sha256"] == sha(binary) and
            build["link_returncode"] == 0, "keeper original fresh R3 product/link receipt required")
    argv, cwd = build["link_argv"], Path(build["link_cwd"]).resolve(strict=True)
    outputs = [argv[i + 1] for i, value in enumerate(argv[:-1]) if value == "-o"]
    require(len(outputs) == 1, "one original actual linker output")
    output = Path(outputs[0]); output = output if output.is_absolute() else cwd / output
    require(output.resolve(strict=True) == binary, "original actual link did not produce this binary")
    before_path, after_path = (Path(build[name]).resolve(strict=True) for name in ("source_before_file", "source_after_file"))
    require(sha(before_path) == sha(after_path), "fresh original build source/dependency fingerprint changed")
    fingerprint = json.loads(before_path.read_text())
    entries = fingerprint["files"]
    require(len({row["path"] for row in entries}) == len(entries) and
            fingerprint["source_id"] == "sha256:" + hashlib.sha256(json.dumps(entries, sort_keys=True, separators=(",", ":")).encode()).hexdigest(),
            "original build fingerprint file-list identity")
    actual = {str(path.relative_to(root)): sha(path) for path in sorted((root / "src").rglob("*"))
              if path.is_file() and path.name != ".DS_Store" and not path.name.startswith("._")}
    recorded = {row["path"]: row["sha256"] for row in entries}
    require({key: value for key, value in recorded.items() if key.startswith("src/")} == actual,
            "actual complete production source differs from fresh R3 build")
    for entry in entries:
        require(sha(root / entry["path"]) == entry["sha256"], "actual build dependency changed", entry["path"])
    # This receipt is an original build input, not execution authority. The
    # keeper separately qualifies fresh runtime objects/common layout and libs.
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=False)
    source = root / "test/0017.runtime/fixtures/debug_source_step_c.c"
    adapter, broker = root / "tools/debug/dap_adapter.py", root / "tools/debug/secure_server.py"
    # LLVM drivers select their mode from argv[0] (wasm-ld versus lld,
    # clang++ versus clang). Validate the target but preserve the invoked name.
    tools = {name: getattr(args, name).absolute() for name in ("wasm_clang", "wasm_ld", "wasm_tools", "llvm_dwarfdump")}
    for tool in tools.values():
        tool.resolve(strict=True)
    new_wat = root / "test/0017.runtime/fixtures/debug_current_macos_replace_new.wat"
    gc_wats = [root / "test/0018.debugger/fixtures/debug_replace_gc_funcref_tag_base.wat",
               root / "test/0018.debugger/fixtures/debug_replace_gc_funcref_tag_new.wat"]
    immutable = [binary, build_path, before_path, after_path, source, new_wat, adapter, broker, guard, Path(__file__).resolve(),
                 Path(source_cli.__file__), Path(source_cli.metadata_cli.__file__), *tools.values(), *gc_wats]
    inputs = {str(path): sha(path) for path in immutable}
    summary = {"passed": False, "scope": "actual Linux broker/stdin DAP only; not actual VS Code, Windows or Mac acceptance",
               "product_sha256": sha(binary), "build_source_id": fingerprint["source_id"], "inputs_before": inputs,
               "commands": [], "cases": [], "production_before": actual}

    def run(command, name):
        log = out / (name + ".log")
        record = {"actual_argv": [str(value) for value in command], "cwd": str(root), "log": str(log)}
        summary["commands"].append(record)
        with log.open("wb") as stream:
            result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT, timeout=180)
        record.update(returncode=result.returncode, log_sha256=sha(log))
        require(result.returncode == 0 and log.stat().st_size <= 16 * 1024 * 1024,
                "actual official tool failed/budget", record)
        return log.read_text(errors="strict")

    try:
        for name, tool in tools.items():
            run([tool, "--version"], "version-" + name)
        original = source.read_text()
        needle = '__attribute__((export_name("_start"))) void _start(void)'
        require(original.count(needle) == 1, "exact current C entry derivation")
        # Preserve every original source line/marker and function body. Only the
        # entry signature changes; a separately compiled tiny driver repeats it
        # so the live broker client cannot arrive after a finite guest exited.
        derived = out / source.name
        derived.write_text(original.replace(needle, "void source_step_once(void)", 1))
        driver = out / "dap_source_loop_driver.c"
        driver.write_text('void source_step_once(void);\nvolatile unsigned source_step_completed;\n'
                          '__attribute__((export_name("_start"))) void _start(void) {\n'
                          '  for (;;) { source_step_once(); ++source_step_completed; }\n}\n')
        markers = {}
        for marker in ("STEP_BEFORE_INLINE", "STEP_PHYSICAL_CALL", "STEP_AFTER_CALL", "STEP_LEAF_ENTRY"):
            lines = [i for i, line in enumerate(derived.read_text().splitlines(), 1) if marker in line]
            require(len(lines) == 1, "one exact original source marker", marker); markers[marker] = lines[0]
        source_inputs = {str(path): sha(path) for path in (derived, driver)}
        objects = [out / "source.o", out / "driver.o"]
        for input_path, obj in zip((derived, driver), objects):
            run([tools["wasm_clang"], "--target=wasm32-unknown-unknown", "-std=c17", "-O1", "-g", "-gdwarf-5",
                 "-nostdlib", "-c", input_path, "-o", obj], "compile-" + obj.stem)
        require({str(path): sha(path) for path in (derived, driver)} == source_inputs,
                "actual compiler source inputs changed during compilation")
        object_before = {str(path): sha(path) for path in objects}
        wasm = out / "c5-loop.wasm"
        # Keep distinct debug strings in this multi-CU fixture. wasm-ld's
        # string merging can create suffix offsets that the strict DWARF
        # verifier rejects; retain verification and the -O1 guest code.
        run([tools["wasm_ld"], "-O0", "--no-entry", "--export-all", *objects, "-o", wasm], "link-source")
        require({str(path): sha(path) for path in objects} == object_before, "actual objects changed during official link")
        wasm_sha = sha(wasm)
        run([tools["wasm_tools"], "validate", wasm], "validate-source")
        run([tools["llvm_dwarfdump"], "--verify", wasm], "verify-source-dwarf")
        oracle = run([tools["llvm_dwarfdump"], "--debug-info", "--debug-line", "--debug-ranges", "--debug-rnglists", wasm], "source-oracle")
        require(sha(wasm) == wasm_sha and oracle.count("DW_TAG_inlined_subroutine") >= 4 and
                "source_step_inner" in oracle and "source_step_middle" in oracle,
                "actual verified nested/repeated embedded inline fixture absent")
        sequences = source_cli.line_sequences(oracle)
        source_cli.code_expressions(wasm)  # Explicitly refuse actual imports.
        numeric_wat = out / "native-loop.wat"
        numeric_wat.write_text('(module (func $window (export "_start") (local $value i32)\n'
                              '  i32.const 17 local.set $value\n'
                              '  (loop $again local.get $value i32.const 41 i32.xor local.set $value\n'
                              '    local.get $value i32.const 1 i32.rotl local.set $value br $again)))\n')
        numeric = out / "native-loop.wasm"
        replace_wat = out / "replace-loop.wat"
        replace_wat.write_text('(module (memory (export "memory") 1)\n'
                               ' (func $leaf (export "numeric_leaf") (param i32) (result i32) local.get 0 i32.const 7 i32.add)\n'
                               ' (func (export "_start") (loop $again i32.const 0 i32.const 5 call $leaf i32.store br $again)))\n')
        replace = out / "replace-loop.wasm"
        new_wasm = out / "replacement.wasm"
        wat_inputs = {str(path): sha(path) for path in (numeric_wat, replace_wat, new_wat)}
        for wat, module in ((numeric_wat, numeric), (replace_wat, replace), (new_wat, new_wasm)):
            run([tools["wasm_tools"], "parse", wat, "-o", module], "parse-" + module.stem)
            emitted = sha(module)
            run([tools["wasm_tools"], "validate", module], "validate-" + module.stem)
            require(sha(module) == emitted, "official verified module changed", str(module))
            source_cli.code_expressions(module)
        require({str(path): sha(path) for path in (numeric_wat, replace_wat, new_wat)} == wat_inputs,
                "actual WAT input changed during official encoding/validation")
        replacement_body = out / "replacement.body"
        replacement_body.write_bytes(source_cli.metadata_cli.function(new_wasm, "numeric_leaf")[1])
        bad_body = out / "invalid.body"; bad_body.write_bytes(b"\0\xff\x0b")
        gc_modules = [out / "gc-rooted-base.wasm", out / "gc-rooted-new.wasm"]
        for wat, module in zip(gc_wats, gc_modules):
            run([tools["wasm_tools"], "parse", wat, "-o", module], "parse-" + module.stem)
            emitted = sha(module)
            run([tools["wasm_tools"], "validate", "--features", "all", module], "validate-" + module.stem)
            require(sha(module) == emitted, "official GC/tag validated module changed")
            run([tools["wasm_tools"], "print", module], "print-" + module.stem)

        def reject_gc_opcode(module, name):
            log = out / (name + "-official-validate.log")
            argv = [str(tools["wasm_tools"]), "validate", "--features", "all", str(module)]
            record = {"actual_argv": argv, "cwd": str(root), "log": str(log), "expected_valid": False}
            summary["commands"].append(record)
            with log.open("wb") as stream:
                result = subprocess.run(argv, cwd=root, stdout=stream, stderr=subprocess.STDOUT, timeout=30)
            record.update(returncode=result.returncode, log_sha256=sha(log))
            require(result.returncode > 0 and 0 < log.stat().st_size <= 1024 * 1024,
                    "official invalid GC fixture signal/launcher/budget failure", record)
            text = log.read_text(errors="strict")
            require(re.search(r"(?i)(illegal|invalid|unknown) (primary )?opcode", text) and "0xff" in text.lower(),
                    "official invalid GC fixture failed for an unrelated reason", text)

        gc_fixtures = replacement_gc_prepare(*gc_modules, out, reject_gc_opcode)
        immutable += [*gc_modules, *(out / ("print-" + module.stem + ".log") for module in gc_modules)]
        for fixture in gc_fixtures.values():
            immutable += [fixture["replacement"], fixture["bad_body"], fixture["invalid_module"]]
        immutable += [derived, driver, *objects, wasm, out / "source-oracle.log", numeric_wat, numeric,
                      replace_wat, replace, new_wat, new_wasm, replacement_body, bad_body]
        inputs.update({str(path): sha(path) for path in immutable})
        cases = args.case or ("source-finish", "source-next", "source-into", "wasm-native", "replacement", "replacement-script",
                              "replacement-gc-funcref", "replacement-gc-tag")
        require(len(set(cases)) == len(cases), "duplicate cases would overwrite actual evidence")
        mode = ["-Raot"] if args.ros else ["-Rcc", "jit", "-Rcm", "full"]
        for policy in ("instruction", "unwind"):
            for kind in cases:
                folder = out / (policy + "-" + kind); folder.mkdir()
                module = (wasm if kind.startswith("source-") else numeric if kind == "wasm-native" else
                          gc_modules[0] if kind in gc_fixtures else replace)
                vm_args = ["-m", "run", *mode, "-Rct", "0", "-Rllvm-call-stack", policy,
                           "-Rllvm-exception-dispatch", "native-unwind", "-Rllvm-cache-path", "disable", "--run", str(module)]
                if kind in gc_fixtures:
                    vm_args[2:2] = ["-WFE-reference-types", "-WFE-function-references", "-WFE-gc", "-WFE-exceptions"]
                server = BrokerSession(binary, broker, adapter, vm_args, folder)
                row = {"case": kind, "policy": policy, "passed": False, "fixture_sha256": sha(module)}
                summary["cases"].append(row)
                try:
                    if kind.startswith("source-"):
                        row.update(source_case(server, wasm, derived, sequences, markers, kind))
                    elif kind == "wasm-native":
                        row.update(native_case(server, numeric))
                    elif kind in gc_fixtures:
                        row.update(replacement_gc_case(server, gc_modules[0], gc_fixtures[kind], kind))
                    else:
                        row.update(replacement_case(server, replace, replacement_body, bad_body,
                                                    scripted_commit=kind == "replacement-script"))
                    row["passed"] = True
                except BaseException as error:
                    row["error"] = repr(error)
                    raise
                finally:
                    try:
                        row["cleanup"] = server.close()
                    except BaseException as error:
                        row["passed"] = False; row["cleanup_error"] = repr(error)
                        raise
        subprocess.run(["bash", str(guard)], check=True)
        summary["inputs_after"] = {str(path): sha(path) for path in immutable}
        require(summary["inputs_after"] == inputs, "actual source/tool/product/oracle/module changed")
        summary["production_after"] = {str(path.relative_to(root)): sha(path) for path in sorted((root / "src").rglob("*"))
                                       if path.is_file() and path.name != ".DS_Store" and not path.name.startswith("._")}
        require(summary["production_after"] == actual and all(sha(root / entry["path"]) == entry["sha256"] for entry in entries),
                "fresh production/dependency closure changed during live DAP")
        summary["passed"] = True
    except BaseException as error:
        summary["error"] = repr(error)
        raise
    finally:
        summary["source_derivation"] = {"original": str(source), "original_sha256": sha(source),
                                       "change": "unique entry signature only; same line count/markers; separate official repeating driver"}
        summary["artifact_receipts"] = [{"path": str(path.relative_to(out)), "sha256": sha(path)}
                                       for path in sorted(out.rglob("*")) if path.is_file() and path.name != "summary.json"]
        (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"PASS actual current Linux broker/DAP: {len(summary['cases'])} named cases; no IDE/platform/global debugger claim")


if __name__ == "__main__":
    main()
