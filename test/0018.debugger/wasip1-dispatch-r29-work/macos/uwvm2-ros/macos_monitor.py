#!/usr/bin/env python3
"""Serial, no-fork macOS test ownership; observations are not an aggregate hard cap.

Native calls are the public installed SDK libproc/sysctl/Mach ABI. This test
executor has no debugger credential, guest memory reader or runtime authority.
The root is not reaped until every other member of its owned session/group has
retired. A fixed inherited no-fork sandbox is tested before the payload starts.
"""
from __future__ import annotations
import argparse
import ctypes as C
from dataclasses import dataclass
import errno
import fcntl
import hashlib
import json
import os
from pathlib import Path
import resource
import secrets
import signal
import stat
import subprocess
import sys
import time

PHYSICAL_LIMIT = 4 << 30
RSS_STOP = 512 << 20  # fixed tiny fixtures; early observation, not a kernel hard cap
OUTPUT_LIMIT = 8 << 20
POLL_SECONDS = 0.02
MAX_MEMBERS = 8
MAX_RETAINED_RSS_SAMPLES = 16
PROFILE = '(version 1)(allow default)(deny process-fork)'
SANDBOX = Path('/usr/bin/sandbox-exec')


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


@dataclass(frozen=True)
class Birth:
    pid: int
    seconds: int
    microseconds: int
    uid: int
    pgid: int

    def valid(self) -> bool:
        return self.pid > 0 and self.seconds > 0 and 0 <= self.microseconds < 1000000 and self.uid >= 0 and self.pgid > 0


def identity_matches(expected: Birth, current: Birth) -> bool:
    return expected.valid() and current.valid() and expected == current


def check_rss(supervisor: int, members: list[int]) -> int:
    require(supervisor >= 0 and len(members) <= MAX_MEMBERS and all(value >= 0 for value in members), 'invalid bounded RSS sample')
    total = supervisor + sum(members)
    require(total < RSS_STOP, 'owned RSS reached the conservative 512-MiB early-stop threshold')
    return total


def record_rss_sample(receipt: dict, rows: list[tuple[str, Birth, int]], total: int) -> None:
    """Keep bounded actual observations; no sample is a lifetime upper bound."""
    require(1 <= len(rows) <= MAX_MEMBERS + 1 and len({birth.pid for _, birth, _ in rows}) == len(rows) and
            all(birth.valid() and type(value) is int and value >= 0 for _, birth, value in rows) and
            total == sum(value for _, _, value in rows), 'bounded unique actual RSS observation required')
    maxima = receipt.setdefault('rss_sample_role_maxima', {})
    require(len(set(maxima) | {str(birth.pid) for _, birth, _ in rows}) <= MAX_MEMBERS + 1, 'bounded actual role identity history required')
    recorded = []
    for role, birth, value in rows:
        require(role in ('supervisor', 'root', 'witness', 'startup-witness'), 'unknown accounting role')
        key, identity = str(birth.pid), birth.__dict__.copy()
        prior = maxima.get(key)
        require(prior is None or prior['birth'] == identity, 'accounting PID birth was reused')
        row = {'role': role, 'birth': identity, 'rss_bytes': value}
        recorded.append(row)
        maxima[key] = {'role': role, 'birth': identity, 'sampled_max_rss_bytes': max(value, 0 if prior is None else prior['sampled_max_rss_bytes'])}
    retained = receipt.setdefault('rss_samples_retained', [])
    sequence = receipt.get('rss_observation_count', 0) + 1
    receipt['rss_observation_count'] = sequence
    sample = {'sequence': sequence, 'total_rss_bytes': total, 'roles': recorded,
              'scope': 'sequential actual RSS observation, not a lifetime upper bound'}
    retained.append(sample)
    if len(retained) > MAX_RETAINED_RSS_SAMPLES:
        del retained[0]
    if total >= receipt.get('rss_peak_sample', {}).get('total_rss_bytes', -1):
        receipt['rss_peak_sample'] = sample


class BsdInfo(C.Structure):
    _fields_ = [(name, C.c_uint32) for name in (
        'flags', 'status', 'xstatus', 'pid', 'ppid', 'uid', 'gid', 'ruid', 'rgid', 'svuid', 'svgid', 'reserved')]
    _fields_ += [('comm', C.c_char * 16), ('name', C.c_char * 32)]
    _fields_ += [(name, C.c_uint32) for name in ('nfiles', 'pgid', 'pjobc', 'tdev', 'tpgid')]
    _fields_ += [('nice', C.c_int32), ('start_sec', C.c_uint64), ('start_usec', C.c_uint64)]


class TaskInfo(C.Structure):
    _fields_ = [(name, C.c_uint64) for name in ('virtual', 'resident', 'total_user', 'total_system', 'threads_user', 'threads_system')]
    _fields_ += [(name, C.c_int32) for name in ('policy', 'faults', 'pageins', 'cow_faults', 'messages_sent', 'messages_received',
                                             'syscalls_mach', 'syscalls_unix', 'csw', 'threadnum', 'numrunning', 'priority')]


class MachTaskBasicInfo(C.Structure):
    # Installed public mach/task_info.h; only numeric accounting, never memory addresses.
    _pack_ = 4
    _fields_ = [(name, C.c_uint64) for name in ('virtual', 'resident', 'resident_max')]
    _fields_ += [(name, C.c_int32) for name in ('user_sec', 'user_usec', 'system_sec', 'system_usec', 'policy', 'suspend_count')]


def peak_upper(root: int, supervisor: int, witness: int = 0) -> int:
    require(all(type(value) is int and 0 <= value < PHYSICAL_LIMIT for value in (root, supervisor, witness)),
            'invalid actual reported task peak RSS')
    total = root + supervisor + witness
    require(total < PHYSICAL_LIMIT, 'sum of actual reported task peak RSS is not below four GiB')
    return total


class SwapUsage(C.Structure):
    _fields_ = [('total', C.c_uint64), ('avail', C.c_uint64), ('used', C.c_uint64), ('pagesize', C.c_uint32), ('encrypted', C.c_int32)]


class VmInfo64(C.Structure):
    _fields_ = [(name, C.c_uint32) for name in ('free', 'active', 'inactive', 'wire')]
    _fields_ += [(name, C.c_uint64) for name in ('zero_fill', 'reactivations', 'pageins', 'pageouts', 'faults', 'cow_faults', 'lookups', 'hits', 'purges')]
    _fields_ += [('purgeable', C.c_uint32), ('speculative', C.c_uint32)]
    _fields_ += [(name, C.c_uint64) for name in ('decompressions', 'compressions', 'swapins', 'swapouts')]
    _fields_ += [(name, C.c_uint32) for name in ('compressor_page', 'throttled', 'external_page', 'internal_page')]
    _fields_ += [('total_uncompressed', C.c_uint64), ('swapped_count', C.c_uint64)]


class MacNative:
    def __init__(self) -> None:
        require(sys.platform == 'darwin' and os.uname().machine == 'arm64', 'current native debugger qualification requires macOS arm64')
        require(C.sizeof(BsdInfo) == 136 and BsdInfo.start_sec.offset == 120 and C.sizeof(TaskInfo) == 96 and
                C.sizeof(SwapUsage) == 32 and C.sizeof(VmInfo64) == 160 and VmInfo64.swapins.offset == 112,
                'public SDK ABI layout is not this qualified arm64 layout')
        self.proc = C.CDLL('/usr/lib/libproc.dylib', use_errno=True)
        self.system = C.CDLL('/usr/lib/libSystem.B.dylib', use_errno=True)
        self.proc.proc_pidinfo.argtypes = (C.c_int, C.c_int, C.c_uint64, C.c_void_p, C.c_int)
        self.proc.proc_pidinfo.restype = C.c_int
        self.proc.proc_listpgrppids.argtypes = (C.c_int, C.c_void_p, C.c_int)
        self.proc.proc_listpgrppids.restype = C.c_int  # public wrapper returns PID count, not bytes
        self.system.sysctlbyname.argtypes = (C.c_char_p, C.c_void_p, C.POINTER(C.c_size_t), C.c_void_p, C.c_size_t)
        self.system.sysctlbyname.restype = C.c_int
        self.system.mach_host_self.argtypes = ()
        self.system.mach_host_self.restype = C.c_uint32
        self.system.host_statistics64.argtypes = (C.c_uint32, C.c_int, C.POINTER(C.c_int32), C.POINTER(C.c_uint32))
        self.system.host_statistics64.restype = C.c_int
        self.system.mach_port_deallocate.argtypes = (C.c_uint32, C.c_uint32)
        self.system.mach_port_deallocate.restype = C.c_int
        self.system.task_name_for_pid.argtypes = (C.c_uint32, C.c_int, C.POINTER(C.c_uint32))
        self.system.task_name_for_pid.restype = C.c_int
        self.system.task_info.argtypes = (C.c_uint32, C.c_int, C.POINTER(C.c_int32), C.POINTER(C.c_uint32))
        self.system.task_info.restype = C.c_int
        require(C.sizeof(MachTaskBasicInfo) == 48 and MachTaskBasicInfo.resident_max.offset == 16,
                'public MACH_TASK_BASIC_INFO ABI layout mismatch')
        self.task_port = C.c_uint32.in_dll(self.system, 'mach_task_self_').value
        self.host_port = self.system.mach_host_self()
        require(self.host_port != 0, 'actual host statistics port unavailable')

    def close(self) -> None:
        if self.host_port:
            port, self.host_port = self.host_port, 0
            require(self.system.mach_port_deallocate(self.task_port, port) == 0, 'host statistics port retirement failed')

    def bsd(self, pid: int, missing: bool = False) -> BsdInfo | None:
        require(pid > 0, 'invalid PID')
        value = BsdInfo()
        # The ctypes-owned output lives for the complete synchronous call.
        # PROC_PIDTBSDINFO arg=1 includes the actual unreaped zombie; it does
        # not relax the exact returned PID, birth or process-group checks.
        C.set_errno(0)
        size = self.proc.proc_pidinfo(pid, 3, 1, C.byref(value), C.sizeof(value))
        actual_errno = C.get_errno()  # preserve the original result before any other API
        if size == 0 and missing and actual_errno in (errno.ESRCH, errno.ENOENT):
            return None
        require(size == C.sizeof(value) and value.pid == pid,
                f'actual proc_pidinfo BSD identity unavailable or partial: pid={pid}, bytes={size}, '
                f'errno={actual_errno}, returned_pid={value.pid}, status={value.status}')
        return value

    @staticmethod
    def birth(value: BsdInfo) -> Birth:
        result = Birth(value.pid, value.start_sec, value.start_usec, value.uid, value.pgid)
        require(result.valid(), 'invalid actual kernel birth identity')
        return result

    def rss(self, pid: int, bsd: BsdInfo) -> int:
        if bsd.status == 5:  # actual SZOMB, still not reaped by this owner
            return 0
        value = TaskInfo()
        # Borrow the complete ctypes-owned numeric output only for this call.
        C.set_errno(0)
        size = self.proc.proc_pidinfo(pid, 4, 0, C.byref(value), C.sizeof(value))
        actual_errno = C.get_errno()
        if size == 0:
            again = self.bsd(pid, missing=True)
            require(again is None or (again.status == 5 and identity_matches(self.birth(bsd), self.birth(again))),
                    f'live RSS became unavailable: pid={pid}, bytes={size}, errno={actual_errno}, '
                    f'initial_status={bsd.status}, returned_status={None if again is None else again.status}')
            return 0
        require(size == C.sizeof(value),
                f'actual task RSS result is partial: pid={pid}, bytes={size}, errno={actual_errno}')
        return int(value.resident)

    def stopped_peak(self, expected: Birth) -> int:
        # The actual unreaped root anchors the group. The complete witness is
        # already kernel-stopped, so there is no user execution after this peak.
        before = self.bsd(expected.pid)
        require(before.status == 4 and identity_matches(expected, self.birth(before)), 'actual stopped witness birth differs')
        port, info, count = C.c_uint32(), MachTaskBasicInfo(), C.c_uint32(12)
        # Borrow only this owned numeric port output through the synchronous SDK call.
        result = self.system.task_name_for_pid(self.task_port, expected.pid, C.byref(port))
        require(result == 0 and port.value != 0, f'actual accounting NAME port unavailable: kern_return={result}')
        try:
            # Borrow complete owned output as the SDK integer_t array for this
            # synchronous accounting call. NAME is not a memory-read/control port.
            output = C.cast(C.byref(info), C.POINTER(C.c_int32))
            result = self.system.task_info(port.value, 20, output, C.byref(count))
            require(result == 0 and count.value == 12, f'actual maximum resident accounting unavailable: kern_return={result}, count={count.value}')
            after = self.bsd(expected.pid)
            require(after.status == 4 and identity_matches(expected, self.birth(after)), 'witness resumed or birth changed during actual peak query')
            require(info.resident_max >= info.resident and 0 < info.resident_max < PHYSICAL_LIMIT, 'invalid actual lifetime resident peak')
            return int(info.resident_max)
        finally:
            require(self.system.mach_port_deallocate(self.task_port, port.value) == 0, 'accounting NAME port retirement failed')

    def members(self, group: int) -> list[int]:
        # One spare entry proves a truncated list cannot be treated as complete.
        storage = (C.c_int32 * (MAX_MEMBERS + 1))()
        C.set_errno(0)
        count = self.proc.proc_listpgrppids(group, storage, C.sizeof(storage))
        require(count != 0 or C.get_errno() == 0, 'group inventory syscall failed')
        require(0 <= count <= MAX_MEMBERS, 'owned process group inventory unavailable or exceeds budget')
        values = list(storage[:count])
        require(all(pid > 0 for pid in values) and len(set(values)) == len(values), 'invalid/duplicate actual group members')
        return values

    def swap(self) -> dict:
        usage, length = SwapUsage(), C.c_size_t(C.sizeof(SwapUsage))
        require(self.system.sysctlbyname(b'vm.swapusage', C.byref(usage), C.byref(length), None, 0) == 0 and
                length.value == C.sizeof(usage), 'actual vm.swapusage unavailable')
        vm, count = VmInfo64(), C.c_uint32(C.sizeof(VmInfo64) // C.sizeof(C.c_int32))
        # Borrow the complete owned structure as the public integer_t array;
        # no guest buffer or interpreted address participates in this call.
        pointer = C.cast(C.byref(vm), C.POINTER(C.c_int32))
        require(self.system.host_statistics64(self.host_port, 4, pointer, C.byref(count)) == 0 and count.value == 40,
                'actual latest host VM statistics unavailable')
        return {'used_bytes': int(usage.used), 'swapins': int(vm.swapins), 'swapouts': int(vm.swapouts),
                'swapped_count': int(vm.swapped_count), 'pagesize': int(usage.pagesize)}


class Lease:
    """One serial supervisor; not a capability to any debugger source/value API."""
    def __init__(self, commands: tuple[tuple[str, ...], ...] = (), *, as_failure: dict | None = None) -> None:
        # Caller is the fixed qualified driver, not a generic arbitrary-payload
        # command line. Standalone entry admits only the fixed /usr/bin/true probe.
        require(len(commands) <= 7 and len(set(commands)) == len(commands), 'bounded exact current-case argv whitelist')
        self.as_failure = as_failure
        self.commands = {('/usr/bin/true',): sha(Path('/usr/bin/true'))}
        for command in commands:
            require(len(command) >= 6 and command[1:3] == ('-m', 'debug-jit') and '--run' in command and
                    all(type(value) is str and '\0' not in value for value in command), 'fixed debug-full fixture command required')
            target = Path(command[0]).resolve(strict=True)
            self.commands[(str(target), *command[1:])] = sha(target)
        self.fd = -1
        self.native = None
        self.active = None
        self.failed_retirement = False

    def __enter__(self) -> Lease:
        require(sys.platform == 'darwin', 'actual macOS execution required')
        path = Path('/tmp') / f'uwvm-current-debug-{os.getuid()}.lock'
        self.fd = os.open(path, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
        try:
            value = os.fstat(self.fd)
            require(stat.S_ISREG(value.st_mode) and value.st_uid == os.getuid() and stat.S_IMODE(value.st_mode) == 0o600, 'private serial lock identity/mode mismatch')
            fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            # r1 actual EINVAL/readback-infinity is preserved separately. AS is
            # recorded here and is not a physical-memory admission requirement.
            self.as_readback = list(resource.getrlimit(resource.RLIMIT_AS))
            resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
            require(SANDBOX.is_file(), 'fixed sandbox-exec unavailable; arbitrary-fork closure is not admitted')
            self.sandbox_sha = sha(SANDBOX)
            self.native = MacNative()
            self.birth = self.native.birth(self.native.bsd(os.getpid()))
            self.swap_before = self.native.swap()
            require(self.swap_before['used_bytes'] == 0 and self.swap_before['swapped_count'] == 0, 'global no-swap baseline unavailable')
            return self
        except BaseException:
            self.__exit__(*sys.exc_info())
            raise

    def __exit__(self, kind, value, trace) -> None:
        try:
            if self.active is not None:
                self.active.abort()
            if self.native is not None:
                self.native.close()
        finally:
            if self.fd >= 0:
                os.close(self.fd)
                self.fd = -1

    def start(self, argv: list[str], output: Path, *, orphan: bool = False, timeout: float = 90) -> Owned:
        require(self.active is None and not self.failed_retirement, 'previous owned tree did not retire; no next payload admitted')
        command = tuple([str(Path(argv[0]).resolve(strict=True)), *argv[1:]]) if argv else ()
        require(command in self.commands and sha(Path(command[0])) == self.commands[command], 'only exact pinned qualified case/probe argv is admitted')
        require(not orphan or command == ('/usr/bin/true',), 'orphan witness only accompanies the fixed tiny admission probe')
        return Owned(self, list(command), output, orphan=orphan, timeout=timeout)


class Owned:
    def __init__(self, lease: Lease, argv: list[str], output: Path, *, orphan: bool, timeout: float) -> None:
        require(lease.native is not None and argv and all(isinstance(value, str) and '\0' not in value for value in argv) and
                sum(len(value.encode()) + 1 for value in argv) < 32768 and 0 < timeout <= 300, 'bounded direct payload argv/deadline required')
        target = Path(argv[0]).resolve(strict=True)
        require(target.is_file() and os.access(target, os.X_OK) and not target.stat().st_mode & (stat.S_ISUID | stat.S_ISGID), 'actual unprivileged executable payload required')
        self.lease, self.argv = lease, [str(target), *argv[1:]]
        self.output = output.resolve()
        self.output.mkdir(parents=True, exist_ok=False)
        self.stdout_path, self.stderr_path = self.output / 'stdout.log', self.output / 'stderr.log'
        self.closed, self.root, self.child = False, None, None
        self.expected, self.orphan = {}, None
        self.deadline, self.peak, self.samples = time.monotonic() + timeout, 0, 0
        self.events, self.attestations = [], []
        self.lease.active = self
        self.receipt = {'schema': 1, 'guard_source_path': str(Path(__file__).resolve()), 'guard_source_sha256': sha(Path(__file__).resolve()),
                        'python_path': str(Path(sys.executable).resolve(strict=True)), 'python_sha256': sha(Path(sys.executable).resolve(strict=True)), 'purpose': 'actual-macos-fixed-current-debug-no-fork-rss-command', 'passed': False,
                        'actual_argv': self.argv, 'cwd': os.getcwd(), 'target_sha256': sha(target),
                        'prior_actual_AS_failure': lease.as_failure, 'as_readback': lease.as_readback, 'rss_stop_bytes': RSS_STOP, 'rss_ceiling_bytes': PHYSICAL_LIMIT,
                        'resource_scope': 'observed RSS/no-swap plus reported final-task counters; exec can replace a task; no full-PID lifetime upper bound, aggregate kernel cap or AS claim',
                        'sandbox_path': str(SANDBOX), 'sandbox_sha256': lease.sandbox_sha,
                        'supervisor_birth': lease.birth.__dict__, 'uname': list(os.uname()),
                        'swap_before': lease.native.swap(), 'returncode': None, 'root_reaped': False, 'group_retired': False,
                        'orphan_requested': orphan, 'events': self.events, 'attestations': self.attestations}
        self.attest_r = self.gate_w = -1
        out = err = None
        attest_w = gate_r = -1
        try:
            attest_r, attest_w = os.pipe()
            gate_r, gate_w = os.pipe()
            self.attest_r, self.gate_w = attest_r, gate_w
            os.set_blocking(attest_r, False)
            for fd in (attest_r, attest_w, gate_r, gate_w):
                os.set_inheritable(fd, False)
            self.nonce = secrets.token_hex(32)
            guard = [sys.executable, str(Path(__file__).resolve()), '--guard-before', str(attest_w), str(gate_r), self.nonce,
                     'orphan' if orphan else 'normal', lease.sandbox_sha, json.dumps(self.argv, separators=(',', ':'))]
            out, err = self.stdout_path.open('xb'), self.stderr_path.open('xb')
            self.child = subprocess.Popen(guard, stdin=subprocess.PIPE, stdout=out, stderr=err,
                                          close_fds=True, pass_fds=(attest_w, gate_r), start_new_session=True)
            os.close(attest_w); os.close(gate_r)
            attest_w = gate_r = -1
            out.close(); err.close(); out = err = None
            value = lease.native.bsd(self.child.pid)
            self.root = lease.native.birth(value)
            require(self.root.pgid == self.root.pid and value.ppid == os.getpid() and self.root.uid == os.getuid(), 'spawned direct child/session birth mismatch')
            self.expected[self.root.pid] = self.root
            self.receipt['root_birth'] = self.root.__dict__
            self.pending = bytearray()
            got_root = got_witness = False
            witness_registered = not orphan
            while not (got_root and got_witness if orphan else got_root):
                self.check()
                try:
                    chunk = os.read(attest_r, 8192)
                except BlockingIOError:
                    chunk = None
                require(chunk != b'', 'guard exited without all actual startup attestations')
                if chunk:
                    self.pending.extend(chunk)
                    require(len(self.pending) <= 16384, 'bounded private startup channel')
                    while b'\n' in self.pending:
                        line, _, rest = self.pending.partition(b'\n')
                        self.pending[:] = rest
                        row = json.loads(line)
                        require(row.get('nonce') == self.nonce and row.get('as_readback') == lease.as_readback, 'private guard nonce/inherited recorded AS readback mismatch')
                        pid = row.get('pid')
                        if row.get('kind') == 'witness-created':
                            require(orphan and not witness_registered and isinstance(pid, int), 'unexpected witness registration')
                            info = lease.native.bsd(pid)
                            birth = lease.native.birth(info)
                            require(info.ppid == self.root.pid and birth.pgid == self.root.pgid and birth.uid == self.root.uid and info.status == 4,
                                    'witness must be a real stopped direct child in the owned group')
                            self.expected[pid], self.orphan = birth, birth
                            witness_registered = True
                            self.events.append({'event': 'witness-owned-before-guest', 'birth': birth.__dict__})
                            os.kill(pid, signal.SIGCONT)  # still held by unreaped guard parent; cannot reuse
                            os.write(self.gate_w, b'w')
                        else:
                            require(row.get('kind') in ('payload-ready', 'witness-ready') and row.get('fork_errno') == errno.EPERM and
                                    row.get('spawn_errno') == errno.EPERM and row.get('rss_protocol') == 'physical-observation-r2' and
                                    row.get('sandbox_sha256') == lease.sandbox_sha,
                                    'actual inherited no-fork physical-budget protocol proof failed')
                            require(pid in self.expected and identity_matches(self.expected[pid], lease.native.birth(lease.native.bsd(pid))), 'attested PID birth changed')
                            if row['kind'] == 'payload-ready':
                                require(pid == self.root.pid and not got_root and witness_registered, 'duplicate/unregistered payload guard')
                                got_root = True
                            else:
                                require(orphan and self.orphan is not None and pid == self.orphan.pid and not got_witness, 'unexpected witness guard')
                                got_witness = True
                            self.attestations.append(row)
                if not chunk:
                    time.sleep(POLL_SECONDS)
            require(not self.pending, 'unexpected startup bytes after qualified proofs')
            self.check()
            os.write(self.gate_w, b'g')
            os.close(self.gate_w); self.gate_w = -1
            os.close(self.attest_r); self.attest_r = -1
            self.events.append({'event': 'actual-payload-admitted', 'root': self.root.__dict__})
        except BaseException as error:
            self.receipt['error'] = repr(error)
            self.abort()
            raise
        finally:
            for fd in (attest_w, gate_r):
                if fd >= 0:
                    os.close(fd)
            for stream in (out, err):
                if stream is not None:
                    stream.close()

    def check(self, *, retiring: bool = False) -> BsdInfo:
        require(not self.closed and self.child is not None and self.root is not None, 'no live owned root anchor')
        native = self.lease.native
        root = native.bsd(self.root.pid)
        require(identity_matches(self.root, native.birth(root)), 'unreaped actual root birth/group changed')
        members = native.members(self.root.pgid)
        require(self.root.pid in members, 'unreaped owned root is missing from actual group inventory')
        # Startup may briefly contain the one guard-created witness before its
        # message is consumed. It remains below an unexecuted, gate-held root.
        unknown = set(members) - self.expected.keys()
        if unknown and not retiring:
            require(not any(row['kind'] == 'payload-ready' for row in self.attestations) and self.receipt['orphan_requested'] and
                    self.orphan is None and len(unknown) == 1, 'unregistered actual group member')
            for pid in unknown:
                info = native.bsd(pid)
                require(info.ppid == self.root.pid and info.pgid == self.root.pgid, 'unknown member is not the held startup witness')
        rss, rss_roles = [], []
        for pid in members:
            info = native.bsd(pid, missing=True)
            if info is None:
                continue
            require(info.pgid == self.root.pgid, 'actual inventory member left the owned group')
            if pid in self.expected:
                require(identity_matches(self.expected[pid], native.birth(info)), 'owned member PID birth/group changed')
            elif retiring:
                require(info.status == 5, 'unregistered live group member during retirement')
            if not retiring:
                value = native.rss(pid, info)
                rss.append(value)
                role = 'root' if pid == self.root.pid else 'witness' if self.orphan is not None and pid == self.orphan.pid else 'startup-witness'
                rss_roles.append((role, native.birth(info), value))
        for pid, birth in self.expected.items():
            info = native.bsd(pid, missing=True)
            require(info is None or identity_matches(birth, native.birth(info)), 'owned member escaped or was reused')
        supervisor = native.bsd(os.getpid())
        require(identity_matches(self.lease.birth, native.birth(supervisor)), 'supervisor birth changed')
        swap = native.swap()
        if retiring:
            # This private phase follows anchored group kill or an already
            # exited root. Task teardown can precede the BSD zombie transition.
            # Do not invent zero RSS or a qualified sample: actual kernel
            # root/self/witness peaks are still required after full retirement.
            self.receipt['retirement_rss_measurement'] = {
                'qualified': False, 'reason': 'task RSS not sampled during kernel task retirement'}
            return root
        supervisor_rss = native.rss(os.getpid(), supervisor)
        total = supervisor_rss + sum(rss)
        self.peak, self.samples = max(self.peak, total), self.samples + 1
        record_rss_sample(self.receipt, [('supervisor', self.lease.birth, supervisor_rss), *rss_roles], total)
        require(swap['used_bytes'] == 0 and swap['swapped_count'] == 0 and swap['swapins'] == self.lease.swap_before['swapins'] and
                swap['swapouts'] == self.lease.swap_before['swapouts'], 'observed global swap activity; no-swap qualification failed')
        check_rss(supervisor_rss, rss)
        require(time.monotonic() < self.deadline, 'owned command deadline exceeded')
        require(all(path.stat().st_size <= OUTPUT_LIMIT for path in (self.stdout_path, self.stderr_path)), 'bounded regular output exceeded')
        return root

    def send(self, line: str) -> None:
        self.check()
        require('\n' not in line and '\r' not in line and '\0' not in line and len(line.encode()) <= 4096, 'one bounded control line required')
        self.child.stdin.write(line.encode() + b'\n')
        self.child.stdin.flush()

    def finish(self, *, terminate: bool = False) -> int:
        require(not self.closed and self.child is not None, 'owned command already retired')
        try:
            if self.child.stdin is not None and not self.child.stdin.closed:
                self.child.stdin.close()
            if terminate:
                self._kill_group()
            root = self.check(retiring=terminate)
            root_deadline = time.monotonic() + 15 if terminate else self.deadline
            while root.status != 5:
                require(time.monotonic() < root_deadline, 'owned root did not exit within its bounded retirement deadline')
                self.check(retiring=terminate)
                time.sleep(POLL_SECONDS)
                root = self.check(retiring=terminate)
            self.events.append({'event': 'root-exited-unreaped', 'birth': self.root.__dict__})
            if self.orphan is not None and not terminate:
                witness = self.lease.native.bsd(self.orphan.pid, missing=True)
                require(witness is not None and identity_matches(self.orphan, self.lease.native.birth(witness)) and witness.ppid != self.root.pid,
                        'actual post-exit orphan witness was not retained and reparented')
                self.receipt['orphan_after_root_exit'] = {'birth': self.orphan.__dict__, 'actual_parent': int(witness.ppid)}
                # Signal only the group held by our actual unreaped zombie root,
                # never an unanchored, potentially reusable individual PID.
                os.killpg(self.root.pgid, signal.SIGSTOP)
                deadline = time.monotonic() + 5
                while True:
                    stopped = self.lease.native.bsd(self.orphan.pid)
                    require(identity_matches(self.orphan, self.lease.native.birth(stopped)), 'orphan birth changed before kernel peak')
                    if stopped.status == 4:
                        break
                    require(time.monotonic() < deadline, 'trusted orphan failed to stop for exact peak capture')
                    self.check(); time.sleep(POLL_SECONDS)
                self.receipt['witness_stopped_kernel_maxrss_bytes'] = self.lease.native.stopped_peak(self.orphan)
                self.events.append({'event': 'stopped-witness-actual-lifetime-resident-peak', 'birth': self.orphan.__dict__})
            if len(self.lease.native.members(self.root.pgid)) > 1:
                self._kill_group()  # only while the actual unreaped root anchors this group
                self.events.append({'event': 'anchored-group-retirement'})
            deadline = time.monotonic() + 15
            while set(self.lease.native.members(self.root.pgid)) != {self.root.pid}:
                require(time.monotonic() < deadline, 'post-root owned descendants did not retire; no next case allowed')
                self.check(retiring=True)
                time.sleep(POLL_SECONDS)
            self.check(retiring=True)
            self.receipt['group_retired'] = True
            pid, status, usage = os.wait4(self.root.pid, 0)  # first and only root reap, after descendant inventory is empty
            require(pid == self.root.pid, 'actual root wait identity mismatch')
            result = os.waitstatus_to_exitcode(status)
            self.child.returncode = result
            self.receipt.update(root_reaped=True, returncode=result, wait4_root_maxrss_bytes=int(usage.ru_maxrss),
                                sampled_owned_peak_rss_bytes=self.peak, rss_samples=self.samples, swap_after=self.lease.native.swap())
            require(self.lease.native.members(self.root.pgid) == [], 'actual owned group still exists after root reap')
            require(self.lease.native.bsd(self.root.pid, missing=True) is None, 'root PID already reused immediately after retirement; refuse ambiguous proof')
            require(self.receipt['swap_after']['used_bytes'] == 0 and self.receipt['swap_after']['swapped_count'] == 0 and
                    self.receipt['swap_after']['swapins'] == self.lease.swap_before['swapins'] and
                    self.receipt['swap_after']['swapouts'] == self.lease.swap_before['swapouts'], 'post-retirement swap activity observed')
            require(sha(SANDBOX) == self.lease.sandbox_sha and sha(Path(self.argv[0])) == self.receipt['target_sha256'] and
                    sha(Path(__file__).resolve()) == self.receipt['guard_source_sha256'] and
                    sha(Path(self.receipt['python_path'])) == self.receipt['python_sha256'], 'actual executable/guard changed during owned command')
            supervisor_peak = int(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
            self.receipt['supervisor_kernel_maxrss_bytes'] = supervisor_peak
            if not terminate:
                witness_peak = self.receipt.get('witness_stopped_kernel_maxrss_bytes', 0)
                require(self.orphan is None or witness_peak > 0, 'real witness peak missing; sampling cannot substitute')
                reported = peak_upper(int(usage.ru_maxrss), supervisor_peak, witness_peak)
                self.receipt['kernel_reported_task_peak_sum_bytes'] = reported
                self.receipt['max_of_recorded_budget_evidence_bytes'] = max(self.peak, reported)
                self.receipt['kernel_counter_scope'] = 'final payload/witness tasks and supervisor; not earlier exec task lifetimes'
            self.receipt['passed'] = not terminate
            return result
        except BaseException as error:
            self.receipt['error'] = repr(error)
            self.lease.failed_retirement = True
            raise
        finally:
            if self.receipt['root_reaped']:
                self.closed = True
                self.lease.active = None
                self._write_receipt()

    def _kill_group(self) -> None:
        require(self.child is not None and not self.receipt['root_reaped'], 'no actual unreaped child/group anchor')
        try:
            os.killpg(self.child.pid, signal.SIGKILL)
        except ProcessLookupError:
            # ESRCH can mean every anchored group member is already a zombie.
            # Subsequent kernel inventories/birth checks still must prove drain.
            pass

    def abort(self) -> None:
        if self.closed:
            return
        try:
            if self.child is not None:
                # Popen proved successful setsid/exec startup; this direct child
                # has never been reaped, even if libproc startup proof failed.
                self._kill_group()
                if self.root is None:
                    info = self.lease.native.bsd(self.child.pid)
                    self.root = self.lease.native.birth(info)
                    self.expected[self.root.pid] = self.root
                self.finish(terminate=True)
            else:
                self.closed = True
                self.lease.active = None
                self._write_receipt()
        except BaseException as error:
            self.lease.failed_retirement = True
            self.receipt['retirement_error'] = repr(error)
            self._write_receipt()
            raise
        finally:
            for name in ('attest_r', 'gate_w'):
                fd = getattr(self, name)
                if fd >= 0:
                    os.close(fd)
                    setattr(self, name, -1)

    def _write_receipt(self) -> None:
        for name in ('stdout_path', 'stderr_path'):
            path = getattr(self, name)
            if path.is_file():
                self.receipt[name] = str(path)
                self.receipt[name + '_sha256'] = sha(path)
        path = self.output / 'owner.receipt.json'
        temporary = path.with_suffix('.tmp')
        with temporary.open('x') as stream:
            json.dump(self.receipt, stream, indent=2)
            stream.write('\n')
            stream.flush(); os.fsync(stream.fileno())
        os.replace(temporary, path)


def attest(fd: int, value: dict) -> None:
    data = json.dumps(value, separators=(',', ':')).encode() + b'\n'
    require(len(data) <= 4096 and os.write(fd, data) == len(data), 'one atomic private startup record required')


def guard_after(fd: int, gate: int, nonce: str, mode: str, sandbox_sha: str, payload: str) -> None:
    require(sha(SANDBOX) == sandbox_sha, 'sandbox executable changed before proof')
    fork_errno = spawn_errno = None
    try:
        pid = os.fork()
    except OSError as error:
        fork_errno = error.errno
    else:
        if pid == 0:
            os._exit(79)
        os.waitpid(pid, 0)
        raise RuntimeError('fixed sandbox did not deny actual fork')
    try:
        pid = os.posix_spawn('/usr/bin/true', ['/usr/bin/true'], os.environ)
    except OSError as error:
        spawn_errno = error.errno
    else:
        os.waitpid(pid, 0)
        raise RuntimeError('fixed sandbox did not deny actual posix_spawn')
    require(fork_errno == errno.EPERM and spawn_errno == errno.EPERM, 'actual no-fork policy proof requires EPERM')
    attest(fd, {'kind': 'witness-ready' if mode == 'witness' else 'payload-ready', 'pid': os.getpid(), 'nonce': nonce,
                'as_readback': list(resource.getrlimit(resource.RLIMIT_AS)), 'fork_errno': fork_errno, 'spawn_errno': spawn_errno,
                'rss_protocol': 'physical-observation-r2', 'sandbox_sha256': sandbox_sha})
    os.close(fd)
    if mode == 'witness':
        if gate >= 0:
            os.close(gate)
        while True:
            signal.pause()
    require(os.read(gate, 1) == b'g', 'private payload admission gate denied')
    os.close(gate)
    argv = json.loads(payload)
    require(isinstance(argv, list) and bool(argv) and Path(argv[0]).is_absolute(), 'direct actual payload required')
    for name in ('SIGPIPE', 'SIGXFZ', 'SIGXFSZ'):
        number = getattr(signal, name, None)
        if number is not None:
            signal.signal(number, signal.SIG_DFL)
    os.execve(argv[0], argv, os.environ)


def guard_before(fd: int, gate: int, nonce: str, mode: str, sandbox_sha: str, payload: str) -> None:
    require(sys.platform == 'darwin' and len(nonce) == 64 and os.getpgrp() == os.getpid(), 'actual private owned session required')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_FSIZE, (OUTPUT_LIMIT, OUTPUT_LIMIT))
    for handle in (fd, gate):
        os.set_inheritable(handle, True)
    if mode == 'orphan':
        witness = os.fork()  # one admitted witness, before applying no-fork policy
        if witness == 0:
            os.close(0)  # no management-input borrow survives in the witness
            os.close(gate)
            os.kill(os.getpid(), signal.SIGSTOP)
            command = [str(SANDBOX), '-p', PROFILE, sys.executable, str(Path(__file__).resolve()),
                       '--guard-after', str(fd), '-1', nonce, 'witness', sandbox_sha, '[]']
            os.execve(str(SANDBOX), command, os.environ)
        actual, status = os.waitpid(witness, os.WUNTRACED)
        require(actual == witness and os.WIFSTOPPED(status) and os.WSTOPSIG(status) == signal.SIGSTOP, 'actual witness failed to stop before registration')
        attest(fd, {'kind': 'witness-created', 'pid': witness, 'nonce': nonce, 'as_readback': list(resource.getrlimit(resource.RLIMIT_AS))})
        require(os.read(gate, 1) == b'w', 'witness registration gate denied')
    command = [str(SANDBOX), '-p', PROFILE, sys.executable, str(Path(__file__).resolve()),
               '--guard-after', str(fd), str(gate), nonce, 'payload', sandbox_sha, payload]
    os.execve(str(SANDBOX), command, os.environ)



def prior_as_failure(path: Path, digest: str) -> dict:
    require(path.is_file() and path.stat().st_size <= 16384 and sha(path) == digest, 'actual preserved r1 AS failure record pin differs')
    value = json.loads(path.read_text())
    require(value.get('passed') is False and value.get('rules_relaxed') is False and value.get('actual_returncode') == 1 and
            value.get('requested_AS_bytes') == PHYSICAL_LIMIT and value.get('source_sha256') ==
            '097f1ba5fe29ea4bf21d9145d612ace8f8506c9f390d22a5bda5b1e4ec514926' and
            value.get('read_only_original_rlimit_AS_observation') == [(1 << 63)-1] * 2 and
            'setrlimit RLIMIT_AS' in value.get('phase', '') and value.get('actual_exception') == 'ValueError: current limit exceeds maximum limit',
            'record is not the actual preserved pre-payload r1 AS failure')
    return {'path': str(path.resolve(strict=True)), 'sha256': digest, 'record': value}


def main() -> None:
    if len(sys.argv) == 8 and sys.argv[1] in ('--guard-before', '--guard-after'):
        function = guard_before if sys.argv[1] == '--guard-before' else guard_after
        function(int(sys.argv[2]), int(sys.argv[3]), *sys.argv[4:])
        return
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--as-failure-receipt', type=Path, required=True)
    parser.add_argument('--as-failure-sha256', required=True)
    parser.add_argument('--orphan-witness', action='store_true')
    parser.add_argument('--timeout', type=float, default=90)

    args = parser.parse_args()
    command = ['/usr/bin/true']  # standalone admission has no arbitrary-payload option
    with Lease(as_failure=prior_as_failure(args.as_failure_receipt, args.as_failure_sha256)) as lease:
        owned = lease.start(command, args.out, orphan=args.orphan_witness, timeout=args.timeout)
        result = owned.finish()
        require(result == 0, 'actual owned payload returned nonzero')
    print('PASS actual owned no-fork command; observed RSS/no-swap and reported task counters below4GiB; no full-lifetime upper-bound or aggregate kernel-cap claim')


if __name__ == '__main__':
    main()
