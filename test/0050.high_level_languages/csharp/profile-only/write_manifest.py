#!/usr/bin/env python3
"""Write a hash-bound gated case after both reference runners pass it."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--references', type=Path, required=True)
    parser.add_argument('--iterations', type=int, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    report = json.loads(args.references.read_text())
    assert report['passed'] and 1 <= args.iterations <= 100000000
    rows = [r for r in report['runs'] if r['iterations'] == args.iterations]
    assert {r['runner'] for r in rows} == {'native-dotnet', 'wasmtime'} and all(r['passed'] for r in rows)
    assert len({r['checksum'] for r in rows}) == 1
    wasm = Path(report['wasm'])
    assert hashlib.sha256(wasm.read_bytes()).hexdigest() == report['wasm_sha256']
    checksum = rows[0]['checksum']
    semantic = ('csharp classes=42 arrays=55 strings=7 exceptions=3 iterations='
                + str(args.iterations) + ' checksum=' + str(checksum))
    case = {'name': 'csharp-mono-wasm-profile-' + str(args.iterations), 'language': 'csharp',
            'target': 'mono-wasm', 'backend': 'mono-interpreter', 'optimization': 'release',
            'rounds': args.iterations, 'wasm': str(wasm), 'wasm_sha256': report['wasm_sha256'],
            'expected_checksum': checksum, 'clock_kind': 'monotonic-ticks', 'mode': 'release',
            'purpose': 'hardware-gated', 'checks': [semantic], 'kernel_time': 'ticks',
            'kernel_clock': 'monotonic', 'args': [str(args.iterations)],
            'clock_line_prefix': 'csharp kernel iterations=' + str(args.iterations) + ' ticks=',
            'checksum_line': semantic, 'generated_source': str(Path(__file__).resolve().parent / 'ProgramProfile.cs'),
            'oracle': str(args.references.resolve()), 'gate_stream': 'stdout',
            'gate_ready': 'UWVM_PROFILE_READY', 'gate_done': 'UWVM_PROFILE_DONE',
            'gate_release': 'G', 'gate_finish': 'D', 'gate_ready_timeout': 900, 'kernel_timeout': 28}
    args.out.write_text(json.dumps([case], indent=2) + '\n')


if __name__ == '__main__':
    main()
