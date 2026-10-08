#!/usr/bin/env python3
"""Check exact native exception traces during concurrent lazy host execution.

Only one thread throws per process, so a second fatal cannot interleave frames.
All eight threads rendezvous inside actual JIT frames before the elected throw.
This executes a freshly linked runtime; it is not an IR or CFI-only probe.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--wasm-tools', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--rounds', type=int, default=2)
    a = p.parse_args()
    if a.rounds < 1:
        p.error('--rounds must be positive')
    subprocess.run(['bash', str(Path(__file__).resolve().parents[2] / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    a.out.mkdir(parents=True, exist_ok=False)
    source = ['(module (import "trace-host" "rendezvous" (func $rendezvous)) (tag $t (param i32)) (func $warm)']
    for group in range(8):
        source += [f'''(func $leaf{group} (param i32) (result i32)
          call $rendezvous local.get 0 if i32.const {group} throw $t end i32.const 1)
          (func $middle{group} (param i32) (result i32) local.get 0 call $leaf{group} i32.const 1 i32.add)
          (func $entry{group} (param i32) (result i32) local.get 0 call $middle{group} i32.const 1 i32.add)''']
    source += [')']
    wat, wasm = a.out / 'concurrent.wat', a.out / 'concurrent.wasm'
    wat.write_text('\n'.join(source) + '\n')
    subprocess.run([str(a.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
    subprocess.run([str(a.wasm_tools), 'validate', str(wasm)], check=True)
    rows = []
    control_command = [str(a.probe), str(wasm), '8']
    control = subprocess.run(control_command, capture_output=True, timeout=120)
    (a.out / 'nonthrowing-control.log').write_bytes(control.stdout + control.stderr)
    control_passed = (control.returncode == 0 and b'PASS nonthrowing concurrent outputs=3' in control.stdout and
                      b'PROBE concurrent_native_entries=8' in control.stdout and
                      b'FAIL' not in control.stdout + control.stderr)
    rows.append({'kind': 'nonthrowing-control', 'command': control_command, 'exit': control.returncode, 'passed': control_passed})
    (a.out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    if not control_passed:
        raise RuntimeError('concurrent native normal-return control failed')
    for repeat in range(a.rounds):
        for group in range(8):
            command = [str(a.probe), str(wasm), str(group)]
            result = subprocess.run(command, capture_output=True, timeout=120)
            raw = result.stdout + result.stderr
            log_path = a.out / f'round-{repeat}-group-{group}.log'
            log_path.write_bytes(raw)
            log = re.sub(r'\x1b\[[0-9;]*m', '', raw.decode(errors='replace'))
            frames = [int(i) for i in re.findall(r'(?m)^uwvm: \[info\]\s+#\d+ .*?func_idx=(\d+)', log)]
            expected = [2+3*group, 3+3*group, 4+3*group]
            payload = re.findall(r'(?m)^uwvm: \[info\]\s+payload\[0\] i32 bits=0x([0-9a-fA-F]+) signed=(-?\d+)$', log)
            passed = (result.returncode != 0 and frames == expected and
                      'PROBE concurrent_native_entries=8' in log and
                      'Uncaught WebAssembly exception' in log and
                      len(payload) == 1 and int(payload[0][0], 16) == group and int(payload[0][1]) == group and
                      'truncated' not in log.lower() and 'FAIL' not in log)
            rows.append({'repeat': repeat, 'group': group, 'command': command,
                         'exit': result.returncode, 'frames': frames, 'expected': expected, 'passed': passed})
            (a.out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
            if not passed:
                raise RuntimeError(f'incomplete or incorrect concurrent native trace: {log_path}')
    (a.out / 'summary.json').write_text(json.dumps({
        'passed': True, 'executions': len(rows), 'uncaught_executions': 8*a.rounds, 'normal_control_executions': 1, 'concurrent_hosts_per_process': 8,
        'exact_native_frames_per_exception': 3, 'trace_policy': 'unwind', 'runtime_mode': 'llvm-jit-lazy',
        'probe_sha256': hashlib.sha256(a.probe.read_bytes()).hexdigest(),
        'fixture_sha256': hashlib.sha256(wasm.read_bytes()).hexdigest()}, indent=2) + '\n')
    print(f'PASS {8*a.rounds} elected uncaught traces plus normal-return control, 8 concurrent native chains each, exact 3-frame stacks')


if __name__ == '__main__':
    main()
