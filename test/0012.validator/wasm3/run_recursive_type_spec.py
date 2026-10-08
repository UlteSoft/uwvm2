#!/usr/bin/env python3
"""Validate exact type-section payloads from pinned Core 3 recursive/GC tests.

This is a type-system test, not a claim to execute whole GC modules. Type-only
upstream modules retain their original assertion. For modules with other
sections, we compare the extracted original type bytes with both independent
validators and explicitly record that reduced scope. Run in the Linux cgroup.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

HASHES = {
    'type.wast': '72df248a2a7afe432a46aea636f96c8930a894bdc285f8fe365aa28736bc9f73',
    'type-rec.wast': '490fcb220f3dea47f5c41bd1971bd5f89a849071f34aeeb32c0a1bc7c85779a4',
    'type-canon.wast': '51506c9b325f4943c627b5bf42fbafcf30db33fe20922360ed4641932cbbbaf4',
    'type-equivalence.wast': 'b2dfe2e2c165b23e82092129be1f3141f3f11680f30bd0c4923aed06919f5cff',
    'gc/type-subtyping.wast': '6762aa8aa9d0ec4677970ee9e804b9c62f63bc5134b1dc024053bff6ca282798',
}


def u32(data, pos):
    value = 0
    for shift in range(0, 35, 7):
        if pos == len(data):
            raise ValueError('truncated section length')
        part = data[pos]
        pos += 1
        if shift == 28 and part > 15:
            raise ValueError('overflow section length')
        value |= (part & 127) << shift
        if part < 128:
            return value, pos
    raise ValueError('invalid length')


def type_section(data):
    if data[:8] != b'\0asm\x01\0\0\0':
        raise ValueError('not a core module')
    pos, payload, encoded = 8, None, None
    type_only = True
    while pos < len(data):
        begin = pos
        kind = data[pos]
        size, pos = u32(data, pos + 1)
        end = pos + size
        if end > len(data):
            raise ValueError('truncated section payload')
        if kind == 1:
            if payload is not None:
                raise ValueError('duplicate types')
            payload, encoded = data[pos:end], data[begin:end]
        elif kind != 0:
            type_only = False
        pos = end
    if payload is None:
        raise ValueError('no type section')
    return payload, b'\0asm\x01\0\0\0' + encoded, type_only


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--upstream', type=Path, required=True)
    parser.add_argument('--wasm-tools', required=True)
    parser.add_argument('--wasmtime', required=True)
    parser.add_argument('--validator', required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    subprocess.run(['bash', str(repo / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    args.out.mkdir(parents=True, exist_ok=True)
    rows, skipped = [], []
    for source, digest in HASHES.items():
        path = args.upstream / source
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, source
        target = args.out / source.replace('/', '_').removesuffix('.wast')
        target.mkdir(exist_ok=True)
        script = target / 'script.json'
        converted = subprocess.run([args.wasm_tools, 'json-from-wast', str(path), '--wasm-dir', str(target), '-o', str(script)], capture_output=True)
        (target / 'convert.log').write_bytes(converted.stderr)
        if converted.returncode and source == 'gc/type-subtyping.wast':
            # wasm-tools' text grammar refuses the official multiple-parent assertion before
            # emitting binary. Encode exactly that one reviewed three-type module by hand;
            # all other upstream text stays byte-for-byte unchanged, including line numbers.
            original_text = path.read_text()
            rejected = '''(module
    (type $parent1 (sub (struct)))
    (type $parent2 (sub (struct)))
    (type $child (sub $parent1 $parent2 (struct))))'''
            assert original_text.count(rejected) == 1
            payload = bytes.fromhex('03 50 00 5f 00 50 00 5f 00 50 02 00 01 5f 00')
            binary = b'\0asm\x01\0\0\0' + bytes([1, len(payload)]) + payload
            escaped = ''.join('\\' + f'{b:02x}' for b in binary)
            replacement = '(module binary "' + escaped + '")' + '\n' * rejected.count('\n')
            converted_path = target / 'multiple-parent-binary.wast'
            converted_path.write_text(original_text.replace(rejected, replacement))
            (target / 'text-encoder-workaround.json').write_text(json.dumps({
                'original_sha256': digest, 'reason': 'text encoder rejects multiple parents',
                'replacement_module_hex': binary.hex(), 'original_text': rejected}, indent=2) + '\n')
            converted = subprocess.run([args.wasm_tools, 'json-from-wast', str(converted_path), '--wasm-dir', str(target), '-o', str(script)], capture_output=True)
            (target / 'convert-binary-workaround.log').write_bytes(converted.stderr)
        assert converted.returncode == 0, converted.stderr.decode(errors='replace')
        for number, command in enumerate(json.loads(script.read_text())['commands']):
            if command['type'] not in ('module', 'assert_invalid', 'assert_malformed', 'assert_unlinkable', 'assert_uninstantiable'):
                continue
            filename = command.get('filename')
            if not filename or not filename.endswith('.wasm'):
                skipped.append({'source': source, 'number': number, 'reason': 'text-malformed or non-binary module'})
                continue
            original = target / Path(filename).name
            try:
                payload, reduced, type_only = type_section(original.read_bytes())
            except ValueError as error:
                skipped.append({'source': source, 'number': number, 'reason': str(error)})
                continue
            case = target / f'case-{number:04}'
            case.mkdir(exist_ok=True)
            data = case / 'type.payload'
            module = case / 'types.wasm'
            data.write_bytes(payload)
            module.write_bytes(reduced)
            commands = {
                'uwvm': [args.validator, str(data)],
                'wasm_tools': [args.wasm_tools, 'validate', str(module)],
                'wasmtime': [args.wasmtime, 'compile', '-C', 'cache=n', '-W', 'gc=y,function-references=y,exceptions=y',
                             str(module), '-o', str(case / 'oracle.cwasm')],
            }
            status = {}
            for name, cmd in commands.items():
                proc = subprocess.run(cmd, capture_output=True, timeout=60)
                (case / f'{name}.stdout').write_bytes(proc.stdout)
                (case / f'{name}.stderr').write_bytes(proc.stderr)
                status[name] = proc.returncode
                assert proc.returncode in ((0, 2, 3) if name == 'uwvm' else (0, 1)), (source, number, name, proc.returncode)
            valid = status['uwvm'] == 0
            row = {'source': source, 'command': number, 'line': command.get('line'), 'original_kind': command['type'],
                   'scope': 'original-type-only-assertion' if type_only else 'original-type-section-only',
                   'payload_sha256': hashlib.sha256(payload).hexdigest(), 'exit': status}
            rows.append(row)
            (args.out / 'results.json').write_text(json.dumps({'rows': rows, 'skipped': skipped}, indent=2) + '\n')
            assert all((code == 0) == valid for code in status.values()), row
            if type_only and command['type'] in ('module', 'assert_invalid', 'assert_malformed'):
                assert valid == (command['type'] == 'module'), row
    assert rows and any(r['original_kind'] == 'assert_invalid' and r['exit']['uwvm'] for r in rows)
    print(f'PASS recursive type spec: {len(rows)} exact type payloads, {sum(r["scope"] == "original-type-only-assertion" for r in rows)} original type-only assertions; {len(skipped)} explicitly skipped')


if __name__ == '__main__':
    main()
