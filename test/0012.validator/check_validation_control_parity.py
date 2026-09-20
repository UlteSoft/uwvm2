#!/usr/bin/env python3
"""Cross-check bottom typing against Core 1/2 and actual full/lazy execution.

Every invalid function is called: Core 2 section 7.2.2 permits deferred validation
of cold functions, but requires the entire body to validate before any execution.
The direct backend probe additionally bypasses the shared standard prepass.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import subprocess


# name: (body, result type, minimum profile, outcome). All locals are numeric so
# the same fixture can test strict MVP without importing later proposal syntax.
CASES = {
    'if-dead-drop': ('i32.const 0 if unreachable else unreachable end drop', '', 1, 'invalid'),
    'if-br-drop': ('i32.const 0 if br 0 else br 0 end drop', '', 1, 'invalid'),
    'loop-dead-drop': ('loop unreachable end drop', '', 1, 'invalid'),
    'loop-br-drop': ('loop br 0 end drop', '', 1, 'invalid'),
    'if-dead-missing-result': ('i32.const 0 if unreachable else unreachable end', 'i32', 1, 'invalid'),
    'parent-dead-child-drop': ('unreachable block drop end', '', 1, 'invalid'),
    'parent-dead-after-child-drop': ('unreachable block end drop', '', 1, 'trap'),
    'if-dead-valid': ('i32.const 0 if unreachable else unreachable end', '', 1, 'trap'),
    'loop-dead-valid': ('loop unreachable end', '', 1, 'trap'),
    'if-br-value': ('i32.const 0 if (result i32) i32.const 7 br 0 else i32.const 8 br 0 end drop', '', 1, 'ok'),
    'select-extra': ('unreachable select', '', 1, 'invalid'),
    'select-extra-with-cond': ('unreachable i32.const 0 select', '', 1, 'invalid'),
    'select-drop': ('unreachable select drop', '', 1, 'trap'),
    'select-result': ('unreachable select', 'f64', 1, 'trap'),
    'select-chain': ('unreachable select select f64.neg drop', '', 1, 'trap'),
    'select-meet-known': ('unreachable select f64.const 0 i32.const 0 select f64.neg drop', '', 1, 'trap'),
    'select-meet-refines': ('unreachable select f64.const 0 i32.const 0 select i64.eqz drop', '', 1, 'invalid'),
    'select-cond': ('unreachable select if nop end', '', 1, 'trap'),
    'select-memory': ('unreachable select i64.load drop', '', 1, 'trap'),
    'select-store': ('unreachable select select i64.store', '', 1, 'trap'),
    'select-tee': ('unreachable select local.tee 0 i64.eqz drop', '', 1, 'trap'),
    'select-tee-refines': ('unreachable select local.tee 0 f64.neg drop', '', 1, 'invalid'),
    'select-brif-refines': ('block (result i64) unreachable select i32.const 0 br_if 0 f64.neg drop end drop', '', 1, 'invalid'),
    'select-brif-valid': ('block (result i64) unreachable select i32.const 0 br_if 0 i64.eqz drop end drop', '', 1, 'trap'),
    'select-global': ('unreachable select global.set 0', '', 1, 'trap'),
    'select-call': ('unreachable select call $sink', '', 1, 'trap'),
    'select-end': ('block (result f64) unreachable select end drop', '', 1, 'trap'),
    'select-else': ('i32.const 0 if (result f64) unreachable select else f64.const 1 end drop', '', 1, 'ok'),
    'partial-end-wrong': ('block (result i32 f64) unreachable i64.const 0 end drop drop', '', 2, 'invalid'),
    'partial-else-wrong': ('i32.const 0 if (result i32 f64) unreachable i64.const 0 else i32.const 0 f64.const 0 end drop drop', '', 2, 'invalid'),
    'partial-end-valid': ('block (result i32 f64) unreachable f64.const 0 end drop drop', '', 2, 'trap'),
    'select-ref-is-null': ('unreachable select ref.is_null drop', '', 2, 'trap'),
    'select-typed-ref': ('unreachable select select i32.const 0 select (result externref) ref.is_null drop', '', 2, 'trap'),
    'select-known-ref': ('unreachable ref.null extern i32.const 0 select drop', '', 2, 'invalid'),
    'select-vector': ('unreachable select i32x4.abs drop', '', 2, 'trap'),
    # Core 1 requires identical labels; Core 2 permits a shared Unknown argument.
    'br-table-bottom': ('block (result f64) block (result f32) unreachable select i32.const 0 br_table 0 1 end drop f64.const 0 end drop', '', 1, 'versioned'),
}

# Binary-only grammar regressions; a WAT frontend cannot express these malformed
# encodings. All functions start with unreachable, so an incremental lazy decoder
# must reject the later defect before executing even that first instruction.
for name, body, minimum, outcome in [
    ('typed-select-empty', '00 1c 00 0b', 2, 'invalid'),
    ('typed-select-many', '00 1c 02 7f 7f 1a 1a 0b', 2, 'invalid'),
    ('typed-select-padded-count', '00 1c 81 00 7f 1a 0b', 2, 'trap'),
    ('typed-select-bad-type', '00 1c 01 7a 1a 0b', 2, 'invalid'),
    ('blocktype-padded-empty', '00 02 c0 7f 0b 0b', 1, 'invalid'),
    ('blocktype-six-byte-index', '00 02 80 80 80 80 80 00 0b 0b', 1, 'invalid'),
    ('i32-overflow', '00 41 80 80 80 80 10 1a 0b', 1, 'invalid'),
    ('local-index-overflow', '00 20 ff ff ff ff 1f 1a 0b', 1, 'invalid'),
    ('simd-lane-out-of-range', '00 fd 1b 04 1a 0b', 2, 'invalid'),
    ('simd-prefix-overlong', '00 fd 80 80 80 80 80 0b', 2, 'invalid'),
    ('illegal-primary', '00 ff 0b', 1, 'invalid'),
]:
    CASES[name] = (bytes.fromhex(body), '', minimum, outcome)


def uint(value):
    data = bytearray()
    while value >= 128:
        data.append((value & 127) | 128)
        value >>= 7
    data.append(value)
    return bytes(data)


def binary_fixture(body):
    def section(kind, contents):
        return bytes([kind]) + uint(len(contents)) + contents
    function = b'\x00' + body  # empty locals vector
    return (b'\x00asm\x01\x00\x00\x00' + section(1, b'\x01\x60\x00\x00') +
            section(3, b'\x01\x00') + section(7, b'\x01\x06_start\x00\x00') +
            section(10, b'\x01' + uint(len(function)) + function))


def execute(command, **kwargs):
    return subprocess.run(list(map(str, command)), capture_output=True, text=True, timeout=30, **kwargs)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--full-cli', type=Path, required=True)
    p.add_argument('--ros-cli', type=Path)
    p.add_argument('--full-backend-probe', type=Path)
    p.add_argument('--ros-backend-probe', type=Path)
    p.add_argument('--full-standard-probe', type=Path)
    p.add_argument('--ros-standard-probe', type=Path)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--wat2wasm', default='wat2wasm')
    p.add_argument('--wasm-validate', default='wasm-validate')
    p.add_argument('--jobs', type=int, default=4)
    p.add_argument('--bottom-matrix', action='store_true',
                   help='Also test 490 deterministic Unknown/concrete select-consumer combinations')
    p.add_argument('--skip-execution', action='store_true', help='Run only the explicit validator probes')
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=False)
    if a.bottom_matrix:
        operands = ['select', 'i32.const 0', 'i64.const 0', 'f32.const 0',
                    'f64.const 0', 'v128.const i32x4 0 0 0 0', 'ref.null extern']
        consumers = ['drop', 'i32.eqz drop', 'i64.eqz drop', 'f32.neg drop',
                     'f64.neg drop', 'i32x4.abs drop', 'ref.is_null drop',
                     'local.tee 0 drop', 'global.set 0', 'call $sink']
        for left, x in enumerate(operands):
            for right, y in enumerate(operands):
                for consumer, suffix in enumerate(consumers):
                    CASES[f'matrix-{left}-{right}-{consumer}'] = (
                        f'unreachable {x} {y} i32.const 0 select {suffix}', '', 2, 'oracle')
    paths, oracle_disagreements = {}, []
    for name, (body, result, _, outcome) in CASES.items():
        source, binary = a.out/(name+'.wat'), a.out/(name+'.wasm')
        if isinstance(body, bytes):
            binary.write_bytes(binary_fixture(body))
            source.with_suffix('.hex').write_text(body.hex(' ')+'\n')
        else:
            wat = ('(module (memory 1) (global (mut f64) (f64.const 0)) '
               '(func $sink (param f64)) (func $f ' + (f'(result {result}) ' if result else '') +
               '(local i64) ' + body + ') (func (export "_start") call $f ' + ('drop' if result else '') + '))')
            source.write_text(wat)
            converted = execute([a.wat2wasm, '--no-check', source, '-o', binary])
            if converted.returncode:
                raise RuntimeError(converted.stderr)
        oracle = execute([a.wasm_validate, binary])
        if oracle.returncode < 0:
            raise RuntimeError(f'Oracle crashed for {name}')
        if outcome == 'oracle':
            # The generator deliberately includes ill-typed intermediate selects;
            # derive validity independently instead of assuming a local stack model.
            CASES[name] = (body, result, 2, 'trap' if oracle.returncode == 0 else 'invalid')
        elif name == 'blocktype-padded-empty' and oracle.returncode == 0:
            # WABT 1.0.36 accepts this non-minimal negative s33, but Core 2
            # section 5.4.1 permits only literal 0x40, a one-byte valtype, or a
            # NON-NEGATIVE s33 type index. Keep the spec oracle and report the
            # external tool disagreement rather than accepting its permissiveness.
            oracle_disagreements.append(dict(case=name, tool=a.wasm_validate,
                expected='invalid', actual='valid', basis='Core 2 section 5.4.1 blocktype grammar'))
        elif (oracle.returncode == 0) != (outcome != 'invalid'):
            raise RuntimeError(f'Bad fixture {name}: {oracle.stderr}')
        paths[name] = binary.resolve()
    jobs = []
    for product, cli in [('full', a.full_cli), ('ros', a.ros_cli)]:
        if cli is None:
            continue
        for backend in ('int', 'jit'):
            for mode in (('full', 'lazy') if product == 'full' else ('full',)):
                for profile in ('mvp', 'wasm1p1', 'wasm2'):
                    for name, (_, _, minimum, outcome) in CASES.items():
                        if minimum == 2 and profile == 'mvp':
                            continue
                        expected = ('invalid' if profile == 'mvp' else 'trap') if outcome == 'versioned' else outcome
                        jobs.append((product, cli, backend, mode, profile, name, expected))
    def run(job):
        product, cli, backend, mode, profile, name, expected = job
        options = ['-Rcm', mode, '-Rcc', backend] if product == 'full' else ['-Rint' if backend == 'int' else '-Raot']
        feature = 'wasmmvp' if profile == 'mvp' else profile
        command = [cli.resolve(), *options, '-Rct', '0', '-Rllvm-cache-path', 'disable', '--wasm-feature-'+feature, '--run', paths[name]]
        label = '-'.join((product, backend, mode, profile, name))
        try:
            result = execute(command)
            log = result.stdout + result.stderr
            validation = 'validation error in webassembly code' in log.lower()
            trap = 'unreachable' in log.lower() and ('backtrace' in log.lower() or 'runtime' in log.lower())
            passed = (validation and result.returncode != 0 if expected == 'invalid' else
                      not validation and result.returncode == 0 if expected == 'ok' else
                      not validation and result.returncode != 0 and trap)
            record = dict(test=label, expected=expected, exit=result.returncode, validation=validation, passed=passed)
        except subprocess.TimeoutExpired as e:
            log = str(e)
            record = dict(test=label, expected=expected, timeout=True, passed=False)
        (a.out/(label+'.log')).write_text(log)
        return record
    if a.skip_execution:
        if not any((a.full_backend_probe, a.ros_backend_probe, a.full_standard_probe, a.ros_standard_probe)):
            p.error('--skip-execution requires at least one probe')
        jobs.clear()
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        records = list(pool.map(run, jobs))
    for product, probe in [('full', a.full_backend_probe), ('ros', a.ros_backend_probe)]:
        if probe is None:
            continue
        for profile in ('mvp', 'wasm1p1', 'wasm2'):
            names = [n for n, c in CASES.items() if profile != 'mvp' or c[2] == 1]
            result = execute([probe.resolve(), profile], input=''.join(str(paths[n])+'\n' for n in names))
            (a.out/(product+'-'+profile+'-backend.stderr')).write_text(result.stderr)
            (a.out/(product+'-'+profile+'-backend.stdout')).write_text(result.stdout)
            lines = result.stdout.splitlines()
            if result.returncode or len(lines) != len(names):
                records.append(dict(test=product+'-'+profile+'-backend-process', passed=False, exit=result.returncode, lines=len(lines)))
                continue
            for name, line in zip(names, lines):
                rejected = CASES[name][3] == 'invalid' or (CASES[name][3] == 'versioned' and profile == 'mvp')
                codes = list(map(int, line.split()))
                records.append(dict(test=product+'-'+profile+'-backend-'+name, codes=codes,
                                    passed=len(codes) == 3 and all((c != 0) == rejected for c in codes)))
    for product, probe in [('full', a.full_standard_probe), ('ros', a.ros_standard_probe)]:
        if probe is None:
            continue
        for api, profile in [('wasm1','mvp'), ('wasm1p1','mvp'), ('runtime','mvp'),
                             ('wasm1p1','wasm1p1'), ('runtime','wasm1p1'),
                             ('wasm1p1','wasm2'), ('wasm2','wasm2'), ('runtime','wasm2')]:
            names = [n for n, c in CASES.items() if profile != 'mvp' or c[2] == 1]
            result = execute([probe.resolve(), api, profile], input=''.join(str(paths[n])+'\n' for n in names))
            lines = result.stdout.splitlines()
            if result.returncode or len(lines) != len(names):
                raise RuntimeError(f'{product}/{api}/{profile}: standard probe failed')
            for name, line in zip(names, lines):
                rejected = CASES[name][3] == 'invalid' or (CASES[name][3] == 'versioned' and profile == 'mvp')
                records.append(dict(test=product+'-'+api+'-'+profile+'-standard-'+name, actual=line,
                                    passed=(line == 'ok 0 0') != rejected and not line.startswith('io ')))
    failures = [r for r in records if not r['passed']]
    hashes = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in
              (a.full_cli, a.ros_cli, a.full_backend_probe, a.ros_backend_probe,
               a.full_standard_probe, a.ros_standard_probe) if path}
    (a.out/'results.json').write_text(json.dumps(dict(records=records, failures=failures,
        binary_sha256=hashes, oracle_disagreements=oracle_disagreements), indent=2)+'\n')
    print(json.dumps(dict(checks=len(records), failures=len(failures))))
    for failure in failures:
        print(json.dumps(failure))
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
