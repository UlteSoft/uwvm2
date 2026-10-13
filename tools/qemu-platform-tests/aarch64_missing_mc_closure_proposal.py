#!/usr/bin/env python3
"""SOURCE PROPOSAL: bind the finite13 AArch64 GNU LLVM supplement.

This metadata helper never runs a compiler, subprocess, archive tool or VM.
The original reviewed PIDFD guardian must execute the 13 frozen native argv.
Successful metadata/ET_REL checks do not qualify any product, guest or ABI.
"""
from pathlib import Path, PurePosixPath
import ast
import hashlib
import json
import os
import re
import shlex
import stat
import struct
import sys

B = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
T = Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm')
SDK = B.parent / 'deps'
G = B / 'llvm-aarch64-b23-ros9/build'
SOURCE = B / 'candidates/core3-b23efab6-ros-linux-test'
D = B / 'qemu-platform-tests/aarch64-GNU-missing-MC-source-20261003-r1'
OUT = B / 'qemu-platform-tests/aarch64-GNU-missing-MC-components-20261003-r1'
EVIDENCE = B / 'evidence/qemu-aarch64-GNU-missing-MC-components-20261003-r1'
PLAN = D / 'recipe.json'
PLAN_PIN = {'bytes': 108090, 'sha256': '2f1b008d4cb595af3e27de573395261c13940ba45eabcbdd08d3c42cd2bcce46'}
MANIFEST_PIN = {'bytes': 2764169, 'sha256': '43bb4796ee6ff84b94bbd736fb5ca3bdaaf1fa0d67813dc5affe4aa128aaa717'}
OLD_SOURCE_ID = 'sha256:b23efab67c1a95dc28d598274e885c1616b152f22fce6b651064d10df8b41b12'
HELPER = B / 'builds/current-r10-qualified-header-probe-20261003-r1/closure-r2.py'
HELPER_PIN = {'bytes': 13527, 'sha256': '97e24bc58a0c5e07d7c18d18eb22decd5e82675272ed4394d4199c55d56b289a'}
BOOT = '83f582ec-ff7f-41ce-9631-2c0fbb865aeb'
CG = Path('/sys/fs/cgroup/system.slice/docker-bec3a6e4e013ba07c1583e46eeb81707c90c59375ae1c208a6bb87cc4f773f45.scope')
EXPECTED_TABLES = {
    'AArch64GenDisassemblerTables.inc': {'bytes': 4027517, 'sha256': '1ce7d1f79fe6871e0282ae90051c8e28e6ee93c4eed8cfd3a2ede9c80c232bcc'},
    'AArch64GenAsmMatcher.inc': {'bytes': 7655891, 'sha256': 'a5ca3b77f9afc3c5761d6ddde3289a32a58c7c4d9f611502cd4270252552b77b'},
}
GROUPS = {
    'libLLVMMCDisassembler.a': ['Disassembler', 'MCDisassembler', 'MCExternalSymbolizer', 'MCRelocationInfo', 'MCSymbolizer'],
    'libLLVMAArch64Disassembler.a': ['AArch64Disassembler', 'AArch64ExternalSymbolizer'],
    'libLLVMAArch64AsmParser.a': ['AArch64AsmParser'],
}
SDK_BUILD_PINS = {
    'CMakeCache.txt': {'bytes': 73501, 'sha256': '89ad31e57ade37cc71dbd4dcc8a519f5cbbeb7ea06e746bf09e61a73c0eec3dd'},
    'build.ninja': {'bytes': 8344052, 'sha256': '94d365d4776cd6ea29491cc9e4228da4dcb9bbb6d6a66d6949329bd3b33aca45'},
    'CMakeFiles/rules.ninja': {'bytes': 171924, 'sha256': '5b419bb29a5f57f5f4f86ab917c0fdee479cc2c7a7f2211faf5be3e7863bd284'},
    'compile_commands.json': {'bytes': 5254165, 'sha256': 'a2c74b6b4b516ed8e481adbd774524c16cd3c9053fc6940d53d38e768cd9c318'},
}
TOOL_PINS = {
    '/usr/bin/env': {'bytes': 10832184, 'sha256': '2a9b9ccf4e9724a6d6d8c97c835c9932223fbdefc9706fef9d0dfef7a9076739'},
    '/usr/bin/python3': {'bytes': 7481192, 'sha256': 'b8d8288faefdd300201f43fcf00f6f539a27218eeed3a3dff5ab10b9c4c99700'},
    str(T / 'bin/clang++'): {'bytes': 114912, 'sha256': '0e800e4decf0de00b5ebeefed814b2cf6551d17291a220ee107b5198cd7ca23b'},
    str(T / 'bin/llvm-ar'): {'bytes': 70336, 'sha256': '4b000995495f154aba90d4aed08127e48b64319841c0cec58e14a0f5003f0bd3'},
}


def pin(path):
    path = Path(path)
    with path.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    return {'bytes': path.stat().st_size, 'sha256': digest}


def load(path):
    return json.loads(Path(path).read_bytes())


def save_new(path, value):
    with Path(path).open('x') as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write('\n')


def record(path):
    path = Path(path)
    resolved = path.resolve(strict=True)
    info = resolved.stat()
    assert stat.S_ISREG(info.st_mode), str(path)
    # These identities are observed within this new run and compared with its
    # own later snapshot; old static inventory inodes are never authority.
    return {'resolved': str(resolved), 'device': info.st_dev, 'inode': info.st_ino,
            **pin(resolved)}


def add(records, path):
    path = Path(path)
    value = record(path)
    key = str(path)
    assert key not in records or records[key] == value
    records[key] = value


def plan():
    assert pin(PLAN) == PLAN_PIN
    value = load(PLAN)
    assert value['source_id'] == OLD_SOURCE_ID and value['source_manifest_sha256'] == MANIFEST_PIN['sha256']
    assert value['output_root'] == str(OUT)
    commands = value['fixed_literal13_native_commands']
    assert len(commands) == 13 and len({row[0] for row in commands}) == 13
    assert load(D / 'commands.json') == commands
    assert value['resources']['kernel_memory_max_bytes'] == 64 << 30
    assert value['resources']['owned_aggregate_RSS_soft_bytes'] == 8 << 30
    assert value['resources']['swap_max_bytes'] == 0
    return value


def pure_elf_helpers():
    assert pin(HELPER) == HELPER_PIN
    tree = ast.parse(HELPER.read_text())
    selected = [node for node in tree.body if isinstance(node, (ast.Import, ast.ImportFrom)) or
                isinstance(node, ast.FunctionDef) and node.name in ('elf', 'candidates')]
    namespace = {'__name__': 'source_metadata_only', '__file__': str(HELPER)}
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(HELPER), 'exec'), namespace)
    return namespace


def source_and_provider_snapshot():
    spec = plan()
    records = {}
    for name in ('source-manifest.json', 'source-manifest-after.json'):
        path = G.parent / name
        assert pin(path) == MANIFEST_PIN
        add(records, path)
    manifest = load(G.parent / 'source-manifest.json')
    assert manifest['source_id'] == OLD_SOURCE_ID and len(manifest['files']) == 15570
    count = 0
    for item in manifest['files']:
        relative = PurePosixPath(item['path'])
        assert not relative.is_absolute() and '..' not in relative.parts
        if not str(relative).startswith('third-parties/llvm/llvm/'):
            continue
        path = SOURCE / str(relative)
        assert not path.is_symlink() and pin(path)['sha256'] == item['sha256'], str(path)
        add(records, path)
        count += 1
    assert count >= 1000
    for name, expected in SDK_BUILD_PINS.items():
        assert pin(G / name) == expected, name
    for path in (G / 'CMakeCache.txt', G / 'build.ninja', G / 'CMakeFiles/rules.ninja',
                 G / 'compile_commands.json', PLAN, D / 'commands.json', D / 'supervisor.py',
                 D / 'lane_launch.py', D / 'closure.py', HELPER):
        add(records, path)
    for root in (G / 'include', G / 'lib/Target/AArch64', G / 'lib/MC/MCDisassembler'):
        assert root.is_dir(), str(root)
        for path in sorted(root.rglob('*')):
            if path.is_file() and path.suffix in ('.h', '.inc', '.def', '.td'):
                add(records, path)
    for name, expected in EXPECTED_TABLES.items():
        path = G / 'lib/Target/AArch64' / name
        assert pin(path) == expected
        add(records, path)
    header_roots = (SDK / 'usr/aarch64-linux-gnu/include',
                    SDK / 'usr/lib/gcc-cross/aarch64-linux-gnu/15', T / 'lib/clang')
    for root in header_roots:
        assert root.is_dir(), str(root)
        for path in sorted(root.rglob('*')):
            if path.is_file():
                add(records, path)
    helpers = pure_elf_helpers()
    tool_paths = [Path('/usr/bin/env'), Path('/usr/bin/python3'), T / 'bin/clang++',
                  T / 'bin/llvm-ar', Path(spec['generator_host_tool']['path'])]
    assert pin(tool_paths[-1]) == {key: spec['generator_host_tool'][key] for key in ('bytes', 'sha256')}
    for path, expected in TOOL_PINS.items():
        assert pin(path) == expected, path
    pending = list(tool_paths)
    seen = set()
    dynamic = {}
    roots = (T / 'lib', T / 'lib/x86_64-unknown-linux-gnu', SDK / 'usr/lib/x86_64-linux-gnu',
             Path('/usr/lib/x86_64-linux-gnu'), Path('/usr/lib'))
    while pending:
        path = pending.pop()
        add(records, path)
        resolved = path.resolve(strict=True)
        if str(resolved) in seen:
            continue
        seen.add(str(resolved))
        metadata = helpers['elf'](resolved)
        dynamic[str(resolved)] = metadata
        if metadata['interpreter']:
            pending.append(Path(metadata['interpreter']))
        for needed in metadata['needed']:
            matches = {candidate.resolve(strict=True) for root in roots for candidate in root.glob(needed) if candidate.is_file()}
            assert matches, (resolved, needed)
            pending.extend(sorted(matches))
    for path in sorted(Path('/usr/lib/python3.14').rglob('*')):
        if path.is_file():
            add(records, path)
    assert records['/usr/bin/env']['sha256'] == '2a9b9ccf4e9724a6d6d8c97c835c9932223fbdefc9706fef9d0dfef7a9076739'
    return {'source_id': OLD_SOURCE_ID, 'records': records, 'ELF_candidate_metadata': dynamic,
            'loaded_provider_maps_qualified': False, 'target_source_files': count}


def idle():
    assert Path('/proc/sys/kernel/random/boot_id').read_text().strip() == BOOT
    assert sorted(map(int, (CG / 'cgroup.procs').read_text().split())) == [10154]
    assert int((CG / 'memory.max').read_text()) == 64 << 30
    assert (CG / 'memory.swap.max').read_text().strip() == '0'
    assert (CG / 'cpuset.cpus.effective').read_text().strip() == '0,2,4,6,16-31'
    return {key: int(value) for key, value in
            (line.split() for line in (CG / 'memory.events').read_text().splitlines())}


def before():
    assert not OUT.exists() and not EVIDENCE.exists()
    free = os.statvfs(B)
    assert free.f_bavail * free.f_frsize >= (8 << 30) + (256 << 20)
    initial_events = idle()
    baseline = source_and_provider_snapshot()
    assert idle() == initial_events
    OUT.mkdir(mode=0o700)
    for name in ('objects', 'lib', 'generated'):
        (OUT / name).mkdir(mode=0o700)
    save_new(OUT / 'before.json', {'schema': 'A64-missing-MC-before-v1', 'snapshot': baseline,
                                 'events': initial_events, 'native_executed': False})


def verify():
    before_value = load(OUT / 'before.json')
    assert before_value['snapshot'] == source_and_provider_snapshot()
    assert idle() == before_value['events']
    return before_value


def dependencies(path, expected_output, baseline, generated):
    text = Path(path).read_text().replace('\\\n', '')
    left, right = text.split(':', 1)
    assert shlex.split(left) == [str(expected_output)]
    tokens = shlex.split(right)
    assert tokens and all(Path(token).is_absolute() for token in tokens)
    pinned = {}
    allowed = {item['resolved']: item for item in baseline['records'].values()}
    for token in tokens:
        resolved = Path(token).resolve(strict=True)
        value = record(token)
        if str(resolved) in generated:
            assert pin(resolved) == generated[str(resolved)]
        else:
            assert str(resolved) in allowed and value == allowed[str(resolved)], token
        pinned[token] = value
    return pinned


def elf_object(raw):
    assert len(raw) >= 64 and raw[:6] == b'\x7fELF\x02\x01'
    kind, machine = struct.unpack_from('<HH', raw, 16)
    assert kind == 1 and machine == 183
    return {'class': 2, 'endianness': 1, 'e_type': kind, 'machine': machine}


def archive(path, objects):
    raw = Path(path).read_bytes()
    assert len(raw) <= 256 << 20 and raw[:8] == b'!<arch>\n'
    offset = 8
    strings = b''
    members = {}
    while offset < len(raw):
        assert len(raw) - offset >= 60
        header = raw[offset:offset + 60]
        assert header[58:60] == b'`\n'
        size_text = header[48:58].strip()
        assert size_text.isdigit()
        size = int(size_text)
        offset += 60
        assert size <= len(raw) - offset
        payload = raw[offset:offset + size]
        offset += size
        if offset & 1:
            assert raw[offset:offset + 1] == b'\n'
            offset += 1
        name = header[:16].decode('ascii').strip()
        if name == '//':
            assert not strings
            strings = payload
            continue
        if name in ('/', '/SYM64/'):
            continue
        if name.startswith('/'):
            assert name[1:].isdigit()
            index = int(name[1:])
            assert index < len(strings)
            end = strings.index(b'/\n', index)
            name = strings[index:end].decode('ascii')
        else:
            assert name.endswith('/')
            name = name[:-1]
        assert name in objects and name not in members
        expected = objects[name]
        assert payload == Path(expected).read_bytes()
        members[name] = {'pin': pin(expected), 'ELF': elf_object(payload)}
    assert offset == len(raw) and set(members) == set(objects)
    return {'pin': pin(path), 'actual_ELF_members': members}


def after():
    assert not (OUT / 'after.json').exists()
    before_value = verify()
    spec = plan()
    commands = spec['fixed_literal13_native_commands']
    rows = load(EVIDENCE / 'receipts.json')
    assert len(rows) == len(commands) == 13
    for row, (label, argv) in zip(rows, commands):
        assert row['label'] == label and row['argv'] == argv and row['returncode'] == 0 and row['passed'] is True
        assert not row.get('error') and not row.get('cleanup_discovery_error')
        assert row['command_file_sha256'] == pin(D / 'commands.json')['sha256']
        assert row['supervisor_sha256'] == pin(D / 'supervisor.py')['sha256']
        assert row['remaining_roster'] == [10154] and row['memory_events_before'] == row['memory_events_after'] == before_value['events']
        assert row['memory_max_bytes'] == 64 << 30 and row['swap_max_bytes'] == 0
        assert row['aggregate_owned_rss_budget_bytes'] == 8 << 30
        assert row['Popen_root_pidfd_retirement']['pidfd_readable'] and row['Popen_root_pidfd_retirement']['actual_reaped_returncode'] == 0
        assert row['retirement'] and all(item['pidfd_readable'] for item in row['retirement'])
        assert pin(EVIDENCE / (label + '.log'))['sha256'] == row['log_sha256']
    bridge = load(EVIDENCE / 'lane-retirement.json')
    assert bridge['supervisor_complete'] and bridge['owned_tasks_retired'] and bridge['work_passed']
    assert bridge['planned_commands'] == bridge['executed_commands'] == 13
    assert bridge['original_receipts_sha256'] == pin(EVIDENCE / 'receipts.json')['sha256']
    assert bridge['commands_sha256'] == pin(D / 'commands.json')['sha256'] and bridge['supervisor_sha256'] == pin(D / 'supervisor.py')['sha256']
    ticket_id = bridge['ticket_id']
    assert re.fullmatch('[0-9a-f]{32}', ticket_id)
    ticket_path = B / 'authority' / (ticket_id + '.json')
    ticket = load(ticket_path)
    assert ticket['state'] == 'retired' and ticket['ticket_id'] == ticket_id
    assert ticket['owner'] == '/root/qemu_platform_tests'
    assert ticket['suite'] == 'aarch64-GNU-missing-MC-components-20261003-r1'
    assert ticket['commands_sha256'] == pin(D / 'commands.json')['sha256']
    assert ticket['commands'] == str(D / 'commands.json')
    assert ticket['supervisor_receipt_sha256'] == pin(EVIDENCE / 'lane-retirement.json')['sha256']
    generated = {}
    dependency_proofs = {}
    for name, expected in EXPECTED_TABLES.items():
        path = OUT / 'generated' / name
        assert pin(path) == expected
        generated[str(path.resolve(strict=True))] = expected
        dependency_proofs[name] = dependencies(path.with_name(name + '.d'), path, before_value['snapshot'], generated)
    objects = {}
    for names in GROUPS.values():
        for stem in names:
            path = OUT / 'objects' / (stem + '.o')
            objects[stem + '.o'] = {'pin': pin(path), 'ELF': elf_object(path.read_bytes())}
            dependency_proofs[stem] = dependencies(OUT / 'objects' / (stem + '.d'), path, before_value['snapshot'], generated)
    archives = {name: archive(OUT / 'lib' / name, {stem + '.o': OUT / 'objects' / (stem + '.o') for stem in stems})
                for name, stems in GROUPS.items()}
    total = sum(path.stat().st_size for path in OUT.rglob('*') if path.is_file())
    assert total <= 256 << 20
    assert verify() == before_value
    save_new(OUT / 'after.json', {'schema': 'A64-missing-MC-after-v1', 'passed': True,
                                 'scope': 'same-source GNU LLVM target ET_REL supplemental archives only',
                                 'before': pin(OUT / 'before.json'), 'receipts': pin(EVIDENCE / 'receipts.json'),
                                 'retirement_bridge': pin(EVIDENCE / 'lane-retirement.json'), 'ticket': pin(ticket_path),
                                 'generated_tables': generated, 'objects': objects, 'archives': archives,
                                 'actual_fresh_dependency_proofs': dependency_proofs,
                                 'source_provider_before_after_equal': True, 'archive_member_ELF_qualified': True,
                                 'loaded_provider_maps_qualified': False, 'cross_archive_link_ABI_qualified': False,
                                 'full_three_TU_product_qualified': False, 'guest_execution_qualified': False,
                                 'ROS_bundled_paired_runtime_product_qualified': False})


if __name__ == '__main__':
    assert not sys.flags.optimize and not os.environ.get('PYTHONOPTIMIZE')
    assert len(sys.argv) == 2 and sys.argv[1] in ('before', 'verify', 'after')
    {'before': before, 'verify': verify, 'after': after}[sys.argv[1]]()
