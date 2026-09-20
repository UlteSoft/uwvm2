#!/usr/bin/env python3
"""Check the real CRT entry's network-service platform selection.

Preprocessing uses UWVM_MODULE to avoid host SDK includes while examining the
real function bodies. Windows/Cygwin/Wine rows are macro-selection checks only,
not Windows compilation or execution. A full module build verifies native use.
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
    source = repo/'src/uwvm2/uwvm/crtmain/uwvm.h'
    # The concrete RAII type must remain exported by the actual bundled module.
    exports = repo/'third-parties/fast_io/share/fast_io/fast_io_inc/host/win32.inc'
    assert 'using ::fast_io::win32_wsa_service;' in exports.read_text()
    rows = []
    for name, defines, wanted, entry in [
        ('posix', [], 0, 'uwvm_main_non_winnt'),
        ('winnt', ['_WIN32'], 1, 'uwvm_main_winnt'),
        ('win9x', ['_WIN32', '_WIN32_WINDOWS'], 1, 'uwvm_main_non_winnt'),
        ('cygwin', ['_WIN32', '__CYGWIN__'], 0, 'uwvm_main_non_winnt'),
        ('wine', ['_WIN32', '__WINE__'], 0, 'uwvm_main_winnt'),
        ('wasi', ['__wasi__'], 0, 'uwvm_main_non_winnt'),
    ]:
        command = [args.cxx, '-std=c++2c', '-x', 'c++', '-E', '-P', '-DUWVM_MODULE',
                   '-U_WIN32', '-U_WIN32_WINDOWS', '-U__CYGWIN__', '-U__WINE__', '-U__wasi__',
                   *('-D'+define for define in defines), str(source)]
        result = subprocess.run(command, capture_output=True, timeout=30)
        (out/(name+'.log')).write_bytes(result.stderr)
        body = result.stdout.decode()
        service = '::fast_io::win32_wsa_service service{};'
        passed = (result.returncode == 0 and body.count(service) == wanted and
                  '::fast_io::net_service' not in body and entry in body)
        # On Windows, construction must still precede the call into Wasm and
        # remain an automatic object in the same entry scope (RAII cleanup).
        if wanted:
            tail = body[body.index(service):]
            passed = passed and 'return uwvm_uz_u8main(argc_uz, argv_u8);' in tail
        rows.append(dict(name=name, command=command, returncode=result.returncode,
                         service_count=body.count(service), passed=passed,
                         preprocessed_sha256=hashlib.sha256(result.stdout).hexdigest()))
        print(name, 'PASS' if passed else 'FAIL', flush=True)
    (out/'results.json').write_text(json.dumps(dict(note=__doc__, rows=rows,
        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        exports_sha256=hashlib.sha256(exports.read_bytes()).hexdigest()), indent=2)+'\n')
    raise SystemExit(not all(row['passed'] for row in rows))


if __name__ == '__main__':
    main()
