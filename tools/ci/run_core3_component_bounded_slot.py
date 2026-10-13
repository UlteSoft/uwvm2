#!/usr/bin/env python3
"""Host-namespace guard for one separately authorized E16 native component job.

This deliberately observes host /proc: the Windows compiler root is outside the
Docker PID namespace. A unique marker identifies the stopped bootstrap once;
birth identities, pidfds and actual ancestry identify its subsequent children.
No formal performance is admitted.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import signal
import shutil
import subprocess
import sys
import time
import traceback
import uuid


EXPECTED_CPUSET = set([0, 2, 4, 6, *range(16, 32)])
MARKER_NAME = "UWVM_GC_COMPONENT_SLOT_MARKER"
BOOTSTRAP_CODE = """import json, os, signal, sys
os.sched_setaffinity(0, {16})
os.kill(os.getpid(), signal.SIGSTOP)
argv = json.loads(sys.argv[1])
os.execvpe(argv[0], argv, os.environ)
"""


def parse_cpu_set(text):
    result = set()
    for entry in text.strip().split(","):
        if not entry:
            continue
        first, separator, last = entry.partition("-")
        result.update(range(int(first), int(last) + 1) if separator else [int(first)])
    return result


def sha(path):
    with Path(path).open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def start_time(pid):
    text = Path(f"/proc/{pid}/stat").read_text()
    return int(text[text.rfind(")") + 2:].split()[19])


def identity(pid):
    text = Path(f"/proc/{pid}/stat").read_text()
    fields = text[text.rfind(")") + 2:].split()
    return int(fields[19]), fields[0]


def tree(root):
    pending, seen, rows = [root], set(), []
    page_bytes = os.sysconf("SC_PAGE_SIZE")
    while pending:
        pid = pending.pop()
        if pid in seen:
            continue
        seen.add(pid)
        proc = Path("/proc") / str(pid)
        try:
            birth, state = identity(pid)
            status = dict(line.split(":", 1) for line in (proc / "status").read_text().splitlines() if ":" in line)
            row = {"pid": pid, "start_time": birth, "state": state,
                   "parent_pid": int(status["PPid"]), "uid": int(status["Uid"].split()[0]),
                   "rss_bytes": int((proc / "statm").read_text().split()[1]) * page_bytes,
                   "cgroup": (proc / "cgroup").read_text().strip(), "thread_affinities": {}}
            for task in (proc / "task").iterdir():
                try:
                    status = dict(line.split(":", 1) for line in (task / "status").read_text().splitlines() if ":" in line)
                    row["thread_affinities"][task.name] = sorted(parse_cpu_set(status["Cpus_allowed_list"]))
                    pending.extend(int(child) for child in (task / "children").read_text().split())
                except (FileNotFoundError, ProcessLookupError):
                    if task.exists():
                        raise RuntimeError("a live host task lost readable accounting")
            if identity(pid)[0] != birth:
                raise RuntimeError("host PID birth changed during resource accounting")
            rows.append(row)
        except (FileNotFoundError, ProcessLookupError):
            if proc.exists():
                raise RuntimeError("a live host process lost readable accounting")
    return rows


def validate_membership(rows, cgroup, required_affinity=None):
    # A compiler can naturally exit between the tree and cgroup.procs reads.
    # Retain that observed row for provenance; only a still-live matching PID
    # outside the group is an escape. Zombies have no executing descendants.
    members = set(map(int, (cgroup / "cgroup.procs").read_text().split()))
    for row in rows:
        if row["pid"] not in members:
            try:
                birth, state = identity(row["pid"])
                retired = birth != row["start_time"] or state in ("Z", "X")
            except (FileNotFoundError, ProcessLookupError):
                retired = True
            row["retired_during_membership_snapshot"] = retired
            if not retired:
                raise RuntimeError("a live observed process escaped the selected shared cgroup")
        if required_affinity is not None and any(
                cpus != required_affinity for cpus in row["thread_affinities"].values()):
            raise RuntimeError("component worker escaped E16 affinity")


def marked_bootstraps(cgroup, marker, worker_uid, bootstrap_argv):
    expected = (MARKER_NAME + "=" + marker).encode()
    found = []
    for pid in map(int, (cgroup / "cgroup.procs").read_text().split()):
        try:
            status = dict(line.split(":", 1) for line in Path(f"/proc/{pid}/status").read_text().splitlines() if ":" in line)
            if int(status["Uid"].split()[0]) != worker_uid:
                continue
            argv = [value.decode() for value in Path(f"/proc/{pid}/cmdline").read_bytes().split(b"\0") if value]
            if not argv or Path(argv[0]).name != "python3" or argv[1:] != bootstrap_argv[1:]:
                continue
            # Read the exact fixed bootstrap's marker only. A compiler can be
            # non-dumpable during exec; descendant environment is not evidence
            # of ancestry and no peer environment is inspected.
            if expected in Path(f"/proc/{pid}/environ").read_bytes().split(b"\0"):
                found.append(pid)
        except (FileNotFoundError, ProcessLookupError):
            pass
    return found


class OwnedWorkerTree:
    """Keep pidfds only for the pinned bootstrap and proven descendants."""

    def __init__(self, row, uid, cgroup, root_pidfd):
        self.uid, self.cgroup = uid, cgroup
        self.known = {row["pid"]: row}
        self.pidfds = {row["pid"]: root_pidfd}

    def refresh(self):
        observed = {}
        # Previously proved children remain owned if a parent naturally exits
        # and Linux reparents them. A recycled PID never enters this set.
        for pid, old in list(self.known.items()):
            try:
                birth, state = identity(pid)
            except (FileNotFoundError, ProcessLookupError):
                if Path(f"/proc/{pid}").exists():
                    raise RuntimeError("a proved live worker lost identity accounting")
                continue
            if birth != old["start_time"] or state in ("Z", "X"):
                continue
            for row in tree(pid):
                observed[row["pid"]] = row
        for pid, row in list(observed.items()):
            if row["state"] in ("Z", "X"):
                continue
            if row["uid"] != self.uid:
                raise RuntimeError("a component descendant changed its host UID")
            old = self.known.get(pid)
            if old is not None and old["start_time"] != row["start_time"]:
                raise RuntimeError("a component PID was reused during observation")
            if old is None:
                parent = observed.get(row["parent_pid"]) or self.known.get(row["parent_pid"])
                if parent is None or row["start_time"] < parent["start_time"]:
                    raise RuntimeError("a new component descendant has no proven parent")
                try:
                    if identity(parent["pid"])[0] != parent["start_time"]:
                        raise RuntimeError("component parent PID birth changed")
                except (FileNotFoundError, ProcessLookupError) as error:
                    raise RuntimeError("component parent retired before ancestry proof") from error
                try:
                    descriptor = os.pidfd_open(pid)
                except ProcessLookupError:
                    # A short native proof may naturally retire after the
                    # readable tree snapshot. Exclude only a genuinely gone
                    # process; never accept unreadable live accounting as zero.
                    if Path(f"/proc/{pid}").exists():
                        birth, state = identity(pid)
                        if birth == row["start_time"] and state not in ("Z", "X"):
                            raise
                    observed.pop(pid)
                    continue
                try:
                    if identity(pid)[0] != row["start_time"]:
                        raise RuntimeError("component descendant PID birth changed while pinning")
                    status = dict(line.split(":", 1) for line in Path(f"/proc/{pid}/status").read_text().splitlines() if ":" in line)
                    if (int(status["PPid"]) != parent["pid"] or int(status["Uid"].split()[0]) != self.uid or
                            identity(parent["pid"])[0] != parent["start_time"]):
                        raise RuntimeError("component child ancestry changed while pinning")
                except (FileNotFoundError, ProcessLookupError):
                    os.close(descriptor)
                    if Path(f"/proc/{pid}").exists():
                        raise RuntimeError("a pinned live child lost readable identity")
                    observed.pop(pid)
                    continue
                except Exception:
                    os.close(descriptor)
                    raise
                self.pidfds[pid] = descriptor
            self.known[pid] = row
        rows = list(observed.values())
        validate_membership(rows, self.cgroup, [16])
        return rows

    def stop_and_kill(self):
        errors = []
        # Freeze only already pinned workers before finding last descendants.
        # Signals use pidfds, so a recycled numeric PID cannot target a peer.
        for descriptor in self.pidfds.values():
            try:
                signal.pidfd_send_signal(descriptor, signal.SIGSTOP)
            except ProcessLookupError:
                pass
        try:
            self.refresh()
        except Exception as error:
            errors.append(str(error))
        for descriptor in self.pidfds.values():
            try:
                signal.pidfd_send_signal(descriptor, signal.SIGKILL)
            except ProcessLookupError:
                pass
        return errors

    def close(self):
        for descriptor in self.pidfds.values():
            os.close(descriptor)
        self.pidfds.clear()


def snapshot(cgroup, args, marker=None, bootstrap_pending=False, bootstrap_argv=None, owned=None):
    stat = {key: int(value) for key, value in
            (line.split() for line in (cgroup / "memory.stat").read_text().splitlines())}
    events = {key: int(value) for key, value in
              (line.split() for line in (cgroup / "memory.events").read_text().splitlines())}
    windows = []
    if not args.windows_idle_manifest:
        windows = tree(args.windows_root_pid)
        if (not windows or windows[0]["start_time"] != args.windows_root_start_time or
                windows[0]["state"] in ("Z", "X")):
            raise RuntimeError("Windows host root disappeared or its PID birth identity changed")
        validate_membership(windows, cgroup)
    else:
        try:
            birth, state = identity(args.windows_root_pid)
            if birth == args.windows_root_start_time and state not in ("Z", "X"):
                raise RuntimeError("retired Windows build root is still alive")
        except (FileNotFoundError, ProcessLookupError):
            pass
    cpus = parse_cpu_set((cgroup / "cpuset.cpus.effective").read_text())
    memory_max, swap_max = ((cgroup / name).read_text().strip() for name in ("memory.max", "memory.swap.max"))
    if memory_max != str(64 * 1024**3) or swap_max != "0" or cpus != EXPECTED_CPUSET:
        raise RuntimeError("shared cgroup must be exact 64GiB/swap0/4P+16E")
    worker = []
    if owned is not None:
        worker = owned.refresh()
    elif marker:
        if not bootstrap_pending or bootstrap_argv is None:
            raise RuntimeError("a marker may identify only the fixed stopped bootstrap")
        selected = marked_bootstraps(cgroup, marker, args.component_uid, bootstrap_argv)
        seen = set()
        for pid in selected:
            for row in tree(pid):
                if row["pid"] not in seen:
                    worker.append(row)
                    seen.add(row["pid"])
        validate_membership(worker, cgroup, None if bootstrap_pending else [16])
    peers = []
    if args.windows_idle_manifest:
        own = {row["pid"] for row in worker}
        seen = set(own)
        for pid in map(int, (cgroup / "cgroup.procs").read_text().split()):
            if pid in seen:
                continue
            for row in tree(pid):
                if row["pid"] not in seen:
                    peers.append(row)
                    seen.add(row["pid"])
        validate_membership(peers, cgroup)
    return {"monotonic_seconds": time.monotonic(), "cgroup_path": str(cgroup),
            "memory.current": int((cgroup / "memory.current").read_text()),
            "memory.max": memory_max, "memory.swap.max": swap_max, "cpuset.cpus.effective": sorted(cpus),
            "memory.events": events, "memory.stat": stat,
            "irreclaimable_bytes": stat.get("anon", 0) + stat.get("shmem", 0) + stat.get("kernel", 0),
            "irreclaimable_definition": "anon + shmem + kernel; shmem is within file and is counted once",
            "windows_tree": windows,
            "windows_tree_rss_bytes": None if args.windows_idle_manifest else sum(row["rss_bytes"] for row in windows),
            "windows_observation_scope": ("retired root bound to finished-build manifest; complete current peer roster observed separately" if
                                          args.windows_idle_manifest else "live Windows root and all descendants"),
            "peer_tree": peers, "peer_tree_rss_bytes": sum(row["rss_bytes"] for row in peers),
            "windows_or_peer_tree_rss_bytes": sum(row["rss_bytes"] for row in (peers if args.windows_idle_manifest else windows)),
            "component_tree": worker, "component_tree_rss_bytes": sum(row["rss_bytes"] for row in worker),
            "disk_free_bytes": shutil.disk_usage(args.out.parent).free}


def violation(row, args, reserve=False):
    extra = args.max_component_tree_rss_bytes + args.peer_reserve_bytes if reserve else 0
    if row["memory.current"] + extra >= args.stop_memory_bytes:
        return "shared memory.current lacks the reserved component headroom"
    if row["irreclaimable_bytes"] + extra >= args.max_irreclaimable_bytes:
        return "shared anon+shmem+kernel exceeds the reserved irreclaimable bound"
    if row["windows_or_peer_tree_rss_bytes"] >= args.max_windows_tree_rss_bytes:
        return "Windows or explicitly observed post-handoff peer tree exceeds 32GiB bound"
    if row["component_tree_rss_bytes"] >= args.max_component_tree_rss_bytes:
        return "component host tree exceeds 2GiB bound"
    if row["disk_free_bytes"] < args.disk_floor_bytes:
        return "host filesystem crossed the separately authorized disk floor"
    if any(row["memory.events"].get(key, 0) for key in ("oom", "oom_kill")):
        return "shared cgroup recorded an OOM event"
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--windows-root-pid", type=int, required=True)
    parser.add_argument("--windows-root-start-time", type=int, required=True)
    parser.add_argument("--windows-controller", type=Path, required=True)
    parser.add_argument("--windows-controller-sha256", required=True)
    parser.add_argument("--windows-idle-manifest", type=Path,
                        help="explicit finished-build handoff; never substitute a sleep PID for Windows")
    parser.add_argument("--windows-idle-manifest-sha256")
    parser.add_argument("--cgroup-path", type=Path,
                        help="exact host cgroup required when the original Windows root has retired")
    parser.add_argument("--container", default="uwvm3-implementation")
    parser.add_argument("--component-uid", type=int, default=os.getuid(),
                        help="run the bounded Docker exec as the observable host owner")
    parser.add_argument("--component-gid", type=int, default=os.getgid())
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--inner-command-json", type=Path)
    parser.add_argument("--preflight-only", action="store_true")
    parser.add_argument("--stop-memory-bytes", type=int, default=63_000_000_000)
    parser.add_argument("--max-irreclaimable-bytes", type=int, default=40 * 1024**3)
    parser.add_argument("--max-windows-tree-rss-bytes", type=int, default=32 * 1024**3)
    parser.add_argument("--max-component-tree-rss-bytes", type=int, default=2 * 1024**3)
    parser.add_argument("--peer-reserve-bytes", type=int, default=0,
                        help="additional startup headroom for a separately authorized bounded peer")
    parser.add_argument("--disk-floor-bytes", type=int, default=16 * 1024**3)
    parser.add_argument("--timeout-seconds", type=int, default=2400)
    args = parser.parse_args()
    if (sys.platform != "linux" or not 0 < args.stop_memory_bytes < 64 * 1024**3 or
            args.peer_reserve_bytes < 0):
        parser.error("host Linux and a guard below the 64GiB hard limit are required")
    if not args.preflight_only and args.inner_command_json is None:
        parser.error("--inner-command-json is required for execution")
    if sha(args.windows_controller) != args.windows_controller_sha256:
        raise RuntimeError("reviewed Windows controller source changed")
    if args.windows_idle_manifest:
        if not args.windows_idle_manifest_sha256 or not args.cgroup_path:
            raise RuntimeError("finished Windows handoff requires its pinned manifest and exact cgroup")
        if sha(args.windows_idle_manifest) != args.windows_idle_manifest_sha256:
            raise RuntimeError("finished Windows build manifest changed")
        retired = json.loads(args.windows_idle_manifest.read_text())
        if (not retired.get("P0_build_finished") or retired.get("memory_max") != 64 * 1024**3 or
                retired.get("memory_swap_max") != 0):
            raise RuntimeError("pinned Windows manifest does not prove completed bounded build")
        cgroup = args.cgroup_path.resolve(strict=True)
        if not cgroup.is_relative_to(Path("/sys/fs/cgroup")) or "docker-" not in cgroup.name:
            raise RuntimeError("post-handoff cgroup must identify the existing Docker host group")
    else:
        if start_time(args.windows_root_pid) != args.windows_root_start_time:
            raise RuntimeError("Windows root birth identity changed before preflight")
        group_record = Path(f"/proc/{args.windows_root_pid}/cgroup").read_text().strip()
        if not group_record.startswith("0::/") or "docker-" not in group_record:
            raise RuntimeError("expected a unified Docker cgroup on the host")
        cgroup = Path("/sys/fs/cgroup") / group_record[4:]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    os.sched_setaffinity(0, {16})
    before = snapshot(cgroup, args)
    admission = violation(before, args, reserve=True)
    result = {"passed": False, "scope": "bounded E16 native compile/sanitizer/graph qualification, not performance",
              "formal_performance_qualified": False, "controller_sha256": sha(__file__),
              "windows_controller_sha256": args.windows_controller_sha256,
              "limits": vars(args) | {"out": str(out), "windows_controller": str(args.windows_controller),
                                      "windows_idle_manifest": str(args.windows_idle_manifest) if args.windows_idle_manifest else None,
                                      "cgroup_path": str(args.cgroup_path) if args.cgroup_path else None,
                                      "inner_command_json": str(args.inner_command_json) if args.inner_command_json else None},
              "before": before, "admitted": admission is None, "admission_failure": admission}
    (out / "preflight.json").write_text(json.dumps(result, indent=2) + "\n")
    if admission or args.preflight_only:
        result["passed"] = admission is None
        (out / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
        print("ADMITTED bounded E16 slot" if admission is None else "REJECTED bounded E16 slot: " + admission)
        return 0 if admission is None else 2
    inner = json.loads(args.inner_command_json.read_text())
    if not isinstance(inner, list) or not inner or any(not isinstance(value, str) for value in inner):
        raise RuntimeError("inner command must be an argv JSON list")
    if "--build-only" not in inner or not any(Path(value).name in
            ("run_gc_explicit_sweep_component.py", "run_gc_single_block_collect_ab.py") for value in inner):
        raise RuntimeError("this shared-build slot admits only the reviewed native component build-only qualifiers")
    marker = str(uuid.uuid4())
    bootstrap_argv = ["python3", "-c", BOOTSTRAP_CODE, json.dumps(inner, separators=(",", ":"))]
    command = ["docker", "exec", "--user", f"{args.component_uid}:{args.component_gid}",
               "-e", MARKER_NAME + "=" + marker, "-e", "UWVM_TEST_CPUSET=0,2,4,6,16-31",
               args.container, *bootstrap_argv]
    result.update(command=command, inner_command_sha256=sha(args.inner_command_json), marker=marker)
    (out / "command.txt").write_text(shlex.join(command) + "\n")
    started = time.monotonic()
    process = None
    pidfd = None
    owned = None
    try:
        with (out / "worker.log").open("wb") as log, (out / "telemetry.jsonl").open("w") as telemetry:
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            # The fixed bootstrap cannot read qualification inputs or exec a
            # compiler until this host observer validates its stopped identity.
            # Initial Docker affinity applies only to this control bootstrap.
            while True:
                if process.poll() is not None:
                    raise RuntimeError("Docker exec exited before stopped-bootstrap qualification")
                row = snapshot(cgroup, args, marker, bootstrap_pending=True, bootstrap_argv=bootstrap_argv)
                telemetry.write(json.dumps(row | {"phase": "control_bootstrap"}) + "\n")
                telemetry.flush()
                fail = violation(row, args, reserve=True)
                if fail or time.monotonic() - started > 10:
                    raise RuntimeError(fail or "control bootstrap did not stop within ten seconds")
                workers = row["component_tree"]
                if workers:
                    if len(workers) != 1:
                        raise RuntimeError("control bootstrap has unexpected worker descendants")
                    worker = workers[0]
                    argv = [value.decode() for value in Path(f"/proc/{worker['pid']}/cmdline").read_bytes().split(b"\0") if value]
                    if not argv or Path(argv[0]).name != "python3" or argv[1:] != bootstrap_argv[1:]:
                        raise RuntimeError("unrecognized marked control-bootstrap image")
                    if worker["state"] == "T":
                        validate_membership(workers, cgroup, [16])
                        pidfd = os.pidfd_open(worker["pid"])
                        if identity(worker["pid"]) != (worker["start_time"], "T"):
                            raise RuntimeError("stopped bootstrap PID birth or state changed")
                        result["stopped_bootstrap_proof"] = row
                        result["bootstrap_code_sha256"] = hashlib.sha256(BOOTSTRAP_CODE.encode()).hexdigest()
                        owned = OwnedWorkerTree(worker, args.component_uid, cgroup, pidfd)
                        descriptor = pidfd
                        pidfd = None
                        signal.pidfd_send_signal(descriptor, signal.SIGCONT)
                        break
                time.sleep(0.02)
            while process.poll() is None:
                row = snapshot(cgroup, args, owned=owned)
                telemetry.write(json.dumps(row) + "\n")
                telemetry.flush()
                fail = violation(row, args)
                if fail or time.monotonic() - started > args.timeout_seconds:
                    raise RuntimeError(fail or "bounded E16 component timed out")
                time.sleep(0.10)
        result.update(exit=process.returncode, elapsed_seconds=time.monotonic() - started,
                      after=snapshot(cgroup, args, owned=owned), worker_log_sha256=sha(out / "worker.log"))
        if process.returncode or violation(result["after"], args):
            raise RuntimeError("bounded component exited nonzero or final resource validation failed")
        result["passed"] = True
        return 0
    except Exception as error:
        result["failure"] = str(error)
        result["failure_traceback"] = traceback.format_exc()
        # Docker exec's client is not the worker's parent. Ownership established
        # at the stopped handshake remains the only signal authority.
        if owned is not None:
            result["cleanup_observation_errors"] = owned.stop_and_kill()
        else:
            try:
                for worker_pid in marked_bootstraps(cgroup, marker, args.component_uid, bootstrap_argv):
                    descriptor = os.pidfd_open(worker_pid)
                    try:
                        # The root has not been continued, so no compiler exists.
                        if identity(worker_pid)[1] == "T":
                            signal.pidfd_send_signal(descriptor, signal.SIGKILL)
                    finally:
                        os.close(descriptor)
            except Exception as cleanup_error:
                result["cleanup_observation_errors"] = [str(cleanup_error)]
        if process is not None and process.poll() is None:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
        return 1
    finally:
        if pidfd is not None:
            os.close(pidfd)
        if owned is not None:
            owned.close()
        (out / "summary.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
