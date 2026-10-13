#!/usr/bin/env python3
"""Exercise actual target LLVM-full Core 3 EH plus shared-memory atomics in QEMU.

This runner consumes an already-linked target VM and its verified cross-build
summary. It never rebuilds LLVM or the VM, and reports native-object inspection
separately when the target permits a signed JIT cache. All commands execute
inside the configured Linux test cgroup.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import subprocess
import sys


WAT = '''(module
  (memory 1 1 shared)
  (tag $t (param i32))
  (func $raise i32.const 42 throw $t)
  (func $atomic (result i32)
    i32.const 0 i32.const 42 i32.atomic.store
    i32.const 0 i32.atomic.load)
  (func (export "_start")
    block $out (result i32)
      try_table (catch $t $out) call $raise end unreachable
    end
    i32.const 42 i32.ne if unreachable end
    call $atomic i32.const 42 i32.ne if unreachable end))
'''


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root', type=Path, required=True)
    p.add_argument('--uwvm', type=Path, required=True)
    p.add_argument('--build-summary', type=Path,
                   help='Cross-build summary.json (default: beside --uwvm)')
    p.add_argument('--qemu', type=Path, required=True)
    p.add_argument('--target', choices=['aarch64-linux-gnu', 'riscv64-linux-gnu'], required=True)
    p.add_argument('--sysroot', type=Path, required=True)
    p.add_argument('--deps', type=Path, default=Path('/work/deps'))
    p.add_argument('--wasm-tools', type=Path, required=True)
    p.add_argument('--wasmtime', type=Path, required=True)
    p.add_argument('--llvm-bin', type=Path, default=Path('/toolchain/bin'))
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--ros', action='store_true')
    p.add_argument('--test-object-capture', action='store_true',
                   help='Inspect a test-only transient MCJIT object without enabling unsafe persistent cache reuse')
    p.add_argument('--allow-cache-disabled', action='store_true',
                   help='Record limited execution-only evidence when target JIT caching is disabled')
    a = p.parse_args()
    root = a.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = a.out.resolve()
    if out == root or root in out.parents:
        p.error('output directory must be outside the frozen source tree')
    out.mkdir(mode=0o700, parents=True, exist_ok=False)
    build_summary = a.build_summary or a.uwvm.parent / 'summary.json'
    paths = {'uwvm': a.uwvm, 'build_summary': build_summary,
             'qemu': a.qemu, 'wasm_tools': a.wasm_tools,
             'wasmtime': a.wasmtime, 'llvm_objdump': a.llvm_bin / 'llvm-objdump',
             'llvm_readobj': a.llvm_bin / 'llvm-readobj',
             'llvm_nm': a.llvm_bin / 'llvm-nm',
             'runner': Path(__file__),
             'object_decoder': root / 'test/0014.llvm_jit/check_wasm3_native_frame_codegen.py',
             'fingerprint_script': root / 'tools/ci/wasm3_source_fingerprint.py',
             'cgroup_guard': root / 'tools/ci/require_wasm3_test_cgroup.sh',
             'target_loader': a.sysroot / 'lib' / ('ld-linux-aarch64.so.1' if a.target.startswith('aarch64')
                                                    else 'ld-linux-riscv64-lp64d.so.1')}
    for name, path in paths.items():
        if not path.is_file():
            p.error(f'missing {name}: {path}')
        paths[name] = path.resolve(strict=True)
    (out / 'input-sha256.json').write_text(json.dumps({key: digest(path) for key, path in paths.items()}, indent=2) + '\n')
    source_id = subprocess.check_output([sys.executable, str(root / 'tools/ci/wasm3_source_fingerprint.py'),
                                         str(root), str(out / 'source-before.json')], text=True).strip()
    provenance = json.loads(paths['build_summary'].read_text())
    expected = {'passed': True, 'target': a.target, 'source_id': source_id,
                'binary_sha256': digest(paths['uwvm'])}
    for field, value in expected.items():
        if provenance.get(field) != value:
            raise RuntimeError(f'cross-build provenance mismatch: {field}: expected {value!r}, got {provenance.get(field)!r}')
    if a.test_object_capture and provenance.get('test_object_capture') is not True:
        raise RuntimeError('cross-build provenance does not identify a test-only object-capture binary')
    rows = []
    host_env = dict(os.environ)
    host_toolchain_libs = (a.deps / 'usr/lib/x86_64-linux-gnu', Path('/toolchain/lib'),
                           Path('/toolchain/lib/x86_64-unknown-linux-gnu'))
    host_env['LD_LIBRARY_PATH'] = ':'.join(map(str, host_toolchain_libs)) + ':' + host_env.get('LD_LIBRARY_PATH', '')

    def run(label, argv, timeout=120):
        command = [str(value) for value in argv]
        try:
            result = subprocess.run(command, capture_output=True, timeout=timeout, env=host_env)
        except subprocess.TimeoutExpired as error:
            data = (error.stdout or b'') + (error.stderr or b'')
            (out / (label + '.log')).write_bytes(data)
            rows.append({'label': label, 'command': command, 'timeout_seconds': timeout,
                         'sha256': hashlib.sha256(data).hexdigest()})
            (out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
            raise RuntimeError(f'{label}: timed out after {timeout}s; see {out / (label + ".log")}') from error
        data = result.stdout + result.stderr
        (out / (label + '.log')).write_bytes(data)
        rows.append({'label': label, 'command': command, 'exit': result.returncode,
                     'sha256': hashlib.sha256(data).hexdigest()})
        (out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
        if result.returncode != 0:
            raise RuntimeError(f'{label}: exit={result.returncode}; {data.decode(errors="replace")[-8000:]}')
        return data.decode(errors='replace')

    header = run('target-elf', [paths['llvm_readobj'], '--file-headers', paths['uwvm']])
    machine = 'EM_AARCH64' if a.target.startswith('aarch64') else 'EM_RISCV'
    if machine not in header:
        raise RuntimeError(f'target VM is not {machine}')
    wat = out / 'core3-eh-threads.wat'
    wasm = out / 'core3-eh-threads.wasm'
    wat.write_text(WAT)
    run('assemble', [paths['wasm_tools'], 'parse', wat, '-o', wasm])
    run('validate', [paths['wasm_tools'], 'validate', wasm])
    run('wasmtime', [paths['wasmtime'], '-C', 'cache=n', '-W', 'exceptions=y',
                     '-W', 'threads=y', '-W', 'shared-memory=y', wasm])
    qemu = [paths['qemu'], '-U', 'LD_LIBRARY_PATH', '-E',
            'LD_LIBRARY_PATH=' + str(a.deps / 'usr/lib' / a.target) + ':' + str(a.sysroot / 'lib'),
            '-L', a.sysroot, paths['uwvm']]
    modes = ['-Raot'] if a.ros else ['-Rcc', 'jit', '-Rcm', 'full']
    objects = []
    for policy in ('instruction', 'unwind'):
        cache = out / (policy + '-cache')
        cache.mkdir()
        captured_object = out / (policy + '.o')
        if a.test_object_capture and (captured_object.exists() or captured_object.is_symlink()):
            raise RuntimeError(f'{policy}: transient capture target already exists')
        target_qemu = (qemu[:-1] + ['-E', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT=' + str(captured_object)] + qemu[-1:]
                       if a.test_object_capture else qemu)
        compile_log = out / (policy + '.compile.log')
        args = [*modes, '-Rct', '0', '-Rllvm-call-stack', policy, '-Rllvm-full-policy', 'pb-o3',
                '-Rllvm-cache-path', 'path', cache, '-Rclog', 'file', compile_log,
                '-WFE-exceptions', '-WFE-threads', '--run', wasm]
        run(policy, [*target_qemu, *args])
        compiled = compile_log.read_text(errors='replace') if compile_log.exists() else ''
        full_lines = [line for line in compiled.splitlines() if '[llvm-jit-full] optimize-start ' in line]
        if len(full_lines) != 1:
            raise RuntimeError(f'{policy}: expected one target LLVM-full optimize-start, found {len(full_lines)}')
        fields = dict(re.findall(r'\b(call_stack|call_stack_frames|unwind_check|unwind_replace_frames)=([^\s,;]+)', full_lines[0]))
        expected_fields = {'call_stack': policy,
                           'call_stack_frames': 'emit' if policy == 'instruction' else 'omit',
                           'unwind_check': 'off' if policy == 'instruction' else 'live',
                           'unwind_replace_frames': 'yes'}
        if any(fields.get(field) != value for field, value in expected_fields.items()):
            raise RuntimeError(f'{policy}: target LLVM-full frame policy mismatch: {fields!r}')
        # QEMU user-mode -L redirects directory-relative openat/mkdirat from
        # the guest's /dev/shm into <sysroot>/dev/shm. Direct openat calls can
        # still reach host /dev/shm, so inspect both physical locations.
        redirected_cache = a.sysroot / cache.relative_to('/')
        cached = list(cache.rglob('*.uwvm-ljc')) + list(redirected_cache.rglob('*.uwvm-ljc'))
        redirected_capture = a.sysroot / captured_object.relative_to('/')
        captured_path = (captured_object if captured_object.is_file() else redirected_capture)
        captured = captured_path.is_file() and captured_path.stat().st_size != 0
        if a.test_object_capture and not captured:
            raise RuntimeError(f'{policy}: test-only MCJIT callback did not capture an actual object')
        if not cached and not captured:
            if not a.allow_cache_disabled:
                raise RuntimeError(f'{policy}: no actual JIT object; use --allow-cache-disabled only for an explicit execution-only qualification')
            objects.append({'policy': policy, 'assembly_qualified': False, 'reason': 'target JIT cache unavailable'})
            continue
        if captured:
            if cached:
                raise RuntimeError(f'{policy}: transient capture unexpectedly enabled persistent cache')
            if not any('object-cache-store-enqueue ' in line and 'status=disabled' in line
                       for line in compiled.splitlines()):
                raise RuntimeError(f'{policy}: transient capture did not fail closed on persistent cache')
            obj = captured_path
            signed_cache_replay = False
        else:
            if len(cached) != 1:
                raise RuntimeError(f'{policy}: expected one actual JIT cache object, found {len(cached)}')
            replay_log = out / (policy + '.replay.compile.log')
            replay_args = args.copy()
            replay_args[replay_args.index('-Rclog') + 2] = replay_log
            run(policy + '-replay', [*target_qemu, *replay_args])
            replay_compiled = replay_log.read_text(errors='replace') if replay_log.exists() else ''
            if not any('object-cache-hit ' in line and 'signature_verified=1' in line
                       for line in replay_compiled.splitlines()):
                raise RuntimeError(f'{policy}: target did not replay the authenticated JIT object')
            sys.path.insert(0, str(root / 'test/0014.llvm_jit'))
            from check_wasm3_native_frame_codegen import decode_object
            obj = captured_object
            obj.write_bytes(decode_object(cached[0].read_bytes(), a.ros))
            signed_cache_replay = True
        object_header = run(policy + '-elf', [paths['llvm_readobj'], '--file-headers', obj])
        sections = run(policy + '-sections', [paths['llvm_readobj'], '--sections', obj])
        relocs = run(policy + '-relocations', [paths['llvm_readobj'], '--relocations', obj])
        symbols = run(policy + '-symbols', [paths['llvm_nm'], '--defined-only', obj])
        assembly = run(policy + '-assembly', [paths['llvm_objdump'], '-dr', obj])
        if machine not in object_header:
            raise RuntimeError(f'{policy}: JIT object is not {machine}')
        if '.eh_frame' not in sections or '.gcc_except_table' not in sections:
            raise RuntimeError(f'{policy}: missing native EH/CFI sections')
        if '__gxx_personality_v0' not in relocs or 'uwvm_guest_exception_typeinfo_v1' not in relocs:
            raise RuntimeError(f'{policy}: missing native typed-EH relocations')
        function_ids = {int(value) for value in re.findall(r'\buwvm_m_[0-9a-f]+_func_(\d+)$', symbols, re.M)}
        if not {0, 1, 2} <= function_ids:
            raise RuntimeError(f'{policy}: missing actual throw, atomic, or catch function: {sorted(function_ids)}')
        atomic_function = re.search(
            r'(?ms)^[0-9a-f]+ <uwvm_m_[0-9a-f]+_func_1>:\n(.*?)(?=^[0-9a-f]+ <|\Z)', assembly)
        if atomic_function is None:
            raise RuntimeError(f'{policy}: missing actual translated atomic disassembly')
        atomic_mnemonics = re.findall(r'(?m)^\s*[0-9a-f]+:\s+[0-9a-f]+\s+([a-z][a-z0-9.]*)\b',
                                      atomic_function.group(1))
        if a.target == 'riscv64-linux-gnu':
            atomic_sequence = ['sw', 'fence', 'fence', 'lw']
            if not any(atomic_mnemonics[index:index + 4] == atomic_sequence
                       for index in range(len(atomic_mnemonics) - 3)):
                raise RuntimeError(f'{policy}: RISC-V atomic store/load sequence lacks pinned memory-order fences')
            leaf_calls = sum(opcode in ('jal', 'jalr') for opcode in atomic_mnemonics)
            if leaf_calls != (2 if policy == 'instruction' else 0):
                raise RuntimeError(f'{policy}: unexpected RISC-V atomic leaf frame-maintenance calls: {leaf_calls}')
        else:
            if not any(atomic_mnemonics[index:index + 2] == ['stlr', 'ldar']
                       for index in range(len(atomic_mnemonics) - 1)):
                raise RuntimeError(f'{policy}: AArch64 atomic store/load lost release/acquire instructions')
            leaf_calls = sum(opcode in ('bl', 'blr') for opcode in atomic_mnemonics)
            if leaf_calls != (2 if policy == 'instruction' else 0):
                raise RuntimeError(f'{policy}: unexpected AArch64 atomic leaf frame-maintenance calls: {leaf_calls}')
        trace_symbols = re.findall(r'uwvm_bridge_[0-9a-f]+', relocs)
        objects.append({'policy': policy, 'assembly_qualified': True, 'object_sha256': digest(obj),
                        'cache_sha256': digest(cached[0]) if cached else None,
                        'signed_cache_replay': signed_cache_replay,
                        'transient_test_capture': captured,
                        'function_ids': sorted(function_ids), 'frame_fields': fields,
                        'atomic_mnemonics': atomic_mnemonics,
                        'atomic_leaf_calls': leaf_calls,
                        'trace_bridge_relocations': len(trace_symbols)})
    if source_id != subprocess.check_output([sys.executable, str(root / 'tools/ci/wasm3_source_fingerprint.py'),
                                              str(root), str(out / 'source-after.json')], text=True).strip():
        raise RuntimeError('source changed during target execution')
    if {key: digest(path) for key, path in paths.items()} != json.loads((out / 'input-sha256.json').read_text()):
        raise RuntimeError('target tool or provider changed during execution')
    summary = {'passed': True, 'target': a.target, 'product': 'uwvm2-ros' if a.ros else 'uwvm2',
               'source_id': source_id, 'wasm_sha256': digest(wasm), 'objects': objects,
               'build_summary_sha256': digest(paths['build_summary']),
               'execution_policies': ['instruction', 'unwind'], 'qemu_performance_result': False}
    (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('PASS QEMU Core 3 cross-function exception and shared-memory atomic full JIT:', a.target)


if __name__ == '__main__':
    main()
