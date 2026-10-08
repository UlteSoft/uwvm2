#!/usr/bin/env python3
"""Fresh real-product debugger quit/EOF/error checks after native-output migration.

Run on Linux inside the required test cgroup. This is a CLI/output-lifetime test;
it does not claim guest drain, cache persistence, or a handler-performance result.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def sha256(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--uwvm', type=Path, required=True)
parser.add_argument('--wasm-tools', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--source-root', type=Path)
parser.add_argument('--timeout', type=float, default=20)
args = parser.parse_args()
binary = args.uwvm.resolve(strict=True)
wasm_tools = args.wasm_tools.resolve(strict=True)
root = (args.source_root.resolve(strict=True) if args.source_root else
        Path(__file__).resolve().parents[2])
subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=False)

wat = '''(module
  (tag $e (param i32))
  (func (export "_start")
    (block $caught (result i32)
      (try_table (catch $e $caught) i32.const 7 throw $e)
      unreachable)
    drop
    (loop $forever br $forever)))\n'''
(out / 'live-core3.wat').write_text(wat)
wasm = out / 'live-core3.wasm'
subprocess.run([str(wasm_tools), 'parse', str(out / 'live-core3.wat'),
                '-o', str(wasm)], check=True)
subprocess.run([str(wasm_tools), 'validate', str(wasm)], check=True)
rows = []
source_paths = [
    'src/uwvm2/uwvm/run/run.h',
    'src/uwvm2/runtime/lib/uwvm_runtime_posix_abi.h',
    'src/uwvm2/uwvm/debugger/console.h',
    'src/uwvm2/uwvm_predefine/io/output.h',
    'src/uwvm2/uwvm_predefine/io/runtime_log.h',
    'third-parties/fast_io/include/fast_io_legacy_impl/defined_types.h',
]
provenance = {
    'binary': str(binary), 'binary_sha256': sha256(binary),
    'wasm_sha256': sha256(wasm),
    'sources': {name: sha256(root / name) for name in source_paths},
}
(out / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')


def check_case(name, mode, policy, script, expected_exit=0, input_error=False):
    command = [str(binary), *mode, '-Rct', '0', '-Rllvm-call-stack', policy,
               '-Rllvm-cache-path', 'disable', '-WFE-exceptions',
               '--run', str(wasm)]
    start = time.monotonic()
    read_fd = write_fd = None
    try:
        if input_error:
            # A write-only pipe is a real unreadable input descriptor. The
            # launch seal must reject it before guest entry/console creation.
            # An empty NONBLOCK reader is temporary EAGAIN, not input_failure;
            # its interruptible wait is covered by the keyboard component.
            read_fd, write_fd = os.pipe()
            child = subprocess.Popen(command, stdin=write_fd, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT)
            try:
                output, _ = child.communicate(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                child.kill()
                output, _ = child.communicate()
                (out / (name + '.log')).write_bytes(output)
                raise
            exit_code = child.returncode
        else:
            result = subprocess.run(command, input=script, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=args.timeout)
            output, exit_code = result.stdout, result.returncode
    finally:
        if read_fd is not None:
            os.close(read_fd)
        if write_fd is not None:
            os.close(write_fd)
    elapsed = time.monotonic() - start
    (out / (name + '.log')).write_bytes(output)
    row = {'name': name, 'command': command, 'stdin': repr(script),
           'input_error': input_error, 'exit': exit_code, 'seconds': elapsed,
           'log_sha256': hashlib.sha256(output).hexdigest(), 'passed': False}
    rows.append(row)
    (out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
    assert exit_code == expected_exit, (name, exit_code, output[-8000:])
    if input_error:
        assert b'input descriptor is not readable' in output, (name, output[-8000:])
        assert b'[fatal]' in output, (name, output[-8000:])
        assert b'UWVM LLVM full debugger.' not in output and b'(uwvm-debug) ' not in output, (name, output[-8000:])
    else:
        assert b'UWVM LLVM full debugger.' in output, (name, output[-8000:])
        assert b'(uwvm-debug) ' in output, (name, output[-8000:])
        assert b'[fatal]' not in output, (name, output[-8000:])
    if script and b'help\n' in script:
        # The complete final help line must survive immediate process exit.
        assert b'quit/EOF terminates this CLI process;' in output, (name, output[-8000:])
    if script and b'not-a-debug-command\n' in script:
        assert b'error: invalid command; type help\n' in output, (name, output[-8000:])
    row['passed'] = True
    (out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')


for policy in ('instruction', 'unwind', 'none'):
    mode = ['-m', 'debug-jit']
    check_case(policy + '-quit', mode, policy, b'help\nquit\n')
    check_case(policy + '-eof', mode, policy, b'help\n')
    check_case(policy + '-empty-eof', mode, policy, b'')
    check_case(policy + '-invalid-then-quit', mode, policy,
               b'not-a-debug-command\nhelp\nquit\n')
    check_case(policy + '-live-quit', mode, policy, b'continue\nhelp\nquit\n')
    check_case(policy + '-read-error', mode, policy, None,
               expected_exit=1, input_error=True)

# The runtime aliases must enter the same console and preserve its exit policy.
for name, mode in [('short-alias', ['-Rdbg']), ('long-alias', ['--runtime-debug'])]:
    check_case(name, mode, 'unwind', b'help\nquit\n')

(out / 'summary.json').write_text(json.dumps({
    'passed': True, 'processes': len(rows), **provenance,
    'scope': 'Real Linux product native-output quit/EOF/input-error status and '
             'bounded live-guest exit with Core3 try_table and unreadable-input launch rejection; no console-read-error/drain/finalizer guarantee.',
}, indent=2) + '\n')
print('PASS actual debug fast_io exit CLI', len(rows), 'processes')
