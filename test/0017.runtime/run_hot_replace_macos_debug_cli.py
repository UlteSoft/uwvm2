#!/usr/bin/env python3
"""Exercise function-level LLVM-full hot replacement on native macOS ARM64."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time

from run_native_step_macos_debug_cli import Console, capped


WAT = '''(module
  (func $value (result i32) i32.const 51)
  (func (export "_start")
    call $value
    i32.const 52
    i32.ne
    if unreachable end))
'''


def resume_to_breakpoint(console: Console, transitions: list[str]) -> None:
    """Accept the debugger's asynchronous stopping state, then require the breakpoint."""
    deadline = time.monotonic() + 30
    reply = console.send('continue')
    for _ in range(100):
        transitions.append(reply.decode(errors='replace'))
        if b'stopped: breakpoint' in reply:
            return
        if b'guest exited:' in reply or b'debug domain closed' in reply:
            break
        if time.monotonic() >= deadline:
            break
        if b'pause requested; executions are not all stopped' in reply:
            time.sleep(0.01)
            reply = console.send('wait')
        elif b'running' in reply:
            reply = console.send('wait')
        elif b'stopped: pause' in reply or b'prepared; no Wasm instruction executed' in reply:
            reply = console.send('continue')
        else:
            break
    raise AssertionError(('breakpoint was not reached', transitions))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, default=shutil.which('wasm-tools'))
    parser.add_argument('--policy', choices=('instruction', 'unwind'), default='unwind')
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    if not args.wasm_tools:
        parser.error('wasm-tools is required')
    root = Path(__file__).resolve().parents[2]
    watchdog = root / 'test/0017.runtime/macos_rss_limit.py'
    binary = args.uwvm.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix='uwvm-macos-hot-replace-') as temporary:
        work = Path(temporary)
        wat = work / 'replace.wat'
        wasm = work / 'replace.wasm'
        wat.write_text(WAT)
        capped(watchdog, [str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)])
        capped(watchdog, [str(args.wasm_tools), 'validate', str(wasm)])
        bodies = {'malformed': b'\x00\xff\x0b',
                  'wrong_abi': b'\x00\x42\x01\x0b',
                  'good': b'\x00\x41\x34\x0b'}
        for name, body in bodies.items():
            (work / (name + '.bin')).write_bytes(body)
        # The wrong-ABI body is real, well-typed Wasm for an i64 result. The
        # target function has an i32 result, so importing that exact body must
        # fail the authoritative validator against the target's retained type.
        # The external body-only API cannot carry a new typeidx; its internal
        # abi_mismatch status protects registry consistency instead.
        alternate_wat = work / 'alternate-i64.wat'
        alternate_wasm = work / 'alternate-i64.wasm'
        target_wat = work / 'target-i32.wat'
        target_wasm = work / 'target-i32.wasm'
        alternate_wat.write_text('(module (func (result i64) i64.const 1))\n')
        target_wat.write_text('(module (func (result i32) i64.const 1))\n')
        for source, result in ((alternate_wat, alternate_wasm), (target_wat, target_wasm)):
            capped(watchdog, [str(args.wasm_tools), 'parse', str(source), '-o', str(result)])
        capped(watchdog, [str(args.wasm_tools), 'validate', str(alternate_wasm)])
        assert alternate_wasm.read_bytes().endswith(b'\x04' + bodies['wrong_abi'])
        mismatch_check = subprocess.run(
            [sys.executable, str(watchdog), '--', str(args.wasm_tools),
             'validate', str(target_wasm)], capture_output=True, timeout=180)
        mismatch_text = (mismatch_check.stdout + mismatch_check.stderr).decode(errors='replace')
        mismatch_peak = re.search(r'^PEAK_PROCESS_TREE_RSS_BYTES=(\d+)$', mismatch_text, re.M)
        assert (mismatch_check.returncode != 0 and mismatch_peak is not None and
                int(mismatch_peak[1]) <= 4 * 1024**3 and
                'type mismatch: expected i32, found i64' in mismatch_text), mismatch_text
        abi_probe = {
            'valid_alternate_wat_sha256': hashlib.sha256(alternate_wat.read_bytes()).hexdigest(),
            'valid_alternate_wasm_sha256': hashlib.sha256(alternate_wasm.read_bytes()).hexdigest(),
            'target_mismatch_wat_sha256': hashlib.sha256(target_wat.read_bytes()).hexdigest(),
            'target_mismatch_wasm_sha256': hashlib.sha256(target_wasm.read_bytes()).hexdigest(),
            'replacement_body_sha256': hashlib.sha256(bodies['wrong_abi']).hexdigest(),
            'target_validator_exit': mismatch_check.returncode,
            'target_validator_diagnostic': 'type mismatch: expected i32, found i64',
            'target_validator_peak_rss_bytes': int(mismatch_peak[1]),
        }
        console = Console(watchdog, binary, None, wasm, args.policy)
        responses = {}
        try:
            assert b'prepared; no Wasm instruction executed' in console.send('status')
            for name in ('malformed', 'wrong_abi'):
                response = console.send(f'replace 0 0 1 {work / (name + ".bin")}')
                responses[name] = response.decode(errors='replace')
                assert b'replacement body failed WebAssembly validation' in response, response
            response = console.send(f'replace 0 0 1 {work / "good.bin"}')
            responses['good'] = response.decode(errors='replace')
            assert b'function replaced; generation 2' in response, response
            response = console.send(f'replace 0 0 1 {work / "good.bin"}')
            responses['stale'] = response.decode(errors='replace')
            assert b'function generation changed' in response, response
            assert b'running' in console.send('continue')
            assert b'guest exited: 0' in console.until(b'guest exited: 0')
            peak, log = console.close()
        except BaseException:
            console.abort()
            raise
        active_wat = work / 'active.wat'
        active_wasm = work / 'active.wasm'
        active_wat.write_text(WAT.replace('i32.const 52', 'i32.const 51'))
        capped(watchdog, [str(args.wasm_tools), 'parse', str(active_wat),
                          '-o', str(active_wasm)])
        capped(watchdog, [str(args.wasm_tools), 'validate', str(active_wasm)])
        active = Console(watchdog, binary, None, active_wasm, args.policy)
        try:
            assert b'breakpoint' in active.send('break 0 0 0')
            transitions = []
            responses['active_transition'] = transitions
            resume_to_breakpoint(active, transitions)
            response = active.send(f'replace 0 0 1 {work / "good.bin"}')
            responses['active_frame'] = response.decode(errors='replace')
            assert b'active on a stopped Wasm stack' in response, response
            assert b'running' in active.send('continue')
            assert b'guest exited: 0' in active.until(b'guest exited: 0')
            active_peak, active_log = active.close()
        except BaseException:
            if args.out:
                args.out.mkdir(parents=True, exist_ok=True)
                (args.out / 'active-frame-failure.log').write_bytes(active.log)
                (args.out / 'active-transition-failure.json').write_text(
                    json.dumps({'commands': active.commands,
                                'transitions': responses.get('active_transition')}, indent=2) + '\n')
            active.abort()
            raise
        summary = {'product_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                   'call_stack_policy': args.policy,
                   'wasm_sha256': hashlib.sha256(wasm.read_bytes()).hexdigest(),
                   'peak_process_tree_rss_bytes': max(peak, active_peak),
                   'abi_probe': abi_probe,
                   'responses': responses}
        if args.out:
            args.out.mkdir(parents=True, exist_ok=True)
            for item in (alternate_wat, alternate_wasm, target_wat, target_wasm,
                         work / 'wrong_abi.bin'):
                shutil.copy2(item, args.out / item.name)
            (args.out / 'target-validator.log').write_text(mismatch_text)
            (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
            (args.out / 'console.log').write_bytes(log)
            (args.out / 'active-frame.log').write_bytes(active_log)
        print('PASS macOS LLVM-full hot replacement, ABI, stale generation, active-frame rejection')
    return 0


if __name__ == '__main__':
    sys.exit(main())
