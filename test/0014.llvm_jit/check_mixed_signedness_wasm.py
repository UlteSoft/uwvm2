#!/usr/bin/env python3
"""Require real VM conversion results, including authenticated full-JIT replay."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--full', type=Path, required=True)
    p.add_argument('--ros', type=Path, required=True)
    p.add_argument('--products', nargs='+', choices=['full', 'ros'], default=['full', 'ros'])
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--wat', type=Path, default=Path(__file__).parent/'wat/mixed_signedness_conversion.wat')
    a = p.parse_args()
    a.out.mkdir(exist_ok=False)
    out = a.out.resolve()
    wasm = out/'repro.wasm'
    subprocess.run(['wat2wasm', a.wat, '-o', wasm], check=True)
    rows = []
    for product, mode in [('full', 'full'), ('full', 'lazy'), ('ros', 'full')]:
        if product not in a.products:
            continue
        cli = getattr(a, product).resolve()
        for backend in ['int', 'jit']:
            policies = ['auto'] if backend == 'int' else (['auto', 'debug', 'light', 'balanced'] if mode == 'lazy' else
                                                        ['auto', 'debug', 'legacy-light', 'pb-o1', 'pb-o2', 'pb-o3'])
            for policy in policies:
                stem = f'{product}-{mode}-{backend}-{policy}'
                options = ['-Rcm', mode, '-Rcc', backend] if product == 'full' else ['-Rint' if backend == 'int' else '-Raot']
                cache_test = backend == 'jit' and mode == 'full'
                cache = ['-Rllvm-cache-path', 'path', str(out/(stem+'-cache'))] if cache_test else ['-Rllvm-cache-path', 'disable']
                command = [str(cli), *options, '-Rct', '0', '-Rclog', 'out', *cache]
                if backend == 'jit':
                    command += ['-Rllvm-'+mode+'-policy', policy]
                command += ['--run', str(wasm)]
                for phase in (['cold', 'replay'] if cache_test else ['execute']):
                    r = subprocess.run(command, capture_output=True, timeout=60)
                    log = r.stdout+r.stderr
                    (out/(stem+'-'+phase+'.log')).write_bytes(log)
                    plain = re.sub(rb'\x1b\[[0-9;]*[A-Za-z]', b'', log)
                    hit = any(b'object-cache-hit ' in line and b'signature_verified=1' in line for line in plain.splitlines())
                    passed = r.returncode == 0 and (phase != 'replay' or hit)
                    rows.append(dict(product=product, mode=mode, backend=backend, policy=policy, phase=phase,
                                     passed=passed, authenticated_hit=hit, returncode=r.returncode, command=command,
                                     binary_sha256=hashlib.sha256(cli.read_bytes()).hexdigest()))
                    (out/'results.json').write_text(json.dumps(rows, indent=2)+'\n')
                    if not passed:
                        raise RuntimeError((stem, phase, r.returncode, log[-1200:]))
    print(json.dumps(dict(runs=len(rows), assertions=8*len(rows), authenticated_replays=sum(r['phase']=='replay' for r in rows), failed=0)))


if __name__ == '__main__':
    main()
