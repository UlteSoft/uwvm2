#!/usr/bin/env python3
"""Generate self-contained Wasm and equivalent native kernels for debugger profiling.

Requires wasm-tools and a native C compiler on the test host. Every Wasm times
only its kernel with WASI CLOCK_MONOTONIC, after initializing its working set.
Native comparison validates checksums independently; compiler policies differ.
"""
import argparse, hashlib, json, subprocess
from pathlib import Path

COMMON = r'''
 (import "wasi_snapshot_preview1" "clock_time_get" (func $clock (param i32 i64 i32) (result i32)))
 (import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
 (memory (export "memory") 130)
 (data (i32.const 64) "Kernel ns: ")
 (data (i32.const 80) "Checksum: ")
 (data (i32.const 96) "\0a")
 (func $text (param $p i32) (param $n i32)
  (i32.store (i32.const 0) (local.get $p))
  (i32.store (i32.const 4) (local.get $n))
  (drop (call $write (i32.const 1) (i32.const 0) (i32.const 1) (i32.const 8))))
 (func $number (param $v i64) (local $p i32)
  (local.set $p (i32.const 512))
  (loop $digits
   (local.set $p (i32.sub (local.get $p) (i32.const 1)))
   (i32.store8 (local.get $p) (i32.add (i32.wrap_i64 (i64.rem_u (local.get $v) (i64.const 10))) (i32.const 48)))
   (local.set $v (i64.div_u (local.get $v) (i64.const 10)))
   (br_if $digits (i64.ne (local.get $v) (i64.const 0))))
  (call $text (local.get $p) (i32.sub (i32.const 512) (local.get $p))))
 (func $probe_cold (export "probe_cold") (result i32) (i32.const 123))
'''

C_HEADER = r'''
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
static uint32_t memory_words[2097152];
__attribute__((noinline)) uint32_t probe_cold(void) { return 123; }
static uint64_t clock_ns(void) {
 struct timespec t; if (clock_gettime(CLOCK_MONOTONIC, &t)) abort();
 return (uint64_t)t.tv_sec*1000000000ull+(uint64_t)t.tv_nsec;
}
'''


def kernels(tiny, deep_calls=False):
    n = 32 if tiny else 300000
    mix = '(i32.add (i32.mul (local.get $x) (i32.const 1664525)) (i32.const 1013904223))'
    common = f'''(local.set $x (i32.const 7)) (local.set $i (i32.const {n}))
     (loop $again (local.set $x {mix}) BODY
      (local.set $i (i32.sub (local.get $i) (i32.const 1))) (br_if $again (local.get $i)))
     (i64.extend_i32_u (local.get $s))'''
    yield 'integer', 'integer recurrence', common.replace('BODY', '''
     (local.set $x (i32.xor (local.get $x) (i32.shr_u (local.get $x) (i32.const 13))))
     (local.set $s (i32.add (local.get $s) (local.get $x)))'''), f'''
     uint32_t x=7,s=0; for(uint32_t i=0;i<{n};++i) {{x=x*1664525u+1013904223u;x^=x>>13;s+=x;}} return s;''', '', '', 0
    branch = '''(if (i32.and (local.get $x) (i32.const 4096))
     (then (local.set $s (i32.add (local.get $s) (i32.rotl (local.get $x) (i32.const 3)))))
     (else (local.set $s (i32.xor (local.get $s) (i32.shr_u (local.get $x) (i32.const 7))))))
     (if (i32.and (local.get $x) (i32.const 65536))
      (then (local.set $s (i32.add (local.get $s) (i32.const 71))))
      (else (local.set $s (i32.sub (local.get $s) (i32.const 19)))))'''
    yield 'branches', 'data-dependent nested branches', common.replace('BODY', branch), f'''
     uint32_t x=7,s=0; for(uint32_t i=0;i<{n};++i) {{x=x*1664525u+1013904223u;
      if(x&4096u) s+=(x<<3)|(x>>29);else s^=x>>7;if(x&65536u)s+=71u;else s-=19u;}}return s;''', '', '', 0
    table = '''(block $done (block $c3 (block $c2 (block $c1 (block $c0
      (br_table $c0 $c1 $c2 $c3 (i32.and (i32.shr_u (local.get $x) (i32.const 17)) (i32.const 3))))
      (local.set $s (i32.add (local.get $s) (local.get $x))) (br $done))
      (local.set $s (i32.xor (local.get $s) (local.get $x))) (br $done))
      (local.set $s (i32.sub (local.get $s) (local.get $x))) (br $done))
      (local.set $s (i32.add (local.get $s) (i32.shr_u (local.get $x) (i32.const 3)))))'''
    yield 'dispatch', 'br_table dispatch', common.replace('BODY', table), f'''
     uint32_t x=7,s=0;for(uint32_t i=0;i<{n};++i){{x=x*1664525u+1013904223u;
      switch((x>>17)&3u){{case 0:s+=x;break;case 1:s^=x;break;case 2:s-=x;break;default:s+=x>>3;break;}}}}return s;''', '', '', 0
    for name, words in [('scan32k', 8192), ('scan256k', 65536), ('scan8m', 2097152), ('chase8m', 2097152)]:
        if tiny: words = 256
        accesses = 32 if tiny else 2000000
        init = f'''(local.set $i (i32.const 0)) (loop $fill
         (i32.store (i32.add (i32.const 65536) (i32.shl (local.get $i) (i32.const 2)))
          (i32.and (i32.add (i32.mul (local.get $i) (i32.const 1664525)) (i32.const 1013904223)) (i32.const {words-1})))
         (local.set $i (i32.add (local.get $i) (i32.const 1))) (br_if $fill (i32.lt_u (local.get $i) (i32.const {words}))))'''
        next_x = '(local.get $v)' if name.startswith('chase') else f'(i32.and (i32.add (local.get $x) (i32.const 1)) (i32.const {words-1}))'
        store = '' if name.startswith('chase') else '''(i32.store (local.get $p) (i32.xor (local.get $v) (local.get $s)))'''
        body = f'''(local.set $i (i32.const {accesses})) (loop $again
         (local.set $p (i32.add (i32.const 65536) (i32.shl (local.get $x) (i32.const 2))))
         (local.set $v (i32.load (local.get $p))) (local.set $s (i32.add (local.get $s) (local.get $v)))
         {store} (local.set $x {next_x})
         (local.set $i (i32.sub (local.get $i) (i32.const 1))) (br_if $again (local.get $i)))
         (i64.extend_i32_u (local.get $s))'''
        c_next = 'x=v;' if name.startswith('chase') else f'memory_words[x]=v^s;x=(x+1)&{words-1}u;'
        yield name, 'dependent pointer chase' if name.startswith('chase') else 'sequential read/write', body, f'''
         uint32_t x=0,s=0;for(uint32_t i=0;i<{accesses};++i){{uint32_t v=memory_words[x];s+=v;{c_next}}}return s;''', init, f'for(uint32_t i=0;i<{words};++i) memory_words[i]=(i*1664525u+1013904223u)&{words-1}u;', words*4
    call_cases = [('calls', 0, 300000), ('callchain', 16, 20000)]
    if deep_calls: call_cases.append(('deepcallchain', 64, 5000))
    for name, depth, count in call_cases:
        if tiny: count = 4
        extra = '''(func $probe_hot (export "probe_hot") (param $x i32) (result i32)
         (i32.xor (i32.add (i32.mul (local.get $x) (i32.const 1664525)) (i32.const 1013904223)) (i32.shr_u (local.get $x) (i32.const 13))))
         (func $descend (param $d i32) (param $x i32) (result i32)
          (if (result i32) (local.get $d)
           (then (i32.xor (call $descend (i32.sub (local.get $d) (i32.const 1)) (call $probe_hot (local.get $x))) (local.get $d)))
           (else (call $probe_hot (local.get $x)))))'''
        call = '(call $probe_hot (local.get $x))' if depth == 0 else f'(call $descend (i32.const {depth}) (local.get $x))'
        body = f'''(local.set $x (i32.const 7)) (local.set $i (i32.const {count}))
         (loop $again (local.set $x {call}) (local.set $s (i32.add (local.get $s) (local.get $x)))
          (local.set $i (i32.sub (local.get $i) (i32.const 1))) (br_if $again (local.get $i))) (i64.extend_i32_u (local.get $s))'''
        native_extra = '''__attribute__((noinline)) uint32_t probe_hot(uint32_t x) { return (x*1664525u+1013904223u)^(x>>13); }
         __attribute__((noinline)) uint32_t descend(uint32_t d,uint32_t x) { return d ? (descend(d-1,probe_hot(x))^d) : probe_hot(x); }'''
        c_call = 'probe_hot(x)' if depth == 0 else f'descend({depth},x)'
        yield name, 'direct calls' if depth == 0 else f'nested calls / activation depth {depth+2}', body, f'uint32_t x=7,s=0;for(uint32_t i=0;i<{count};++i){{x={c_call};s+=x;}}return s;', extra, native_extra, -1
    if deep_calls:
        # The same leaf repeatedly enters at a deep actual call depth. This
        # catches a cache limited to shallow depths instead of inactive slots.
        count = 4 if tiny else 300000
        extra = f'''(func $probe_hot (export "probe_hot") (param $x i32) (result i32)
         (i32.xor (i32.add (i32.mul (local.get $x) (i32.const 1664525)) (i32.const 1013904223)) (i32.shr_u (local.get $x) (i32.const 13))))
         (func $descend (param $d i32) (param $x i32) (result i32) (local $i i32) (local $s i32)
          (if (result i32) (local.get $d)
           (then (i32.xor (call $descend (i32.sub (local.get $d) (i32.const 1)) (local.get $x)) (local.get $d)))
           (else (local.set $i (i32.const {count}))
            (loop $again (local.set $x (call $probe_hot (local.get $x)))
             (local.set $s (i32.add (local.get $s) (local.get $x)))
             (local.set $i (i32.sub (local.get $i) (i32.const 1))) (br_if $again (local.get $i)))
            (local.get $s))))'''
        native_extra = f'''__attribute__((noinline)) uint32_t probe_hot(uint32_t x) {{ return (x*1664525u+1013904223u)^(x>>13); }}
         __attribute__((noinline)) uint32_t descend(uint32_t d,uint32_t x) {{
          if(d) return descend(d-1,x)^d;
          uint32_t s=0;for(uint32_t i=0;i<{count};++i){{x=probe_hot(x);s+=x;}}return s; }}'''
        yield 'deepbusy', 'repeated leaf calls with 64 suspended callers', '(i64.extend_i32_u (call $descend (i32.const 64) (i32.const 7)))', 'return descend(64,7);', extra, native_extra, -1
    yield 'float', 'dependent f64 recurrence', f'''
     (local.set $f (f64.const 0.25)) (local.set $i (i32.const {n}))
     (loop $again (local.set $f (f64.add (f64.mul (local.get $f) (f64.const 0.999999)) (f64.const 1)))
      (local.set $i (i32.sub (local.get $i) (i32.const 1))) (br_if $again (local.get $i))) (i64.trunc_f64_u (local.get $f))''', f'double f=.25;for(uint32_t i=0;i<{n};++i)f=f*.999999+1;return (uint64_t)f;', '', '', 0


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--wasm-tools', type=Path, required=True)
    p.add_argument('--cc', type=Path, required=True)
    p.add_argument('--tiny', action='store_true', help='Small exact-trace qualification versions, not performance evidence.')
    p.add_argument('--deep-calls', action='store_true', help='Also generate a 64-level call chain and a leaf loop with 64 live callers.')
    a = p.parse_args(); a.out.mkdir(parents=True, exist_ok=False)
    rows = []
    for name, kind, body, c_body, init, c_init, size in kernels(a.tiny, a.deep_calls):
        extra, c_extra = (init, c_init) if size == -1 else ('', '')
        if size == -1: init = c_init = ''; size = 0
        wat = '(module\n' + COMMON + extra + f'''
 (func $initialize (local $i i32) {init})
 (func $kernel (export "kernel") (result i64)
  (local $i i32) (local $x i32) (local $s i32) (local $p i32) (local $v i32) (local $f f64)
  {body})
 (func (export "_start") (local $before i64) (local $elapsed i64) (local $sum i64)
  (call $initialize)
  (drop (call $clock (i32.const 1) (i64.const 1) (i32.const 16)))
  (local.set $before (i64.load (i32.const 16)))
  (local.set $sum (call $kernel))
  (drop (call $clock (i32.const 1) (i64.const 1) (i32.const 16)))
  (local.set $elapsed (i64.sub (i64.load (i32.const 16)) (local.get $before)))
  (call $text (i32.const 64) (i32.const 11)) (call $number (local.get $elapsed)) (call $text (i32.const 96) (i32.const 1))
  (call $text (i32.const 80) (i32.const 10)) (call $number (local.get $sum)) (call $text (i32.const 96) (i32.const 1)))
)'''
        c = C_HEADER + c_extra + '\n__attribute__((noinline)) uint64_t kernel(void) {' + c_body + '}\n' + f'''
int main(void) {{ {c_init} uint64_t t=clock_ns(),s=kernel();t=clock_ns()-t;
 printf("Kernel ns: %llu\\nChecksum: %llu\\n",(unsigned long long)t,(unsigned long long)s);return 0;}}
'''
        (a.out/(name+'.wat')).write_text(wat)
        (a.out/(name+'.c')).write_text(c)
        subprocess.run([str(a.wasm_tools), 'parse', str(a.out/(name+'.wat')), '-o', str(a.out/(name+'.wasm'))], check=True)
        subprocess.run([str(a.wasm_tools), 'validate', str(a.out/(name+'.wasm'))], check=True)
        subprocess.run([str(a.cc), '-O2', '-g', '-ffp-contract=off', str(a.out/(name+'.c')), '-o', str(a.out/(name+'-native'))], check=True)
        rows.append({'name':name, 'kind':kind, 'working_set_bytes':size, 'tiny':a.tiny,
                     'wasm_sha256':hashlib.sha256((a.out/(name+'.wasm')).read_bytes()).hexdigest(),
                     'native_sha256':hashlib.sha256((a.out/(name+'-native')).read_bytes()).hexdigest()})
        print(name, kind, size, flush=True)
    (a.out/'manifest.json').write_text(json.dumps(rows, indent=2))


if __name__ == '__main__': main()
