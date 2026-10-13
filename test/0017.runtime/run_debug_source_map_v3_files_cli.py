#!/usr/bin/env python3
"""Real fast_io sidecar file boundaries, designated Linux cgroup only.

Use the qualified parser unit, the unchanged original producer Wasm and its
sidecar. Expected rejection does not qualify source-language runtime behavior.
"""
from pathlib import Path
import argparse, hashlib, json, os, subprocess, sys
import run_debug_source_map_v3_oracle_cli as oracle


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for key in ('program', 'wasm', 'out'):
        ap.add_argument('--' + key, type=Path, required=True)
    a = ap.parse_args(); assert sys.platform == 'linux'
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    a.out.mkdir(parents=True, exist_ok=False)
    data, (begin, size), url = oracle.layout(a.wasm)
    sidecar = a.wasm.parent / url
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    paths = (a.program, a.wasm, sidecar, Path(__file__), Path(oracle.__file__))
    before = {str(p): sha(p) for p in paths}
    records = []
    report = dict(passed=False, typed_language_runtime_PASS=False, inputs_before=before, records=records)
    try:
        payload = sidecar.read_bytes()
        cases = ('regular', 'regular-dot-prefix', 'symlink', 'fifo', 'directory',
                 'empty', 'oversize', 'invalid-json', 'parent-url', 'absolute-url')
        for name in cases:
            directory = a.out / name; directory.mkdir()
            module = directory / 'original.wasm'; module.write_bytes(data)
            target = directory / 'p.map'; request = 'p.map'
            if name in ('regular', 'regular-dot-prefix', 'parent-url', 'absolute-url'):
                target.write_bytes(payload)
                if name == 'regular-dot-prefix': request = './p.map'
                if name == 'parent-url': request = '../p.map'
                if name == 'absolute-url': request = str(target)
            elif name == 'symlink': target.symlink_to(sidecar.resolve())
            elif name == 'fifo': os.mkfifo(target, 0o600)
            elif name == 'directory': target.mkdir()
            elif name == 'empty': target.touch()
            elif name == 'oversize':
                with target.open('wb') as stream: stream.truncate(16 * 1024 * 1024 + 1)
            elif name == 'invalid-json': target.write_bytes(b'{"version":3,"sources":["p.ts"],"mappings":"oBAAA","version":3}')
            completed = subprocess.run([str(a.program), str(module), str(begin), str(size),
                                        str(len(data)), request], capture_output=True, timeout=5)
            accepted = name in ('regular', 'regular-dot-prefix')
            assert completed.returncode == 0 if accepted else completed.returncode in (-4, -6, 132, 134), (name, completed.returncode)
            if accepted:
                expected = oracle.original_rows(json.loads(payload), begin, size, len(data))
                lines = completed.stdout.decode().splitlines()
                assert [[int(v) for v in line.split()] for line in lines[1:]] == expected
            else:
                assert b'FAIL bounded source-map v3 line ' in completed.stderr, (name, completed.stderr)
                assert not completed.stdout
            (directory / 'result.log').write_bytes(completed.stdout + completed.stderr)
            assert module.read_bytes() == data
            records.append(dict(case=name, expected_acceptance=accepted, returncode=completed.returncode,
                                passed=True, original_wasm_unchanged=True))
        after = {str(p): sha(p) for p in paths}; report['inputs_after'] = after
        assert before == after
        subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
        report['passed'] = True
    except BaseException as error:
        report['error'] = repr(error); raise
    finally:
        (a.out / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS real sidecar regular/dot-prefix reads and bounded symlink/FIFO/file/URL rejection')


if __name__ == '__main__':
    main()
