#!/usr/bin/env python3
"""Actual integer-only GC read ABI equivalence in the remote Linux cgroup.

The native body is extracted verbatim from the reviewed emitter candidate.
Real selected module/store headers are recompiled; no old product object or
generated substitute is linked. LLVM lowering and whole-VM P-core timing are
separate gates, including explicit template-instance symbol discrimination.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import resource
import shutil
import sys


START = "template<::std::uint_least32_t FixedOpcode = 0xffff'ffffu, ::std::size_t FixedInputs = SIZE_MAX>"
STOP = "[[nodiscard]] inline ::std::uintptr_t llvm_jit_gc_input_allocate_bridge"
SCOPE = "exact native scalar32/fixed-buffer ABI equivalence with actual module/store headers; wholeVM=false"


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError('required source-bound control module is missing')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--source-id', required=True)
    parser.add_argument('--candidate-header', type=Path, required=True)
    parser.add_argument('--candidate-sha256', required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cxx', default='/toolchain/bin/clang++')
    parser.add_argument('--objdump', default='/toolchain/bin/llvm-objdump')
    parser.add_argument('--tsan', action='store_true')
    parser.add_argument('--command-budget-gib', type=int, choices=(2, 3), default=2)
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    own = Path(__file__).resolve().parents[2]
    candidate = args.candidate_header.resolve(strict=True)
    if sha(candidate) != args.candidate_sha256:
        raise RuntimeError('reviewed emitter candidate SHA drifted')
    source = candidate.read_text()
    if source.count(START) != 1 or source.count(STOP) != 1:
        raise RuntimeError('native bridge extraction markers are not unique')
    begin, end = source.index(START), source.index(STOP)
    body = source[begin:end]
    if begin >= end or '::llvm::' in body or body.count('llvm_jit_gc_struct_get32_bridge(') != 1:
        raise RuntimeError('extraction does not contain exactly the reviewed native helper')
    controls_path = own / 'tools/ci/run_core3_component_bounded_slot.py'
    reader_path = own / 'test/0014.llvm_jit/run_native_unwind_noexcept_abi.py'
    bounded_path = own / 'test/0017.runtime/run_exception_compact_trace.py'
    controls = load(controls_path, 'uwvm_scalar32_owned_process_tree')
    reader = load(reader_path, 'uwvm_scalar32_cgroup_state')
    bounded = load(bounded_path, 'uwvm_scalar32_bounded_native_invoke')
    before = reader.cgroup_state(root)
    if any(before['events'].get(key, 0) for key in ('oom', 'oom_kill')):
        raise RuntimeError('preceding shared OOM invalidates this native component gate')
    if 16 not in os.sched_getaffinity(0):
        raise RuntimeError('established E16 CPU is unavailable')
    os.sched_setaffinity(0, {16})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = args.out.resolve()
    if any(out.is_relative_to(root / part) for part in ('src', 'third-parties')):
        raise RuntimeError('qualification must not write frozen source inputs')
    out.mkdir(parents=True, exist_ok=False)
    generated = out / 'native_gc_aggregate_bridge.h'
    generated.write_text(body)
    fixture = own / 'test/0017.runtime/wasm3_gc_scalar32_bridge.cc'
    compiler = Path(shutil.which(args.cxx) or args.cxx).absolute().resolve(strict=True)
    store = root / 'src/uwvm2/uwvm/runtime/storage/gc_object.h'
    module_header = root / 'src/uwvm2/uwvm/runtime/storage/wasm_module.h'
    pinned = [candidate, fixture, Path(__file__).resolve(), controls_path, reader_path, bounded_path,
        generated, compiler, store, module_header, root / 'tools/ci/wasm3_source_fingerprint.py']
    inputs = {str(path): sha(path) for path in pinned}
    summary = {'passed': False, 'scope': SCOPE, 'whole_vm_qualified': False,
        'performance_qualified': False, 'source_id': args.source_id, 'cgroup_before': before,
        'inputs_before': inputs, 'commands': [], 'profiles': [],
        'command_budget_bytes': args.command_budget_gib * 1024**3}
    (out / 'inputs.json').write_text(json.dumps(summary, indent=2) + '\n')

    def run(name, command, env=None):
        row = bounded.bounded_invoke(command, out, name, root, controls, reader.cgroup_state, env,
            memory_budget_bytes=args.command_budget_gib * 1024**3)
        summary['commands'].append(row)
        (out / 'commands.json').write_text(json.dumps(summary['commands'], indent=2) + '\n')
        if row['exit'] != 0 or row['failure'] is not None:
            raise RuntimeError(f'{name} failed; preserve actual log {row["log"]}')

    def fingerprint(stage):
        path = out / f'source-fingerprint-{stage}.json'
        run(f'source-fingerprint-{stage}', [sys.executable, root / 'tools/ci/wasm3_source_fingerprint.py', root, path])
        if json.loads(path.read_text())['source_id'] != args.source_id:
            raise RuntimeError('selected source tree differs from the reviewed source ID')

    native_defines = ['-DUWVM=2', '-DUWVM_TEST=2', '-DUWVM_USE_DEFAULT_INT',
        '-DUWVM_DISABLE_JIT', '-DUWVM_DISABLE_DEBUG_INT', '-DUWVM_USE_THREAD_LOCAL',
        '-DUWVM_VERSION_X=2', '-DUWVM_VERSION_Y=0', '-DUWVM_VERSION_Z=4', '-DUWVM_VERSION_S=0',
        '-DNDEBUG', '-DUWVM_MODE_RELEASE']
    includes = [out, root / 'src', root / 'third-parties/fast_io/include',
        root / 'third-parties/bizwen/include', root / 'third-parties/boost_unordered/include']
    if not all(directory.is_dir() for directory in includes):
        raise RuntimeError('actual module/container include closure is incomplete')
    flags = [args.cxx, '-std=c++26', '-stdlib=libc++', *native_defines, '-fno-rtti', '-pthread',
        '-fstack-clash-protection', '-mstack-probe-size=4096',
        *[part for directory in includes for part in ('-I', str(directory))]]
    link = ['-fuse-ld=lld', '-rtlib=compiler-rt', '-unwindlib=libunwind']
    profiles = [('o3', ['-O3'], dict(os.environ)),
        ('asan-ubsan', ['-O1', '-g1', '-fno-omit-frame-pointer', '-fsanitize=address,undefined'],
            dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',
                UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))]
    if args.tsan:
        profiles.append(('tsan', ['-O1', '-g1', '-fsanitize=thread'],
            dict(os.environ, TSAN_OPTIONS='halt_on_error=1:second_deadlock_stack=1')))
    try:
        fingerprint('before')
        run('compiler-version', [args.cxx, '--version'])
        for name, options, environment in profiles:
            executable = out / name
            dependency = out / f'{name}.d'
            run(name + '-build', flags + options + ['-MD', '-MF', dependency, fixture, '-o', executable] + link)
            dependencies = dependency.read_text().replace('\\\n', ' ')
            if str(generated) not in dependencies or str(module_header) not in dependencies or str(store) not in dependencies:
                raise RuntimeError('actual compiled dependency closure does not include the selected module/store and exact native body')
            run(name + '-run', [executable], environment)
            summary['profiles'].append({'name': name, 'executable_sha256': sha(executable),
                'dependency_sha256': sha(dependency)})
        assembly = out / 'scalar32.native.o3.s'
        run('o3-assembly', flags + ['-O3', '-S', fixture, '-o', assembly])
        run('o3-native-disassembly', [args.objdump, '--demangle', '--disassemble', out / 'o3'])
        summary['assembly_sha256'] = sha(assembly)
        summary['assembly_review'] = 'pending; follow actual scalar and fixed-buffer template bodies'
        fingerprint('after')
        if {str(path): sha(path) for path in pinned} != inputs:
            raise RuntimeError('a reviewed input changed during qualification')
        after = reader.cgroup_state(root)
        if any(after['events'].get(key, 0) != before['events'].get(key, 0) for key in ('oom', 'oom_kill')):
            raise RuntimeError('shared OOM evidence changed')
        summary['cgroup_after'] = after
        summary['passed'] = True
    except BaseException as error:
        summary['failure'] = str(error)
        raise
    finally:
        (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('PASS exact native scalar32 bridge equivalence; LLVM/fullVM/P-core gates remain')


if __name__ == '__main__':
    main()
