#!/usr/bin/env python3
"""Validate the independent profiling guest's two real stdin gates."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import select
import signal
import subprocess
import sys
import time


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def checksum(iterations):
    value = 0x12345678
    for index in range(iterations):
        value ^= (value << 13) & 0xffffffff
        value ^= value >> 17
        value ^= (value << 5) & 0xffffffff
        value = (value + index) & 0xffffffff
    return value


def gated(command, iterations, oracle):
    started = time.monotonic()
    child = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, start_new_session=True)
    output = bytearray()

    def until(marker, timeout):
        deadline = time.monotonic() + timeout
        while marker not in output:
            remaining = deadline - time.monotonic()
            assert remaining > 0, ('gate deadline', marker, bytes(output))
            if select.select([child.stdout], [], [], min(remaining, .1))[0]:
                block = os.read(child.stdout.fileno(), 65536)
                assert block, ('unexpected guest EOF', marker, bytes(output))
                output.extend(block)
                assert len(output) <= 65536

    try:
        until(b'UWVM_PROFILE_READY\n', 600)
        assert bytes(output) == b'UWVM_PROFILE_READY\n', 'semantic output must wait for D'
        ready_seconds = time.monotonic() - started
        # A completed marker is not evidence of blocking; require a live process
        # with no stdout until its host releases the corresponding byte gate.
        assert child.poll() is None and not select.select([child.stdout], [], [], .05)[0]
        kernel_release = time.monotonic()
        child.stdin.write(b'G')
        child.stdin.flush()
        until(b'UWVM_PROFILE_DONE\n', 600)
        done_seconds = time.monotonic() - kernel_release
        assert bytes(output) == b'UWVM_PROFILE_READY\nUWVM_PROFILE_DONE\n'
        assert child.poll() is None and not select.select([child.stdout], [], [], .05)[0]
        tail, errors = child.communicate(input=b'D', timeout=120)
        output.extend(tail)
        text = output.decode()
        expected = ('csharp classes=42 arrays=55 strings=7 exceptions=3 iterations='
                    + str(iterations) + ' checksum=' + str(oracle))
        lines = text.splitlines()
        assert child.returncode == 0 and errors == b'' and len(lines) == 4
        assert lines[2] == expected, text
        match = re.fullmatch(r'csharp kernel iterations=(\d+) ticks=(\d+) frequency=(\d+)', lines[3])
        assert match and int(match[1]) == iterations and int(match[2]) > 0 and int(match[3]) > 0
        return {'passed': True, 'argv': command, 'exit': child.returncode, 'stdout': text,
                'stderr': errors.decode(), 'ready_seconds': ready_seconds,
                'gate_release_to_done_seconds': done_seconds, 'kernel_ticks': int(match[2]),
                'kernel_frequency': int(match[3]), 'kernel_seconds': int(match[2]) / int(match[3]),
                'seconds': time.monotonic() - started, 'both_gates_blocked_until_released': True}
    finally:
        if child.poll() is None:
            os.killpg(child.pid, signal.SIGKILL)
            child.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--dotnet', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--iterations', type=int, action='append')
    args = parser.parse_args()
    assert sys.platform == 'linux'
    expected_cgroup = '/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
    assert Path('/proc/self/cgroup').read_text() == '0::' + expected_cgroup + '\n'
    group = Path('/sys/fs/cgroup') / expected_cgroup.lstrip('/')
    assert (group / 'memory.max').read_text().strip() == str(64 << 30)
    assert (group / 'memory.swap.max').read_text().strip() == '0'
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    wasm = args.build / 'AppBundle/highlevel-csharp-profile.wasm'
    native = args.build / 'native/highlevel-csharp-profile-native.dll'
    source = Path(__file__).resolve().parent
    report = {'passed': False, 'scope': 'independent profile-only guest; original benchmark Wasm unchanged',
              'membership': Path('/proc/self/cgroup').read_text(),
              'memory_events_before': (group / 'memory.events').read_text(),
              'wasm': str(wasm), 'wasm_sha256': digest(wasm), 'native_sha256': digest(native),
              'source_sha256': digest(source / 'ProgramProfile.cs'), 'script_sha256': digest(__file__),
              'sdk_version': subprocess.check_output([str(args.dotnet), '--version'], text=True).strip(),
              'wasmtime_version': subprocess.check_output([str(args.wasmtime), '--version'], text=True).strip(),
              'runs': []}
    assert report['sdk_version'] == '8.0.408'
    assert report['wasmtime_version'].startswith('wasmtime 42.0.1 ')
    for iterations in args.iterations or [1000, 20000, 1000000]:
        assert 1 <= iterations <= 100000000
        oracle = checksum(iterations)
        for label, command in [('native-dotnet', [str(args.dotnet), str(native), str(iterations)]),
                               ('wasmtime', [str(args.wasmtime), 'run', str(wasm), str(iterations)])]:
            row = dict(gated(command, iterations, oracle), runner=label, iterations=iterations, checksum=oracle)
            report['runs'].append(row)
            (args.out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
            print(json.dumps({'runner': label, 'iterations': iterations, 'passed': True,
                              'kernel_seconds': row['kernel_seconds']}), flush=True)
    report['memory_events_after'] = (group / 'memory.events').read_text()
    report['passed'] = len(report['runs']) > 0 and all(row['passed'] for row in report['runs'])
    assert report['wasm_sha256'] == digest(wasm) and report['native_sha256'] == digest(native)
    (args.out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
