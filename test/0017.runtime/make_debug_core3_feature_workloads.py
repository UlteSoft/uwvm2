#!/usr/bin/env python3
"""Author one independent, timed input per Core 3 feature or important variant.

Writing WAT is not execution. --compile requires the actual Linux test cgroup;
neither wasm-tools nor an engine is launched on the author's workstation.
The feature inventory follows the specification's Release 3.0 change history.
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

SPEC = "https://webassembly.github.io/spec/core/appendix/changes.html#release-3-0"
PRELUDE = r'''
 (import "wasi_snapshot_preview1" "clock_time_get" (func $clock (param i32 i64 i32) (result i32)))
 (import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
 (memory $io (export "memory") 2)
 (data (i32.const 64) "Kernel ns: ")
 (data (i32.const 80) "Checksum: ")
 (data (i32.const 96) "\0a")
 (func $text (param $p i32) (param $n i32)
  (i32.store (i32.const 0) (local.get $p))
  (i32.store (i32.const 4) (local.get $n))
  (call $write (i32.const 1) (i32.const 0) (i32.const 1) (i32.const 8))
  if unreachable end)
 (func $number (param $v i64) (local $p i32)
  (local.set $p (i32.const 512))
  (loop $digits
   (local.set $p (i32.sub (local.get $p) (i32.const 1)))
   (i32.store8 (local.get $p) (i32.add (i32.wrap_i64 (i64.rem_u (local.get $v) (i64.const 10))) (i32.const 48)))
   (local.set $v (i64.div_u (local.get $v) (i64.const 10)))
   (br_if $digits (i64.ne (local.get $v) (i64.const 0))))
  (call $text (local.get $p) (i32.sub (i32.const 512) (local.get $p))))
 (func $probe_cold (export "probe_cold") (result i32) i32.const 123)
'''
EPILOGUE = r'''
 (func $start (export "_start") (local $answer i64) (local $elapsed i64) (local $remaining i32)
  i32.const 1 i64.const 1 i32.const 1024 call $clock if unreachable end
  global.get $kernel_repeats local.set $remaining
  (loop $repeat_kernel
   global.get $iterations call $kernel local.get $answer i64.add local.set $answer
   local.get $remaining i32.const 1 i32.sub local.tee $remaining br_if $repeat_kernel)
  i32.const 1 i64.const 1 i32.const 1032 call $clock if unreachable end
  i32.const 1032 i64.load i32.const 1024 i64.load i64.sub local.set $elapsed
  i32.const 64 i32.const 11 call $text local.get $elapsed call $number
  i32.const 96 i32.const 1 call $text
  i32.const 80 i32.const 10 call $text local.get $answer call $number
  i32.const 96 i32.const 1 call $text)
'''


def loop(body, *, locals="", init="", result="local.get $sum i64.extend_i32_u"):
    return f'''(func $kernel (export "kernel") (param $n i32) (result i64)
      (local $i i32) (local $sum i32) {locals}
      {init}
      (loop $again {body}
       local.get $i i32.const 1 i32.add local.tee $i
       local.get $n i32.lt_u br_if $again)
      {result})'''


ADD = "local.get $sum local.get $value i32.add local.set $sum"


def definitions():
    # Oracle formulas are scalar arithmetic independent of engine results.
    yield "memory64", "64-bit address space", ["memory64", "multi-memory"], "i64.load", "sum17", "", \
        '(memory $wide i64 2)', loop('''
        local.get $i i32.const 4095 i32.and i64.extend_i32_u i64.const 3 i64.shl local.set $address
        local.get $address local.get $i i32.const 17 i32.add i64.extend_i32_u i64.store $wide
        local.get $address i64.load $wide i32.wrap_i64 local.set $value
        ''' + ADD, locals='(local $address i64) (local $value i32)')
    yield "table64", "64-bit address space", ["table64", "reference-types"], "call_indirect", "table", "", \
        '''(type $unary (func (param i32) (result i32)))
        (table $wide i64 2 funcref)
        (elem (table $wide) (i64.const 0) func $plus $times)
        (func $plus (type $unary) local.get 0 i32.const 3 i32.add)
        (func $times (type $unary) local.get 0 i32.const 5 i32.mul)''', \
        loop('local.get $i local.get $i i32.const 1 i32.and i64.extend_i32_u call_indirect $wide (type $unary) local.set $value ' + ADD,
             locals='(local $value i32)')
    yield "multi_memory", "Multiple memories", ["multi-memory"], "memory.copy", "sum17", "", \
        '(memory $other 2)', loop('''
        local.get $i i32.const 4095 i32.and i32.const 2 i32.shl local.set $address
        local.get $address local.get $i i32.const 17 i32.add i32.store $other
        local.get $address i32.const 8192 i32.add local.get $address i32.const 4 memory.copy $io $other
        local.get $address i32.const 8192 i32.add i32.load $io local.set $value
        ''' + ADD, locals='(local $address i32) (local $value i32)')
    yield "extended_const", "Extended constant expressions", ["extended-const"], "global.get", "sum26", \
        "constant initialization precedes the kernel clock; kernel measures reads of initialized globals, process time also includes initialization", \
        '''(global $base i32 (i32.const 7))
        (global $derived i32 (i32.add (i32.mul (global.get $base) (i32.const 3)) (i32.const 5)))
        (global $wide i64 (i64.sub (i64.const 100) (i64.const 74)))''', \
        loop('local.get $i global.get $derived i32.add local.set $value ' + ADD,
             locals='(local $value i32)', init='global.get $wide i64.const 26 i64.ne if unreachable end')
    for name, nondefault in (("typed_call_ref", False), ("nondefaultable_local", True)):
        yield name, "Typed references", ["function-references", "reference-types"], "call_ref", "typed", "", \
            '''(type $unary (func (param i32) (result i32)))
            (elem declare func $apply)
            (func $apply (type $unary) local.get 0 i32.const 3 i32.mul i32.const 1 i32.add)''', \
            loop('local.get $i local.get $target ' + ('' if nondefault else 'ref.as_non_null ') +
                 'call_ref $unary local.set $value ' + ADD,
                 locals=f'(local $target (ref {"" if nondefault else "null "}$unary)) (local $value i32)',
                 init='ref.func $apply local.set $target')
    walk = '''(func $walk (param $left i32) (param $x i32) (result i32)
        local.get $left i32.eqz if local.get $x return end
        local.get $left i32.const 1 i32.sub local.get $x i32.const 3 i32.add
        return_call $walk)'''
    kernel = '''(func $kernel (export "kernel") (param $n i32) (result i64)
        local.get $n i32.const 7 call $walk i64.extend_i32_u)'''
    yield "tail_direct", "Tail calls", ["tail-call"], "return_call", "tail", "", walk, kernel
    yield "tail_indirect", "Tail calls", ["tail-call", "reference-types"], "return_call_indirect", "tail_indirect", "", \
        '''(type $signature (func (param i32 i32) (result i32)))
        (table $targets 2 funcref) (elem (i32.const 0) func $walk $odd)
        (func $walk (type $signature) (param $left i32) (param $x i32) (result i32)
         local.get $left i32.eqz if local.get $x return end
         local.get $left i32.const 1 i32.sub local.get $x i32.const 3 i32.add
         local.get $left i32.const 1 i32.and return_call_indirect $targets (type $signature))
        (func $odd (type $signature) (param $left i32) (param $x i32) (result i32)
         local.get $left i32.eqz if local.get $x return end
         local.get $left i32.const 1 i32.sub local.get $x i32.const 5 i32.add
         local.get $left i32.const 1 i32.and return_call_indirect $targets (type $signature))''', kernel
    yield "tail_ref", "Tail calls", ["tail-call", "function-references"], "return_call_ref", "tail", "", \
        '''(type $signature (func (param i32 i32) (result i32))) (elem declare func $walk)
        (func $walk (type $signature) (param $left i32) (param $x i32) (result i32)
         local.get $left i32.eqz if local.get $x return end
         local.get $left i32.const 1 i32.sub local.get $x i32.const 3 i32.add
         ref.func $walk return_call_ref $signature)''', kernel
    for name, rethrow in (("exception_payload", False), ("exception_rethrow_ref", True)):
        body = '''block $caught (result i32)
          try_table (catch $event $caught) INNER end unreachable
         end local.set $value ''' + ADD
        inner = 'local.get $i i32.const 17 i32.add call $raise drop'
        if rethrow:
            inner = '''block $reference (result (ref exn))
              try_table (catch_all_ref $reference)
               local.get $i i32.const 17 i32.add call $raise drop
              end unreachable
             end throw_ref'''
        yield name, "Exception handling", ["exceptions"], "throw_ref" if rethrow else "try_table", "sum17", "", \
            '(tag $event (param i32)) (func $raise (param i32) (result i32) local.get 0 throw $event)', \
            loop(body.replace('INNER', inner), locals='(local $value i32)')
    for name, allocate in (("gc_struct", False), ("gc_allocation", True)):
        init = 'i32.const 7 i32.const -1 struct.new $record local.set $object'
        body = 'local.get $i i32.const 17 i32.add i32.const -1 struct.new $record local.set $object' if allocate else \
            'local.get $object local.get $i i32.const 17 i32.add struct.set $record 0'
        body += ' local.get $object struct.get $record 0 local.get $object struct.get_u $record 1 i32.add local.set $value ' + ADD
        yield name, "Garbage collection", ["gc", "function-references"], "struct.new", "sum272", "", \
            '(type $record (struct (field (mut i32)) (field i8)))', \
            loop(body, locals='(local $object (ref null $record)) (local $value i32)', init=init)
    yield "gc_array", "Garbage collection", ["gc", "function-references"], "array.set", "array", "", \
        '(type $items (array (mut i32)))', loop('''
        local.get $array local.get $i i32.const 255 i32.and local.get $i i32.const 17 i32.add array.set $items
        local.get $array local.get $i i32.const 255 i32.and array.get $items local.set $value
        ''' + ADD, locals='(local $array (ref null $items)) (local $value i32)',
        init='i32.const 256 array.new_default $items local.set $array',
        result='local.get $sum local.get $array array.len i32.add i64.extend_i32_u')
    yield "gc_i31", "Garbage collection", ["gc", "function-references"], "ref.i31", "sum0", "", "", \
        loop('local.get $i ref.i31 i31.get_u local.set $value ' + ADD, locals='(local $value i32)')
    yield "gc_recursive", "Garbage collection", ["gc", "function-references"], "struct.new", "recursive", "", \
        '(rec (type $node (struct (field (ref null $node)) (field i32))))', \
        loop('''local.get $head local.get $i i32.const 17 i32.add struct.new $node local.set $head
        local.get $head struct.get $node 0 ref.as_non_null struct.get $node 1 local.set $value ''' + ADD,
        locals='(local $head (ref null $node)) (local $value i32)',
        init='ref.null $node i32.const 7 struct.new $node local.set $head')
    yield "relaxed_swizzle", "Relaxed vector instructions", ["simd", "relaxed-simd"], "i8x16.relaxed_swizzle", "swizzle", "", "", \
        loop('''local.get $vector v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 0
        i8x16.relaxed_swizzle local.tee $vector i8x16.extract_lane_u 0 local.set $value ''' + ADD,
        locals='(local $vector v128) (local $value i32)',
        init='v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 local.set $vector')
    yield "relaxed_madd", "Relaxed vector instructions", ["simd", "relaxed-simd"], "f32x4.relaxed_madd", "count", "", "", \
        loop('''local.get $vector v128.const f32x4 1 1 1 1 v128.const f32x4 1 1 1 1
        f32x4.relaxed_madd local.set $vector''', locals='(local $vector v128)',
        result='local.get $vector f32x4.extract_lane 0 i32.trunc_f32_u i64.extend_i32_u')
    yield "deterministic_profile", "Profiles", ["simd", "relaxed-simd"], "f32.div", "nan", \
        "requires NaN canonicalization and deterministic relaxed SIMD; ordinary NaN legality is insufficient", "", \
        loop('''f32.const 0 f32.const 0 f32.div i32.reinterpret_f32 i32.const 0x7fc00000 i32.ne if unreachable end
        v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
        v128.const i8x16 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17
        i8x16.relaxed_swizzle i8x16.extract_lane_u 0 i32.eqz i32.eqz if unreachable end
        local.get $sum i32.const 1 i32.add local.set $sum''')
    yield "annotations", "Custom annotations", [], "i32.add", "sum17", "text and custom-section transport, no new executable opcode", \
        '(@custom "uwvm.debug.performance" "annotations-v1")', \
        loop('local.get $i i32.const 17 i32.add local.set $value ' + ADD, locals='(local $value i32)').replace(
            '(func $kernel ', '(func $kernel (@name "annotated kernel") ')


def expected(kind, n):
    sums = {"sum0": 0, "sum17": 17, "sum26": 26, "sum272": 272}
    if kind in sums:
        value = n * (n - 1) // 2 + sums[kind] * n
    elif kind == "table":
        even, odd = (n + 1) // 2, n // 2
        value = even * (even - 1) + 3 * even + 5 * odd * odd
    elif kind == "typed": value = 3 * n * (n - 1) // 2 + n
    elif kind == "tail": value = 7 + 3 * n
    elif kind == "tail_indirect": value = 7 + 3 * n + 2 * ((n - 1) // 2)
    elif kind == "array": value = n * (n - 1) // 2 + 17 * n + 256
    elif kind == "recursive": value = 7 + (n - 1) * (n - 2) // 2 + 17 * (n - 1)
    elif kind == "swizzle": value = (n // 16) * 120 + (n % 16) * (n % 16 + 1) // 2
    elif kind in ("count", "nan"): value = n
    else: raise ValueError(kind)
    return value & 0xffffffff


def author(out, iterations, repeats, profile_gates=False):
    out.mkdir(parents=True, exist_ok=False)
    rows = []
    for name, feature, flags, opcode, oracle, note, declaration, kernel in definitions():
        epilogue = EPILOGUE
        if profile_gates:
            # Both debugger backends stop here after the result, before guest
            # exit and runtime retirement. This call is outside kernel clocks.
            epilogue = epilogue.replace('i32.const 96 i32.const 1 call $text)\n',
                                         'i32.const 96 i32.const 1 call $text\n  call $profile_end)\n')
            epilogue += '(func $profile_end (export "profile_end") nop)\n'
        text = (f';; {feature}: {name}. Source inventory: {SPEC}\n'
                f';; {note}\n(module\n{PRELUDE}\n'
                f' (global $iterations i32 (i32.const {iterations}))\n'
                f' (global $kernel_repeats i32 (i32.const {repeats}))\n{declaration}\n{kernel}\n{epilogue})\n')
        text = '\n'.join(line.rstrip() for line in text.splitlines()) + '\n'
        wat = out / (name + '.wat')
        wat.write_text(text)
        rows.append({'name': name, 'feature': feature, 'required_features': flags,
                     'opcode_witness': opcode, 'oracle': oracle, 'iterations': iterations,
                     'kernel_repeats': repeats, 'expected_checksum': expected(oracle, iterations) * repeats, 'notes': note,
                     'wat_sha256': hashlib.sha256(wat.read_bytes()).hexdigest(),
                     'validation': 'pending', 'runtime_validation': 'pending'})
    assert len({row['feature'] for row in rows}) == 10
    (out / 'manifest.json').write_text(json.dumps(rows, indent=2) + '\n')
    return rows


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--iterations', type=int, default=10000)
    p.add_argument('--kernel-repeats', type=int, default=1,
                   help='Extend the real guest kernel for hardware sampling; checksum sums every repetition.')
    p.add_argument('--compile', action='store_true')
    p.add_argument('--profile-gates', action='store_true',
                   help='Add a post-result breakpoint function outside kernel clocks for matched VTune windows.')
    p.add_argument('--wasm-tools', type=Path)
    a = p.parse_args()
    if not 1 <= a.iterations <= 1000000: p.error('iterations must be in [1, 1000000]')
    if not 1 <= a.kernel_repeats <= 100000: p.error('kernel-repeats must be in [1, 100000]')
    if a.compile:
        if not a.wasm_tools: p.error('--compile requires --wasm-tools')
        root = Path(__file__).resolve().parents[2]
        subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    rows = author(a.out, a.iterations, a.kernel_repeats, a.profile_gates)
    if a.profile_gates:
        for row in rows: row['profile_gates'] = {'before': 'start', 'after': 'profile_end'}
        (a.out / 'manifest.json').write_text(json.dumps(rows, indent=2) + '\n')
    if a.compile:
        for row in rows:
            wat, wasm = a.out / (row['name'] + '.wat'), a.out / (row['name'] + '.wasm')
            row['validation'] = 'failed'
            row['tool_runs'] = []
            for label, argv in (
                ('parse', [str(a.wasm_tools), 'parse', str(wat), '-o', str(wasm)]),
                ('validate', [str(a.wasm_tools), 'validate', '--features', 'all', str(wasm)]),
                ('print', [str(a.wasm_tools), 'print', str(wasm)]),
            ):
                try:
                    process = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
                except subprocess.TimeoutExpired as error:
                    (a.out / (row['name'] + '.' + label + '.log')).write_bytes(error.stdout or b'')
                    row['tool_runs'].append({'argv': argv, 'exit': None, 'timed_out': True, 'timeout_s': 60})
                    break
                (a.out / (row['name'] + '.' + label + '.log')).write_bytes(process.stdout)
                row['tool_runs'].append({'argv': argv, 'exit': process.returncode})
                if process.returncode != 0: break
                if label == 'print':
                    binary_text = process.stdout.decode()
                    if row['opcode_witness'] not in binary_text: break
                    if row['name'] == 'annotations' and b'uwvm.debug.performance' not in wasm.read_bytes(): break
                    row.update(wasm_sha256=hashlib.sha256(wasm.read_bytes()).hexdigest(), validation='passed')
            (a.out / 'manifest.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(json.dumps({'inputs': len(rows), 'feature_categories': 10, 'compiled': a.compile}))
    return 1 if a.compile and any(row['validation'] != 'passed' for row in rows) else 0


if __name__ == '__main__': raise SystemExit(main())
