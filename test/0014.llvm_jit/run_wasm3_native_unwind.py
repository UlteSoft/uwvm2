#!/usr/bin/env python3
"""Require independent native unwinding of new Core 3 syntax, including cache replay.

Run the actual full-JIT CLI, optionally through a QEMU wrapper. Auto must select
native unwind on the tested target; an instruction fallback is a test failure.
Each cold compilation checks the emitted-frame policy, and fresh processes must
recover repeated recursive activations from the authenticated cached object.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


CASES = {
    'multi-memory-scalar': ('', 'i32.const 65535 i32.load $small drop', []),
    'multi-memory-simd': ('', 'i32.const 65530 v128.load $small drop', []),
    'extended-const-memory': (
        '(global $offset i32 (i32.add (i32.const 65530) (i32.const 5)))',
        'global.get $offset i32.load $small drop', ['extended-const']),
    'relaxed-simd-memory': (
        '', '''i32.const 65530
        v128.const f32x4 1 1 1 1 v128.const f32x4 2 2 2 2 v128.const f32x4 3 3 3 3
        f32x4.relaxed_madd f32x4.extract_lane 0 i32.trunc_f32_s i32.add i32.load $small drop''',
        ['relaxed-simd']),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wat2wasm', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--optimization', action='append', choices=('debug', 'pb-o3'))
    parser.add_argument('--cache', choices=('signed', 'disabled'), default='signed',
                        help='Use fresh compilation on targets with process-local address lowering; never count a miss as a replay')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')
    for case, (declaration, body, extra_features) in CASES.items():
        wat = args.output / (case + '.wat')
        wasm = wat.with_suffix('.wasm')
        wat.write_text('''(module (memory $unused 2) (memory $small 1)
          %s
          (func $recurse (param $n i32)
            local.get $n if
              local.get $n i32.const 1 i32.sub call $recurse return
            end %s)
          (func (export "_start") (param $trap i32)
            local.get $trap if i32.const 7 call $recurse end))
''' % (declaration, body))
        features = ['multi-memory', *extra_features]
        subprocess.run([str(args.wat2wasm), *['--enable-' + f for f in features],
                        str(wat), '-o', str(wasm)], check=True, timeout=60)
        for optimization in args.optimization or ('debug', 'pb-o3'):
            for policy in ('instruction', 'unwind', 'auto'):
                label = case + '-' + optimization + '-' + policy
                common = [str(args.uwvm.resolve())]
                common += ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
                common += ['-Rct', '0', '-Rllvm-full-policy', optimization,
                           '-Rllvm-call-stack', policy, '-Rllvm-cache-path']
                common += (['path', str((args.output / (label + '-cache')).resolve())]
                           if args.cache == 'signed' else ['disable'])
                common += ['-WFE-' + feature for feature in features]
                for phase, argument in (('cold', '0'), ('replay1', '1'), ('replay2', '1')):
                    compile_log = args.output / (label + '-' + phase + '.compile.log')
                    command = common + ['-Rclog', 'file', str(compile_log.resolve()),
                                        '-Wstart', '1', argument, '--run', str(wasm.resolve())]
                    run = subprocess.run(command, capture_output=True, timeout=120)
                    output = ansi.sub('', (run.stdout + run.stderr).decode(errors='replace'))
                    (args.output / (label + '-' + phase + '.log')).write_text(output)
                    compilation = compile_log.read_text() if compile_log.is_file() else ''
                    fields = dict(re.findall(r'\b(call_stack|call_stack_frames|unwind_check|unwind_replace_frames)=([^\s,;]+)', compilation))
                    stack = [int(n) for n in re.findall(r'\bfunc_idx=(\d+)', output)]
                    native = policy != 'instruction'
                    expected_policy = 'unwind' if native else 'instruction'
                    verified_hit = any('object-cache-hit ' in line and 'signature_verified=1' in line
                                       for line in compilation.splitlines())
                    generated_policy_ok = fields.get('call_stack') == expected_policy
                    generated_policy_ok &= fields.get('call_stack_frames') == ('omit' if native else 'emit')
                    if native:
                        generated_policy_ok &= fields.get('unwind_check') == 'live' and fields.get('unwind_replace_frames') == 'yes'
                    if phase == 'cold':
                        passed = run.returncode == 0 and generated_policy_ok
                    else:
                        passed = (run.returncode != 0 and stack == [0] * 8 + [1]
                                  and 'out of bounds' in output.lower())
                        passed &= verified_hit if args.cache == 'signed' else generated_policy_ok and not verified_hit
                    rows.append(dict(case=case, optimization=optimization, requested=policy, phase=phase,
                                     command=command, exit=run.returncode, fields=fields, stack=stack,
                                     cache_mode=args.cache, signature_verified_cache_hit=verified_hit, passed=bool(passed)))
                    (args.output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
                    if not passed:
                        raise RuntimeError(f'{label}/{phase}: native/frame/cache contract failed; see {args.output}')
                print(label, 'PASS generated policy and two recursive traps; cache=' + args.cache, flush=True)
    (args.output / 'metadata.json').write_text(json.dumps(dict(
        scope=__doc__, executable=str(args.uwvm.resolve()),
        executable_sha256=hashlib.sha256(args.uwvm.read_bytes()).hexdigest(),
        new_syntax_cases=list(CASES), cache_mode=args.cache, runs=len(rows)), indent=2) + '\n')


if __name__ == '__main__':
    main()
