#!/usr/bin/env python3
"""Require actual T2 entry while checking ordinary scalar and SIMD FP bits."""
import argparse
import json
from pathlib import Path
import re
import subprocess
from ieee_oracle import IEEE
from numeric_cases import criterion
from run_matrix import digest, execute, save, verify


def fixture():
    f32, f64 = IEEE(32), IEEE(64)
    one32, one64 = f32.bias << f32.frac, f64.bias << f64.frac
    a, b, c = one32 + 1, one32 - 2, one32 | f32.sign
    rounded = f32.binary('add', f32.binary('mul', a, b), c)
    assert rounded != f32.pack(f32.finite(a) * f32.finite(b) + f32.finite(c))
    half = one64 + (1 << (f64.frac - 1))
    lanes_a = [1, one32 + 1, f32.sign, one32 | f32.sign]
    lanes_b = [one32, one32 - 2, one32, one32]
    lane_checks = ' '.join(
        f'local.get $v i32x4.extract_lane {i} '
        f'{criterion("i32", f32.binary("mul", x, y))} if unreachable end'
        for i, (x, y) in enumerate(zip(lanes_a, lanes_b)))
    return f'''(module
      (global $a (mut i32) (i32.const {a}))
      (global $b (mut i32) (i32.const {b}))
      (global $c (mut i32) (i32.const {c}))
      (global $d (mut i64) (i64.const {half}))
      (global $va (mut v128) (v128.const i32x4 {' '.join(map(str, lanes_a))}))
      (global $vb (mut v128) (v128.const i32x4 {' '.join(map(str, lanes_b))}))
      (func $unused)
      (func $hot (result i32) (local $v v128)
        global.get $a f32.reinterpret_i32 global.get $b f32.reinterpret_i32 f32.mul
        global.get $c f32.reinterpret_i32 f32.add i32.reinterpret_f32
        {criterion('i32', rounded)} if unreachable end
        global.get $d f64.reinterpret_i64 f64.nearest i64.reinterpret_f64
        {criterion('i64', f64.unary('nearest', half))} if unreachable end
        global.get $va global.get $vb f32x4.mul local.set $v {lane_checks}
        i32.const 42)
      (func (export "_start") (local $n i32)
        i32.const 4000000 local.set $n
        loop $warm call $hot i32.const 42 i32.ne if unreachable end
          local.get $n i32.const 1 i32.sub local.tee $n br_if $warm end))'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    parser.add_argument('output', type=Path)
    a = parser.parse_args()
    config = json.loads(a.config.read_text())
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    verify(config['pins'])
    assert not config.get('ros'), 'ROS has no tiered backend'
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    code_pins = {str(p.resolve()): digest(p) for p in Path(__file__).parent.glob('*.py')}
    wat, wasm = out / 'tiered-numeric.wat', out / 'tiered-numeric.wasm'
    high_memory = config.get('fixture_kind') == 'memory64-high'
    if high_memory:
        from tiered_high_memory import fixture as high_memory_fixture
        wat.write_text(high_memory_fixture() + '\n')
    elif config.get('fixture_kind') == 'simd-sign':
        from tiered_simd_sign import fixture as sign_fixture
        wat.write_text(sign_fixture() + '\n')
    else:
        wat.write_text(fixture() + '\n')
    for stage, argv in [
        ('parse', [config['wasm_tools'], 'parse', str(wat), '-o', str(wasm)]),
        ('validate', [config['wasm_tools'], 'validate', '--features', 'all', str(wasm)])]:
        row = execute(argv, out / (stage + '.log'), out)
        assert row['exit'] == 0, row
    rows = []
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')
    for mode in config.get('modes', ['lazy', 'lazy+verification']):
        for level in config.get('levels', [1, 2, 3]):
            label = f'{mode}-O{level}'
            work = out / label
            work.mkdir()
            prefix = list(config.get('prefix', []))
            guest_root = None
            if prefix:
                loader_root = Path(prefix[prefix.index('-L') + 1])
                guest_root = work / 'guest-root'
                guest_root.mkdir()
                for directory in ('lib', 'lib64', 'usr'):
                    target = loader_root / directory
                    if target.exists():
                        (guest_root / directory).symlink_to(target, target_is_directory=True)
                prefix[prefix.index('-L') + 1] = str(guest_root)
            compiler_log = work / 'compiler.log'
            argv = [*prefix, config['product'], '-Rcc', 'tiered', '-Rcm', mode,
                    '-Rct', '2', '-Rllvm-full-policy', f'pb-o{level}',
                    '-Rllvm-cache-path', 'disable', '-Rllvm-call-stack', 'instruction',
                    '-Rclog', 'file', str(compiler_log),
                    *(['-WFE-memory64', '-WFE-simd', '-WFE-threads'] if high_memory else
                      ['-WFE-simd'] if config.get('fixture_kind') == 'simd-sign' else []),
                    '--run', str(wasm)]
            row = execute(argv, work / 'run.log', work, config.get('timeout', 300))
            assert row['exit'] == 0, row
            if not compiler_log.exists() and guest_root is not None:
                compiler_log = guest_root / str(compiler_log).lstrip('/')
            log = ansi.sub('', compiler_log.read_text())
            entered = bool(re.search(r'tiered-full-enter .*?\bfn=1\b', log))
            ready = bool(re.search(r'tiered-full-ready .*?\bfunctions=3\b', log))
            counters = {key: max(map(int, re.findall(r'\b' + key + r'=(\d+)', log)), default=0)
                        for key in ['tiered_full_ready', 'tiered_full_failed', 'tiered_switches']}
            row.update(mode=mode, optimization=level, actual_t2_target_entry=entered,
                       full_module_ready=ready, counters=counters,
                       physical_compiler_log=str(compiler_log),
                       compiler_log_sha256=digest(compiler_log), checked_calls=4_000_000,
                       bit_checks_per_call=12 if config.get('fixture_kind') == 'simd-sign' else 6,
                       wasm_sha256=digest(wasm))
            rows.append(row)
            passed = entered and ready and counters['tiered_full_ready'] > 0 and counters['tiered_full_failed'] == 0
            save(out / 'checkpoint.json', dict(state='running' if passed else 'failed', rows=rows))
            assert passed, (label, row, log[-8000:])
            print('PASS actual T2', 'high memory64 entry' if high_memory else 'numeric entry', label, flush=True)
    verify(config['pins'])
    verify(code_pins)
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    save(out / 'summary.json', dict(passed=True, rows=rows, code_pins=code_pins,
        config_sha256=digest(a.config), product_sha256=digest(config['product']),
        fixture_kind=config.get('fixture_kind', 'numeric'),
        scope=('Actual T2 target entry; high memory64 integer/FP bits, SIMD, atomic old/new values and low alias guards; not complete tiered/Core3'
               if high_memory else 'Actual T2 target entry; ordinary f32 unfused arithmetic, f64 ties-even and f32x4 lanes; not complete tiered/Core 3 coverage')))


if __name__ == '__main__':
    main()
