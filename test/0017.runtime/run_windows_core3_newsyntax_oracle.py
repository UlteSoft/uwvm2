#!/usr/bin/env python3
"""Bind four Core 3 Windows fixtures to wasm-tools and Wasmtime outcomes."""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import shutil
import subprocess
import sys


CASES = {
    'memory64': 'test/0017.runtime/fixtures/core3_memory64_scalar_execution.wat',
    'relaxed-simd': 'test/0017.runtime/fixtures/core3_relaxed_simd_swizzle_execution.wat',
    'exnref-table64': 'test/0017.runtime/fixtures/exnref_table64_execution.wat',
    'atomic-fence': 'test/0014.llvm_jit/fixtures/threads_fence.wat',
}
WASM_TOOLS_SHA256 = '115d5986a8a1aeb112a5f2d98209c144f70c26188a5d41e266698a1e20de4ed1'
WASMTIME_SHA256 = '9f3f3e1b1b048f802c1d35a17607c064425a9c746f00bda3c2466b92ddcb5c92'


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def events() -> dict[str, int]:
    return {name: int(value) for name, value in
            (line.split() for line in Path('/sys/fs/cgroup/memory.events').read_text().splitlines())}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--source-id', required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.source_root.resolve(strict=True)
    wasm_tools = args.wasm_tools.resolve(strict=True)
    wasmtime = args.wasmtime.resolve(strict=True)
    if digest(wasm_tools) != WASM_TOOLS_SHA256 or digest(wasmtime) != WASMTIME_SHA256:
        raise RuntimeError('official Wasm oracle tool hash differs')
    if (Path('/sys/fs/cgroup/memory.max').read_text().strip() != str(64 << 30) or
            Path('/sys/fs/cgroup/memory.swap.max').read_text().strip() != '0' or
            Path('/sys/fs/cgroup/cpuset.cpus.effective').read_text().strip() != '0,2,4,6,16-31'):
        raise RuntimeError('Core 3 oracle is outside the required 64 GiB/20 CPU cgroup')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    output = args.output.resolve()
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    source_id = subprocess.check_output(
        [sys.executable, str(source / 'tools/ci/wasm3_source_fingerprint.py'),
         str(source), str(output / 'source-before.json')], text=True).strip()
    if source_id != args.source_id:
        raise RuntimeError(f'fixture source changed: {source_id}')
    before = events()
    rows = []
    for stem, relative in CASES.items():
        wat = source / relative
        staged_wat = output / (stem + '.wat')
        staged_wasm = output / (stem + '.wasm')
        shutil.copyfile(wat, staged_wat)
        commands = (
            [str(wasm_tools), 'parse', str(staged_wat), '-o', str(staged_wasm)],
            [str(wasm_tools), 'validate', '--features', 'all', str(staged_wasm)],
            [str(wasmtime), 'run', '-C', 'cache=n', '-W', 'exceptions=y', '-W', 'gc=y',
             str(staged_wasm)],
        )
        checks = []
        for index, command in enumerate(commands):
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    timeout=30, check=False)
            log = output / f'{stem}-step{index}.log'
            log.write_bytes(result.stdout)
            checks.append({'command': command, 'exit_code': result.returncode,
                           'log_sha256': digest(log)})
            if result.returncode:
                raise RuntimeError(f'{stem} oracle step {index} failed: {result.stdout[-800:]!r}')
        rows.append({'stem': stem, 'source_relative': relative,
                     'wat_sha256': digest(staged_wat),
                     'wasm_sha256': digest(staged_wasm), 'checks': checks})
    after = events()
    if any(after[key] != before[key] for key in ('oom', 'oom_kill')):
        raise RuntimeError('cgroup OOM while running official Core 3 oracle')
    source_after = subprocess.check_output(
        [sys.executable, str(source / 'tools/ci/wasm3_source_fingerprint.py'),
         str(source), str(output / 'source-after.json')], text=True).strip()
    if source_after != source_id:
        raise RuntimeError('source changed during official Core 3 oracle')
    summary = {'schema': 1, 'status': 'official-oracle-passed', 'source_id': source_id,
               'wasm_tools_sha256': digest(wasm_tools), 'wasmtime_sha256': digest(wasmtime),
               'cgroup_memory_max': str(64 << 30), 'cgroup_swap_max': '0',
               'cgroup_cpuset': '0,2,4,6,16-31',
               'cgroup_events_before': before, 'cgroup_events_after': after,
               'fixtures': rows}
    summary_file = output / 'summary.json'
    summary_file.write_text(json.dumps(summary, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'passed': True, 'source_id': source_id, 'checks': len(rows) * 3,
                      'summary_sha256': digest(summary_file)}, sort_keys=True))


if __name__ == '__main__':
    main()
