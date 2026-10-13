#!/usr/bin/env python3
"""Inspect actual cached full-JIT numeric EH code and mmap normal-memory code.

Runtime compilation optimization and LLVM-generated optimization are recorded
separately. This is structural/code-identity evidence, never a timing benchmark.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import struct
import subprocess
from check_wasm3_native_frame_codegen import decode_object
from elf_executable_sections import write_executable_evidence, normalize_relocations, compare_executable_images


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--uwvm', type=Path, required=True)
    p.add_argument('--wasm-tools', type=Path, required=True)
    p.add_argument('--wasmtime', type=Path, required=True)
    p.add_argument('--bridge-names', type=Path, required=True,
                   help='JSON emitted by llvm_jit_exception_bridge_names.cc with matching compiler ABI')
    p.add_argument('--llvm', type=Path, default=Path('/toolchain/bin'))
    p.add_argument('--runtime-optimization', required=True)
    p.add_argument('--ros', action='store_true')
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    a.out = a.out.resolve()
    a.out.mkdir(parents=True, exist_ok=False)
    names = json.loads(a.bridge_names.read_text())
    if set(names) != {'push', 'pop', 'throw', 'matches', 'copy'} or not all(
            re.fullmatch(r'uwvm_bridge_[0-9a-f]+', v) for v in names.values()):
        raise ValueError('invalid exact production bridge prefixes')
    (a.out/'bridge-names.json').write_text(json.dumps(names, indent=2)+'\n')
    commands, checks, objects = [], [], []

    def run(command, output, expected=0):
        command = list(map(str, command))
        result = subprocess.run(command, capture_output=True, timeout=120)
        output.write_bytes(result.stdout+result.stderr)
        commands.append(dict(command=command, exit=result.returncode, log=str(output)))
        (a.out/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
        if result.returncode != expected:
            raise RuntimeError(f'{command[0]} exit={result.returncode}: {output}\n{output.read_text(errors="replace")[-12000:]}')
        return (result.stdout+result.stderr).decode(errors='replace')

    memory_leaf = '''(func $step (param $index i32) (result i32) (local $address i32)
      local.get $index i32.const 255 i32.and i32.const 2 i32.shl local.tee $address
      local.get $index i32.const 1 i32.add i32.store
      local.get $address i32.load)'''
    memory_loop = '''i32.const 0 local.set $i i32.const 0 local.set $sum
      loop $again local.get $i call $step local.get $sum i32.add local.set $sum
        local.get $i i32.const 1 i32.add local.tee $i i32.const 128 i32.lt_u br_if $again end
      local.get $sum i32.const 8256 i32.ne if unreachable end'''
    fixtures = {
        'memory-loop': f'(module (memory 1) {memory_leaf} (func (export "_start") (local $i i32) (local $sum i32) {memory_loop}))',
        'protected-normal': f'''(module (memory 1) {memory_leaf}
          (func (export "_start") (local $i i32) (local $sum i32)
            block $exit try_table (catch_all $exit) {memory_loop} return end end unreachable))''',
        'cross-throw': '''(module (tag $t (param i32))
          (func $leaf (param i32) local.get 0 throw $t)
          (func $middle (param i32) local.get 0 call $leaf)
          (func (export "_start") block $out (result i32) try_table (catch $t $out)
            i32.const 42 call $middle end unreachable end i32.const 42 i32.ne if unreachable end))''',
        'tail-throw': '''(module (tag $t (param i32))
          (func $leaf (result i32) i32.const 42 throw $t)
          (func $retired (result i32) block $wrong (result i32)
            try_table (result i32) (catch $t $wrong) return_call $leaf end end drop i32.const 13)
          (func (export "_start") block $out (result i32) try_table (catch $t $out)
            call $retired drop end unreachable end i32.const 42 i32.ne if unreachable end))''',
    }
    for name, source in fixtures.items():
        wat = a.out/(name+'.wat')
        wasm = wat.with_suffix('.wasm')
        wat.write_text(source+'\n')
        run([a.wasm_tools, 'parse', wat, '-o', wasm], a.out/(name+'-assemble.log'))
        run([a.wasm_tools, 'validate', wasm], a.out/(name+'-validate.log'))
        run([a.wasmtime, '-C', 'cache=n', '-W', 'exceptions=y', '-W', 'tail-call=y', wasm], a.out/(name+'-reference.log'))

    def emit(name, policy, enabled=True):
        out = a.out/(name+'-'+policy+('-on' if enabled else '-off'))
        out.mkdir()
        cache = out/'cache'
        cache.mkdir()
        command = [a.uwvm]+(['-Raot'] if a.ros else ['-Rcc', 'jit', '-Rcm', 'full'])
        command += ['-Rct', '0', '-Rllvm-call-stack', policy, '-Rllvm-full-policy', 'pb-o3',
                    '-Rllvm-cache-path', 'path', cache, '-Rclog', 'out',
                    '-WFE-exceptions' if enabled else '-WFD-exceptions', '-WFE-tail-call',
                    '--run', a.out/(name+'.wasm')]
        run(command, out/'run.log')
        files = list(cache.rglob('*.uwvm-ljc'))
        if len(files) != 1:
            raise RuntimeError(f'{name}: expected one actual cache object, got {len(files)}')
        cache_bytes = files[0].read_bytes()
        _, _, header, _, _, _, _, isa_bytes, context_bytes, _ = struct.unpack('<8sIIIIQQQQQ', cache_bytes[:64])
        context = cache_bytes[header+isa_bytes:header+isa_bytes+context_bytes]
        (out/'cache-context.bin').write_bytes(context)
        if b'native-memory-backend' not in context or b'mmap' not in context:
            raise RuntimeError('required mmap binary/cache ABI not established')
        obj = out/'native.o'
        obj.write_bytes(decode_object(cache_bytes, a.ros))
        executable = write_executable_evidence(obj, out)
        relocations = run([a.llvm/'llvm-readobj', '--relocations', obj], out/'relocations.txt')
        sections = run([a.llvm/'llvm-readobj', '--sections', obj], out/'sections.txt')
        assembly = run([a.llvm/'llvm-objdump', '-dr', obj], out/'native.s')
        symbols = run([a.llvm/'llvm-nm', '--defined-only', obj], out/'symbols.txt')
        cfi = run([a.llvm/'llvm-dwarfdump', '--eh-frame', obj], out/'eh-frame.txt')
        if '.eh_frame' not in sections or len(re.findall(r'\bFDE\b', cfi)) < 2:
            raise RuntimeError('missing actual frame-unwind records')
        if policy == 'unwind':
            if names['push'] in relocations or names['pop'] in relocations:
                raise RuntimeError('native unwind generated instruction-stack maintenance')
        else:
            if names['push'] not in relocations or names['pop'] not in relocations:
                raise RuntimeError('instruction positive control did not find exact push/pop bridge symbols')
        handler_symbols = ['uwvm_guest_exception_typeinfo_v1', '__gxx_personality_v0', '__cxa_begin_catch', '__cxa_end_catch']
        elided_protected = (name == 'protected-normal' and
                           not any(symbol in relocations for symbol in handler_symbols) and '.gcc_except_table' not in sections)
        if name in ['cross-throw', 'tail-throw', 'protected-normal'] and not elided_protected:
            if not all(symbol in relocations for symbol in handler_symbols):
                raise RuntimeError(f'{name}/{policy}: missing typed native catch relocations')
            if '.gcc_except_table' not in sections:
                raise RuntimeError('missing native landingpad metadata')
        if name in ['cross-throw', 'tail-throw']:
            if not all(names[key] in relocations for key in ['throw', 'matches', 'copy']):
                raise RuntimeError('missing actual numeric throw/match/copy calls')
        row = dict(name=name, policy=policy, enabled=enabled, **executable.summary(),
                   mmap_abi=True, native_cfi=True, instruction_maintenance=(policy=='instruction'),
                   object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest())
        objects.append(row)
        return dict(out=out, object=obj, executable=executable, relocations=relocations,
                    assembly=assembly, symbols=symbols, command=command, elided_protected=elided_protected)

    def body(result, index):
        found = re.findall(r'\b(uwvm_m_[0-9a-f]+_func_'+str(index)+r')$', result['symbols'], re.M)
        if len(found) != 1:
            raise RuntimeError(f'missing unique generated function {index}')
        text = run([a.llvm/'llvm-objdump', '-dr', '--disassemble-symbols='+found[0], result['object']],
                   result['out']/('func-'+str(index)+'.s'))
        if 'file format elf64-x86-64' not in text:
            raise RuntimeError('this machine-instruction assertion explicitly requires x86-64; do not count another ISA as covered')
        content = text.split('<'+found[0]+'>:', 1)[1]
        return re.sub(r'uwvm_m_[0-9a-f]+_', 'uwvm_m_HASH_', content)

    for policy in ['instruction', 'unwind']:
        disabled = emit('memory-loop', policy, False)
        enabled = emit('memory-loop', policy, True)
        compare_executable_images(disabled['executable'], enabled['executable'])
        if normalize_relocations(disabled['relocations']) != normalize_relocations(enabled['relocations']):
            raise RuntimeError('feature enablement changed complete native relocations')
        checks.append(dict(kind='memory-feature-on-off', policy=policy, equal_all_executable_sections=True,
                           equal_complete_relocations=True, executable_bytes=enabled['executable'].text_bytes))
        protected = emit('protected-normal', policy)
        if protected['elided_protected']:
            # The optimizer proves the ordinary memory callee cannot unwind.
            # Accept the absent catch only if the whole code and relocations
            # become exactly those of its equivalent unprotected program.
            compare_executable_images(enabled['executable'], protected['executable'])
            if normalize_relocations(enabled['relocations']) != normalize_relocations(protected['relocations']):
                raise RuntimeError('elided handler did not preserve equivalent complete relocations')
        if body(enabled, 0) != body(protected, 0):
            raise RuntimeError('protected caller changed the same mmap memory leaf machine code')
        leaf = body(enabled, 0)
        if policy == 'unwind':
            if re.search(r'\b(?:callq?|j[a-z]+|lock|mfence|lfence|sfence)\s', leaf):
                raise RuntimeError('mmap memory leaf emitted a helper, conditional guard, or synchronization instruction')
            if not re.search(r'\bmovl\s+%[a-z0-9]+,\s*\(', leaf) or not re.search(r'\bmovl\s+\([^)]*\),\s*%[a-z0-9]+', leaf):
                raise RuntimeError('memory-leaf positive control lacks a real i32 store/load')
        checks.append(dict(kind='protected-caller-memory-leaf', policy=policy, identical_leaf=True,
                           redundant_catch_elided=protected['elided_protected'],
                           normal_memory_helper_calls=0 if policy=='unwind' else 'existing instruction tracing'))
        cross = emit('cross-throw', policy)
        # A fresh process must consume the actual authenticated object and still catch correctly.
        replay = run(cross['command'], cross['out']/'replay.log')
        if 'object-cache-hit ' not in replay or 'signature_verified=1' not in replay:
            raise RuntimeError('native EH cache replay not proven')
        checks.append(dict(kind='cross-throw-cache-replay', policy=policy, signed_hit=True))
        tail = emit('tail-throw', policy)
        retired = body(tail, 1)
        if not re.search(r'\bjmpq?\s', retired) or re.search(r'\bretq?\b', retired):
            raise RuntimeError('retired Wasm function did not preserve native tail dispatch')
        if policy == 'unwind' and re.search(r'\bcallq?\s', retired):
            raise RuntimeError('tail-only unwind function retained an ordinary call')
        checks.append(dict(kind='tail-dispatch', policy=policy, jump_without_return=True,
                           native_tail_body_calls=0 if policy=='unwind' else 'existing instruction tracing'))
    report = dict(passed=True, product='uwvm2-ros' if a.ros else 'uwvm2',
                  runtime_optimization=a.runtime_optimization, generated_optimization='pb-o3',
                  timing_benchmark=False, scope='native x86-64 ELF cached full JIT',
                  binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),
                  bridge_names_sha256=hashlib.sha256(a.bridge_names.read_bytes()).hexdigest(),
                  checks=checks, objects=objects, commands=len(commands))
    (a.out/'summary.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS', len(checks), 'native EH/mmap code checks;', len(objects), 'nonempty actual objects')


if __name__ == '__main__':
    main()
