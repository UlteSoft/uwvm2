#!/usr/bin/env python3
"""Cross-function Core 3 throw/try_table execution against Wasmtime.

Every caught fixture executes a call across a Wasm function boundary. These
cases cannot pass using the earlier same-function throw-to-branch optimization.
Only numeric payloads and catch/catch_all are qualified here: exception
references and packed reference payload codecs have separate requirements.
Run only inside the remote Linux test cgroup. --wasmtime without --uwvm permits
reference qualification while a new runtime binary is being compiled. LLVM
profiles are explicitly selected; the default remains the established interpreter
qualification. See exception_cross_cli.md for policy and scope details.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


CHECK = 'i32.const 42 i32.ne if unreachable end'


def mixed_tuple_case(groups, arguments_from_caller=True):
    fields = []
    for i in range(groups):
        fields.extend([
            ('i32', f'i32.const {1000+i}', f'i32.const {1000+i} i32.ne'),
            ('i64', f'i64.const {9223372036854775000+i}', f'i64.const {9223372036854775000+i} i64.ne'),
            ('f32', f'f32.const nan:0x{0x1000+i:x}', f'i32.reinterpret_f32 i32.const {0x7f801000+i} i32.ne'),
            ('f64', f'f64.const nan:0x{0x2000+i:x}', f'i64.reinterpret_f64 i64.const {0x7ff0000000002000+i} i64.ne'),
            ('v128', f'v128.const i32x4 {i} {i+1} {2000+i} {i+3}',
             f'v128.const i32x4 {i} {i+1} {2000+i} {i+3} i32x4.eq i32x4.all_true i32.eqz'),
        ])
    types = ' '.join(field[0] for field in fields)
    arguments = ' '.join(field[1] for field in fields)
    checks = ' '.join(field[2] + ' if unreachable end' for field in reversed(fields))
    locals_ = ' '.join(f'local.get {i}' for i in range(len(fields)))
    parameters = f'(param {types})' if arguments_from_caller else ''
    leaf_values = locals_ if arguments_from_caller else arguments
    caller_values = arguments if arguments_from_caller else ''
    return f'''(module
      (tag $t (param {types}))
      (func $leaf {parameters} {leaf_values} throw $t)
      (func (export "_start")
        i32.const 42 i64.const -991 f32.const -0 f64.const -0
        block $out (result {types})
          try_table (catch $t $out)
            f64.const -99 {caller_values} call $leaf drop
          end unreachable
        end
        {checks}
        i64.reinterpret_f64 i64.const 0x8000000000000000 i64.ne if unreachable end
        i32.reinterpret_f32 i32.const 0x80000000 i32.ne if unreachable end
        i64.const -991 i64.ne if unreachable end {CHECK}))'''


def fixtures():
    # name, WAT, expectation, optional provider import. Trap expectations are
    # (Wasmtime substring, UWVM substring); "uncaught" is a distinct fatal class.
    yield ('caller-prefix', f'''(module
      (tag $t (param i32))
      (func $leaf (param i32) local.get 0 throw $t)
      (func (export "_start") i32.const 42
        block $out (result i32) try_table (catch $t $out)
          f64.const -99 i32.const 42 call $leaf drop
        end unreachable end {CHECK} {CHECK}))''', None, False)
    yield ('void-cached-prefix', f'''(module
      (tag $t) (func $leaf throw $t)
      (func (export "_start") i32.const 42 i64.const -991
        block $out try_table (catch $t $out) call $leaf end unreachable end
        i64.const -991 i64.ne if unreachable end {CHECK}))''', None, False)
    yield ('mixed-tuple', mixed_tuple_case(1), None, False)
    # The zero-argument/zero-result callee creates its 680-byte payload. No
    # ordinary caller edge ever needs this much storage; only the exceptional
    # forward label accounts for it in the caller's maximum operand-frame size.
    yield ('large-eh-only-tuple', mixed_tuple_case(17, False), None, False)
    yield ('function-label', f'''(module
      (tag $t (param i32)) (func $leaf i32.const 42 throw $t)
      (func $caller (result i32) try_table (catch $t 0) call $leaf end unreachable)
      (func (export "_start") call $caller {CHECK}))''', None, False)
    yield ('normal-and-exception-results', f'''(module
      (tag $t (param i32 i64))
      (func $leaf (param i32) (result i32 i64)
        local.get 0 if i32.const 42 i64.const 991 throw $t end
        i32.const 42 i64.const 991)
      (func $caller (param i32)
        i32.const 42 block $out (result i32 i64)
          try_table (result i32 i64) (catch $t $out) local.get 0 call $leaf end
        end i64.const 991 i64.ne if unreachable end {CHECK} {CHECK})
      (func (export "_start") i32.const 0 call $caller i32.const 1 call $caller))''', None, False)
    yield ('normal-void-call', f'''(module
      (tag $t) (global $g (mut i32) (i32.const 0))
      (func $leaf i32.const 42 global.set $g)
      (func (export "_start") i32.const 42 block $wrong
        try_table (catch_all $wrong) call $leaf global.get $g {CHECK} return end
        unreachable end unreachable))''', None, False)
    yield ('scalar-call-fusion', f'''(module
      (tag $t (param i32))
      (func $leaf (param i32 i32 i32) (result i32)
        local.get 2 if local.get 0 local.get 1 i32.add throw $t end
        local.get 0 local.get 1 i32.add)
      (func $caller (param i32) (local i32) i32.const 42
        block $out (result i32) try_table (result i32) (catch $t $out)
          i32.const 17 i32.const 25 local.get 0 call $leaf local.tee 1
        end end {CHECK} {CHECK})
      (func (export "_start") i32.const 0 call $caller i32.const 1 call $caller))''', None, False)
    yield ('indirect-parameters', f'''(module
      (type $sig (func (param i32 i64))) (tag $t (param i32 i64))
      (table 1 funcref) (elem (i32.const 0) $leaf)
      (func $leaf (type $sig) local.get 0 local.get 1 throw $t)
      (func (export "_start") i32.const 42 f64.const -0
        block $out (result i32 i64) try_table (catch $t $out)
          f64.const 991 i32.const 42 i64.const -991 i32.const 0 call_indirect (type $sig) drop
        end unreachable end i64.const -991 i64.ne if unreachable end {CHECK}
        i64.reinterpret_f64 i64.const 0x8000000000000000 i64.ne if unreachable end {CHECK}))''', None, False)
    yield ('nearest-handler', f'''(module
      (tag $t (param i32)) (func $leaf i32.const 41 throw $t)
      (func (export "_start") block $outer (result i32)
        try_table (catch $t $outer) block $inner (result i32)
          try_table (catch $t $inner) call $leaf end unreachable
        end i32.const 1 i32.add br $outer end unreachable end {CHECK}))''', None, False)
    yield ('ordered-first-tag', f'''(module
      (tag $t (param i32)) (func $leaf i32.const 41 throw $t)
      (func (export "_start") block $outer (result i32) block $inner (result i32)
        try_table (catch $t $inner) (catch $t $outer) call $leaf end unreachable
        end i32.const 1 i32.add end {CHECK}))''', None, False)
    yield ('ordered-catch-all', f'''(module
      (tag $t) (func $leaf throw $t)
      (func (export "_start") (local i32)
        block $outer block $inner
          try_table (catch_all $inner) (catch $t $outer) call $leaf end unreachable
        end i32.const 42 local.set 0 end local.get 0 {CHECK}))''', None, False)
    yield ('middle-tag-mismatch', f'''(module
      (tag $a (param i32)) (tag $b (param i32)) (tag $c (param i32))
      (func $leaf i32.const 42 throw $a)
      (func $middle block $wrong (result i32)
        try_table (catch $b $wrong) call $leaf end unreachable end drop unreachable)
      (func $outer block $wrong (result i32)
        try_table (catch $c $wrong) call $middle end unreachable end drop unreachable)
      (func (export "_start") block $out (result i32)
        try_table (catch $a $out) call $outer end unreachable end {CHECK}))''', None, False)
    yield ('same-frame-tag-mismatch', f'''(module
      (tag $a (param i32)) (tag $b (param i32)) (func $leaf i32.const 42 throw $a)
      (func (export "_start") block $outer (result i32) try_table (catch $a $outer)
        block $wrong (result i32) try_table (catch $b $wrong) call $leaf end unreachable
        end drop unreachable end unreachable end {CHECK}))''', None, False)
    yield ('catch-all-discards-tuple', f'''(module
      (tag $t (param i32 i64 f64 v128))
      (func $leaf i32.const 91 i64.const 92 f64.const 93 v128.const i32x4 1 2 3 4 throw $t)
      (func (export "_start") i32.const 42 block $out
        try_table (catch_all $out) f64.const 999 call $leaf drop end unreachable end {CHECK}))''', None, False)
    yield ('loop-parameters', f'''(module
      (tag $t (param i32)) (func $leaf (param i32) local.get 0 throw $t)
      (func (export "_start") (local i32) i32.const 42 i32.const 19
        loop $again (param i32) (result i32) local.tee 0 if
          try_table (catch $t $again) local.get 0 i32.const 1 i32.sub call $leaf end
        end i32.const 42 end {CHECK} {CHECK}))''', None, False)
    yield ('loop-catch-all', f'''(module
      (tag $t (param i32)) (func $leaf (param i32) local.get 0 throw $t)
      (func (export "_start") (local i32) i32.const 42 i32.const 23 local.set 0
        loop $again try_table (catch_all $again) local.get 0 if
          local.get 0 i32.const 1 i32.sub local.tee 0 call $leaf
        end end end local.get 0 if unreachable end {CHECK}))''', None, False)
    yield ('loop-mixed-parameters', f'''(module
      (tag $t (param i32 i64 f32 f64 v128))
      (func $leaf (param i32 i64 f32 f64 v128)
        local.get 0 local.get 1 local.get 2 local.get 3 local.get 4 throw $t)
      (func (export "_start")
        (local $n i32) (local $wide i64) (local $f f32) (local $d f64) (local $v v128)
        i32.const 42 i32.const 17 i64.const -991 f32.const -0 f64.const nan:0x1234
        v128.const i32x4 1 2 3 4
        loop $again (param i32 i64 f32 f64 v128) (result i32)
          local.set $v local.set $d local.set $f local.set $wide local.set $n
          local.get $wide i64.const -991 i64.ne if unreachable end
          local.get $f i32.reinterpret_f32 i32.const 0x80000000 i32.ne if unreachable end
          local.get $d i64.reinterpret_f64 i64.const 0x7ff0000000001234 i64.ne if unreachable end
          local.get $v v128.const i32x4 1 2 3 4 i32x4.eq i32x4.all_true if else unreachable end
          local.get $n if try_table (catch $t $again)
            local.get $n i32.const 1 i32.sub local.get $wide local.get $f local.get $d local.get $v call $leaf
          end end i32.const 42
        end {CHECK} {CHECK}))''', None, False)
    yield ('exited-handler', f'''(module
      (tag $t (param i32)) (func $noop) (func $leaf i32.const 42 throw $t)
      (func (export "_start") block $outer (result i32) try_table (catch $t $outer)
        block $inner (result i32) try_table (catch $t $inner)
          call $noop i32.const 99 br $inner end unreachable end drop
        call $leaf end unreachable end {CHECK}))''', None, False)
    for indirect in (False, True):
        tail = 'i32.const 0 return_call_indirect (type $sig)' if indirect else 'return_call $leaf'
        yield ('tail-bypass-' + ('indirect' if indirect else 'direct'), f'''(module
          (type $sig (func (result i32))) (tag $t (param i32))
          (table 1 funcref) (elem (i32.const 0) $leaf)
          (func $leaf (type $sig) i32.const 42 throw $t)
          (func $retired (result i32) block $wrong (result i32)
            try_table (result i32) (catch $t $wrong) {tail} end
            end drop i32.const 13)
          (func (export "_start") block $out (result i32)
            try_table (catch $t $out) call $retired drop end unreachable end {CHECK}))''', None, False)
    yield ('scratch-frame-repeat', f'''(module
      (tag $t (param i32))
      (func $leaf (param i32) (local {' '.join(['i64'] * 600)})
        local.get 600 i64.eqz if else unreachable end
        i64.const 99 local.set 600 local.get 0 throw $t)
      (func $middle (param i32) (local {' '.join(['i64'] * 600)})
        local.get 600 i64.eqz if else unreachable end
        i64.const 77 local.set 600 local.get 0 call $leaf)
      (func (export "_start") (local i32) i32.const 1000 local.set 0
        loop $again i32.const 42 block $out (result i32)
          try_table (catch $t $out) local.get 0 call $middle end unreachable
        end local.get 0 i32.ne if unreachable end {CHECK}
        local.get 0 i32.const 1 i32.sub local.tee 0 br_if $again end))''', None, False)
    yield ('import-tag-alias', f'''(module
      (import "p" "raise" (func $leaf (param i32)))
      (import "p" "t" (tag $a (param i32))) (import "p" "t" (tag $b (param i32)))
      (tag $local (param i32))
      (func (export "_start") block $out (result i32) try_table (catch $b $out)
        block $wrong (result i32) try_table (catch $local $wrong)
          i32.const 42 call $leaf end unreachable end drop unreachable
        end unreachable end {CHECK}))''', None, True)
    yield ('trap-unreachable', '''(module (func $leaf unreachable)
      (func (export "_start") block $out try_table (catch_all $out) call $leaf end end))''',
           ('unreachable', 'unreachable'), False)
    yield ('trap-memory', '''(module (memory 1) (func $leaf i32.const 65536 i32.load drop)
      (func (export "_start") block $out try_table (catch_all $out) call $leaf end end))''',
           ('out of bounds memory access', 'out of bounds'), False)
    for name, selector, reference, diagnostic in (
        ('null', 1, 'uninitialized element', 'uninitialized element'),
        ('type', 0, 'indirect call type mismatch', 'signature mismatch'),
        ('bounds', 2, 'out of bounds table access', 'table index out of bounds'),
    ):
        yield ('trap-indirect-' + name, f'''(module
          (type $sig (func)) (table 2 funcref) (elem (i32.const 0) $wrong)
          (func $wrong (param i32))
          (func (export "_start") block $out try_table (catch_all $out)
            i32.const {selector} call_indirect (type $sig) end end))''',
               (reference, diagnostic), False)
    yield ('uncaught-direct', '''(module (tag $t (param i32))
      (func $leaf i32.const 42 throw $t) (func $middle call $leaf)
      (func (export "_start") call $middle))''', 'uncaught', False)
    yield ('uncaught-mixed-payload', '''(module
      (tag $t (param i32 i64 f32 f64 v128))
      (func $leaf i32.const -7 i64.const -11 f32.const nan:0x200001 f64.const -0
        v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 throw $t)
      (func (export "_start") call $leaf))''', 'uncaught', False)
    yield ('uncaught-mismatch', '''(module (tag $a) (tag $b)
      (func $leaf throw $a)
      (func (export "_start") block $out try_table (catch $b $out) call $leaf end end))''', 'uncaught', False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path)
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--backend', choices=['int', 'llvm'], default='int')
    parser.add_argument('--trace', choices=['instruction', 'unwind', 'none'], action='append',
                        help='LLVM call-stack policy; repeat explicitly to compare policies (default: instruction)')
    parser.add_argument('--case', action='append')
    parser.add_argument('--mode', choices=['full', 'lazy', 'lazy+verification'], action='append')
    parser.add_argument('--force-color', action='store_true',
                        help='Require actual ANSI colors and preserve raw uncaught output')
    parser.add_argument('--all-combine-delay', action='store_true',
                        help='Run disable/soft/heavy/extra, each with local delay enabled and disabled')
    args = parser.parse_args()
    if not args.wasmtime and not args.uwvm:
        parser.error('at least one execution engine is required')
    modes = args.mode or (['full'] if args.ros or args.backend == 'llvm' else ['full', 'lazy', 'lazy+verification'])
    if args.ros and modes != ['full']:
        parser.error('ROS only supports full mode')
    if args.backend == 'int' and args.trace:
        parser.error('--trace selects LLVM call-stack policy and requires --backend llvm')
    if args.backend == 'llvm' and args.all_combine_delay:
        parser.error('--all-combine-delay applies only to --backend int')
    trace_policies = (args.trace or ['instruction']) if args.backend == 'llvm' else [None]
    selected = list(fixtures())
    if args.case:
        if not set(args.case) <= {row[0] for row in selected}:
            parser.error('unknown fixture')
        selected = [row for row in selected if row[0] in args.case]
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out / 'runner.py').write_bytes(Path(__file__).read_bytes())
    rows = []
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')

    def run(label, command, expectation=None, reference=False, trace_policy=None):
        result = subprocess.run([str(part) for part in command], capture_output=True, timeout=90)
        raw = result.stdout + result.stderr
        if expectation == 'uncaught' and not reference:
            (args.out / (label + '.raw.log')).write_bytes(raw)
        log = ansi.sub('', raw.decode(errors='replace'))
        (args.out / (label + '.log')).write_text(log)
        if expectation == 'uncaught':
            if reference:
                passed = result.returncode != 0 and 'thrown Wasm exception' in log and 'wasm trap:' not in log
            else:
                passed = result.returncode != 0 and 'uwvm: [fatal] Uncaught WebAssembly exception' in log
                if args.force_color:
                    passed = passed and b'\x1b[' in raw and b'\x1b[0m' in raw
                expected_payload = ([
                    'payload[0] i32 bits=0xfffffff9 signed=-7',
                    'payload[1] i64 bits=0xfffffffffffffff5 signed=-11',
                    'payload[2] f32 bits=0x7fa00001',
                    'payload[3] f64 bits=0x8000000000000000',
                    'payload[4] v128 storage_bytes=[00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f]',
                ] if label.startswith('uncaught-mixed-payload-') else
                    ['payload[0] i32 bits=0x0000002a signed=42'] if label.startswith('uncaught-direct-') else [])
                actual_payload = re.findall(r'(?m)^uwvm: \[info\]\s+(payload\[.*)', log)
                passed = passed and actual_payload == expected_payload
                passed = passed and f'payload_fields={len(expected_payload)}' in log
                if not (args.backend == 'llvm' and trace_policy == 'none'):
                    passed = passed and 'instruction/source locations unavailable' in log
                actual_stack = [int(index) for index in re.findall(r'(?m)^uwvm: \[info\]\s+#\d+ .*?func_idx=(\d+)', log)]
                if args.backend == 'llvm' and trace_policy == 'none':
                    # None intentionally records no JIT diagnostic frames. Require an honest
                    # unavailable report, never fabricated frames or a generic nonzero exit.
                    passed = passed and 'Wasm call stack at throw unavailable (no snapshot captured).' in log
                    passed = passed and not actual_stack and 'Wasm call stack captured at throw' not in log
                else:
                    expected_stack = [0, 1, 2] if label.startswith('uncaught-direct-') else [0, 1]
                    passed = passed and 'Wasm call stack captured at throw' in log and actual_stack == expected_stack
                    passed = passed and 'truncated' not in log.lower()
        elif expectation:
            diagnostic = expectation[0 if reference else 1]
            passed = result.returncode != 0 and diagnostic.lower() in log.lower()
            # A catchable guest exception must never replace a native trap.
            passed = passed and 'Uncaught WebAssembly exception' not in log and 'thrown Wasm exception' not in log
            passed = passed and ('wasm trap:' in log if reference else 'uwvm: [fatal] Runtime crash (' in log)
        else:
            passed = result.returncode == 0
        rows.append(dict(label=label, command=[str(part) for part in command], exit=result.returncode,
                         expectation=expectation, passed=passed, trace_policy=trace_policy))
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if not passed:
            raise RuntimeError(f'{label}: exit={result.returncode}, expected={expectation!r}\n{log}')

    def encode(name, source):
        path = args.out / (name + '.wat')
        path.write_text(source + '\n')
        wasm = path.with_suffix('.wasm')
        run(name + '-assemble', [args.wasm_tools, 'parse', path, '-o', wasm])
        run(name + '-validate', [args.wasm_tools, 'validate', wasm])
        return wasm

    provider = encode('provider', '''(module (tag $t (export "t") (param i32))
      (func (export "raise") (param i32) local.get 0 throw $t))''')
    combinations = [('default', [])]
    if args.all_combine_delay:
        combinations = [(level + ('-no-delay' if no_delay else '-delay'),
                         ['-Rint-op-conbine-level', level] + (['-Rint-no-delay-local'] if no_delay else []))
                        for level in ['disable', 'soft', 'heavy', 'extra'] for no_delay in [False, True]]
    for name, source, expectation, preload in selected:
        wasm = encode(name, source)
        if args.wasmtime:
            command = [args.wasmtime, '-C', 'cache=n', '-W', 'exceptions=y', '-W', 'tail-call=y']
            if preload:
                command += ['--preload', 'p=' + str(provider)]
            run(name + '-wasmtime', command + [wasm], expectation, True)
        if args.uwvm:
            for mode in modes:
                for combination, flags in combinations:
                    for trace_policy in trace_policies:
                        if args.backend == 'llvm':
                            base = ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', mode]
                            base += ['-Rllvm-call-stack', trace_policy, '-Rllvm-cache-path', 'disable']
                        else:
                            base = ['-Rint'] if args.ros else ['-Rcc', 'int', '-Rcm', mode]
                        command = [args.uwvm] + base + flags + ['-WFE-exceptions', '-WFE-tail-call', '-Rct', '0']
                        if preload:
                            command += ['-Wpre', provider, 'p']
                        if args.force_color:
                            command += ['--log-color', 'enable']
                        label = name + '-' + mode + '-' + combination
                        if args.backend == 'llvm':
                            label += '-llvm-' + trace_policy
                        run(label, command + ['--run', wasm], expectation, trace_policy=trace_policy)
        print('PASS', name, flush=True)
    hashes = {}
    for name in ['wasm_tools', 'wasmtime', 'uwvm']:
        path = getattr(args, name)
        if path:
            hashes[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    (args.out / 'summary.json').write_text(json.dumps(dict(passed=True, cases=len(selected), runs=len(rows),
        interpreter_modes=modes if args.uwvm and args.backend == 'int' else [],
        backend=args.backend, modes=modes if args.uwvm else [], force_color=args.force_color,
        call_stack_policies=trace_policies if args.uwvm and args.backend == 'llvm' else [],
        combinations=[name for name, _ in combinations],
        sha256=hashes), indent=2) + '\n')
    print('PASS cross-function numeric exceptions:', len(selected), 'cases,', len(rows), 'commands')


if __name__ == '__main__':
    main()
