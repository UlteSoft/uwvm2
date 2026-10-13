#!/usr/bin/env python3
"""Compare the supported straight-line reference validator with Wasmtime on exact function bytes."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from run_recursive_type_spec import type_section, u32

CASES = {
    'call-typed-null': (True, '(type $c (func (result i32))) (func (result i32) ref.null $c call_ref $c)'),
    'call-generic-null': (False, '(type $c (func (result i32))) (func (result i32) ref.null func call_ref $c)'),
    'call-nofunc': (True, '(type $c (func (result i32))) (func (result i32) ref.null nofunc call_ref $c)'),
    'call-wrong-argument': (False, '(type $c (func (param i32))) (func f64.const 0 ref.null $c call_ref $c)'),
    'call-missing-argument': (False, '(type $c (func (param i32))) (func ref.null $c call_ref $c)'),
    'call-array': (False, '(type $c (array i32)) (func ref.null $c call_ref $c)'),
    'call-unknown-type': (False, '(func ref.null func call_ref 47)'),
    'call-covariance': (True, '(type $a (sub (func (result anyref)))) (type $b (sub $a (func (result eqref)))) (func (result anyref) ref.null $b call_ref $a)'),
    'call-declared-result': (False, '(type $a (sub (func (result anyref)))) (type $b (sub $a (func (result eqref)))) (func (result eqref) ref.null $b call_ref $a)'),
    'tail-correct-result': (True, '(type $c (func (result i32))) (func (result i32) ref.null $c return_call_ref $c)'),
    'tail-wrong-result': (False, '(type $c (func (result i32))) (func (result f64) ref.null $c return_call_ref $c)'),
    'nonnull-refinement': (True, '(func (param (ref null extern)) (result (ref extern)) local.get 0 ref.as_non_null)'),
    'nonnull-wrong-input': (False, '(func i32.const 0 ref.as_non_null drop)'),
    'nonnull-bottom-reference': (True, '(func (result (ref extern)) unreachable ref.as_non_null)'),
    'nonnull-bottom-numeric': (False, '(func (result i32) unreachable ref.as_non_null)'),
    'is-null-bottom': (True, '(func (result i32) unreachable ref.is_null)'),
    'is-null-numeric': (False, '(func (result i32) i32.const 0 ref.is_null)'),
    'typed-local-set': (True, '(func (param (ref extern)) (result (ref extern)) (local (ref extern)) local.get 0 local.set 1 local.get 1)'),
    'typed-local-unset': (False, '(func (result (ref extern)) (local (ref extern)) local.get 0)'),
    'typed-local-null-set': (False, '(func (param (ref null extern)) (result (ref extern)) (local (ref extern)) local.get 0 local.tee 1)'),
}


def body_and_signature(data):
    pos, signature, body = 8, None, None
    while pos < len(data):
        kind = data[pos]
        size, pos = u32(data, pos + 1)
        end = pos + size
        assert end <= len(data)
        if kind == 3:
            count, p = u32(data, pos)
            assert count == 1
            signature, p = u32(data, p)
            assert p == end
        elif kind == 10:
            count, p = u32(data, pos)
            assert count == 1
            size, p = u32(data, p)
            assert p + size == end
            body = data[p:end]
        pos = end
    assert signature is not None and body is not None
    return signature, body


def segment_metadata(data):
    def leb(n):
        out = bytearray()
        while n >= 128:
            out.append((n & 127) | 128)
            n >>= 7
        out.append(n)
        return out
    pos, data_count, elements = 8, 0, []
    while pos < len(data):
        kind = data[pos]
        size, start = u32(data, pos + 1)
        end = start + size
        assert end <= len(data)
        if kind == 12:
            data_count, p = u32(data, start)
            assert p == end
        elif kind == 9:
            count, p = u32(data, start)
            for _ in range(count):
                flags, p = u32(data, p)
                assert flags == 5, 'fixture accepts passive expression segments only'
                begin = p
                prefix = data[p]
                p += 1
                if prefix in (0x63, 0x64):
                    _, p = u32(data, p)
                else:
                    assert 0x69 <= prefix <= 0x74
                elements.append(data[begin:p])
                entries, p = u32(data, p)
                for _ in range(entries):
                    assert data[p] == 0xd0, 'fixture initializer must be ref.null'
                    _, p = u32(data, p + 1)
                    assert data[p] == 11
                    p += 1
            assert p == end
        pos = end
    return bytes(leb(data_count) + leb(len(elements)) + b''.join(elements))


def main(cases=CASES, label='reference instruction subset'):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--wasm-tools', required=True)
    p.add_argument('--wasmtime', required=True)
    p.add_argument('--checker', required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parents[3]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    a.out.mkdir(parents=True, exist_ok=True)
    rows = []
    for name, (valid, text) in cases.items():
        out = a.out / name
        out.mkdir(exist_ok=True)
        wat, wasm = out / 'input.wat', out / 'input.wasm'
        wat.write_text('(module ' + text + ')\n')
        parsed = subprocess.run([a.wasm_tools, 'parse', str(wat), '-o', str(wasm)], capture_output=True)
        (out / 'parse.log').write_bytes(parsed.stderr)
        assert parsed.returncode == 0, (name, parsed.stderr)
        data = wasm.read_bytes()
        types, _, _ = type_section(data)
        signature, body = body_and_signature(data)
        type_file, body_file = out / 'types.payload', out / 'function.body'
        type_file.write_bytes(types)
        body_file.write_bytes(body)
        metadata = out / 'segments.payload'
        metadata.write_bytes(segment_metadata(data))
        ours = subprocess.run([a.checker, str(type_file), str(signature), str(body_file), str(metadata)], capture_output=True, timeout=60)
        oracle = subprocess.run([a.wasmtime, 'compile', '-C', 'cache=n', '-W', 'gc=y,function-references=y,tail-call=y',
                                 str(wasm), '-o', str(out / 'oracle.cwasm')], capture_output=True, timeout=60)
        for artifact_label, proc in (('reference-pass', ours), ('wasmtime', oracle)):
            (out / f'{artifact_label}.stdout').write_bytes(proc.stdout)
            (out / f'{artifact_label}.stderr').write_bytes(proc.stderr)
        row = {'case': name, 'expected': int(not valid), 'reference_exit': ours.returncode, 'wasmtime_exit': oracle.returncode,
               'module_sha256': hashlib.sha256(data).hexdigest()}
        rows.append(row)
        (a.out / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
        assert ours.returncode == oracle.returncode == int(not valid), row
    print(f'PASS Core 3 {label}: {len(rows)} exact function bodies compared with Wasmtime')


if __name__ == '__main__':
    main()
