#!/usr/bin/env python3
"""Read a real callee operand under call_indirect and call_ref in -Rdbg.

Use fixtures/debug_wasm_dynamic_observation.wat compiled by wasm-tools. The
default observation profile must preserve the waiting caller without acquiring
executable checkpoint/restore authority. Run inside the Linux test cgroup.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
from run_debug_source_inline_metadata_cli import sha
from run_debug_source_tinygo_cli import TinyGoConsole


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--uwvm', type=Path, required=True)
    p.add_argument('--wasm', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--ros', action='store_true')
    a = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    if sys.platform != 'linux':
        raise RuntimeError('execute in the controlled Linux cgroup')
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    out = a.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    binary, wasm = a.uwvm.resolve(strict=True), a.wasm.resolve(strict=True)
    prefix = [str(binary), '-Rdbg'] + ([] if a.ros else ['-Rcc', 'jit', '-Rcm', 'full'])
    prefix += ['--wasm-feature-enable-function-references', '-Rct', '0', '-Rllvm-call-stack', 'instruction', '-Rllvm-cache-path', 'disable', '--run', str(wasm)]
    command = [sys.executable, '-c', 'import os,sys,time;time.sleep(.15);os.execvpe(sys.argv[1],sys.argv[1:],os.environ)', *prefix]
    record = {'binary_sha256': sha(binary), 'wasm_sha256': sha(wasm), 'actions': [], 'passed': False}
    console = None
    try:
        console = TinyGoConsole(command, out / 'console.log')
        def ask(command):
            reply = console.send(command).decode(errors='replace')
            record['actions'].append({'command': command, 'reply': reply})
            return reply
        if 'breakpoint ' not in ask('break 0 0 2'):
            raise AssertionError('real child instruction safe point missing')
        for kind in ('call_indirect', 'call_ref'):
            ask('continue')
            for _ in range(32):
                stopped = ask('wait')
                if 'stopped: breakpoint' in stopped:
                    break
                if 'guest exited:' in stopped:
                    raise AssertionError('guest exited before both dynamic calls')
            else:
                raise AssertionError('actual dynamic child stop missing')
            if not re.search(r'thread 1 module=0 function=0 byte-offset=2\b', stopped):
                raise AssertionError(('wrong child location', stopped))
            reply = ask('operands 1')
            if not re.search(r'\bi32 = 35(?:\r?\n|$)', reply) or 'unavailable' in reply:
                raise AssertionError((kind, 'actual child operand missing', reply))
            if reply != ask('operands 1'):
                raise AssertionError('same-stop operand changed')
        record['passed'] = True
    except BaseException as error:
        record['error'] = repr(error)
        raise
    finally:
        if console is not None:
            try:
                console.finish()
                record['quit_returncode'] = console.child.returncode
            except BaseException as error:
                record['passed'] = False
                record['close_error'] = repr(error)
                raise
        (out / 'summary.json').write_text(json.dumps(record, indent=2))


if __name__ == '__main__':
    main()
