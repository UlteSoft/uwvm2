#!/usr/bin/env python3
"""Compare actual instruction/unwind throw stacks across a retired host-tail adapter.

Requires a freshly linked, frozen-runtime probe. Run only inside the configured
SSH Linux cgroup; this runner never compiles and never uses synthetic LLVM IR.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess
import sys


def fixture(indirect):
    transfer = ('i32.const 0 return_call_indirect (type $signature)' if indirect else
                'return_call $reenter')
    return f'''(module
      (type $signature (func (param i32)))
      (import "trace-host" "reenter" (func $reenter (type $signature)))
      (tag $exception (param i32))
      (table 1 funcref)
      (elem (i32.const 0) $reenter)
      (global $normal_marker (mut i32) (i32.const 0))
      (func $leaf (export "leaf") (param i32)
        local.get 0 if i32.const 37 throw $exception end)
      (func $retired (param i32)
        (block $unexpected_catch
          (try_table (catch_all $unexpected_catch)
            local.get 0 {transfer}))
        unreachable)
      (func $entry (export "entry") (param i32)
        local.get 0 call $retired
        i32.const 1 global.set $normal_marker)
      (func (export "normal_marker") (result i32) global.get $normal_marker))
'''


def sha(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def cgroup_state():
    root = Path('/sys/fs/cgroup')
    return {name: (root / name).read_text().strip() for name in
            ('memory.max', 'memory.swap.max', 'cpuset.cpus.effective', 'memory.events')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--product', choices=('uwvm2', 'uwvm2-ros'), required=True)
    parser.add_argument('--runtime-object', type=Path, required=True,
                        help='The exact frozen runtime object used when linking --probe')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    inputs = [args.probe.resolve(), args.runtime_object.resolve(), args.wasm_tools.resolve(),
              Path(__file__).resolve(), Path(__file__).with_name('llvm_exception_host_tail_reentry.cc').resolve()]
    before = {str(path): sha(path) for path in inputs}
    (args.out / 'inputs.json').write_text(json.dumps(before, indent=2) + '\n')
    initial_cgroup = cgroup_state()
    rows = []

    def run(name, command):
        log = args.out / (name + '.log')
        # Each subprocess gets one bounded file, preserving stdout/stderr order.
        # A pathological child cannot fill the shared tmpfs with diagnostics.
        def child_limits():
            resource.setrlimit(resource.RLIMIT_FSIZE, (131072, 131072))
        with log.open('wb') as output:
            try:
                result = subprocess.run([str(part) for part in command], stdout=output,
                                        stderr=subprocess.STDOUT, timeout=120, preexec_fn=child_limits)
                exit_code = result.returncode
            except subprocess.TimeoutExpired:
                exit_code = 'timeout'
        with log.open('rb') as output:
            raw = output.read(65537)
        bounded = len(raw) <= 65536
        text = re.sub(r'\x1b\[[0-9;]*m', '', raw[:65536].decode(errors='replace'))
        row = {'name': name, 'command': [str(part) for part in command],
               'exit': exit_code, 'bounded_output': bounded, 'log': log.name}
        rows.append(row)
        return row, text

    for indirect in (False, True):
        kind = 'indirect' if indirect else 'direct'
        wat, wasm = args.out / (kind + '.wat'), args.out / (kind + '.wasm')
        wat.write_text(fixture(indirect))
        for action, command in (
            ('parse', [args.wasm_tools, 'parse', wat, '-o', wasm]),
            ('validate', [args.wasm_tools, 'validate', wasm]),
        ):
            row, text = run(kind + '-' + action, command)
            row['passed'] = row['exit'] == 0 and row['bounded_output']
            if not row['passed']:
                (args.out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
                raise RuntimeError(f'{kind} fixture {action} failed; see {row["log"]}')
        for policy in ('instruction', 'unwind'):
            for scenario in ('normal', 'throw'):
                row, text = run(f'{kind}-{policy}-{scenario}', [args.probe, wasm, policy, scenario])
                frames = [(int(module), int(function)) for module, function in re.findall(
                    r'(?m)^uwvm: \[info\]\s+#\d+ module_id=(\d+) .*?func_idx=(\d+)', text)]
                functions = [function for _, function in frames]
                expected = [1, 0, 3] if indirect else [1, 3]
                row.update({'policy': policy, 'scenario': scenario, 'tail_kind': kind,
                            'frames': frames, 'expected_function_indices': expected if scenario == 'throw' else []})
                common = row['bounded_output'] and row['exit'] != 'timeout' and 'FAIL' not in text
                if scenario == 'normal':
                    row['passed'] = (common and row['exit'] == 0 and not frames and
                        'PROBE host_reentry_entered parameter=0' in text and
                        'PASS normal host reentry calls=1 returns=1 surviving_entry_continued=1' in text and
                        'Uncaught WebAssembly exception' not in text)
                else:
                    payload = re.findall(r'payload\[0\] i32 bits=0x([0-9a-fA-F]+) signed=(-?\d+)', text)
                    row['passed'] = (common and row['exit'] != 0 and
                        'PROBE host_reentry_entered parameter=1' in text and
                        'Uncaught WebAssembly exception' in text and 'entry_func_idx=1' in text and
                        functions == expected and len({module for module, _ in frames}) == 1 and
                        len(payload) == 1 and int(payload[0][0], 16) == 37 and int(payload[0][1]) == 37 and
                        'truncated' not in text.lower() and 'backtrace unavailable' not in text.lower())
                (args.out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
                print(('PASS' if row['passed'] else 'FAIL') + ' ' + row['name'] +
                      (f' functions={functions}' if scenario == 'throw' else ''), flush=True)

    comparisons = []
    for kind in ('direct', 'indirect'):
        thrown = {row['policy']: row for row in rows if row.get('tail_kind') == kind and row.get('scenario') == 'throw'}
        comparisons.append({'tail_kind': kind,
            'instruction_frames': thrown['instruction']['frames'], 'unwind_frames': thrown['unwind']['frames'],
            'equal': thrown['instruction']['frames'] == thrown['unwind']['frames']})
    unchanged = before == {str(path): sha(path) for path in inputs}
    passed = unchanged and all(row['passed'] for row in rows) and all(row['equal'] for row in comparisons)
    summary = {'passed': passed, 'product': args.product, 'runtime_mode': 'llvm-jit-full',
        'actual_vm_executions': 8, 'unchanged_inputs': unchanged, 'comparisons': comparisons,
        'cgroup_before': initial_cgroup, 'cgroup_after': cgroup_state(),
        'scope': 'Real host import and public raw reentry with Core3 throw/try_table/return_call[_indirect]; no debugger or synthetic IR path.'}
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(('PASS' if passed else 'FAIL') + ' host-tail reentry exception attribution; see summary.json', flush=True)
    return 0 if passed else 1


if __name__ == '__main__':
    sys.exit(main())
