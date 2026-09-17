#!/usr/bin/env python3
"""Instantiate a real strict interpreter fixture in 48 build-option profiles.

Use a successful header semantic compile's JSON command as the target/compiler
baseline. Combine none/soft/heavy/extra, delay-local none/soft/heavy, instruction
reordering off/on and loop unwinding off/on mirror xmake's macro mapping.
The existing f64 strict fixture instantiates both tail and non-tail translators.
This is syntax/template instantiation only: no objects, links, VM execution,
performance measurement or alternate-ISA/named-module validation is implied.
"""
import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import signal
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--compile-command-json', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--timeout', type=int, default=600)
    args = parser.parse_args()
    repo = args.source_root.resolve()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    record = json.loads(args.compile_command_json.read_text())
    assert record['returncode'] == 0 and '-fsyntax-only' in record['command']
    baseline = record['command'][:-1]
    assert not any(arg.startswith('-fmodule-file=') for arg in baseline)
    command = []
    index = 0
    while index < len(baseline):
        arg = baseline[index]
        if arg.startswith('-DUWVM_ENABLE_UWVM_INT_') or arg == '-DUWVM_USE_LLVM_JIT':
            index += 1
            continue
        if arg == '-isystem' and any(part in baseline[index+1] for part in ('/bundled-llvm/', '/third-parties/llvm/')):
            index += 2
            continue
        command.append(arg)
        index += 1
    command.append('-DUWVM_DISABLE_JIT')
    command.append('-DUWVM_TEST=2')
    # Match xmake's existing test-only policy. This fixture includes CLI
    # parameter declarations but not every callback definition. Do not relax
    # production warnings or blanket-disable diagnostics to compile the matrix.
    if 'clang' in Path(command[0]).name:
        command.extend(['-Wno-error=undefined-inline', '-Wno-undefined-inline'])
    source = repo/'test/0013.uwvm_int/strict/extra_heavy/uwvm_int_translate_extra_heavy_f64_sub_2localget_strict.cc'
    rows = []
    cases = itertools.product(('none', 'soft', 'heavy', 'extra'), ('none', 'soft', 'heavy'), (False, True), (False, True))
    for combine, delay, reorder, unwind in cases:
        name = f'{combine}-{delay}-reorder{int(reorder)}-unwind{int(unwind)}'
        defines = []
        if combine != 'none':
            defines.append('COMBINE_OPS')
        if combine in ('heavy', 'extra'):
            defines.append('HEAVY_COMBINE_OPS')
        if combine == 'extra':
            defines.append('EXTRA_HEAVY_COMBINE_OPS')
        if delay != 'none':
            defines.append('DELAY_LOCAL_SOFT')
        if delay == 'heavy':
            defines.append('DELAY_LOCAL_HEAVY')
        if reorder:
            defines.append('INSTRUCTION_REORDER')
        if unwind:
            defines.append('LOOP_UNWIND')
        invocation = command+['-DUWVM_ENABLE_UWVM_INT_'+name for name in defines]+[str(source)]
        started = time.monotonic()
        process = subprocess.Popen(invocation, cwd=repo, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   start_new_session=os.name == 'posix')
        timed_out = False
        try:
            stdout, stderr = process.communicate(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            # The Linux campaign's resource cap is shared by all profiles.
            # Never leave a compiler child behind after a timeout.
            if os.name == 'posix':
                os.killpg(process.pid, signal.SIGKILL)
            else:
                process.kill()
            stdout, stderr = process.communicate()
        returncode = 124 if timed_out else process.returncode
        (out/(name+'.log')).write_bytes(stdout+stderr)
        rows.append(dict(name=name, command=invocation, returncode=returncode, timed_out=timed_out,
                         elapsed_seconds=time.monotonic()-started, passed=returncode == 0))
        report = dict(note=__doc__, expected_profiles=48, completed_profiles=len(rows), rows=rows,
            compiler_baseline_sha256=hashlib.sha256(args.compile_command_json.read_bytes()).hexdigest(),
            fixture_sha256=hashlib.sha256(source.read_bytes()).hexdigest())
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n')
        print(name, 'PASS' if returncode == 0 else 'FAIL', flush=True)
        if returncode:
            print(stderr.decode(errors='replace')[-8000:], flush=True)
            raise SystemExit(returncode)


if __name__ == '__main__':
    main()
