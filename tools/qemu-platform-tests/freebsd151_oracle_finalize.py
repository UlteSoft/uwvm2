#!/usr/bin/env python3
"""Bind completed original FreeBSD cold receipts for later seed preparation.

This reads bounded metadata and existing artifacts only. It executes no tool,
guest or command and confers no guest/product/native-debugger qualification.
The original guardian, not this metadata consumer, owns actual task retirement.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import stat


BASE = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
ROOT = BASE / 'qemu-platform-tests/freebsd-sdk-oracle-cold-20261003-r2-planfix'
PLAN = ROOT / 'build-r2-planfix-final/plan.json'
PLAN_SHA = 'f53de7a66ad71ab2f90b1269db0a49d18b0624bc419116210be1f32ddb69c094'
COMMAND_SHA = 'ae003fcfba8e540d8aaa1dc70a74e28c25868dbfbd1dfd20d30c4ad3c4d33c7b'
GUARD_SHA = '22b67df036ceebc0bfdc11cf1a056c8d1360425f60146296d9f8c86515f223ad'
EVIDENCE = BASE / 'evidence/qemu-freebsd-sdk-oracle-cold-r2-planfix-attempt1'
SDK = BASE / 'qemu-platform-tests/freebsd-sdk-actual-20261003-r3/sysroot'
LABELS = ('uwvm2-ros-eh', 'uwvm2-ros-noeh', 'uwvm2-eh', 'uwvm2-noeh')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read(path, limit, *, allow_empty=False):
    before = path.lstat()
    if (not stat.S_ISREG(before.st_mode) or before.st_uid != 1000
            or not (0 if allow_empty else 1) <= before.st_size <= limit):
        raise RuntimeError('owned bounded regular metadata required: ' + str(path))
    data = path.read_bytes()
    after = path.lstat()
    fields = ('st_dev', 'st_ino', 'st_mode', 'st_uid', 'st_nlink', 'st_size',
              'st_mtime_ns', 'st_ctime_ns')
    if len(data) != before.st_size or any(getattr(before, key) != getattr(after, key) for key in fields):
        raise RuntimeError('metadata changed while reading: ' + str(path))
    return data


def verify_file(row, *, max_bytes=32 << 20):
    path = Path(row['path'])
    before = path.stat()
    if (not stat.S_ISREG(before.st_mode) or not 1 <= before.st_size <= max_bytes
            or any(getattr(before, field) != row[key] for field, key in
                   (('st_size', 'bytes'), ('st_dev', 'device'), ('st_ino', 'inode'), ('st_uid', 'uid')))
            or stat.S_IMODE(before.st_mode) != row['mode']
            or str(path.resolve(strict=True)) != row['resolved']):
        raise RuntimeError('bound artifact/provider identity changed: ' + str(path))
    data = path.read_bytes()
    after = path.stat()
    fields = ('st_dev', 'st_ino', 'st_mode', 'st_uid', 'st_nlink', 'st_size',
              'st_mtime_ns', 'st_ctime_ns')
    if (len(data) != row['bytes'] or digest(data) != row['sha256']
            or any(getattr(before, key) != getattr(after, key) for key in fields)):
        raise RuntimeError('bound artifact/provider bytes changed: ' + str(path))
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    plan_raw = read(PLAN, 64 << 10)
    command_raw = read(PLAN.parent / 'commands.json', 64 << 10)
    guard_raw = read(ROOT / 'final-proposal-r2-planfix/supervisor.py', 64 << 10)
    if (digest(plan_raw) != PLAN_SHA or digest(command_raw) != COMMAND_SHA
            or digest(guard_raw) != GUARD_SHA):
        raise RuntimeError('reviewed exact cold plan/commands/guardian changed')
    plan, commands = json.loads(plan_raw), json.loads(command_raw)
    if (len(commands) != 27 or plan['target'] != 'x86_64-unknown-freebsd15.1'
            or Path(plan['output']) != PLAN.parent or Path(plan['evidence']) != EVIDENCE
            or [row['label'] for row in plan['variants']] != list(LABELS)):
        raise RuntimeError('wrong fixed standalone cold suite')
    receipts_raw = read(EVIDENCE / 'receipts.json', 2 << 20)
    retirement_raw = read(EVIDENCE / 'lane-retirement.json', 64 << 10)
    receipts, retirement = json.loads(receipts_raw), json.loads(retirement_raw)
    if (len(receipts) != 27
            or retirement.get('schema') != 'uwvm-original-supervisor-lane-retirement-v1'
            or retirement.get('original_receipts_sha256') != digest(receipts_raw)
            or retirement.get('commands_sha256') != COMMAND_SHA
            or retirement.get('supervisor_sha256') != GUARD_SHA
            or retirement.get('supervisor_complete') is not True
            or retirement.get('owned_tasks_retired') is not True
            or retirement.get('executed_commands') != 27
            or retirement.get('planned_commands') != 27
            or retirement.get('work_passed') is not True):
        raise RuntimeError('all original cold stages and retirement must actually succeed')
    for receipt, (label, argv) in zip(receipts, commands, strict=True):
        root_retired = receipt.get('Popen_root_pidfd_retirement', {})
        if (receipt['label'] != label or receipt['argv'] != argv
                or receipt['command_file_sha256'] != COMMAND_SHA
                or receipt['supervisor_sha256'] != GUARD_SHA
                or receipt.get('passed') is not True or receipt.get('returncode') != 0
                # The exact pinned guardian emits cancelled only in its
                # exception branch. A normal successful receipt omits it.
                or 'cancelled' in receipt or receipt.get('remaining_roster') != [10154]
                or receipt.get('memory_max_bytes') != 64 << 30 or receipt.get('swap_max_bytes') != 0
                or root_retired.get('pidfd_readable') is not True
                or root_retired.get('actual_reaped_returncode') != 0
                or not receipt.get('retirement')
                or any(row.get('pidfd_readable') is not True for row in receipt['retirement'])
                or receipt['memory_events_after'] != receipt['memory_events_before']):
            raise RuntimeError('actual original stage authority/retirement failed: ' + label)
        log_raw = read(EVIDENCE / (label + '.log'), 32 << 20, allow_empty=True)
        if digest(log_raw) != receipt['log_sha256']:
            raise RuntimeError('actual original raw stage log changed: ' + label)
    before_raw = read(PLAN.parent / 'input-before.json', 16 << 20)
    after_raw = read(PLAN.parent / 'input-after.json', 64 << 10)
    before, after = json.loads(before_raw), json.loads(after_raw)
    if (before['plan_sha256'] != PLAN_SHA or after['input_before_sha256'] != digest(before_raw)
            or len(before.get('directory_aliases', [])) != 1
            or after['actual_guest_qualified'] is not False
            or after['ROS_paired_LLVM23_product_qualified'] is not False):
        raise RuntimeError('actual before/after closure binding is incomplete')
    rows, common_runtime = [], {}
    for variant in plan['variants']:
        label = variant['label']
        object_raw = read(PLAN.parent / (label + '-object.json'), 2 << 20)
        binary_raw = read(PLAN.parent / (label + '-binary.json'), 2 << 20)
        obj, binary = json.loads(object_raw), json.loads(binary_raw)
        if (obj['ELF_class'] != 64 or obj['ELF_data'] != 'little' or obj['ELF_machine'] != 62
                or obj['ELF_type'] != 1 or binary['ELF_class'] != 64
                or binary['ELF_data'] != 'little' or binary['ELF_machine'] != 62
                or binary['ELF_OSABI'] != 9 or binary['interpreters'] != ['/libexec/ld-elf.so.1']
                or binary['actual_guest_qualified'] is not False
                or binary['ROS_paired_LLVM23_product_qualified'] is not False
                or binary['binary']['path'] != variant['binary']
                or obj['object']['path'] != variant['object']):
            raise RuntimeError('actual independent object/target/runtime binding mismatch: ' + label)
        verify_file(obj['object']); verify_file(obj['depfile'])
        verify_file(binary['binary'], max_bytes=2 << 20)
        verify_file(binary['link_log']); verify_file(binary['link_map'])
        runtime = []
        for entry in binary['actual_target_runtime_closure']:
            path = verify_file(entry)
            relative = path.relative_to(SDK)
            guest = '/' + relative.as_posix()
            if (relative.parts[0] not in ('lib', 'libexec', 'usr')
                    or (relative.parts[0] == 'usr' and relative.parts[1] != 'lib')
                    or not re.fullmatch(r'/(?:lib(?:exec)?|usr/lib)/[A-Za-z0-9_+.-]+', guest)):
                raise RuntimeError('unknown actual fixed guest runtime path')
            row = {'guest_path': guest, 'sha256': entry['sha256'], 'bytes': entry['bytes']}
            if guest in common_runtime and common_runtime[guest] != row:
                raise RuntimeError('two fresh profiles require different bytes for one guest provider')
            common_runtime[guest] = row; runtime.append(row)
        if (not 1 <= len(runtime) <= 33
                or sum(row['guest_path'] == '/libexec/ld-elf.so.1' for row in runtime) != 1):
            raise RuntimeError('each actual fresh target needs its exact guest loader closure')
        rows.append({'label': label, 'binary': variant['binary'],
                     'binary_sha256': binary['binary']['sha256'], 'binary_bytes': binary['binary']['bytes'],
                     'object_binding_sha256': digest(object_raw), 'binary_binding_sha256': digest(binary_raw),
                     'target_runtime': runtime})
    providers = [common_runtime[name] for name in sorted(common_runtime)]
    runtime_raw = (json.dumps(providers, indent=2, sort_keys=True) + '\n').encode()
    result = {'schema': 'uwvm-freebsd151-sdk-oracle-cross-build-actual-v1',
              'target': plan['target'], 'cold_object_and_provider_bindings_verified': True,
              'source_manifest_sha256': plan['source_manifest_sha256'],
              'SDK_actual_manifest_sha256': plan['SDK_provider_record_sha256'],
              'original_guard_receipts_sha256': digest(receipts_raw),
              'original_guard_retirement_sha256': digest(retirement_raw),
              'actual_plan_sha256': PLAN_SHA, 'actual_commands_sha256': COMMAND_SHA,
              'actual_before_binding_sha256': digest(before_raw), 'actual_after_binding_sha256': digest(after_raw),
              'variants': rows, 'required_guest_runtime': providers,
              'required_guest_runtime_sha256': digest(runtime_raw),
              'actual_guest_executed': False, 'actual_platform_qualified': False,
              'ROS_LLVM23_product_qualified': False, 'native_debugger_qualified': False,
              'whole_VM_restore_qualified': False,
              'scope': 'Completed standalone SDK header cross-build evidence only; no live lane authority.'}
    data = (json.dumps(result, indent=2, sort_keys=True) + '\n').encode()
    if len(data) > 1 << 20:
        raise RuntimeError('bounded prepared guest-input record exceeded')
    with args.output.open('xb') as stream:
        stream.write(data)
    print(json.dumps({'original_stages_verified': 27, 'guest_runtime_rows': len(providers),
                      'qualified_inputs_sha256': digest(data), 'actual_guest_executed': False}))


if __name__ == '__main__':
    main()
