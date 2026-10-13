#!/usr/bin/env python3
"""Check pre-op GC catchpoints and actual fatal dropped-segment traps, Linux only.

This does not claim an after-failure debugger stop: the current runtime aborts.
Unexpected exit, timeout, diagnostics or successful continuation fail the test.
"""
import argparse
import hashlib
import json
import os
import re
import resource
import signal
import subprocess
import time
from pathlib import Path
import run_wasm_live_containers_cli as containers
from wasm_container_boundary_cases import dropped_elem_trap_examples


def stopped(console, timeout):
    deadline = time.monotonic() + timeout
    while True:
        reply = console.send('status')
        if b'stopped:' in reply:
            return reply
        assert b'guest exited:' not in reply and time.monotonic() < deadline, reply
        time.sleep(.01)


def resume_to_fatal(console, timeout):
    console.transcript.extend(b'\n>>> continue\n')
    console.child.stdin.write(b'continue\n'); console.child.stdin.flush()
    deadline = time.monotonic() + timeout
    while True:
        remaining = deadline - time.monotonic()
        assert remaining > 0 and console.selector.select(remaining), 'trap did not retire before deadline'
        chunk = os.read(console.child.stdout.fileno(), 65536)
        if not chunk:
            break
        console.transcript.extend(chunk)
    return console.child.wait(timeout=max(.01, deadline - time.monotonic()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'binary', 'wasm-tools', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--call-stack-policy', action='append', choices=('instruction', 'unwind'))
    parser.add_argument('--jit-policy', choices=('default', 'max'), required=True)
    parser.add_argument('--runner-prefix-json', type=Path)
    parser.add_argument('--case', action='append')
    parser.add_argument('--stop-timeout', type=int, default=60)
    args = parser.parse_args(); assert 1 <= args.stop_timeout <= 120
    args.out.mkdir(parents=True, exist_ok=False)
    subprocess.run(['bash', str(args.source_root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    # No process-sized abort dumps in the shared test folder.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    runner = json.loads(args.runner_prefix_json.read_text()) if args.runner_prefix_json else []
    assert isinstance(runner, list) and all(isinstance(v, str) and v for v in runner)
    image = args.binary.read_bytes()
    assert image[:7] == b'\x7fELF\x02\x01\x01', 'expected a qualified little-endian ELF64 VM'
    machine = int.from_bytes(image[18:20], 'little')
    # Clang lowers fast_io's __builtin_trap to UD2/EBREAK/BRK respectively.
    expected_signal = {62: signal.SIGILL, 243: signal.SIGILL, 183: signal.SIGTRAP}.get(machine)
    assert expected_signal is not None, ('unqualified fatal signal ABI', machine)
    cases = [case for case in dropped_elem_trap_examples() if args.case is None or case['name'] in args.case]
    assert cases and (args.case is None or {c['name'] for c in cases} == set(args.case))
    containers._cases = {case['name']: case for case in cases}
    results = []
    for case in cases:
        wat = args.out / (case['name'] + '.wat'); wasm = wat.with_suffix('.wasm')
        wat.write_text(case['wat'] + '\n')
        subprocess.run([str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
        subprocess.run([str(args.wasm_tools), 'validate', '--features', 'all', str(wasm)], check=True)
        sites, dump = containers.preview.markers(args.wasm_tools, wasm); wat.with_suffix('.dump').write_text(dump)
        function = case['expected'][0]['function']; assert len(sites[function]) == 2
        for policy in args.call_stack_policy or ('instruction', 'unwind'):
            console = None
            row = dict(case=case['name'], policy=policy, actual_VM=True, observations=[], actual_trap_stop=False)
            try:
                argv = [*runner, str(args.binary), '-Rdbg', '-Rct', '0', '-Rllvm-cache-path', 'disable',
                        '-Rllvm-call-stack', policy, '-Rllvm-policy', args.jit_policy,
                        *['-WFE-' + f for f in ('gc', 'reference-types', 'function-references', 'bulk-memory')],
                        '--run', str(wasm)]
                row['argv'] = argv
                console = containers.ContainerConsole(argv, args.out / (case['name'] + '-' + policy + '.log'))
                console.prompt()
                assert b'prepared; no Wasm instruction executed' in console.send('status')
                assert b'registered' in console.send(f'break 0 {function} {sites[function][0]}')
                console.send('continue'); stopped(console, args.stop_timeout)
                thread, actual_function, offset = console.location()
                assert (actual_function, offset) == (function, sites[function][0])
                observations = []; containers.inspect(console, thread, 0, case['expected'][0]['values'], observations)
                row['observations'].append(dict(kind='before-nop', function=function, offset=offset, frames=observations))
                reply = console.send(f'catch wasm gc 0 {function}')
                catch = re.search(rb'wasm-catchpoint ([0-9]+)', reply); assert catch, reply
                console.send('continue'); reply = stopped(console, args.stop_timeout)
                thread, actual_function, offset = console.location()
                assert (actual_function, offset) == (function, sites[function][0] + 1), reply
                assert b'wasm catchpoint' in reply.lower(), reply
                console.point = 0
                observations = []; containers.inspect(console, thread, 0, case['expected'][0]['values'], observations)
                row['observations'].append(dict(kind='before-trapping-opcode', function=function, offset=offset, frames=observations))
                assert row['observations'][1]['frames'][0]['stop_id'] > row['observations'][0]['frames'][0]['stop_id']
                assert b'error:' not in console.send(f'disable wasm-event {int(catch[1])}')
                exit_code = resume_to_fatal(console, args.stop_timeout)
                transcript = re.sub(rb'\x1b\[[0-9;]*m', b'', bytes(console.transcript))
                assert exit_code == -expected_signal, (exit_code, expected_signal, transcript[-4000:])
                assert case['diagnostic'].encode() in transcript, transcript[-4000:]
                assert b'guest exited: 0' not in transcript and b'Runtime invariant' not in transcript, transcript[-4000:]
                row.update(passed=True, expected_process_signal=signal.Signals(expected_signal).name, actual_process_exit=exit_code,
                           exact_preopcode_preview=True, runtime_diagnostic=case['diagnostic'])
            except Exception as error:
                row.update(passed=False, error=repr(error))
            finally:
                if console is not None:
                    if console.child.poll() is None:
                        try:
                            console.close()
                        except Exception as error:
                            row.update(passed=False, cleanup_error=repr(error))
                    else:
                        console.transcript.extend(console.child.stdout.read())
                        console.record_retirement()
                        console.selector.close(); console.log.write_bytes(console.transcript)
            results.append(row)
            (args.out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
            print(json.dumps({k: v for k, v in row.items() if k not in ('observations', 'argv')}), flush=True)
    (args.out / 'inputs.json').write_text(json.dumps(dict(binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(),
        harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), runner_prefix=runner,
        runner_sha256=hashlib.sha256(Path(runner[0]).read_bytes()).hexdigest() if runner else None,
        jit_policy=args.jit_policy, call_stack_policies=args.call_stack_policy or ['instruction', 'unwind'],
        target_elf_machine=machine, expected_process_signal=signal.Signals(expected_signal).name,
        cgroup=Path('/proc/self/cgroup').read_text()), indent=2) + '\n')
    raise SystemExit(0 if all(row['passed'] for row in results) else 1)


if __name__ == '__main__':
    main()
