#!/usr/bin/env python3
"""Run one complete real-PTY Wasm debugger workflow on a hash-bound producer.

This is the common Wasm layer. Source-language breakpoints and typed managed
values require separate producer-specific qualification. Invoke inside a bounded
process owner in the existing SSH Linux cgroup, with an immutable JSON case.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--repository', choices=('uwvm2', 'uwvm2-stable', 'uwvm2-ros'), required=True)
    parser.add_argument('--case', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cpu', type=int, default=21)
    parser.add_argument('--compile-threads', type=int, choices=range(8), default=0,
                        help='extra native compiler workers; zero minimizes concurrent compiler memory')
    args = parser.parse_args()
    sys.path[:0] = [str(Path(__file__).resolve().parent),
                    str(Path(__file__).resolve().parents[1] / '0017.runtime')]
    from build_products import admission
    from run_debug_run_cli import Console
    from run_matrix import debug_run

    before = admission()
    membership = Path('/proc/self/cgroup').read_text().strip()[3:]
    allowed = set()
    effective = (Path('/sys/fs/cgroup') / membership.lstrip('/') / 'cpuset.cpus.effective').read_text().strip()
    for item in effective.split(','):
        first, _, last = item.partition('-')
        allowed.update(range(int(first), int(last or first) + 1))
    if args.cpu not in allowed:
        raise ValueError('target CPU is outside the admitted cgroup')
    os.sched_setaffinity(0, {args.cpu})
    os.environ.update(TERM='dumb', NO_COLOR='1')
    binary = args.binary.resolve(strict=True)
    case = json.loads(args.case.read_bytes())
    wasm = Path(case['wasm']).resolve(strict=True)
    if sha(wasm) != case['wasm_sha256']:
        raise ValueError('original producer Wasm identity mismatch')
    if case.get('expected_exit', 0) != 0 or case.get('skip_debug', False):
        raise ValueError('select a positive command with an expected normal exit')
    # The exact continue payload must match. Legacy line-subsequence checks do
    # not qualify a new workflow. Preserve every raw PTY byte separately.
    producer_parallel_expectation = case.get('require_parallel_objects', False)
    case = dict(case, wasm=str(wasm), strict_debug_stdout=True,
                debug_quit_timeout=60, debug_compile_threads=args.compile_threads)
    # A producer manifest may carry a parallel-generation assertion from its
    # original cohort. This CLI explicitly selects a serial profile at zero
    # workers; that profile cannot certify parallel object generation.
    if args.compile_threads == 0:
        case['require_parallel_objects'] = False
    args.out.mkdir(parents=True, exist_ok=False)
    pins = {str(path): sha(path) for path in (binary, wasm, args.case, Path(__file__))}
    result = {'passed': False, 'scope': 'Wasm entry, state, backtrace, locals, single-step, breakpoint, restart, complete stdout and normal terminal restoration',
              'source_language_debugger_claim': False,
              'compile_profile': {
                  'extra_workers': args.compile_threads,
                  'producer_manifest_parallel_expectation': producer_parallel_expectation,
                  'effective_parallel_requirement': case.get('require_parallel_objects', False),
                  'scope': ('explicit serial native object generation; no parallel-generation credit'
                            if args.compile_threads == 0 else 'explicit extra compiler workers'),
              },
              'inputs_before': pins,
              'admission_before': before, 'case': case, 'repository': args.repository}
    try:
        result['debug'] = debug_run(Console, binary, args.repository, case, args.out)
        result['admission_after'] = admission()
        result['inputs_after'] = {path: sha(Path(path)) for path in pins}
        result['passed'] = (result['debug']['passed']
                            and result['inputs_before'] == result['inputs_after']
                            and before['membership'] == result['admission_after']['membership']
                            and before['memory_events'] == result['admission_after']['memory_events'])
    finally:
        (args.out / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'passed': result['passed'], 'report': str(args.out / 'report.json')}))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
