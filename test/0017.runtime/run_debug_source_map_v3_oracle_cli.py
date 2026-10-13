#!/usr/bin/env python3
"""Compare the bounded C++ map reader with original AssemblyScript sidecars.

The Python oracle uses arbitrary precision integers and independent JSON/base64
decoding. These are metadata tests; they do not qualify typed language values.
"""
from pathlib import Path
import argparse, hashlib, json, subprocess, sys


def leb(data, at):
    result = 0
    for index in range(5):
        assert at < len(data)
        byte = data[at]; at += 1
        result |= (byte & 127) << (7 * index)
        if byte < 128:
            assert result <= 0xffffffff
            return result, at
    raise AssertionError('overlong Wasm field')


def layout(path):
    data = path.read_bytes(); assert data[:8] == b'\0asm\1\0\0\0'
    at = 8; code = None; url = None
    while at < len(data):
        kind = data[at]; at += 1
        length, at = leb(data, at); end = at + length; assert end <= len(data)
        if kind == 10:
            assert code is None; code = (at, length)
        if kind == 0:
            size, start = leb(data, at); stop = start + size; assert stop <= end
            if data[start:stop] == b'sourceMappingURL':
                assert url is None
                size, start = leb(data, stop); assert start + size == end
                url = data[start:end].decode('utf-8')
        at = end
    assert code is not None and url is not None
    basename = url[2:] if url.startswith('./') else url
    assert Path(basename).name == basename and basename not in ('', '.', '..')
    return data, code, url


def original_rows(mapping, begin, size, file_size):
    alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
    columns = 0; source = line = column = name = 0
    pending = None; result = []
    assert ';' not in mapping['mappings']
    def close(end):
        if pending is not None:
            first = max(pending[0], begin); last = min(end, begin + size)
            if first < last:
                result.append([first - begin, last - begin, *pending[1:]])
    for segment in mapping['mappings'].split(','):
        assert segment
        fields = []; accumulator = shift = 0
        for char in segment:
            number = alphabet.index(char)
            accumulator += (number % 32) * (2 ** shift)
            if number < 32:
                fields.append(-(accumulator // 2) if accumulator % 2 else accumulator // 2)
                accumulator = shift = 0
            else:
                shift += 5
        assert shift == 0 and len(fields) in (1, 4, 5)
        assert fields[0] >= 0
        columns += fields[0]; assert 0 <= columns <= file_size
        close(columns)
        if len(fields) == 1:
            pending = None
        else:
            source += fields[1]; line += fields[2]; column += fields[3]
            assert 0 <= source < len(mapping['sources']) and line >= 0 and column >= 0
            if len(fields) == 5:
                name += fields[4]; assert 0 <= name < len(mapping['names'])
            pending = [columns, source, line + 1, column + 1]
    close(file_size)
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--program', type=Path, required=True)
    ap.add_argument('--wasm', type=Path, action='append', required=True)
    ap.add_argument('--out', type=Path, required=True)
    a = ap.parse_args(); assert sys.platform == 'linux'
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    a.out.mkdir(parents=True, exist_ok=False); records = []
    for wasm in a.wasm:
        data, (begin, size), url = layout(wasm); sidecar = wasm.parent / url
        original = json.loads(sidecar.read_text()); assert original['version'] == 3
        expected = original_rows(original, begin, size, len(data))
        paths = (a.program, wasm, sidecar, Path(__file__))
        sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
        before = {str(p): sha(p) for p in paths}
        completed = subprocess.run([str(a.program), str(wasm), str(begin), str(size), str(len(data)), url],
                                   capture_output=True, timeout=120)
        (a.out / (wasm.stem + '.log')).write_bytes(completed.stdout + completed.stderr)
        assert completed.returncode == 0, (wasm, completed.returncode, completed.stderr)
        lines = completed.stdout.decode().splitlines()
        assert lines[0] == f'sources {len(original["sources"])} rows {len(expected)}'
        actual = [[int(v) for v in line.split()] for line in lines[1:]]
        assert actual == expected, (wasm, 'C++ rows differ from original source-map oracle')
        after = {str(p): sha(p) for p in paths}; assert before == after
        records.append(dict(passed=True, module=str(wasm), code_file_begin=begin, code_size=size,
                            original_rows_compared=len(actual), inputs_before=before, inputs_after=after,
                            typed_language_runtime_PASS=False))
    (a.out / 'summary.json').write_text(json.dumps(dict(passed=True, records=records), indent=2) + '\n')
    print('PASS original AssemblyScript Source Map v3 rows against independent metadata oracle')


if __name__ == '__main__':
    main()
