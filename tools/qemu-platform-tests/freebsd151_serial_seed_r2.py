#!/usr/bin/env python3
"""Prepare fixed FreeBSD CIDATA serial transport from actual bound ELF inputs.

This source is not a VM launcher. A separate owned QEMU/controller/ISO guardian
must first qualify its exact providers, nonce and disposable clean overlay.
Run no compiler, qemu-img, ISO tool, guest or shell here.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import struct

LABELS = ('uwvm2-ros-eh', 'uwvm2-ros-noeh', 'uwvm2-eh', 'uwvm2-noeh')
MAX_ELF_BYTES = 2 << 20
MAX_PAYLOAD_BYTES = 8 << 20
MAX_METADATA_BYTES = 32 << 10


def sha(data):
    return hashlib.sha256(data).hexdigest()


def owned_regular(path):
    record = path.lstat()
    if not stat.S_ISREG(record.st_mode) or record.st_uid != os.getuid() or record.st_nlink != 1:
        raise RuntimeError('private single-link regular payload required')
    return (record.st_dev, record.st_ino, record.st_size, record.st_mtime_ns, record.st_ctime_ns)


def new_file(path, data, mode=0o600):
    with path.open('xb') as stream:
        stream.write(data)
    path.chmod(mode)


def make_root_script(nonce, payloads, runtime):
    # The only substituted values are fixed labels and 64 lower-case hex bytes.
    # No user path, command line or source text can enter this guest program.
    rows = ['#!/bin/sh', 'set -eu', 'exec >/dev/ttyu0 2>&1',
            'umask 077',
            'mkdir -m 0700 /tmp/uwvmqemu-seed',
            'mount -t cd9660 -o ro /dev/iso9660/CIDATA /tmp/uwvmqemu-seed',
            'mkdir -m 0700 /tmp/uwvmqemu-work', 'chown uwvmqemu:uwvmqemu /tmp/uwvmqemu-work']
    for entry in runtime:
        guest, digest, size = entry['guest_path'], entry['sha256'], entry['bytes']
        rows += ['test -f ' + guest,
                 'test "$(stat -L -f %z ' + guest + ')" -eq ' + str(size),
                 'test "$(sha256 -q ' + guest + ')" = ' + digest]
    # This nonce begins only after actual guest loader/library byte checks.
    rows += ['echo UWVM_BSD_BEGIN_' + nonce]
    for row in payloads:
        label, digest = row['label'], row['sha256']
        rows += ['test "$(sha256 -q /tmp/uwvmqemu-seed/bin/' + label + '.elf)" = ' + digest,
                 'cp -p /tmp/uwvmqemu-seed/bin/' + label + '.elf /tmp/uwvmqemu-work/' + label + '.elf',
                 'chmod 0500 /tmp/uwvmqemu-work/' + label + '.elf',
                 'chown uwvmqemu:uwvmqemu /tmp/uwvmqemu-work/' + label + '.elf']
    rows += ['umount /tmp/uwvmqemu-seed', 'set +e',
             "su -m uwvmqemu -c '/bin/sh /tmp/uwvmqemu-run.sh'", 'status=$?', 'set -e',
             'echo UWVM_BSD_END_' + nonce + '_STATUS_$status',
             'shutdown -p now', 'exit "$status"']
    return '\n'.join(rows) + '\n'


def make_user_script(nonce, payloads):
    rows = ['#!/bin/sh', 'set -eu', 'umask 077', 'test "$(id -u)" != 0', 'test "$(id -u -r)" != 0']
    for row in payloads:
        label = row['label']
        rows += ['mkdir -m 0700 /tmp/uwvmqemu-work/' + label,
                 'cd /tmp/uwvmqemu-work/' + label,
                 'echo UWVM_BSD_CELL_BEGIN_' + nonce + '_' + label, 'set +e',
                 '/tmp/uwvmqemu-work/' + label + '.elf', 'status=$?', 'set -e',
                 'echo UWVM_BSD_CELL_END_' + nonce + '_' + label + '_STATUS_$status',
                 'test "$status" = 0']
    return '\n'.join(rows) + '\n'



def required_guest_runtime(spec, variants):
    # Values can enter the fixed shell text only after exact path/digest/size
    # checks. No executable, action, source text or arbitrary path is accepted.
    supplied = spec.get('required_guest_runtime', [])
    if not isinstance(supplied, list) or not 1 <= len(supplied) <= 33:
        raise RuntimeError('bounded actual guest runtime list required')
    def checked(row):
        if (not isinstance(row, dict) or set(row) != {'guest_path', 'sha256', 'bytes'}
                or not isinstance(row['guest_path'], str)
                or not re.fullmatch(r'/(?:lib(?:exec)?|usr/lib)/[A-Za-z0-9_+.-]+', row['guest_path'])
                or not isinstance(row['sha256'], str)
                or not re.fullmatch('[0-9a-f]{64}', row['sha256'])
                or type(row['bytes']) is not int or not 1 <= row['bytes'] <= 32 << 20):
            raise RuntimeError('unknown actual guest runtime path/digest/size')
        return row
    by_path = {}
    for row in supplied:
        row = checked(row)
        if row['guest_path'] in by_path:
            raise RuntimeError('duplicate actual guest runtime provider')
        by_path[row['guest_path']] = row
    if [row['guest_path'] for row in supplied] != sorted(by_path):
        raise RuntimeError('actual common guest runtime ordering changed')
    actual = (json.dumps(supplied, indent=2, sort_keys=True) + '\n').encode()
    if sha(actual) != spec.get('required_guest_runtime_sha256'):
        raise RuntimeError('actual common guest runtime binding changed')
    union = set()
    for variant in variants:
        entries = variant.get('target_runtime', [])
        if not isinstance(entries, list) or not 1 <= len(entries) <= 33:
            raise RuntimeError('every fresh profile needs actual runtime bindings')
        paths = []
        for row in entries:
            row = checked(row); name = row['guest_path']
            if name not in by_path or row != by_path[name]:
                raise RuntimeError('profile/common target runtime mismatch')
            paths.append(name); union.add(name)
        if len(paths) != len(set(paths)) or paths.count('/libexec/ld-elf.so.1') != 1:
            raise RuntimeError('each profile must bind one exact guest loader')
    if union != set(by_path):
        raise RuntimeError('extra common runtime provider is not used by any actual profile')
    return supplied


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qualified-inputs', type=Path, required=True)
    parser.add_argument('--nonce', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not re.fullmatch('[0-9a-f]{64}', args.nonce):
        raise RuntimeError('one fresh fixed 256-bit run nonce required')
    input_identity = owned_regular(args.qualified_inputs)
    raw = args.qualified_inputs.read_bytes()
    if not 1 <= len(raw) <= 1 << 20:
        raise RuntimeError('qualified-input record exceeds its fixed bound')
    spec = json.loads(raw)
    if (spec.get('schema') != 'uwvm-freebsd151-sdk-oracle-cross-build-actual-v1'
            or spec.get('target') != 'x86_64-unknown-freebsd15.1'
            or spec.get('cold_object_and_provider_bindings_verified') is not True
            or spec.get('actual_guest_executed') is not False
            or spec.get('ROS_LLVM23_product_qualified') is not False):
        raise RuntimeError('fresh standalone FreeBSD cross-build scope required')
    source_sha, sdk_sha, receipt_sha = (spec.get(name, '') for name in
        ('source_manifest_sha256', 'SDK_actual_manifest_sha256', 'original_guard_receipts_sha256'))
    if not all(re.fullmatch('[0-9a-f]{64}', value) for value in (source_sha, sdk_sha, receipt_sha)):
        raise RuntimeError('actual source/SDK/original-guard provenance is incomplete')
    source_rows = spec.get('variants', [])
    if [row.get('label') for row in source_rows] != list(LABELS):
        raise RuntimeError('all four exact fresh source variants are required')
    runtime = required_guest_runtime(spec, source_rows)
    payloads, blobs, identities = [], {}, {}
    total = 0
    for row in source_rows:
        path = Path(row['binary'])
        identity = owned_regular(path)
        if not 64 <= identity[2] <= MAX_ELF_BYTES:
            raise RuntimeError('cold oracle ELF exceeds its finite ISO payload budget')
        data = path.read_bytes()
        if len(data) != identity[2] or sha(data) != row.get('binary_sha256'):
            raise RuntimeError('actual bound FreeBSD ELF changed')
        if (data[:4] != b'\x7fELF' or data[4:7] != b'\x02\x01\x01'
                or struct.unpack_from('<H', data, 18)[0] != 62
                or struct.unpack_from('<H', data, 16)[0] not in (2, 3)):
            raise RuntimeError('actual oracle is not a Windows-free x64 ELF64 LSB image')
        total += len(data)
        if total > MAX_PAYLOAD_BYTES:
            raise RuntimeError('all four oracle ELF bytes exceed the finite ISO budget')
        label = row['label']; blobs[label] = data; identities[path] = identity
        payloads.append({'label': label, 'sha256': sha(data), 'bytes': len(data), 'source_binary': str(path)})
    if owned_regular(args.qualified_inputs) != input_identity:
        raise RuntimeError('actual cross-build record changed during seed preparation')
    for path, identity in identities.items():
        if owned_regular(path) != identity:
            raise RuntimeError('actual qualified binary changed while staging')
    # The official FreeBSD nuageinit decodes b64 using a large bit-string. Copy
    # binaries as independent ISO files; its write_files input stays bounded
    # plain text, and the later fixed root script mounts this seed read-only.
    root_script = make_root_script(args.nonce, payloads, runtime)
    user_script = make_user_script(args.nonce, payloads)
    config = {'users': [{'name': 'uwvmqemu', 'shell': '/bin/sh', 'locked': False,
                         'gecos': 'Owned isolated QEMU correctness probe'}],
              'hostname': 'uwvm-freebsd-cold', 'ssh_pwauth': False,
              'package_update': False, 'package_upgrade': False,
              'write_files': [
                  {'path': '/etc/rc.conf.d/firstboot_pkg_upgrade', 'owner': 'root:wheel',
                   'permissions': '0600', 'content': 'firstboot_pkg_upgrade_enable="NO"\n'},
                  {'path': '/etc/rc.conf.d/sshd', 'owner': 'root:wheel',
                   'permissions': '0600', 'content': 'sshd_enable="NO"\n'},
                  {'path': '/tmp/uwvmqemu-root.sh', 'owner': 'root:wheel',
                   'permissions': '0700', 'content': root_script},
                  {'path': '/tmp/uwvmqemu-run.sh', 'owner': 'root:wheel',
                   'permissions': '0555', 'content': user_script}],
              'runcmd': ['/bin/sh /tmp/uwvmqemu-root.sh']}
    userdata = ('#cloud-config\n' + json.dumps(config, indent=2) + '\n').encode()
    metadata = (json.dumps({'instance-id': 'uwvm-freebsd-' + args.nonce,
                           'local-hostname': 'uwvm-freebsd-cold'}, indent=2) + '\n').encode()
    if len(userdata) + len(metadata) > MAX_METADATA_BYTES:
        raise RuntimeError('plain nuageinit metadata exceeds its finite text budget')
    output = args.output.absolute(); output.mkdir(mode=0o700, exist_ok=False)
    (output / 'bin').mkdir(mode=0o700)
    new_file(output / 'user-data', userdata); new_file(output / 'meta-data', metadata)
    for label, data in blobs.items():
        new_file(output / 'bin' / (label + '.elf'), data, 0o500)
    manifest = {'schema': 'uwvm-freebsd151-serial-cidata-prepared-v1', 'nonce': args.nonce,
        'actual_source_inputs_sha256': sha(raw), 'source_manifest_sha256': source_sha,
        'SDK_actual_manifest_sha256': sdk_sha, 'original_guard_receipts_sha256': receipt_sha,
        'required_guest_runtime': runtime,
        'required_guest_runtime_sha256': spec['required_guest_runtime_sha256'],
        'user_data_sha256': sha(userdata), 'meta_data_sha256': sha(metadata), 'payloads': payloads,
        'guest_nic_required': False, 'guest_ssh_or_auth_secret_required': False,
        'original_customized_guest_used': False, 'ISO_tool_executed': False,
        'actual_guest_executed': False, 'actual_platform_qualified': False,
        'ROS_LLVM23_product_qualified': False}
    new_file(output / 'serial-seed-prepared.json', (json.dumps(manifest, indent=2, sort_keys=True) + '\n').encode())
    print(json.dumps({'payload_bytes': total, 'nuage_text_bytes': len(userdata) + len(metadata),
                      'actual_guest_executed': False, 'ISO_tool_executed': False}))


if __name__ == '__main__':
    main()
