#!/usr/bin/env python3
"""Exercise the host-owned LLVM-full debug server after a Core 3 guest starts."""

import argparse
import array
import json
import os
from pathlib import Path
import resource
import selectors
import signal
import socket
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/debug"))
import secure_server


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--uwvm", required=True, type=Path)
parser.add_argument("--wasm-tools", required=True, type=Path)
parser.add_argument("--source-root", required=True, type=Path)
parser.add_argument("--out", required=True, type=Path)
parser.add_argument("--ros", action="store_true")
args = parser.parse_args()
for name in ("uwvm", "wasm_tools", "source_root", "out"):
    setattr(args, name, getattr(args, name).resolve())
subprocess.run(["bash", str(args.source_root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
args.out.mkdir(parents=True, exist_ok=False)
private = args.out / "private"
private.mkdir(mode=0o700)
marker = b"server-running\n"
wat = args.out / "core3-server.wat"
wasm = args.out / "core3-server.wasm"
wat.write_text('''(module
  (import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
  (memory (export "memory") 1 1 shared)
  (tag $e (param i32))
  (data (i32.const 128) "server-running\\0a")
  (func $raise i32.const 17 throw $e)
  (func (export "_start")
    (block $caught (result i32)
      (try_table (catch $e $caught) call $raise)
      unreachable)
    i32.const 17 i32.ne if unreachable end
    i32.const 0 i32.const 1 i32.atomic.store
    i32.const 0 i32.const 128 i32.store
    i32.const 4 i32.const 15 i32.store
    i32.const 1 i32.const 0 i32.const 1 i32.const 16 call $write
    if unreachable end
    (loop $again i32.const 0 i32.atomic.load drop br $again)))
''')
subprocess.run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)], check=True)
subprocess.run([str(args.wasm_tools), "validate", str(wasm)], check=True)

vm_args = ["-m", "run", "-Rct", "0", "-Rllvm-cache-path", "disable",
           "-WFE-exceptions", "-WFE-threads"]
if not args.ros:
    vm_args += ["-Rcc", "jit", "-Rcm", "full"]
vm_args += ["--run", str(wasm)]
command = ["python3", str(args.source_root / "tools/debug/secure_server.py"),
           "serve", "--uwvm", str(args.uwvm), "--socket-dir", str(private),
           "--", *vm_args]
server = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          stdin=subprocess.DEVNULL, start_new_session=True)
output = bytearray()
selector = selectors.DefaultSelector()
selector.register(server.stdout, selectors.EVENT_READ)
rows = []


def receive(channel):
    data = channel.recv(65537)
    assert data and len(data) <= 65536, data
    return data


def connect(token):
    channel = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    channel.settimeout(10)
    channel.connect(str(private / "control.sock"))
    assert channel.send(token) == len(token)
    return channel


def request(channel, message):
    encoded = message.encode()
    assert channel.send(encoded) == len(encoded)
    return receive(channel)


try:
    deadline = time.monotonic() + 60
    while marker not in output:
        assert server.poll() is None and time.monotonic() < deadline, (server.poll(), output[-5000:])
        ready = selector.select(max(0, deadline - time.monotonic()))
        assert ready, output[-5000:]
        chunk = os.read(server.stdout.fileno(), 65536)
        assert chunk, (server.poll(), output[-5000:])
        output.extend(chunk)
    capability_path = private / "capability"
    assert capability_path.stat().st_mode & 0o077 == 0
    capability = capability_path.read_bytes()
    assert len(capability) == 32
    guest_children = (Path(f'/proc/{server.pid}/task/{server.pid}/children').read_text().split())
    assert len(guest_children) == 1, guest_children
    guest_pid = int(guest_children[0])
    replacement_body = args.out / "inactive-void.bin"
    replacement_body.write_bytes(b"\x00\x0b")

    def guest_cpu_ticks():
        # /proc/<pid>/stat fields 14+15, after the parenthesized comm field.
        fields = Path(f'/proc/{guest_pid}/stat').read_text().rsplit(')', 1)[1].split()
        return int(fields[11]) + int(fields[12])

    exposed = subprocess.run(["python3", str(args.source_root / "tools/debug/secure_server.py"),
                              "serve", "--uwvm", str(args.uwvm), "--socket-dir", str(private),
                              "--", "--wasip1-mount-dir", "/guest", "/", "--run", str(wasm)],
                             capture_output=True, timeout=12)
    assert exposed.returncode == 1 and b"overlaps a guest WASI preopen" in exposed.stderr, exposed
    assert server.poll() is None
    rows.append("guest-preopen-capability-overlap-rejected")

    probe_path = private / "same-process.sock"
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as probe:
            probe.bind(str(probe_path))
            probe.listen(1)
            with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as self_client:
                self_client.connect(str(probe_path))
                try:
                    secure_server.accept_client(probe, os.getpid())
                except RuntimeError as error:
                    assert "unauthorized debugger peer" in str(error)
                else:
                    raise AssertionError("guest PID was accepted as a debugger peer")
    finally:
        probe_path.unlink(missing_ok=True)
    rows.append("guest-pid-peer-rejected")

    with connect(b"x" * 32) as attacker:
        assert attacker.recv(1) == b""
    rows.append("wrong-capability-rejected")
    assert server.poll() is None

    with connect(capability) as client:
        assert receive(client) == b"ready\n"
        assert b"running" in request(client, "status")
        stopped = request(client, "pause")
        assert b"stopped" in stopped and b"thread 1" in stopped, stopped
        assert b"#0 module=0 function=2" in request(client, "bt 1")
        replaced = request(client, f"replace 0 1 1 {replacement_body}")
        assert b"function replaced; generation 2" in replaced, replaced
        stepped = request(client, "step wasm 1")
        assert b"stopped: selected participant step" in stepped, stepped
        assert b"running" in request(client, "continue")
        client.send(b"disconnect")
    rows.append("late-attach-core3-pause-backtrace-step-replace-resume")

    with connect(capability) as client:
        assert receive(client) == b"ready\n"
        with open("/dev/null", "rb") as donated:
            client.sendmsg([b"status"], [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                                              array.array("i", [donated.fileno()]))])
        assert client.recv(1) == b""
    rows.append("rights-packet-rejected")
    assert server.poll() is None

    # Linux can deliver SCM_RIGHTS even when recvmsg returns zero data. This
    # must close every passed FD before the broker treats the record as EOF.
    baseline_fds = len(list(Path(f'/proc/{server.pid}/fd').iterdir()))
    with connect(capability) as client:
        assert receive(client) == b"ready\n"
        with open("/dev/null", "rb") as donated:
            assert client.sendmsg([b""], [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                                            array.array("i", [donated.fileno()]))]) == 0
        assert client.recv(1) == b""
    deadline = time.monotonic() + 2
    while len(list(Path(f'/proc/{server.pid}/fd').iterdir())) > baseline_fds and time.monotonic() < deadline:
        time.sleep(0.01)
    assert len(list(Path(f'/proc/{server.pid}/fd').iterdir())) <= baseline_fds
    rows.append("zero-byte-rights-packet-closed-without-fd-leak")

    baseline_fds = len(list(Path(f'/proc/{server.pid}/fd').iterdir()))
    with connect(capability) as client:
        assert receive(client) == b"ready\n"
        with open("/dev/null", "rb") as donated:
            assert client.sendmsg([b"status"], [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                                                    array.array("i", [donated.fileno()] * 64))]) == len(b"status")
        assert client.recv(1) == b""
    deadline = time.monotonic() + 2
    while len(list(Path(f'/proc/{server.pid}/fd').iterdir())) > baseline_fds and time.monotonic() < deadline:
        time.sleep(0.01)
    assert len(list(Path(f'/proc/{server.pid}/fd').iterdir())) <= baseline_fds
    rows.append("truncated-rights-packet-closed-without-fd-leak")

    connected = subprocess.run(["python3", str(args.source_root / "tools/debug/secure_server.py"),
                                "connect", "--socket-dir", str(private), "--command", "status"],
                               capture_output=True, timeout=12)
    assert connected.returncode == 0 and b"running" in connected.stdout, connected
    rows.append("client-cli-reconnect-after-rejected-peer")

    disconnected = subprocess.run(["python3", str(args.source_root / "tools/debug/secure_server.py"),
                                   "connect", "--socket-dir", str(private), "--command", "disconnect"],
                                  capture_output=True, timeout=12)
    assert disconnected.returncode == 0 and not disconnected.stdout, disconnected
    assert server.poll() is None
    rows.append("client-local-disconnect-keeps-server")

    with connect(capability) as client:
        assert receive(client) == b"ready\n"
        assert b"stopped" in request(client, "pause")
        first = request(client, "step asm 1")
        second = request(client, "step asm 1")
        assert b"native instruction 0x" in first and b" bytes=" in first, first
        assert b"native instruction 0x" in second and b" bytes=" in second, second
    rows.append("peer-disconnect-from-native-trap-retains-host-control")

    with connect(capability) as client:
        assert receive(client) == b"ready\n"
        assert b"stopped: native instruction step" in request(client, "status")
        before_cpu = guest_cpu_ticks()
        assert b"detached; guest continues" in request(client, "quit")
    deadline = time.monotonic() + 5
    while server.poll() is None and time.monotonic() < deadline:
        time.sleep(0.01)
    assert server.poll() == 0, (server.poll(), output[-5000:])
    # The supervisor exits without terminating the guest on an explicit host
    # detach; the test owns the isolated process group and will drain it below.
    os.killpg(server.pid, 0)
    deadline = time.monotonic() + 3
    while guest_cpu_ticks() <= before_cpu and time.monotonic() < deadline:
        time.sleep(0.05)
    assert guest_cpu_ticks() > before_cpu, "VM remained parked in the SIGTRAP gate after host detach"
    rows.append("native-trap-detach-releases-running-guest")
    rows.append("host-detach-keeps-guest-running")
finally:
    try:
        os.killpg(server.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        server.wait(timeout=10)
    except subprocess.TimeoutExpired:
        os.killpg(server.pid, signal.SIGKILL)
        server.wait(timeout=10)
    selector.close()
    output.extend(server.stdout.read())
    (args.out / "server.log").write_bytes(output)
    (args.out / "results.json").write_text(json.dumps({"checks": rows, "server_exit": server.returncode,
                                                         "command": command}, indent=2) + "\n")

assert rows == ["guest-preopen-capability-overlap-rejected", "guest-pid-peer-rejected", "wrong-capability-rejected",
                "late-attach-core3-pause-backtrace-step-replace-resume", "rights-packet-rejected",
                "zero-byte-rights-packet-closed-without-fd-leak",
                "truncated-rights-packet-closed-without-fd-leak",
                "client-cli-reconnect-after-rejected-peer", "client-local-disconnect-keeps-server",
                "peer-disconnect-from-native-trap-retains-host-control", "native-trap-detach-releases-running-guest",
                "host-detach-keeps-guest-running"], rows
assert not (private / "control.sock").exists() and not (private / "capability").exists()
print(json.dumps({"checks": len(rows), "server_exit": server.returncode}))
