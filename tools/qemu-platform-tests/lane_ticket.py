#!/usr/bin/env python3
"""Shared admission ticket for the existing guarded Linux and QEMU test lanes.

This module never starts, signals, reaps, or joins a process. The caller must
retain its existing PIDFD supervisor, command manifest, and cgroup checks.
"""

from __future__ import annotations

import fcntl
import hashlib
import json
import os
from pathlib import Path
import stat
import time
import uuid


SCHEMA = "uwvm-native-qemu-lane-ticket-v1"
CG_PREFIX = "/sys/fs/cgroup/system.slice/docker-"
EXPECTED_MEMORY = "68719476736"
EXPECTED_CPUS = "0,2,4,6,16-31"


class AdmissionError(RuntimeError):
    pass


def _sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def _load_exact(path: Path, expected_sha256: str, *, require_object: bool = True):
    if len(expected_sha256) != 64 or any(c not in "0123456789abcdef" for c in expected_sha256):
        raise AdmissionError("invalid immutable input SHA-256")
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected_sha256:
        raise AdmissionError(f"immutable input changed: {path}")
    value = json.loads(data)
    if require_object and not isinstance(value, dict):
        raise AdmissionError(f"immutable input is not an object: {path}")
    return value


def _process(pid: int) -> dict:
    root = Path("/proc") / str(pid)
    raw = (root / "stat").read_text()
    # The comm field can contain spaces and parentheses. Field 22 is the
    # twentieth entry beginning with the state field after the last ')'.
    end = raw.rfind(")")
    if end < 0 or raw[:raw.find(" ")] != str(pid):
        raise AdmissionError(f"invalid stat record for pid {pid}")
    fields = raw[end + 2:].split()
    if len(fields) < 20:
        raise AdmissionError(f"truncated stat record for pid {pid}")
    uid_lines = [line for line in (root / "status").read_text().splitlines()
                 if line.startswith("Uid:")]
    if len(uid_lines) != 1:
        raise AdmissionError(f"missing UID record for pid {pid}")
    uid = [int(value) for value in uid_lines[0].split()[1:]]
    if len(uid) != 4:
        raise AdmissionError(f"invalid UID record for pid {pid}")
    return {"pid": pid, "birth": int(fields[19]), "ppid": int(fields[1]),
            "state": fields[0], "uid": uid,
            "argv": [os.fsdecode(value) for value in (root / "cmdline").read_bytes().split(b"\0")[:-1]],
            "cgroup": (root / "cgroup").read_text().strip(),
            "exe": os.readlink(root / "exe")}


def _safe_json(path: Path, value: dict) -> None:
    temporary = path.with_name(path.name + ".tmp-" + uuid.uuid4().hex)
    data = (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()
    descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


class LaneTicket:
    """An exclusive flock held until the caller explicitly retires its work.

    ``release`` requires an immutable final supervisor receipt and a fresh
    init-only cgroup witness. An exception or controller crash releases the OS
    lock but leaves an active receipt: the next admission cannot silently
    convert it into a successful retirement.
    """

    def __init__(self, *, directory: Path, authority: Path, authority_sha256: str,
                 commands: Path, commands_sha256: str, owner: str, suite: str):
        self.directory = directory
        self.authority_path = authority.resolve(strict=True)
        self.authority_sha256 = authority_sha256
        self.commands_path = commands.resolve(strict=True)
        self.commands_sha256 = commands_sha256
        self.authority = _load_exact(self.authority_path, authority_sha256)
        command_manifest = _load_exact(self.commands_path, commands_sha256, require_object=False)
        if not isinstance(command_manifest, (dict, list)) or not command_manifest:
            raise AdmissionError("empty or invalid command manifest")
        if owner not in ("/root/linux_fused_resume", "/root/qemu_platform_tests"):
            raise AdmissionError("unknown lane owner")
        if not suite or len(suite) > 160 or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_." for c in suite):
            raise AdmissionError("invalid suite label")
        self.owner, self.suite = owner, suite
        self.descriptor = None
        self.record = None
        self.receipt = None

    def _idle_witness(self) -> dict:
        authority = self.authority
        boot = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
        if boot != authority["boot_id"]:
            raise AdmissionError("Linux reboot invalidated the authority")
        cg_path = authority["cgroup_path"]
        if (not isinstance(cg_path, str) or not cg_path.startswith(CG_PREFIX)
                or not cg_path.endswith(".scope") or Path(cg_path).as_posix() != cg_path
                or "/../" in cg_path or "/./" in cg_path):
            raise AdmissionError("invalid outer Docker cgroup path")
        cg = Path(cg_path)
        limits = {name: (cg / name).read_text().strip() for name in
                  ("memory.max", "memory.swap.max", "cpuset.cpus.effective")}
        if limits != {"memory.max": EXPECTED_MEMORY, "memory.swap.max": "0",
                      "cpuset.cpus.effective": EXPECTED_CPUS}:
            raise AdmissionError(f"outer cgroup limit changed: {limits}")
        init_pid = authority["init_pid"]
        if not isinstance(init_pid, int) or isinstance(init_pid, bool) or init_pid <= 1:
            raise AdmissionError("invalid authority init PID")
        before = (cg / "cgroup.procs").read_text().split()
        process = _process(init_pid)
        after = (cg / "cgroup.procs").read_text().split()
        if before != [str(init_pid)] or after != before:
            raise AdmissionError(f"cgroup is not init-only: before={before}, after={after}")
        if (process["birth"] != authority["init_birth"] or process["uid"] != [1000] * 4
                or process["state"] == "Z" or process["exe"] != authority["init_exe"]
                or process["cgroup"] != authority["init_cgroup"]
                or process["argv"] != ["sleep", "infinity"]):
            raise AdmissionError("original container init identity changed")
        if _sha256(Path("/proc") / str(init_pid) / "exe") != authority["init_exe_sha256"]:
            raise AdmissionError("container init executable changed")
        events = {}
        for line in (cg / "memory.events").read_text().splitlines():
            name, value = line.split()
            events[name] = int(value)
        if events.get("oom") != 0 or events.get("oom_kill") != 0:
            raise AdmissionError("outer cgroup already contains an OOM event")
        return {"boot_id": boot, "init": process, "limits": limits,
                "memory_events": events, "observed_at_ns": time.time_ns()}

    def acquire(self) -> dict:
        if self.descriptor is not None:
            raise AdmissionError("ticket already acquired")
        self.directory.mkdir(mode=0o700, parents=True, exist_ok=True)
        directory_stat = self.directory.lstat()
        if (not stat.S_ISDIR(directory_stat.st_mode) or directory_stat.st_uid != 1000
                or directory_stat.st_mode & 0o077):
            raise AdmissionError("lane directory must be UID 1000 and private")
        descriptor = os.open(self.directory / "native-qemu-lane.lock",
                             os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
        try:
            lock_stat = os.fstat(descriptor)
            if (not stat.S_ISREG(lock_stat.st_mode) or lock_stat.st_uid != 1000
                    or lock_stat.st_mode & 0o077):
                raise AdmissionError("lane lock must be UID 1000 and private")
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            idle = self._idle_witness()
            current_path = self.directory / "native-qemu-lane-current.json"
            if current_path.exists():
                current = json.loads(current_path.read_text())
                if current.get("state") != "retired":
                    # An unlocked file after a crash is not authority to adopt
                    # or kill the old owner's processes. Manual reconciliation
                    # must preserve the abandoned receipt before another suite.
                    raise AdmissionError("previous ticket is unretired; reconciliation is required")
            controller = _process(os.getpid())
            if controller["uid"] != [1000] * 4:
                raise AdmissionError("lane controller UID must be 1000")
            ticket_id = uuid.uuid4().hex
            self.receipt = self.directory / (ticket_id + ".json")
            record = {"schema": SCHEMA, "ticket_id": ticket_id, "state": "active",
                      "owner": self.owner, "suite": self.suite, "controller": controller,
                      "authority": str(self.authority_path), "authority_sha256": self.authority_sha256,
                      "commands": str(self.commands_path), "commands_sha256": self.commands_sha256,
                      "admission": idle, "started_at_ns": time.time_ns()}
            _safe_json(self.receipt, record)
            _safe_json(current_path, record)
            self.descriptor, self.record = descriptor, record
            return record
        except BaseException:
            os.close(descriptor)
            raise

    def release(self, *, supervisor_receipt: Path, supervisor_receipt_sha256: str) -> dict:
        if self.descriptor is None or self.record is None:
            raise AdmissionError("no active ticket")
        final_path = supervisor_receipt.resolve(strict=True)
        final = _load_exact(final_path, supervisor_receipt_sha256)
        # These fields must come from the original PIDFD supervisor. A ticket
        # cannot turn an unknown-child observation into owned-task retirement.
        if (final.get("supervisor_complete") is not True
                or final.get("owned_tasks_retired") is not True
                or final.get("commands_sha256") != self.commands_sha256
                or final.get("ticket_id") != self.record["ticket_id"]):
            raise AdmissionError("supervisor receipt does not retire this ticket")
        retirement = self._idle_witness()
        record = dict(self.record, state="retired", retired_at_ns=time.time_ns(),
                      retirement=retirement, supervisor_receipt=str(final_path),
                      supervisor_receipt_sha256=supervisor_receipt_sha256)
        _safe_json(self.receipt, record)
        _safe_json(self.directory / "native-qemu-lane-current.json", record)
        fcntl.flock(self.descriptor, fcntl.LOCK_UN)
        os.close(self.descriptor)
        self.descriptor = None
        self.record = record
        return record

    def abandon(self) -> None:
        """Close only this lock FD, preserving the unretired failure receipt."""
        if self.descriptor is not None:
            os.close(self.descriptor)
            self.descriptor = None
