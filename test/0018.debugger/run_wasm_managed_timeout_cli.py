#!/usr/bin/env python3
"""Real prepared-VM quit timeout and PIDFD-only cleanup, Linux cgroup only."""
import argparse
import hashlib
import json
import signal
import subprocess
import time
from pathlib import Path
from run_wasm_opcode_step_cli import Console


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'binary', 'fixture', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args(); args.out.mkdir(parents=True, exist_ok=False)
    subprocess.run(['bash', str(args.source_root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    console = Console([str(args.binary), '-Rdbg', '-Rct', '0', '-Rllvm-cache-path', 'disable',
        '-WFE-reference-types', '-WFE-function-references', '-WFE-bulk-memory', '--run', str(args.fixture)],
        args.out / 'managed-timeout.log')
    try:
        console.prompt()
        assert b'prepared; no Wasm instruction executed' in console.send('status')
        # Stop only the actual VM this console created, through its retained
        # PIDFD. It cannot read quit, so close must hit the real 30s deadline.
        signal.pidfd_send_signal(console.pidfd, signal.SIGSTOP)
        deadline = time.monotonic() + 2
        while True:
            stat = (Path('/proc') / str(console.child.pid) / 'stat').read_text()
            if stat[stat.rfind(')') + 2:].split()[0] == 'T':
                break
            assert time.monotonic() < deadline, 'owned VM did not enter a real stopped state'
            time.sleep(.005)
        started = time.monotonic()
        try:
            console.close()
        except AssertionError as error:
            assert str(error) == 'managed debug quit did not retire within 30 seconds', repr(error)
        else:
            raise AssertionError('genuine stopped-VM quit timeout was silently accepted')
        elapsed = time.monotonic() - started
        proof = console.retirement_proof
        assert 30 <= elapsed <= 40 and console.child.returncode == -signal.SIGKILL, (elapsed, console.child.returncode)
        assert proof['pidfd_readable'] and proof['actual_reaped_returncode'] == -signal.SIGKILL
        result = dict(passed=True, actual_VM=True, prepared_without_guest_execution=True,
            genuine_wait_timeout=True, elapsed_seconds=elapsed, actual_VM_exit=-signal.SIGKILL,
            expected_process_signal='SIGKILL', VM_PIDFD_proof=proof,
            binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(),
            fixture_sha256=hashlib.sha256(args.fixture.read_bytes()).hexdigest(),
            harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            cgroup=Path('/proc/self/cgroup').read_text())
        (args.out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result), flush=True)
    finally:
        if console.child.poll() is None:
            signal.pidfd_send_signal(console.pidfd, signal.SIGCONT)
            console.close()


if __name__ == '__main__':
    main()
