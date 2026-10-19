#!/usr/bin/env python3
"""Recheck rebuilt artifacts without overwriting pre-reboot reference evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shutil
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def custom_sections(path):
    data = path.read_bytes()
    assert data[:8] == b'\0asm\x01\0\0\0'
    pos = 8

    def leb():
        nonlocal pos
        value, shift = 0, 0
        while True:
            byte = data[pos]
            pos += 1
            value |= (byte & 127) << shift
            if not byte & 128:
                return value
            shift += 7

    names = []
    while pos < len(data):
        tag = data[pos]
        pos += 1
        size = leb()
        end = pos + size
        if tag == 0:
            length = leb()
            names.append(data[pos:pos + length].decode())
        pos = end
    assert pos == len(data)
    return names


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--expected-cgroup', required=True)
    args = parser.parse_args()
    assert os.uname().sysname == 'Linux'
    assert Path('/proc/self/cgroup').read_text() == '0::' + args.expected_cgroup + '\n'
    group = Path('/sys/fs/cgroup') / args.expected_cgroup.lstrip('/')
    assert (group / 'memory.max').read_text().strip() == str(64 << 30)
    assert (group / 'memory.swap.max').read_text().strip() == '0'
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    task = args.arena / 'csharp'
    source = task / 'src'
    sdk = task / 'dotnet/dotnet'
    original = task / 'build/AppBundle/highlevel-csharp.wasm'
    timed = task / 'target/build-timed/AppBundle/highlevel-csharp-timed.wasm'
    native = task / 'target/bin-native/Release/net8.0/highlevel-csharp.dll'
    native_timed = task / 'target/bin-native-timed/Release/net8.0/highlevel-csharp-timed.dll'
    report = {'passed': False, 'scope': 'post-reboot native/Wasmtime references; no UWVM runs',
              'cgroup': args.expected_cgroup, 'memory_events_before': (group / 'memory.events').read_text(),
              'disk_before': list(shutil.disk_usage(args.arena)), 'runs': [], 'artifacts': {}}
    for label, artifact in [('semantic', original), ('timed', timed)]:
        sections = custom_sections(artifact)
        assert {'name', '.debug_info', '.debug_line'}.issubset(sections), sections
        assert artifact.with_suffix('.pdb').is_file()
        report['artifacts'][label] = {'path': str(artifact), 'sha256': digest(artifact),
                                      'size_bytes': artifact.stat().st_size, 'custom_sections': sections,
                                      'pdb_sha256': digest(artifact.with_suffix('.pdb'))}
    report['tools'] = {str(path): digest(path) for path in [sdk, args.wasmtime]}

    def run(engine, kind, argv, check, iterations=None):
        at = time.monotonic()
        proc = subprocess.run([str(token) for token in argv], capture_output=True, text=True, timeout=120)
        row = {'engine': engine, 'kind': kind, 'iterations': iterations, 'argv': [str(token) for token in argv],
               'exit': proc.returncode, 'stdout': proc.stdout, 'stderr': proc.stderr,
               'seconds': time.monotonic() - at, 'passed': check(proc)}
        report['runs'].append(row)
        (args.out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')

    for case in json.loads((source / 'cases.json').read_text()):
        def check(proc, case=case):
            return proc.returncode == case['exit_code'] and proc.stdout == case['stdout'] and proc.stderr == case['stderr']
        for engine, command in [('native', [sdk, native]), ('wasmtime', [args.wasmtime, 'run', original])]:
            run(engine, case['name'], [*command, *case['args']], check)
    for iterations, checksum in [(1000, 701896241), (20000, 2356050119), (1000000, 592549767)]:
        semantic = f'csharp classes=42 arrays=55 strings=7 exceptions=3 iterations={iterations} checksum={checksum}\n'
        pattern = re.compile(re.escape(semantic) + rf'csharp kernel iterations={iterations} ticks=([0-9]+) frequency=([0-9]+)\n')
        def check(proc, pattern=pattern):
            match = pattern.fullmatch(proc.stdout)
            return proc.returncode == 0 and not proc.stderr and match is not None and int(match[2]) > 0
        for engine, command in [('native', [sdk, native_timed]), ('wasmtime', [args.wasmtime, 'run', timed])]:
            run(engine, 'timed', [*command, str(iterations)], check, iterations)
    report['memory_events_after'] = (group / 'memory.events').read_text()
    before = dict(line.split() for line in report['memory_events_before'].splitlines())
    after = dict(line.split() for line in report['memory_events_after'].splitlines())
    assert all(before[key] == after[key] for key in ['oom', 'oom_kill', 'oom_group_kill'])
    report['disk_after'] = list(shutil.disk_usage(args.arena))
    report['passed'] = len(report['runs']) == 22 and all(row['passed'] for row in report['runs'])
    (args.out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'passed': report['passed'], 'cases': len(report['runs']),
                      'artifacts': report['artifacts']}), flush=True)
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
