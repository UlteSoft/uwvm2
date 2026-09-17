#!/usr/bin/env python3
"""Preprocess the real loader partition, including its real global fragment.

Named-module imports cannot define WASI feature macros. A missing local macro
provider can silently remove initialization and group validation, unlike a
missing type import which usually fails compilation. This is a preprocessing
regression, not a complete module compilation or WASI execution test.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', required=True)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    repo = args.source_root.resolve()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    directory = repo/'src/uwvm2/uwvm/run'
    source = directory/'loader.cppm'
    provider = 'uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h'
    original = source.read_text(encoding='utf-8')
    assert original.count(provider) == 1, 'loader must establish its own WASI feature macros'
    # Remove only the provider include, leaving the real surrounding guards
    # and implementation header intact. This reproduces the silent old bug.
    negative = out/'loader-no-feature-provider.cppm'
    negative.write_text(''.join(line for line in original.splitlines(keepends=True) if provider not in line))
    markers = ['validate_wasip1_group_binding_for_loaded_module',
               'validate_loaded_wasip1_group_bindings', 'need_wasip1_environment']
    rows = []
    for name, input_file, flags, enabled in [
            ('hosted', source, [], True),
            ('disabled', source, ['-DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1'], False),
            ('missing-provider-negative', negative, [], False)]:
        command = [args.cxx, '-std=c++2c', '-x', 'c++', '-E', '-P',
                   '-I'+str(repo/'src'), '-iquote'+str(directory), *flags, str(input_file)]
        result = subprocess.run(command, capture_output=True, timeout=30)
        (out/(name+'.stderr')).write_bytes(result.stderr)
        text = result.stdout.decode()
        found = {marker: marker in text for marker in markers}
        passed = result.returncode == 0 and all(value == enabled for value in found.values())
        rows.append(dict(name=name, command=command, returncode=result.returncode,
                         expected_wasi_body=enabled, markers=found, passed=passed,
                         preprocessed_sha256=hashlib.sha256(result.stdout).hexdigest()))
        print(name, 'PASS' if passed else 'FAIL', flush=True)
    report = dict(note=__doc__, source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  header_sha256=hashlib.sha256((directory/'loader.h').read_bytes()).hexdigest(), rows=rows)
    (out/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    raise SystemExit(not all(row['passed'] for row in rows))


if __name__ == '__main__':
    main()
