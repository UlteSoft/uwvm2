#!/usr/bin/env python3
"""Rebuild language artifacts after arena loss; Node correctness only, no benchmark.

Use the pinned tools recorded in recovery-downloads.json, extracted once below
ARENA/other/toolchains. Historical checked-in receipts are never overwritten.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess


EXPECTED = {50: 265424, 200: 739915, 1000: 262020, 20000: 264715,
            200000: 673761, 2000000: 682696}


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, required=True)
    parser.add_argument('--cpu', type=int, default=23)
    args = parser.parse_args()
    assert os.uname().sysname == 'Linux'
    membership = Path('/proc/self/cgroup').read_text().strip()
    assert membership.startswith('0::/system.slice/docker-')
    cgroup = Path('/sys/fs/cgroup') / membership[3:].lstrip('/')
    assert (cgroup / 'memory.max').read_text().strip() == str(64 << 30)
    assert (cgroup / 'memory.swap.max').read_text().strip() == '0'
    os.sched_setaffinity(0, {args.cpu})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    arena = args.arena
    tools = arena / 'other/toolchains'
    source = arena / 'other/test/0050.high_level_languages'
    node = tools / 'node-v22.21.1-linux-x64/bin/node'
    moon = tools / 'moon'
    env = os.environ | {'MOON_HOME': str(moon), 'NODE': str(node),
        'JAVY': str(tools / 'javy'), 'MOON': str(moon / 'bin/moon'),
        'MOONC': str(moon / 'bin/moonc'),
        'PATH': f'{node.parent}:{moon / "bin"}:' + os.environ['PATH'],
        'npm_config_cache': str(arena / 'other/npm-cache')}
    receipt = arena / 'other/recovery-build.json'
    assert not receipt.exists(), 'keep an existing recovery receipt; use a fresh arena for another recovery'
    report = {'passed': False, 'scope': 'producer rebuild and Node correctness only; no comparative benchmark or candidate VM',
              'boot': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
              'cgroup': membership, 'cpu': args.cpu, 'commands': [], 'artifacts': [], 'historical_hash_comparison': []}

    def save():
        receipt.write_text(json.dumps(report, indent=2))

    def run(argv, cwd=None, timeout=300):
        argv = [str(value) for value in argv]
        row = {'argv': argv, 'cwd': str(cwd) if cwd else None}
        print(json.dumps(row), flush=True)
        process = subprocess.run(argv, cwd=cwd, env=env, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, timeout=timeout)
        row.update(exit=process.returncode, stdout=process.stdout.decode(errors='replace'),
                   stderr=process.stderr.decode(errors='replace'))
        report['commands'].append(row)
        save()
        assert process.returncode == 0, row

    try:
        assert node.is_file() and (tools / 'javy').is_file() and (moon / 'bin/moon').is_file()
        run([node, '--version'])
        run([node.parent / 'npm', 'ci', '--ignore-scripts', '--no-audit', '--no-fund'], source / 'assemblyscript')
        for target in ['wasm', 'wasm-gc']:
            run([moon / 'bin/moon', '-C', moon / 'lib/core', 'bundle', '--target', target, '--warn-list', '-a'])
        artifacts = arena / 'other/artifacts'
        for language in ['assemblyscript', 'javascript', 'moonbit']:
            run(['/bin/sh', source / language / 'build.sh', artifacts / language], timeout=1200)
        # The base samples are used by the public matrix; their oracle evidence
        # must also be rebuilt rather than inherited from pre-reboot reports.
        oracle = source / 'javascript/node_oracle.cjs'
        for language, modules in [
            ('assemblyscript', [('assemblyscript-debug', 20000), ('assemblyscript-release', 20000), ('assemblyscript-perf', 2000000)]),
            ('moonbit', [(f'moonbit-{target}-{mode}', rounds) for target in ['wasm', 'wasm-gc']
                        for mode, rounds in [('debug', 20000), ('perf', 2000000)]])]:
            for name, rounds in modules:
                run([node, oracle, artifacts / language / (name + '.wasm'), rounds, 'pinned producer recovery'])
        for path in artifacts.rglob('*.oracle.json'):
            row = json.loads(path.read_text())
            assert row['exit'] == 0 and f'checksum={EXPECTED[row["rounds"]]}' in row['stdout'], path
        engines = arena / 'engines'
        run(['/usr/bin/python3', engines / 'timed/build.py', '--toolchains', tools,
             '--assemblyscript-deps', source / 'assemblyscript', '--out', engines / 'timed/artifacts'], timeout=1200)
        old = json.loads((engines / 'timed/timed-cases.json').read_text())
        new = json.loads((engines / 'timed/artifacts/timed-cases.json').read_text())
        previous = {row['name']: row for row in old}
        assert set(previous) == {row['name'] for row in new}
        for row in new:
            report['historical_hash_comparison'].append({'name': row['name'],
                'previous_sha256': previous[row['name']]['wasm_sha256'],
                'recovery_sha256': row['wasm_sha256'],
                'identical': previous[row['name']]['wasm_sha256'] == row['wasm_sha256']})
        for language in ['assemblyscript', 'javascript', 'moonbit']:
            old = json.loads((source / language / 'node_calibration_reference.json').read_text())
            previous = {row['module']: row['wasm_sha256'] for row in old['reference_runs']}
            for module, old_sha in previous.items():
                path = artifacts / language / module
                report['historical_hash_comparison'].append({'name': module,
                    'previous_sha256': old_sha, 'recovery_sha256': sha(path), 'identical': old_sha == sha(path)})
        for directory in [artifacts, engines / 'timed/artifacts']:
            report['artifacts'].extend({'path': str(path), 'bytes': path.stat().st_size, 'sha256': sha(path)}
                                      for path in sorted(directory.rglob('*.wasm')))
        report['passed'] = True
    except BaseException as error:
        report['error'] = repr(error)
        raise
    finally:
        report['memory_events_after'] = (cgroup / 'memory.events').read_text()
        save()
    print(json.dumps({'passed': report['passed'], 'receipt': str(receipt),
                      'artifacts': len(report['artifacts']),
                      'historical_identical': sum(row['identical'] for row in report['historical_hash_comparison']),
                      'historical_compared': len(report['historical_hash_comparison'])}), flush=True)


if __name__ == '__main__':
    main()
