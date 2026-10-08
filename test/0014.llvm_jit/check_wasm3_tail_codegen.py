#!/usr/bin/env python3
"""Inspect objects emitted by the real full JIT for Core 3 mutual tail calls."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from check_wasm3_native_frame_codegen import decode_object
from run_call_ref_typed_core3 import load_cases
from run_wasm3_tail_transfer import fixtures


def success_path_calls(assembly, *, noreturn_symbols=()):
    # A native trap is a cold non-returning call. Count calls only on CFG paths
    # that reach a normal return or a tail jump; a push/pop bookkeeping callback
    # necessarily lies on such a path and must still fail the unwind check.
    ins=[]
    for line in assembly.splitlines():
        m=re.match(r'^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2}\s+)+\s*([a-z][a-z0-9]*)\s*(.*?)\s*$',line)
        if m: ins.append((int(m[1],16),m[2],m[3]))
    indices={addr:i for i,(addr,_,_) in enumerate(ins)}
    edges={i:[] for i in range(len(ins))};good=set()
    for i,(_,op,arg) in enumerate(ins):
        if op.startswith('ret') or op.startswith('jmp') and arg.startswith('*'):
            good.add(i);continue
        if op in ['ud2','int3']:continue
        # Only explicitly qualified noreturn functions terminate a call edge.
        # Machine code may place another basic block immediately after the call.
        if op.startswith('call') and any(symbol in arg for symbol in noreturn_symbols):continue
        if op.startswith('j'):
            m=re.match(r'0x([0-9a-f]+) ',arg)
            if m and int(m[1],16) in indices: edges[i].append(indices[int(m[1],16)])
            if op.startswith('jmp'):continue
        if i+1<len(ins):edges[i].append(i+1)
    while True:
        prior=len(good)
        good.update(i for i,targets in edges.items() if any(t in good for t in targets))
        if len(good)==prior:break
    assert good, 'no return/tail path found'
    return sum(ins[i][1].startswith('call') for i in good)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('uwvm', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--wat2wasm')
    p.add_argument('--wasm-tools', type=Path)
    p.add_argument('--llvm', type=Path, required=True)
    p.add_argument('--ros', action='store_true')
    p.add_argument('--case', choices=['mutual','tuple-direct','typed-ref-local'], default='mutual')
    a = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    out = a.output.resolve(); out.mkdir(parents=True, exist_ok=False)
    src = out / 'mutual.wat'; wasm = src.with_suffix('.wasm')
    src.write_text('''(module
      (func $a (param $n i32) (param $s i64) (result i64)
        local.get $n i32.eqz if local.get $s return end
        local.get $n i32.const 1 i32.sub local.get $s i64.const 7 i64.add return_call $b)
      (func $b (param $n i32) (param $s i64) (result i64)
        local.get $n i32.eqz if local.get $s return end
        local.get $n i32.const 1 i32.sub local.get $s i64.const 7 i64.add return_call $a)
      (func (export "_start") i32.const 1000001 i64.const 13 call $a
        i64.const 7000020 i64.ne if unreachable end))\n''')
    if a.case == 'typed-ref-local':
        assert a.wasm_tools is not None, '--wasm-tools is required for Core 3 typed references'
        src.write_text(load_cases(root)['tail-typed-local-250k'][1] + '\n')
        subprocess.run([str(a.wasm_tools), 'parse', str(src), '-o', str(wasm)], check=True)
    elif a.case != 'mutual':
        assert a.wat2wasm is not None, '--wat2wasm is required for this fixture'
        src.write_text(next(wat for name, wat, trap in fixtures('jit') if name == a.case))
        subprocess.run([a.wat2wasm, '--enable-tail-call', str(src), '-o', str(wasm)], check=True)
    else:
        assert a.wat2wasm is not None, '--wat2wasm is required for this fixture'
        subprocess.run([a.wat2wasm, '--enable-tail-call', str(src), '-o', str(wasm)], check=True)
    rows = []
    for policy in ['instruction', 'unwind']:
        cache = out / (policy + '-cache'); cache.mkdir()
        base = ['-Raot'] if a.ros else ['-Rcc', 'jit', '-Rcm', 'full']
        command = [str(a.uwvm.resolve()), *base, '-Rct', '0', '-Rllvm-full-policy', 'pb-o3',
                   '-Rllvm-cache-path', 'path', cache.name, '-Rllvm-call-stack', policy,
                   '-Rclog', 'file', policy + '.compile.log',
                   *(['-WFE-function-references'] if a.case == 'typed-ref-local' else []),
                   '-WFE-tail-call', '--run', str(wasm)]
        result = subprocess.run(command, cwd=out, capture_output=True, timeout=90)
        (out / (policy + '.log')).write_bytes(result.stdout + result.stderr)
        assert result.returncode == 0, (policy, result.stderr)
        files = list(cache.rglob('*.uwvm-ljc'))
        assert len(files) == 1, files
        obj = out / (policy + '.o'); obj.write_bytes(decode_object(files[0].read_bytes(), a.ros))
        symbols = subprocess.check_output([str(a.llvm / 'llvm-nm'), '--defined-only', str(obj)], text=True)
        sections = subprocess.check_output([str(a.llvm / 'llvm-objdump'), '-h', str(obj)], text=True)
        (out / (policy + '.sections.txt')).write_text(sections)
        assert '.eh_frame' in sections, 'missing native CFI'
        for index in ([0] if a.case == 'typed-ref-local' else [0, 1]):
            names = re.findall(r'\b(uwvm_m_[0-9a-f]+_func_' + str(index) + r')$', symbols, re.M)
            assert len(names) == 1, names
            name = names[0]
            assembly = subprocess.check_output([str(a.llvm / 'llvm-objdump'), '-dr', '--disassemble-symbols=' + name, str(obj)], text=True)
            (out / f'{policy}-{index}.assembly.txt').write_text(assembly)
            # Qualify actual x86-64 native objects here. Cross-ISA runtime tests
            # have their own runners; this check does not pretend to cover them.
            assert 'file format elf64-x86-64' in assembly
            calls = len(re.findall(r'\bcallq?\s', assembly))
            jumps = len(re.findall(r'\bjmpq?\s+\*', assembly))
            assert jumps >= 1, (policy, name, assembly)
            # Every Wasm call edge is a native jump. Instruction policy needs a
            # push plus a pop on each of the normal-return and tail-return paths.
            if policy == 'unwind' and a.case != 'typed-ref-local':
                assert success_path_calls(assembly) == 0, (name, assembly)
            if a.case == 'typed-ref-local':
                # The authenticated resolver is allowed to call host code before
                # transfer. The actual typed target is entered by a native jump:
                # 250,000 iterations would exhaust the native stack if this edge
                # were lowered as call/return instead of musttail.
                assert result.returncode == 0 and jumps >= 1, (name, assembly)
            rows.append(dict(policy=policy, function=name, calls=calls, success_path_calls=success_path_calls(assembly), indirect_jumps=jumps,
                             object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(), command=command))
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    (out / 'summary.json').write_text(json.dumps(dict(passed=True, scope='actual x86-64 full-JIT objects', fixture=a.case, cases=rows), indent=2) + '\n')
    if a.case == 'typed-ref-local':
        print('PASS typed return_call_ref JIT objects: native tail jumps, resolver bridge calls, CFI retained')
    else:
        print('PASS actual tail-call JIT objects: native jumps, unwind success paths have no calls, CFI retained')


if __name__ == '__main__':
    main()
