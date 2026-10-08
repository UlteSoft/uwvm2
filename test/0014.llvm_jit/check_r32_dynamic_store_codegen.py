#!/usr/bin/env python3
"""Inspect ordinary full-JIT i32.store machine code from its signed cache."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess

from check_wasm3_native_frame_codegen import decode_object


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], log: Path) -> str:
    completed = subprocess.run(command, capture_output=True, text=True, timeout=120)
    output = completed.stdout + completed.stderr
    log.write_text(output)
    if completed.returncode:
        raise RuntimeError(f'{command!r} exited {completed.returncode}: {log}')
    return output


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--uwvm', type=Path, required=True)
    p.add_argument('--build-json', type=Path, required=True)
    p.add_argument('--fixture', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--llvm-objdump', type=Path, default=Path('/toolchain/bin/llvm-objdump'))
    p.add_argument('--llvm-nm', type=Path, default=Path('/toolchain/bin/llvm-nm'))
    a = p.parse_args()
    source = a.source.resolve(strict=True)
    subprocess.run(['bash', str(source / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    events_before = (Path('/sys/fs/cgroup') / 'memory.events').read_text()
    a.out.mkdir(parents=True, exist_ok=False)
    source_before = subprocess.check_output(['python3', str(source / 'tools/ci/wasm3_source_fingerprint.py'),
                                              str(source), str(a.out / 'source-before.json')], text=True).strip()
    build = json.loads(a.build_json.read_text())
    if (build['source_id'] != source_before or digest(a.uwvm) != build['binary_sha256']
            or Path(build['source']).resolve() != source):
        raise RuntimeError('ordinary JIT executable is not the frozen source-bound O3 build')
    rows = []
    for policy in ('instruction', 'unwind'):
        directory = a.out / policy
        directory.mkdir()
        cache = directory / 'cache'
        cache.mkdir()
        command = [str(a.uwvm), '-Rcc', 'jit', '-Rcm', 'full', '-Rct', '0',
                   '-Rllvm-full-policy', 'pb-o3', '-Rllvm-call-stack', policy,
                   '-Rllvm-cache-path', 'path', str(cache), '--run', str(a.fixture)]
        (directory / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
        run(command, directory / 'run.log')
        cached = list(cache.rglob('*.uwvm-ljc'))
        if len(cached) != 1:
            raise RuntimeError(f'expected one actual signed-cache object for {policy}, got {len(cached)}')
        native = directory / 'native.o'
        native.write_bytes(decode_object(cached[0].read_bytes(), False))
        symbols = run([str(a.llvm_nm), '--defined-only', str(native)], directory / 'symbols.txt')
        found = re.findall(r'\b(uwvm_m_[0-9a-f]+_func_0)$', symbols, re.MULTILINE)
        if len(found) != 1:
            raise RuntimeError(f'cannot identify dynamic-store Wasm function: {found}')
        name = found[0]
        assembly = run([str(a.llvm_objdump), '-dr', '--disassemble-symbols=' + name,
                        str(native)], directory / 'store-func0.s')
        if 'file format elf64-x86-64' not in assembly:
            raise RuntimeError('this native store audit requires an x86-64 object')
        body = assembly.split('<' + name + '>:', 1)[1]
        compare = re.search(r'\bcmp\w*\b[^\n]*0xfffd', body)
        branch = re.search(r'\b(jb|jae)\b', body)
        probe = re.search(r'\bmovzb\w*\b[^\n]*0x3\(', body)
        store = re.search(r'\bmovl\b[^\n]*0x11223344[^\n]*\(', body)
        if not all((compare, branch, probe, store)):
            raise RuntimeError(f'missing page compare/branch/probe/store in {directory / "store-func0.s"}')
        assert compare is not None and branch is not None and probe is not None and store is not None
        if not (compare.start() < branch.start() < probe.start() < store.start()):
            raise RuntimeError(f'wrong first-fault order in {directory / "store-func0.s"}')
        hot = body[branch.start():store.end()]
        if (re.search(r'\b(call\w*|lock\w*|\w*fence\w*)\b', hot)
                or 'memory0_length' in body or 'memory0_begin' not in body):
            raise RuntimeError(f'hot store has helper/lock/length load or lacks memory base: {directory / "store-func0.s"}')
        if policy == 'unwind' and re.search(r'\bcall\w*\b', body):
            raise RuntimeError('native unwind store leaf unexpectedly has an instruction-frame helper')
        rows.append({'policy': policy, 'cache_object_sha256': digest(cached[0]),
                     'native_object_sha256': digest(native), 'assembly_sha256': digest(directory / 'store-func0.s'),
                     'symbol': name, 'first_fault_order': ['page compare', 'interior skip',
                                                            'last-byte probe', 'single 32-bit store'],
                     'no_length_load_or_hot_helper_lock_fence': True})
    source_after = subprocess.check_output(['python3', str(source / 'tools/ci/wasm3_source_fingerprint.py'),
                                             str(source), str(a.out / 'source-after.json')], text=True).strip()
    if source_after != source_before:
        raise RuntimeError('production source changed during native store audit')
    events_after = (Path('/sys/fs/cgroup') / 'memory.events').read_text()
    def event_value(contents: str, key: str) -> int:
        return int(next(line.split()[1] for line in contents.splitlines() if line.startswith(key + ' ')))
    for key in ('oom', 'oom_kill'):
        if event_value(events_before, key) != event_value(events_after, key):
            raise RuntimeError(f'cgroup {key} increased during native store audit')
    report = {'passed': True, 'source_id': source_before, 'uwvm_sha256': digest(a.uwvm),
              'build_json_sha256': digest(a.build_json),
              'fixture_sha256': digest(a.fixture), 'runner_sha256': digest(Path(__file__)),
              'cgroup_memory_max': Path('/sys/fs/cgroup/memory.max').read_text().strip(),
              'cgroup_swap_max': Path('/sys/fs/cgroup/memory.swap.max').read_text().strip(),
              'cpuset': Path('/sys/fs/cgroup/cpuset.cpus.effective').read_text().strip(),
              'cgroup_events_before': events_before, 'cgroup_events_after': events_after,
              'checks': rows}
    (a.out / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS two ordinary r32 signed-cache native i32.store leaves: first-fault and single interior store')


if __name__ == '__main__':
    main()
