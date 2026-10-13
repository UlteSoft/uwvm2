#!/usr/bin/env python3
"""Fixed, source-only Windows four-profile native RAII serial controller.

This is not a standalone admission guard. The reviewed outer supervisor must
already own this process, its entire descendant tree and the shared lane ticket.
The only child allowed here is the exact pinned QEMU system command below.
Only the frozen CD-ROM bootstrap keyboard text is admitted; no host shell, external action file, NIC or writable base is admitted. Completed serial cases do not prove guest script exit or post-frame transport retirement.
"""
from __future__ import annotations

if not __debug__:
    raise RuntimeError("original checks require an unoptimized Python host")

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

from windows_raii_serial_case_facts import bounded_json, parse_original_serial
from windows_raii_r5_fixed_keyboard_command import fixed_keyboard_command

B = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
RUN = B / 'evidence/qemu-windows-raii-r5-serial-20261003-r1'
QEMU_USR = Path('/home/macromodel/.local/opt/qemu-10.2.1/usr')
QEMU = QEMU_USR / 'bin/qemu-system-x86_64'
VGA_BIOS = QEMU_USR / 'share/seabios/vgabios-stdvga.bin'
BASE_ROOT = Path('/home/macromodel/Documents/qemu/uwvm2-win11-storage')
ROM = BASE_ROOT / 'windows.rom'
ORIGINAL_VARS = BASE_ROOT / 'windows.vars'
QEMU_DATA = QEMU_USR / 'share/qemu'
CLEAN = BASE_ROOT / 'data.img'
BASE_RECORD = B / 'qemu-platform-tests/windows-readonly-base-actual-20261003-r1/base-inputs-actual.json'
BOOT = '83f582ec-ff7f-41ce-9631-2c0fbb865aeb'
CG_NAME = '/system.slice/docker-bec3a6e4e013ba07c1583e46eeb81707c90c59375ae1c208a6bb87cc4f773f45.scope'
CG = Path('/sys/fs/cgroup') / CG_NAME.lstrip('/')
E = set(range(16, 32))
LABELS = ('uwvm2-ros-eh', 'uwvm2-ros-noeh', 'uwvm2-eh', 'uwvm2-noeh')
MAX_SERIAL = 32 << 20
MAX_LINE = 16 << 10
MAX_OVERLAY = 512 << 20
MAX_ISO = 16 << 20


def digest(path):
    """Own a bounded read-only descriptor; preserve all original input bytes."""
    path = Path(path)
    before = regular(path)
    owned = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        def fields(item):
            return dict(device=item.st_dev, inode=item.st_ino, uid=item.st_uid,
                        gid=item.st_gid, links=item.st_nlink, mode=stat.S_IMODE(item.st_mode),
                        bytes=item.st_size, mtime_ns=item.st_mtime_ns, ctime_ns=item.st_ctime_ns)
        if fields(os.fstat(owned)) != before:
            raise RuntimeError('original immutable input descriptor identity changed')
        if path == CLEAN:
            os.posix_fadvise(owned, 0, 0, os.POSIX_FADV_SEQUENTIAL)
        value = hashlib.sha256(); count = 0
        while count < before['bytes']:
            piece = os.read(owned, min(1 << 20, before['bytes'] - count))
            if not piece:
                raise RuntimeError('original immutable input ended early')
            value.update(piece); count += len(piece)
            if path == CLEAN:
                # The kernel CG remains the memory authority. Do not retain a
                # deliberate sixty-four-GiB sequential page-cache working set.
                os.posix_fadvise(owned, count - len(piece), len(piece), os.POSIX_FADV_DONTNEED)
        if (os.read(owned, 1) or fields(os.fstat(owned)) != before or regular(path) != before):
            raise RuntimeError('original immutable input changed during full hash')
        return value.hexdigest()
    finally:
        os.close(owned)


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
    COMMANDS = {'qmp_capabilities', 'query-status', 'query-kvm', 'query-cpus-fast', 'cont', 'stop', 'send-key'}

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

    def request(self, command, arguments=None):
        if command not in self.COMMANDS:
            raise RuntimeError('fixed QMP command allowlist')
        self.sequence += 1
        identifier = 'uwvm-win-raii-' + str(self.sequence)
        packet = {'execute': command, 'id': identifier}
        if command == 'send-key':
            if (not isinstance(arguments, dict) or set(arguments) != {'keys', 'hold-time'}
                    or arguments['hold-time'] != 40 or not isinstance(arguments['keys'], list)
                    or not 1 <= len(arguments['keys']) <= 2):
                raise RuntimeError('only fixed forty-ms keyboard qcodes are admitted')
            allowed = set('abcdefghijklmnopqrstuvwxyz0123456789') | {'shift', 'meta_l', 'ret', 'spc', 'dot', 'minus', 'slash', 'equal'}
            for key in arguments['keys']:
                if not isinstance(key, dict) or set(key) != {'type', 'data'} or key['type'] != 'qcode' or key['data'] not in allowed:
                    raise RuntimeError('fixed literal keyboard qcode required')
            packet['arguments'] = arguments
        elif arguments is not None:
            raise RuntimeError('arguments are forbidden for every other fixed QMP command')
        self.stream.write(json.dumps(packet).encode() + b'\n')
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


def keyboard_codes(character):
    # QMP send-key is structured; no guest bytes or action file become HMP text.
    # https://www.qemu.org/docs/master/interop/qemu-qmp-ref.html#command-send-key
    if 'a' <= character <= 'z' or '0' <= character <= '9':
        return [character]
    if 'A' <= character <= 'Z':
        return ['shift', character.lower()]
    names = {' ': ['spc'], '.': ['dot'], '-': ['minus'], '/': ['slash'],
             '=': ['equal'], '+': ['shift', 'equal']}
    if character not in names:
        raise RuntimeError('fixed encoded bootstrap alphabet exceeded')
    return names[character]


def original_serial_has_complete_frame(path, nonce):
    """Bounded readiness only; parsed completed-case facts require later pause."""
    before = path.lstat()
    if (not stat.S_ISREG(before.st_mode) or before.st_uid != 1000 or before.st_nlink != 1
            or stat.S_IMODE(before.st_mode) & 0o077 or before.st_size > MAX_SERIAL):
        raise RuntimeError('owned bounded regular serial output required')
    owned = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | os.O_CLOEXEC)
    try:
        now = os.fstat(owned)
        if (now.st_dev, now.st_ino, now.st_uid, now.st_mode, now.st_nlink) != (
                before.st_dev, before.st_ino, before.st_uid, before.st_mode, before.st_nlink):
            raise RuntimeError('original serial output identity changed before owned read')
        pieces = []; count = 0
        while True:
            part = os.read(owned, min(65536, MAX_SERIAL + 1 - count))
            if not part:
                break
            count += len(part)
            if count > MAX_SERIAL:
                raise RuntimeError('serial output exceeds original budget')
            pieces.append(part)
        after = os.fstat(owned); named = path.lstat()
        for item in (after, named):
            if (item.st_dev, item.st_ino, item.st_uid, item.st_mode, item.st_nlink) != (
                    now.st_dev, now.st_ino, now.st_uid, now.st_mode, now.st_nlink):
                raise RuntimeError('original serial output identity changed during readiness read')
        data = b''.join(pieces)
    finally:
        os.close(owned)
    prefix = b'UWVM-WIN-RAII-R5 ' + nonce.encode('ascii') + b' '
    return any(line.startswith(prefix) and line.endswith(b'\n') for line in data.splitlines(keepends=True))


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
    if virtual_bytes != 64 << 30 or regular(CLEAN)['bytes'] != virtual_bytes:
        raise RuntimeError('fixed raw sixty-four-GiB Windows base/overlay size required')
    return dict(qcow2_version=version, backing_file=str(CLEAN), backing_format='raw',
                virtual_bytes=virtual_bytes, encrypted=False, incompatible_features=0)



# This classifier is confined to the original pinned x64 Linux QEMU suite.
# A deleted suffix does not prove a deleted library: Linux shmem_zero_setup
# uses internal backing named dev/zero for shared anonymous mappings.
# https://raw.githubusercontent.com/torvalds/linux/v6.17/mm/shmem.c
# No regular provider identity, open FD, guest-RAM owner or memfd is inferred.
MAP_LINE = re.compile(
    r'([0-9a-f]{1,16})-([0-9a-f]{1,16}) ([r-][w-][x-][ps]) '
    r'([0-9a-f]{1,16}) ([0-9a-f]{1,8}):([0-9a-f]{1,8}) ([0-9]{1,20})'
    r'(?:[ \t]+(.*))?')
MAX_MAPS = 1 << 20
MAX_MAP_ROWS = 16384


def map_rows(raw):
    """Parse every complete kernel maps field; never accept partial lines."""
    if not raw or len(raw.encode()) > MAX_MAPS or not raw.endswith('\n'):
        raise RuntimeError('bounded complete original QEMU maps snapshot required')
    lines = raw.splitlines()
    if len(lines) > MAX_MAP_ROWS:
        raise RuntimeError('original QEMU maps row budget exceeded')
    rows = []
    previous_end = 0
    for line in lines:
        match = MAP_LINE.fullmatch(line)
        if match is None:
            raise RuntimeError('malformed complete original QEMU maps row')
        start, end, offset, major, minor = (
            int(match.group(i), 16) for i in (1, 2, 4, 5, 6))
        inode = int(match.group(7))
        if (not 0 < start < end < 1 << 64 or start < previous_end
                or start % 4096 or end % 4096 or inode >= 1 << 64):
            raise RuntimeError('original QEMU maps address/inode range mismatch')
        previous_end = end
        rows.append(dict(start=start, end=end, permissions=match.group(3),
                         offset=offset, device_major=major, device_minor=minor,
                         inode=inode, pathname=match.group(8) or '', raw_line=line))
    return rows


def pseudo_anonymous_data(row):
    """Only the observed one-page nonexec internal-shmem form is a candidate.

    This exact full-field data classification is not a provider allowlist.
    It cannot authorize any executable mapping or relax regular-file hashes.
    The caller separately binds first admission to the live original PIDFD,
    complete child identity and already verified KVM/prelaunch observation.
    """
    return (row['pathname'] == '/dev/zero (deleted)'
            and row['permissions'] == 'rw-s'
            and row['offset'] == 0
            and row['device_major'] == 0 and row['device_minor'] == 1
            and row['inode'] > 0
            and row['end'] - row['start'] == 4096
            and row['start'] % 4096 == 0 and row['end'] % 4096 == 0)


def fixed_argv(ipc):
    return [str(QEMU), '-name', 'uwvm-windows-raii-r5-serial',
            '-machine', 'q35,dump-guest-core=off,mem-merge=off', '-accel', 'kvm', '-cpu', 'host',
            '-smp', '4', '-m', '8192', '-no-user-config', '-nodefaults', '-display', 'none',
            '-vga', 'none', '-nic', 'none', '-monitor', 'none', '-parallel', 'none',
            '-L', str(QEMU_DATA), '-device', 'VGA,romfile=' + str(VGA_BIOS),
            '-drive', 'if=pflash,unit=0,format=raw,readonly=on,file=' + str(ROM),
            '-drive', 'if=pflash,unit=1,format=raw,file=' + str(RUN / 'windows.vars'),
            '-device', 'virtio-scsi-pci,id=scsi0,addr=0xa',
            '-drive', 'if=none,id=osdisk,format=qcow2,file=' + str(RUN / 'disk.qcow2'),
            '-device', 'scsi-hd,drive=osdisk,bus=scsi0.0,bootindex=1',
            '-drive', 'if=none,id=inputs,media=cdrom,format=raw,readonly=on,file=' + str(RUN / 'inputs.iso'),
            '-device', 'scsi-cd,drive=inputs,bus=scsi0.0',
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
    manifest = bounded_json(raw)
    if (manifest.get('schema') != 'uwvm-windows-raii-r5-serial-vm-bound-v1'
            or manifest.get('run_root') != str(RUN)
            or manifest.get('controller_sha256') != digest(__file__)
            or manifest.get('actual_four_cold_PE_and_providers_bound') is not True
            or manifest.get('actual_guest_executed') is not False
            or manifest.get('ROS_LLVM23_product_qualified') is not False
            or type(manifest.get('deadline_seconds')) is not int
            or not 1 <= manifest['deadline_seconds'] <= 900):
        raise RuntimeError('fresh fixed cold SDK scope/provenance required')
    nonce = manifest.get('nonce', '')
    if not re.fullmatch('[0-9a-f]{64}', nonce):
        raise RuntimeError('fresh one-run 256-bit serial nonce required')
    ipc = Path('/tmp/uwvm-win-raii-' + nonce[:24])
    ipc_identity = private_directory(ipc)
    if manifest.get('ipc_root') != str(ipc) or len(os.fsencode(ipc / 'qmp.sock')) >= 108:
        raise RuntimeError('private exact bounded QMP path required')
    immutable = manifest.get('immutable_inputs', {})
    if not isinstance(immutable, dict) or not immutable:
        raise RuntimeError('actual controller/QEMU/SDK/firmware/base/ISO provider pins required')
    required = {str(QEMU.resolve(strict=True)), str(VGA_BIOS), str(ROM), str(ORIGINAL_VARS),
                str(RUN / 'inputs.iso'), str(Path(__file__).resolve(strict=True)), str(CLEAN),
                str(RUN / 'qualification.json'), str(RUN / 'serial-seed-prepared.json'), str(BASE_RECORD)}
    required |= {str(Path(__file__).with_name(name)) for name in (
        'windows_raii_serial_case_facts.py', 'windows_raii_r5_fixed_keyboard_command.py')}
    if not required <= set(immutable):
        raise RuntimeError('minimum exact immutable Windows host/provider/seed inputs missing')
    immutable_identities = {}
    for name, row in immutable.items():
        path = Path(name)
        identity_before = regular(path)
        if (identity_before != row.get('identity') or row.get('bytes') != identity_before['bytes']
                or not re.fullmatch('[0-9a-f]{64}', row.get('sha256', ''))
                or digest(path) != row['sha256'] or regular(path) != identity_before):
            raise RuntimeError('actual immutable VM input changed: ' + name)
        immutable_identities[name] = identity_before
    base_record = bounded_json(BASE_RECORD.read_bytes())
    if (base_record.get('schema') != 'uwvm-windows-original-readonly-base-hash-actual-v1'
            or base_record.get('boot_id') != BOOT or base_record.get('cgroup') != CG_NAME
            or base_record.get('uid') != 1000 or base_record.get('actual_QEMU_or_guest_executed') is not False
            or base_record.get('whole_ROS_bundled_product_qualified') is not False
            or digest(BASE_RECORD) != manifest.get('actual_base_input_record_sha256')):
        raise RuntimeError('actual original read-only full-hash record binding required')
    base_rows = base_record.get('base_and_firmware_full_hashes')
    if (not isinstance(base_rows, list) or len(base_rows) != 3
            or [row.get('path') for row in base_rows] != [str(CLEAN), str(ROM), str(ORIGINAL_VARS)]):
        raise RuntimeError('exact actual raw-base/firmware three-input record required')
    for row in base_rows:
        expected = immutable[row['path']]
        if (row.get('sha256') != expected['sha256'] or row.get('bytes_read') != expected['bytes']
                or row.get('identity') != immutable_identities[row['path']]
                or row.get('descriptor_opened_readonly') is not True
                or row.get('original_file_modified') is not False):
            raise RuntimeError('original read-only full-hash evidence differs from VM immutable pin')
    qualification_raw = (RUN / 'qualification.json').read_bytes()
    qualification_sha = hashlib.sha256(qualification_raw).hexdigest()
    qualification = bounded_json(qualification_raw)
    seed = bounded_json((RUN / 'serial-seed-prepared.json').read_bytes())
    if (qualification.get('schema') != 'uwvm-windows-raii-r5-paired-launcher-guest-qualification-v1'
            or qualification.get('kind') != 'actual-frozen-standalone-build-input-binding'
            or qualification.get('nonce') != nonce
            or seed.get('schema') != 'uwvm-windows-raii-r5-serial-cidata-prepared-v1'
            or seed.get('nonce') != nonce or seed.get('qualification_sha256') != qualification_sha
            or seed.get('actual_guest_executed') is not False
            or seed.get('whole_product_or_ROS_bundled_qualified') is not False
            or seed.get('actual_base_input_record_sha256') != manifest.get('actual_base_input_record_sha256')
            or [row.get('label') for row in qualification.get('cases', [])] != list(LABELS)
            or [row.get('label') for row in seed.get('cases', [])] != list(LABELS)):
        raise RuntimeError('actual prepared nonce/four exact standalone artifacts provenance mismatch')
    for name in ('actual_four_pe_builds_accepted', 'actual_bridge_assembly_review_accepted',
                 'actual_four_launcher_builds_accepted', 'actual_four_launcher_wmain_assembly_review_accepted',
                 'guest_OS_file_inventory_required', 'actual_SDK_static_LLVM_runtime_provider_bound',
                 'actual_GNU_unwind_imports_rejected', 'standalone_component_only'):
        if qualification.get(name) is not True:
            raise RuntimeError('actual standalone source/build prerequisite not admitted: ' + name)
    for name in ('product_three_tu_abi_qualified', 'named_module_qualified',
                 'new_readonly_sync_provider_qualified', 'actual_component_loaded_DLL_maps_qualified',
                 'ROS_vendored_LLVM23_full_product_qualified'):
        if qualification.get(name) is not False:
            raise RuntimeError('standalone component cannot grant product/module/DLL-map qualification')
    for row in qualification['cases']:
        label = row['label']
        expected_paths = (
            B / 'qemu-platform-tests/windows-raii-cold-build-20261003-r4-llvm-static' / label / 'native-step.exe',
            B / 'qemu-platform-tests/windows-raii-launcher-cold-20261003-r2' / label / 'regular-launcher.exe')
        for path, field, size in zip(expected_paths, ('pe_sha256', 'launcher_pe_sha256'),
                                    ('pe_bytes', 'launcher_pe_bytes')):
            input_row = immutable.get(str(path))
            if (not isinstance(input_row, dict) or input_row.get('sha256') != row.get(field)
                    or input_row.get('bytes') != row.get(size)):
                raise RuntimeError('exact cold fixture/launcher input missing from immutable closure')
        if (row.get('actual_host_fixture_path') != str(expected_paths[0])
                or row.get('actual_host_launcher_path') != str(expected_paths[1])):
            raise RuntimeError('caller cannot substitute an arbitrary build or target path')
    for payload, actual in zip(seed['cases'], qualification['cases']):
        for name in ('label', 'launcher_label', 'pe_sha256', 'launcher_pe_sha256'):
            if payload.get(name) != actual.get(name):
                raise RuntimeError('actual fixture/launcher differs from read-only seed: ' + name)
    if not re.fullmatch('[0-9a-f]{64}', seed.get('bootstrap_sha256', '')):
        raise RuntimeError('actual frozen bootstrap SHA required')
    keyboard = fixed_keyboard_command(seed['bootstrap_sha256'], qualification_sha, nonce)
    if (keyboard['command_ascii_sha256'] != manifest.get('keyboard_command_ascii_sha256')
            or keyboard['command_ascii_sha256'] != seed.get('keyboard_command_ascii_sha256')):
        raise RuntimeError('exact immutable generated keyboard text mismatch')
    vars_file = RUN / 'windows.vars'
    vars_before = regular(vars_file, private=True)
    if (vars_before != manifest.get('vars_before_identity') or vars_before['bytes'] != 540672
            or digest(vars_file) != immutable[str(ORIGINAL_VARS)]['sha256']
            or digest(vars_file) != manifest.get('vars_before_sha256')):
        raise RuntimeError('new private exact firmware-variable copy required')
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
    record = dict(schema='uwvm-windows-raii-r5-serial-vm-actual-v1', passed=False,
                  actual_guest_executed=False, actual_platform_qualified=False,
                  ROS_LLVM23_product_qualified=False, whole_VM_restore_qualified=False,
                  manifest_sha256=manifest_sha, controller_sha256=digest(__file__),
                  argv=fixed_argv(ipc), completed_case_facts=None, children=[], retirement=[],
                  guest_PowerShell_exit_qualified=False,
                  SerialPort_post_frame_Flush_Dispose_retirement_qualified=False,
                  actual_component_loaded_DLL_maps_qualified=False,
                  fixed_keyboard_command_ascii_sha256=keyboard['command_ascii_sha256'],
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
        vars_now = regular(vars_file, private=True)
        if (any(vars_now[key] != vars_before[key] for key in ('device', 'inode', 'uid', 'gid', 'mode', 'links'))
                or vars_now['bytes'] != vars_before['bytes']):
            raise RuntimeError('owned mutable firmware-variable identity/size changed')
        now = regular(overlay, private=True)
        if (any(now[key] != overlay_before[key] for key in ('device', 'inode', 'uid', 'gid', 'mode', 'links'))
                or not overlay_before['bytes'] <= now['bytes'] <= MAX_OVERLAY):
            raise RuntimeError('owned overlay identity/budget changed')
        if any(admitted_limits().get(key) != before_events.get(key) for key in ('oom', 'oom_kill', 'oom_group_kill')):
            raise RuntimeError('original cgroup OOM event changed')
        for name in ('qemu.log', 'serial.log'):
            path = RUN / name
            if path.exists() and regular(path, private=True)['bytes'] > MAX_SERIAL:
                raise RuntimeError('bounded regular VM output exceeded: ' + name)

    pseudo_snapshot_established = False
    pseudo_snapshot_pin = []
    known_display_identities = set()

    def original_child_live():
        if (child_fd is None or child_identity is None
                or select.select([child_fd], [], [], 0)[0]
                or identity(child.pid) != child_identity):
            raise RuntimeError('original live QEMU PIDFD/birth/UID/argv/cgroup changed')

    def display_surface(row, root):
        # QEMU's exact display memfd is data, never a regular/executable DSO.
        # Require its original process to hold a matching live kernel FD. Name
        # alone or a pathname prefix never grants an exemption.
        if (row['pathname'] != '/memfd:displaysurface (deleted)'
                or row['permissions'] != 'rw-s' or row['offset'] != 0
                or not 0 < row['end'] - row['start'] <= 32 << 20):
            raise RuntimeError('exact bounded nonexecuting display memfd mapping required')
        entries = []
        for entry in (root / 'fd').iterdir():
            entries.append(entry)
            if len(entries) > 4096:
                raise RuntimeError('original QEMU FD census exceeded bound')
        matches = []
        for descriptor in entries:
            if not descriptor.name.isdecimal():
                raise RuntimeError('kernel FD census name malformed')
            try:
                if os.readlink(descriptor) != row['pathname']:
                    continue
                before = descriptor.stat(); after = descriptor.stat()
                fields = lambda item: (item.st_dev, item.st_ino, item.st_mode, item.st_uid,
                                       item.st_nlink, item.st_size)
                if fields(before) != fields(after) or os.readlink(descriptor) != row['pathname']:
                    raise RuntimeError('actual display FD identity changed')
                if ((os.major(before.st_dev), os.minor(before.st_dev)) != (row['device_major'], row['device_minor'])
                        or before.st_ino != row['inode']):
                    continue
                if (not stat.S_ISREG(before.st_mode) or before.st_uid != 1000 or before.st_nlink != 0
                        or not 0 < before.st_size <= 32 << 20
                        or row['end'] - row['start'] > ((before.st_size + 4095) // 4096) * 4096):
                    raise RuntimeError('live original display FD shape/bounds mismatch')
                matches.append(dict(fd=int(descriptor.name), device=before.st_dev,
                    inode=before.st_ino, bytes=before.st_size, uid=before.st_uid, links=0))
            except FileNotFoundError:
                continue
        original_child_live()
        if not matches:
            with (root / 'maps').open('r') as stream:
                current = map_rows(stream.read(MAX_MAPS + 1))
            original_child_live()
            if row in current:
                raise RuntimeError('live exact display mapping lacks matching original kernel FD')
            record['retired_display_snapshot_count'] = record.get('retired_display_snapshot_count', 0) + 1
            return None
        key = (row['device_major'], row['device_minor'], row['inode'])
        if key not in known_display_identities:
            if len(known_display_identities) >= 256:
                raise RuntimeError('total display admission census exceeded bound')
            known_display_identities.add(key)
            record.setdefault('actual_display_data_FD_proofs', []).append(dict(
                row=row, matching_original_fds=matches, regular_or_executable_provider_qualified=False))
        return key, matches[0]['bytes']

    def mapped_inputs(*, paused_prelaunch=False):
        nonlocal pseudo_snapshot_established, pseudo_snapshot_pin
        # The original PIDFD stays owned for the entire snapshot. Recheck the
        # complete original identity before and after bounded proc reads and
        # after all regular-file inode/device/provider validation.
        original_child_live()
        root = Path('/proc') / str(child.pid)
        with (root / 'maps').open('r') as stream:
            raw = stream.read(MAX_MAPS + 1)
        original_child_live()
        rows = map_rows(raw)
        candidates = []
        active_displays = {}
        for row in rows:
            name = row['pathname']
            if not name.startswith('/'):
                continue
            if name == '/memfd:displaysurface (deleted)':
                proof = display_surface(row, root)
                if proof is not None:
                    key, size = proof; active_displays[key] = size
                    if len(active_displays) > 8 or sum(active_displays.values()) > 256 << 20:
                        raise RuntimeError('live display data budget exceeded')
                continue
            if name.endswith(' (deleted)'):
                if not pseudo_anonymous_data(row):
                    raise RuntimeError('deleted regular/executable or unknown QEMU mapping is not admitted')
                candidates.append(row)
                if len(candidates) > 1:
                    raise RuntimeError('at most one pseudo-anonymous nonexec QEMU data page is admitted')
                # An unchanged exact row may persist; no new address, inode,
                # extent, permissions or deleted pathname is admitted later.
                if pseudo_snapshot_established and row not in pseudo_snapshot_pin:
                    raise RuntimeError('new or changed pseudo-anonymous QEMU data mapping')
                continue
            resolved = str(Path(name).resolve(strict=True))
            if resolved in (str(overlay), str(vars_file)):
                current = regular(Path(resolved), private=True)
                if 'x' in row['permissions']:
                    raise RuntimeError('executable mutable overlay mapping')
            elif resolved in immutable:
                current = regular(Path(resolved))
                if current != immutable_identities[resolved]:
                    raise RuntimeError('immutable QEMU mapped input identity changed')
            else:
                raise RuntimeError('unknown QEMU mapped input: ' + resolved)
            device = (row['device_major'], row['device_minor'])
            if device != (os.major(current['device']), os.minor(current['device'])) or row['inode'] != current['inode']:
                raise RuntimeError('actual QEMU mapping inode/device mismatch')
            record.setdefault('actual_mapped_inputs', {})[resolved] = current
        original_child_live()
        if not pseudo_snapshot_established:
            if (not paused_prelaunch or record.get('actual_guest_executed') is not False
                    or record.get('actual_kvm') != {'enabled': True, 'present': True}
                    or os.sysconf('SC_PAGE_SIZE') != 4096):
                raise RuntimeError('pseudo-anonymous data pin requires original paused KVM/4096-byte page')
            pseudo_snapshot_pin = candidates
            pseudo_snapshot_established = True
            record['pseudo_anonymous_nonexecuting_data'] = dict(
                scope='data-only complete kernel-maps observation; not regular provider identity',
                original_child_identity=child_identity,
                first_admission_paused_prelaunch=True,
                original_pidfd_live_before_and_after=True,
                rows=pseudo_snapshot_pin, maximum_rows=1, maximum_bytes=4096,
                regular_provider_qualified=False, executable_provider_qualified=False,
                guest_RAM_owner_proved=False, memfd_proved=False)

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
        live_inputs(); mapped_inputs(paused_prelaunch=True)
        qmp.request('cont'); record['actual_guest_executed'] = True; write()
        deadline = started + manifest['deadline_seconds']

        def checked_tick(delay):
            if time.monotonic() + delay > deadline:
                raise RuntimeError('fixed Windows overall deadline exceeded')
            if select.select([child_fd], [], [], delay)[0]:
                raise RuntimeError('original Windows QEMU exited before completed cases')
            live_inputs()
            current_socket = (ipc / 'qmp.sock').lstat()
            if not stat.S_ISSOCK(current_socket.st_mode) or (current_socket.st_dev, current_socket.st_ino, current_socket.st_uid) != socket_identity:
                raise RuntimeError('owned QMP socket inode/device changed')
            original_child_live()

        # This one fixed sequence grants no desktop/login assumption. Absence
        # of the exact nonce response is an actual timeout/failure. It neither
        # downloads nor interpolates any guest-origin command.
        for _ in range(45):
            checked_tick(1); mapped_inputs()
        qmp.request('send-key', {'keys': [{'type': 'qcode', 'data': 'meta_l'},
                                        {'type': 'qcode', 'data': 'r'}], 'hold-time': 40})
        checked_tick(2); mapped_inputs()
        for index, character in enumerate(keyboard['command']):
            qmp.request('send-key', {'keys': [{'type': 'qcode', 'data': code}
                                             for code in keyboard_codes(character)], 'hold-time': 40})
            checked_tick(0.07)
            if index % 16 == 0:
                mapped_inputs()
        qmp.request('send-key', {'keys': [{'type': 'qcode', 'data': 'ret'}], 'hold-time': 40})
        record['fixed_keyboard_transmission_completed'] = True; write()
        while True:
            checked_tick(0.25); mapped_inputs()
            serial_file = RUN / 'serial.log'
            if serial_file.exists() and original_serial_has_complete_frame(serial_file, nonce):
                break
        # Quiesce only this original VM before parsing stable serial bytes.
        # A completed case frame cannot prove the guest script's later exit,
        # SerialPort Flush/Dispose or DLL mappings. Keep those gates false.
        qmp.request('stop')
        paused = qmp.request('query-status')
        if paused.get('running') is not False or paused.get('status') != 'paused':
            raise RuntimeError('original VM failed to pause after completed case frame')
        checked_tick(0.1); mapped_inputs()
        record['completed_case_facts'] = parse_original_serial(
            RUN / 'serial.log', qualification, qualification_sha)
        record['original_VM_paused_after_reported_completed_cases'] = True
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
            record['private_vars_after_sha256'] = digest(vars_file)
        except BaseException as error:
            record['after_input_error'] = type(error).__name__ + ': ' + str(error)
            record['passed'] = False
        record['wall_seconds'] = time.monotonic() - started
        write()
    # The original VM is intentionally retired by signal after a valid case
    # frame. Preserve its actual raw returncode; it is not guest/script exit0.
    # The outer original guard and lane-retirement receipt are additional gates.
    # Reported completed cases are not published as whole platform/ROS/product/script-exit acceptance.
    return 0 if record['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
