#!/usr/bin/env python3
"""Fixed, source-only FreeBSD SDK oracle serial VM controller.

This is not a standalone admission guard. The reviewed outer supervisor must
already own this process, its entire descendant tree and the shared lane ticket.
The only child allowed here is the exact pinned QEMU system command below.
No shell, guest command, external action file, NIC or writable base is admitted.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import select
import signal
import socket
import stat
import struct
import subprocess
import time
import traceback

B = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
RUN = B / 'evidence/qemu-freebsd151-sdk-oracle-vm-20261003-r2'
QEMU_USR = Path('/home/macromodel/.local/opt/qemu-10.2.1/usr')
QEMU = QEMU_USR / 'bin/qemu-system-x86_64'
BIOS = QEMU_USR / 'share/seabios/bios-256k.bin'
QEMU_DATA = QEMU_USR / 'share/qemu'
CLEAN = B / 'qemu-platform-tests/freebsd-clean-image-actual-20261003-r2/FreeBSD-15.1-clean.qcow2'
BOOT = '83f582ec-ff7f-41ce-9631-2c0fbb865aeb'
CG_NAME = '/system.slice/docker-bec3a6e4e013ba07c1583e46eeb81707c90c59375ae1c208a6bb87cc4f773f45.scope'
CG = Path('/sys/fs/cgroup') / CG_NAME.lstrip('/')
E = set(range(16, 32))
LABELS = ('uwvm2-ros-eh', 'uwvm2-ros-noeh', 'uwvm2-eh', 'uwvm2-noeh')
PASS = (b'PASS FreeBSD SDK/kernel and fast_io native_file/status/64-bit-seek/'
        b'mapped-loader/decimal/LE/LEB; filesystem durability/VM continuation/'
        b'ROS LLVM23 product acceptance=false')
MAX_SERIAL = 32 << 20
MAX_LINE = 16 << 10
MAX_OVERLAY = 512 << 20
MAX_ISO = 16 << 20


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def regular(path, *, private=False):
    path = Path(path)
    record = path.lstat()
    if (not stat.S_ISREG(record.st_mode) or path.resolve(strict=True) != path
            or record.st_uid not in (0, 1000) or record.st_nlink < 1):
        raise RuntimeError('resolved pinned root/UID1000 regular provider required: ' + str(path))
    if private and (record.st_uid != 1000 or record.st_nlink != 1 or stat.S_IMODE(record.st_mode) & 0o077):
        raise RuntimeError('private regular mode required: ' + str(path))
    return dict(device=record.st_dev, inode=record.st_ino, uid=record.st_uid,
                gid=record.st_gid, links=record.st_nlink, mode=stat.S_IMODE(record.st_mode), bytes=record.st_size,
                mtime_ns=record.st_mtime_ns, ctime_ns=record.st_ctime_ns)


def private_directory(path):
    record = path.lstat()
    if (not stat.S_ISDIR(record.st_mode) or path.resolve(strict=True) != path
            or record.st_uid != 1000 or stat.S_IMODE(record.st_mode) != 0o700):
        raise RuntimeError('resolved UID1000 private directory required: ' + str(path))
    return (record.st_dev, record.st_ino, record.st_uid, stat.S_IMODE(record.st_mode))


def identity(pid):
    root = Path('/proc') / str(pid)
    text = (root / 'stat').read_text()
    fields = text[text.rfind(')') + 2:].split()
    uid = next(row for row in (root / 'status').read_text().splitlines() if row.startswith('Uid:'))
    return dict(pid=pid, birth=int(fields[19]), ppid=int(fields[1]),
                uid=[int(value) for value in uid.split()[1:]],
                cgroup=(root / 'cgroup').read_text().strip(),
                affinity=sorted(os.sched_getaffinity(pid)), exe=os.readlink(root / 'exe'),
                argv=[os.fsdecode(value) for value in (root / 'cmdline').read_bytes().split(b'\0')[:-1]])


def admitted_limits():
    if Path('/proc/sys/kernel/random/boot_id').read_text().strip() != BOOT:
        raise RuntimeError('reboot invalidated fixed VM authority')
    if (Path('/proc/self/cgroup').read_text().strip() != '0::' + CG_NAME
            or os.getuid() != 1000 or os.geteuid() != 1000
            or not os.sched_getaffinity(0) <= E or not os.sched_getaffinity(0)):
        raise RuntimeError('outer owned UID/cgroup/E-core admission is required')
    if (CG.joinpath('memory.max').read_text().strip() != str(64 << 30)
            or CG.joinpath('memory.swap.max').read_text().strip() != '0'
            or CG.joinpath('cpuset.cpus.effective').read_text().strip() != '0,2,4,6,16-31'):
        raise RuntimeError('original 64GiB/swap0/cpuset authority changed')
    return dict((name, int(value)) for name, value in
                (row.split() for row in CG.joinpath('memory.events').read_text().splitlines()))


class FixedQMP:
    COMMANDS = {'qmp_capabilities', 'query-status', 'query-kvm', 'query-cpus-fast', 'cont'}

    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX)
        self.sock.settimeout(3)
        self.stream = None
        self.sequence = 0
        try:
            self.sock.connect(str(path))
            self.stream = self.sock.makefile('rwb', buffering=0)
            if 'QMP' not in self.read():
                raise RuntimeError('actual QMP greeting required')
            self.request('qmp_capabilities')
        except BaseException:
            self.close()
            raise

    def read(self):
        row = self.stream.readline(MAX_LINE + 1)
        if not row.endswith(b'\n') or len(row) > MAX_LINE:
            raise RuntimeError('QMP framed response exceeds bound or closed')
        value = json.loads(row)
        if not isinstance(value, dict):
            raise RuntimeError('actual QMP response must be one object')
        return value

    def request(self, command):
        if command not in self.COMMANDS:
            raise RuntimeError('fixed QMP command allowlist')
        self.sequence += 1
        identifier = 'uwvm-bsd-' + str(self.sequence)
        self.stream.write(json.dumps({'execute': command, 'id': identifier}).encode() + b'\n')
        for _ in range(32):
            answer = self.read()
            if 'event' in answer:
                continue
            if answer.get('id') != identifier or 'error' in answer or 'return' not in answer:
                raise RuntimeError('actual QMP answer failed: ' + repr(answer))
            return answer['return']
        raise RuntimeError('QMP event/response budget exceeded')

    def close(self):
        if self.stream is not None:
            self.stream.close()
            self.stream = None
        self.sock.close()


def serial_cells(data, nonce):
    """Accept ordered actual four-cell output; never execute or evaluate it."""
    if len(data) > MAX_SERIAL:
        raise RuntimeError('actual guest serial output exceeds bound')
    lines = data.replace(b'\r\n', b'\n').split(b'\n')
    prefix = ('UWVM_BSD_BEGIN_' + nonce).encode()
    begins = [index for index, row in enumerate(lines) if row == prefix]
    if len(begins) != 1:
        raise RuntimeError('one actual fresh-nonce serial begin required')
    start = begins[0] + 1
    cells = []
    for label in LABELS:
        first = ('UWVM_BSD_CELL_BEGIN_' + nonce + '_' + label).encode()
        last_prefix = ('UWVM_BSD_CELL_END_' + nonce + '_' + label + '_STATUS_').encode()
        if start >= len(lines) or lines[start] != first:
            raise RuntimeError('fixed serial cell sequence missing: ' + label)
        finish = next((i for i in range(start + 1, len(lines)) if lines[i].startswith(last_prefix)), None)
        if finish is None or lines[finish] != last_prefix + b'0':
            raise RuntimeError('actual serial cell did not exit successfully: ' + label)
        payload = lines[start + 1:finish]
        if payload.count(PASS) != 1 or any(row.startswith(b'FAIL ') for row in payload):
            raise RuntimeError('actual SDK oracle PASS marker/error mismatch: ' + label)
        if not any(re.fullmatch(rb'kernel.sysname=FreeBSD; release=15\.1-RELEASE[^;]*; machine=amd64; sdk.version=[0-9]+; native.endian=little', row) for row in payload):
            raise RuntimeError('actual guest kernel/architecture identity mismatch: ' + label)
        if not any(row.startswith(b'sdk.stat.size=') for row in payload):
            raise RuntimeError('actual SDK stat layout observation missing: ' + label)
        cells.append(dict(label=label, exit_status=0, output_sha256=hashlib.sha256(b'\n'.join(payload) + b'\n').hexdigest(),
                          output_lines=len(payload)))
        start = finish + 1
    end = ('UWVM_BSD_END_' + nonce + '_STATUS_0').encode()
    if start >= len(lines) or lines[start] != end or lines.count(end) != 1:
        raise RuntimeError('one actual four-cell final success required')
    # Control markers cannot occur in payloads or the later shutdown transcript.
    expected = [prefix, *[marker for label in LABELS for marker in (
        ('UWVM_BSD_CELL_BEGIN_' + nonce + '_' + label).encode(),
        ('UWVM_BSD_CELL_END_' + nonce + '_' + label + '_STATUS_0').encode())], end]
    if [row for row in lines if row.startswith(b'UWVM_BSD_')] != expected:
        raise RuntimeError('duplicate or extra serial control marker')
    return cells



def actual_overlay_backing(overlay):
    """Read only the bounded public QCOW2 header/backing-name fields."""
    with overlay.open('rb') as stream:
        header = stream.read(104)
        if len(header) != 104 or header[:4] != b'QFI\xfb':
            raise RuntimeError('complete actual QCOW2 header required')
        version = struct.unpack_from('>I', header, 4)[0]
        offset = struct.unpack_from('>Q', header, 8)[0]
        length, cluster_bits = struct.unpack_from('>II', header, 16)
        virtual_bytes = struct.unpack_from('>Q', header, 24)[0]
        if (version not in (2, 3) or not 9 <= cluster_bits <= 21
                or not 1 <= length <= 1023 or not 104 <= offset <= (1 << cluster_bits) - length
                or offset + length > regular(overlay, private=True)['bytes']
                or struct.unpack_from('>I', header, 32)[0] != 0
                or (version == 3 and struct.unpack_from('>Q', header, 72)[0] != 0)):
            raise RuntimeError('new unencrypted clean QCOW2 overlay/header extent required')
        stream.seek(offset)
        name = stream.read(length)
        if len(name) != length or name != os.fsencode(CLEAN):
            raise RuntimeError('overlay must name only the exact bound read-only clean base')
    with CLEAN.open('rb') as stream:
        base = stream.read(36)
    if (len(base) != 36 or base[:4] != b'QFI\xfb'
            or struct.unpack_from('>Q', base, 8)[0] != 0
            or struct.unpack_from('>Q', base, 24)[0] != virtual_bytes):
        raise RuntimeError('exact clean base must have no parent and match overlay virtual size')
    return dict(qcow2_version=version, backing_file=str(CLEAN), virtual_bytes=virtual_bytes,
                encrypted=False, incompatible_features=0)


def fixed_argv(ipc):
    return [str(QEMU), '-name', 'uwvm-freebsd151-cold',
            '-machine', 'q35,dump-guest-core=off,mem-merge=off', '-accel', 'kvm', '-cpu', 'host',
            '-smp', '4', '-m', '4096', '-no-user-config', '-nodefaults', '-display', 'none',
            '-vga', 'none', '-nic', 'none', '-monitor', 'none', '-parallel', 'none',
            '-L', str(QEMU_DATA), '-bios', str(BIOS),
            '-device', 'virtio-scsi-pci,id=scsi0,addr=0xa',
            '-drive', 'if=none,id=osdisk,format=qcow2,file=' + str(RUN / 'disk.qcow2'),
            '-device', 'scsi-hd,drive=osdisk,bus=scsi0.0,bootindex=1',
            '-drive', 'if=none,id=seed,media=cdrom,format=raw,readonly=on,file=' + str(RUN / 'inputs.iso'),
            '-device', 'scsi-cd,drive=seed,bus=scsi0.0',
            '-serial', 'file:' + str(RUN / 'serial.log'),
            '-qmp', 'unix:' + str(ipc / 'qmp.sock') + ',server=on,wait=off', '-S']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    before_events = admitted_limits()
    os.umask(0o077)
    run_identity = private_directory(RUN)
    manifest_identity = regular(args.manifest, private=True)
    if not args.manifest.is_relative_to(RUN) or manifest_identity['bytes'] > 1 << 20:
        raise RuntimeError('private bounded actual VM manifest required')
    raw = args.manifest.read_bytes()
    manifest_sha = hashlib.sha256(raw).hexdigest()
    manifest = json.loads(raw)
    if (manifest.get('schema') != 'uwvm-freebsd151-sdk-oracle-serial-vm-bound-v1'
            or manifest.get('run_root') != str(RUN)
            or manifest.get('controller_sha256') != digest(__file__)
            or manifest.get('actual_cold_ELF_and_providers_bound') is not True
            or manifest.get('actual_guest_executed') is not False
            or manifest.get('ROS_LLVM23_product_qualified') is not False
            or not 1 <= manifest.get('deadline_seconds', 0) <= 900):
        raise RuntimeError('fresh fixed cold SDK scope/provenance required')
    nonce = manifest.get('nonce', '')
    if not re.fullmatch('[0-9a-f]{64}', nonce):
        raise RuntimeError('fresh one-run 256-bit serial nonce required')
    ipc = Path('/tmp/uwvm-bsd151-' + nonce[:24])
    ipc_identity = private_directory(ipc)
    if manifest.get('ipc_root') != str(ipc) or len(os.fsencode(ipc / 'qmp.sock')) >= 108:
        raise RuntimeError('private exact bounded QMP path required')
    immutable = manifest.get('immutable_inputs', {})
    if not isinstance(immutable, dict) or not immutable:
        raise RuntimeError('actual controller/QEMU/SDK/firmware/base/ISO provider pins required')
    required = {str(QEMU.resolve(strict=True)), str(BIOS), str(RUN / 'inputs.iso'),
                str(Path(__file__).resolve(strict=True)), str(CLEAN),
                str(RUN / 'serial-seed-prepared.json'), str(RUN / 'cold-qualified.json')}
    if not required <= set(immutable):
        raise RuntimeError('minimum exact immutable QEMU/provider closure missing')
    immutable_identities = {}
    for name, row in immutable.items():
        path = Path(name)
        identity_before = regular(path)
        if (identity_before != row.get('identity') or row.get('bytes') != identity_before['bytes']
                or not re.fullmatch('[0-9a-f]{64}', row.get('sha256', ''))
                or digest(path) != row['sha256'] or regular(path) != identity_before):
            raise RuntimeError('actual immutable VM input changed: ' + name)
        immutable_identities[name] = identity_before
    seed = json.loads((RUN / 'serial-seed-prepared.json').read_bytes())
    cold = json.loads((RUN / 'cold-qualified.json').read_bytes())
    if (seed.get('schema') != 'uwvm-freebsd151-serial-cidata-prepared-v1'
            or seed.get('nonce') != nonce
            or seed.get('actual_source_inputs_sha256') != digest(RUN / 'cold-qualified.json')
            or cold.get('schema') != 'uwvm-freebsd151-sdk-oracle-cross-build-actual-v1'
            or cold.get('target') != 'x86_64-unknown-freebsd15.1'
            or cold.get('cold_object_and_provider_bindings_verified') is not True
            or cold.get('actual_guest_executed') is not False
            or cold.get('ROS_LLVM23_product_qualified') is not False
            or seed.get('required_guest_runtime') != cold.get('required_guest_runtime')
            or seed.get('required_guest_runtime_sha256') != cold.get('required_guest_runtime_sha256')
            or not re.fullmatch('[0-9a-f]{64}', cold.get('required_guest_runtime_sha256', ''))
            or [row.get('label') for row in seed.get('payloads', [])] != list(LABELS)
            or [row.get('label') for row in cold.get('variants', [])] != list(LABELS)):
        raise RuntimeError('actual prepared seed/nonce/four cold artifacts provenance mismatch')
    for name in ('source_manifest_sha256', 'SDK_actual_manifest_sha256', 'original_guard_receipts_sha256'):
        if seed.get(name) != cold.get(name) or not re.fullmatch('[0-9a-f]{64}', seed.get(name, '')):
            raise RuntimeError('actual SDK/source/original guard receipt changed: ' + name)
    for payload, actual in zip(seed['payloads'], cold['variants']):
        if payload.get('sha256') != actual.get('binary_sha256'):
            raise RuntimeError('actual ELF hash differs from the admitted read-only ISO seed')
    overlay = RUN / 'disk.qcow2'
    overlay_before = regular(overlay, private=True)
    if (overlay_before != manifest.get('overlay_before_identity') or overlay_before['bytes'] > MAX_OVERLAY
            or digest(overlay) != manifest.get('overlay_before_sha256')
            or manifest.get('overlay_has_only_bound_readonly_base') is not True
            or manifest.get('overlay_maximum_bytes') != MAX_OVERLAY
            or regular(RUN / 'inputs.iso')['bytes'] > MAX_ISO):
        raise RuntimeError('actual private finite overlay/ISO/base binding required')
    if any((RUN / name).exists() for name in ('serial.log', 'qemu.log', 'controller-receipt.json')) or (ipc / 'qmp.sock').exists():
        raise RuntimeError('all mutable VM outputs must be new')
    overlay_backing = actual_overlay_backing(overlay)
    record = dict(schema='uwvm-freebsd151-sdk-oracle-serial-vm-actual-v1', passed=False,
                  actual_guest_executed=False, actual_platform_qualified=False,
                  ROS_LLVM23_product_qualified=False, whole_VM_restore_qualified=False,
                  manifest_sha256=manifest_sha, controller_sha256=digest(__file__),
                  argv=fixed_argv(ipc), serial_cells=[], children=[], retirement=[],
                  immutable_inputs=immutable, before_events=before_events, actual_overlay_backing=overlay_backing)
    child = None; child_fd = None; child_identity = None; qmp = None; log = None
    started = time.monotonic()

    def write():
        temporary = RUN / 'controller-receipt.tmp'
        descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        with os.fdopen(descriptor, 'w') as stream:
            json.dump(record, stream, indent=2, sort_keys=True); stream.write('\n')
        os.replace(temporary, RUN / 'controller-receipt.json')

    def live_inputs():
        if private_directory(RUN) != run_identity or private_directory(ipc) != ipc_identity:
            raise RuntimeError('owned VM directory identity changed')
        now = regular(overlay, private=True)
        if (any(now[key] != overlay_before[key] for key in ('device', 'inode', 'uid', 'gid', 'mode'))
                or not overlay_before['bytes'] <= now['bytes'] <= MAX_OVERLAY):
            raise RuntimeError('owned overlay identity/budget changed')
        if any(admitted_limits().get(key) != before_events.get(key) for key in ('oom', 'oom_kill', 'oom_group_kill')):
            raise RuntimeError('original cgroup OOM event changed')
        for name in ('qemu.log', 'serial.log'):
            path = RUN / name
            if path.exists() and regular(path, private=True)['bytes'] > MAX_SERIAL:
                raise RuntimeError('bounded regular VM output exceeded: ' + name)

    def mapped_inputs():
        root = Path('/proc') / str(child.pid)
        for line in (root / 'maps').read_text().splitlines():
            fields = line.split(maxsplit=5)
            if len(fields) != 6 or not fields[5].startswith('/'):
                continue
            name = fields[5]
            if name.endswith(' (deleted)'):
                raise RuntimeError('deleted QEMU file mapping is not admitted')
            resolved = str(Path(name).resolve(strict=True))
            if resolved == str(overlay):
                current = regular(overlay, private=True)
                if 'x' in fields[1]:
                    raise RuntimeError('executable mutable overlay mapping')
            elif resolved in immutable:
                current = regular(Path(resolved))
                if current != immutable_identities[resolved]:
                    raise RuntimeError('immutable QEMU mapped input identity changed')
            else:
                raise RuntimeError('unknown QEMU mapped input: ' + resolved)
            device = tuple(int(value, 16) for value in fields[3].split(':'))
            if device != (os.major(current['device']), os.minor(current['device'])) or int(fields[4]) != current['inode']:
                raise RuntimeError('actual QEMU mapping inode/device mismatch')
            record.setdefault('actual_mapped_inputs', {})[resolved] = current

    def cancel(number, frame):
        raise KeyboardInterrupt('fixed VM controller cancellation ' + str(number))

    for number in (signal.SIGINT, signal.SIGTERM):
        signal.signal(number, cancel)
    try:
        write()
        log = (RUN / 'qemu.log').open('xb', buffering=0)
        (RUN / 'qemu.log').chmod(0o600)
        oldmask = signal.pthread_sigmask(signal.SIG_BLOCK, {signal.SIGINT, signal.SIGTERM})
        try:
            # A fixed environment removes inherited loader injection and
            # locale/plugin configuration before the pinned executable starts.
            # Actual /proc mappings must still match the immutable provider pins.
            env = {'PATH': '/usr/bin:/bin', 'LANG': 'C', 'LC_ALL': 'C',
                   'LD_LIBRARY_PATH': str(QEMU_USR / 'lib/x86_64-linux-gnu') + ':' + str(QEMU_USR / 'lib')}
            record['actual_child_environment'] = env
            child = subprocess.Popen(fixed_argv(ipc), stdout=log, stderr=subprocess.STDOUT,
                                     stdin=subprocess.DEVNULL, cwd=RUN, env=env, close_fds=True)
            # Acquire this original FD before any fallible /proc/provenance read.
            # The outer guard owns the whole subtree if acquisition itself fails.
            child_fd = os.pidfd_open(child.pid)
            child_identity = identity(child.pid)
            if (child_identity['ppid'] != os.getpid() or child_identity['uid'] != [1000] * 4
                    or child_identity['cgroup'] != '0::' + CG_NAME
                    or not set(child_identity['affinity']) <= E
                    or child_identity['exe'] != str(QEMU.resolve(strict=True))
                    or child_identity['argv'] != fixed_argv(ipc)
                    or identity(child.pid) != child_identity):
                raise RuntimeError('actual original QEMU child identity/admission failed')
            record['children'].append(dict(identity=child_identity, original_pidfd_acquired=True))
            write()
        finally:
            signal.pthread_sigmask(signal.SIG_SETMASK, oldmask)
        while not (ipc / 'qmp.sock').exists():
            if time.monotonic() - started > 30 or select.select([child_fd], [], [], 0)[0]:
                raise RuntimeError('actual QEMU bootstrap exited or exceeded deadline')
            live_inputs(); time.sleep(0.1)
        socket_record = (ipc / 'qmp.sock').lstat()
        if not stat.S_ISSOCK(socket_record.st_mode) or socket_record.st_uid != 1000:
            raise RuntimeError('owned actual QMP socket required')
        qmp = FixedQMP(ipc / 'qmp.sock')
        socket_identity = (socket_record.st_dev, socket_record.st_ino, socket_record.st_uid)
        record['actual_kvm'] = qmp.request('query-kvm')
        if record['actual_kvm'] != {'enabled': True, 'present': True}:
            raise RuntimeError('actual KVM enabled/present required; no silent TCG fallback')
        record['actual_vcpus'] = qmp.request('query-cpus-fast')
        cpus = record['actual_vcpus']
        if (not isinstance(cpus, list) or len(cpus) != 4
                or sorted(row.get('cpu-index') for row in cpus) != [0, 1, 2, 3]
                or any(row.get('target') != 'x86_64' for row in cpus)):
            raise RuntimeError('actual fixed four x64 vCPUs required')
        for row in cpus:
            tid = row.get('thread-id')
            thread_status = (Path('/proc') / str(tid) / 'status').read_text() if isinstance(tid, int) else ''
            tgid = next((row.split()[1] for row in thread_status.splitlines() if row.startswith('Tgid:')), None)
            if not isinstance(tid, int) or tgid != str(child.pid) or not os.sched_getaffinity(tid) <= E:
                raise RuntimeError('actual guest vCPU thread escaped E-core pool')
        status = qmp.request('query-status')
        if status.get('running') is not False or status.get('status') != 'prelaunch':
            raise RuntimeError('actual QEMU must start paused before validation')
        live_inputs(); mapped_inputs()
        qmp.request('cont'); record['actual_guest_executed'] = True; write()
        deadline = started + manifest['deadline_seconds']
        while not select.select([child_fd], [], [], 0.25)[0]:
            if time.monotonic() > deadline:
                raise RuntimeError('fixed guest overall deadline exceeded')
            live_inputs()
            current_socket = (ipc / 'qmp.sock').lstat()
            if not stat.S_ISSOCK(current_socket.st_mode) or (current_socket.st_dev, current_socket.st_ino, current_socket.st_uid) != socket_identity:
                raise RuntimeError('owned QMP socket inode/device changed')
            now = identity(child.pid)
            if now != child_identity:
                raise RuntimeError('original live QEMU identity changed')
            mapped_inputs()
        # This controller reaps only its own original child, never a guessed PID.
        code = child.wait(timeout=5)
        record['original_child_returncode'] = code
        if code != 0:
            raise RuntimeError('actual QEMU did not exit successfully')
        live_inputs()
        serial = (RUN / 'serial.log').read_bytes()
        record['serial_sha256'] = hashlib.sha256(serial).hexdigest()
        record['serial_bytes'] = len(serial)
        record['serial_cells'] = serial_cells(serial, nonce)
        record['passed'] = True
    except BaseException as error:
        record['error'] = type(error).__name__ + ': ' + str(error)
        record['traceback'] = traceback.format_exc()
    finally:
        signal.pthread_sigmask(signal.SIG_BLOCK, {signal.SIGINT, signal.SIGTERM})
        if qmp is not None:
            try: qmp.close()
            except OSError: pass
        if child is not None and child_fd is None:
            record['unadmitted_child_cleanup_required_by_outer_supervisor'] = child.pid
            record['passed'] = False
        if child_fd is not None:
            code = None; ready = False
            def signal_original_child(number):
                try:
                    signal.pidfd_send_signal(child_fd, number)
                except ProcessLookupError:
                    # Exit can occur between the zero-time readiness check and
                    # this signal. Accept ESRCH only with this original PIDFD
                    # actually ready; still wait/reap the exact Popen child.
                    if not select.select([child_fd], [], [], 0)[0]:
                        raise
                    record.setdefault('original_pidfd_exit_signal_races', []).append(
                        dict(signal=number, original_pidfd_ready=True))
            try:
                if not select.select([child_fd], [], [], 0)[0]:
                    signal_original_child(signal.SIGTERM)
                try: code = child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    signal_original_child(signal.SIGKILL)
                    code = child.wait(timeout=10)
                ready = bool(select.select([child_fd], [], [], 0)[0])
            except BaseException as error:
                record['cleanup_error'] = type(error).__name__ + ': ' + str(error)
                record['passed'] = False
            finally:
                record['retirement'].append(dict(pid=child.pid,
                    birth=None if child_identity is None else child_identity['birth'],
                    original_pidfd_ready=ready, actual_wait_returncode=code))
                os.close(child_fd)
                if not ready or code is None:
                    record['passed'] = False
        if log is not None:
            log.close()
        try:
            for name, row in immutable.items():
                if regular(Path(name)) != immutable_identities[name] or digest(name) != row['sha256']:
                    raise RuntimeError('original immutable VM input changed: ' + name)
            if regular(args.manifest, private=True) != manifest_identity or digest(args.manifest) != manifest_sha:
                raise RuntimeError('original VM manifest changed')
            live_inputs()
            record['immutable_after_match'] = True
            record['overlay_after_identity'] = regular(overlay, private=True)
            record['overlay_after_sha256'] = digest(overlay)
        except BaseException as error:
            record['after_input_error'] = type(error).__name__ + ': ' + str(error)
            record['passed'] = False
        record['wall_seconds'] = time.monotonic() - started
        write()
    # The outer original guard and lane-retirement receipt are additional gates.
    # Inner serial success is not published as platform or ROS product acceptance.
    return 0 if record['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
