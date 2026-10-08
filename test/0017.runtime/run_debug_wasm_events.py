#!/usr/bin/env python3
"""Keeper-only actual Core3 Wasm event/catch/script/trace test, using a fresh binary.

This runner never builds a native TU. The keeper MUST qualify the production
producer/runtime/CLI+host consumer layout as one fresh source closure first.
An input binary digest below identifies the artifact and is not build proof.
No local execution or platform result is implied by this source file.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import resource
import shlex
import subprocess
import sys


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--binary', type=Path, required=True,
                        help='Fresh real debug_wasm_events_runtime host fixture binary')
    parser.add_argument('--binary-sha256', required=True)
    parser.add_argument('--qualified-build-record', type=Path, required=True,
                        help='Keeper-owned build provenance record for fresh all-TU/macro closure; externally reviewed')
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'linux':
        raise RuntimeError('sole remote Linux/cgroup runner only; other platforms require their qualified harness')
    root = args.source_root.resolve(strict=True)
    relative = 'test/0017.runtime/run_debug_wasm_events.py'
    if Path(__file__).resolve(strict=True) != root / relative:
        raise RuntimeError('runner must come from precisely the qualified --source-root')
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    binary = args.binary.resolve(strict=True)
    expected = args.binary_sha256.lower()
    if len(expected) != 64 or any(c not in '0123456789abcdef' for c in expected) or sha(binary) != expected:
        raise RuntimeError('fixture binary identification mismatch')
    build_record = args.qualified_build_record.resolve(strict=True)
    if not build_record.is_file() or build_record.stat().st_size == 0:
        raise RuntimeError('externally qualified actual build record is required')
    tool = args.wasm_tools.resolve(strict=True)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    inputs = [relative, 'test/0017.runtime/debug_wasm_events_runtime.cc',
              'test/0017.runtime/debug_wasm_events_runtime.wat',
              'src/uwvm2/uwvm/debugger/controller.h', 'src/uwvm2/uwvm/debugger/controller.cppm',
              'src/uwvm2/uwvm/debugger/command.h', 'src/uwvm2/uwvm/debugger/native_step.h',
              'src/uwvm2/uwvm/debugger/native_step_windows.h', 'src/uwvm2/uwvm/debugger/native_step_macos.h',
              'src/uwvm2/uwvm/debugger/native_registers.h', 'src/uwvm2/uwvm/debugger/native_disassembly.h',
              'src/uwvm2/uwvm/debugger/native_owned_instruction_semantics.h',
              'src/uwvm2/uwvm/debugger/native_instruction_semantics.h',
              'src/uwvm2/uwvm/debugger/native_next_policy.h', 'src/uwvm2/uwvm/debugger/console.h',
              'src/uwvm2/uwvm/debugger/wasm_events.h',
              'src/uwvm2/utils/thread/cooperative_pause_domain.h',
              'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp', 'src/uwvm2/runtime/lib/uwvm_runtime.h',
              'src/uwvm2/runtime/lib/uwvm_runtime_debug_native_code_api.h',
              'src/uwvm2/runtime/lib/uwvm_runtime_debug_activation_capture.h',
              'src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_emit.h']
    hashes = {name: sha(root / name) for name in inputs}
    (out / 'source-inputs.json').write_text(json.dumps(hashes, indent=2) + '\n')
    (out / 'qualified-build-record.copy').write_bytes(build_record.read_bytes())
    wasm = out / 'fixture.wasm'
    rows = []

    def run(name: str, argv: list[str], timeout: int = 45) -> None:
        (out / (name + '.command')).write_text(shlex.join(argv) + '\n')
        with (out / (name + '.log')).open('wb') as log:
            result = subprocess.run(argv, cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
        row = {'name': name, 'argv': argv, 'exit_code': result.returncode}
        rows.append(row)
        (out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
        if result.returncode == 77:
            raise RuntimeError('unavailable host adapter SKIP cannot be counted as a product platform pass')
        if result.returncode != 0:
            raise RuntimeError((out / (name + '.log')).read_text(errors='replace')[-10000:])

    run('official-parse', [str(tool), 'parse', str(root / inputs[2]), '-o', str(wasm)])
    run('official-validate', [str(tool), 'validate', str(wasm)])
    for policy in ('instruction', 'unwind'):
        name = policy + '-actual-Core3-events-catch-script-trace'
        run(name, [str(binary), str(wasm), policy])
        text = (out / (name + '.log')).read_text()
        marker = 'debug_wasm_events_runtime: PASS actual Core3 Wasm events policy=' + policy
        if (marker not in text or 'real-gc-and-throw-catchpoints=2 before-instruction=yes' not in text or
            'mem64-table64-gc-EH-tailcall-simd=yes' not in text):
            raise RuntimeError('actual Core3 callback/catch/opcode witness missing')
        # Cross-check emitted actual host counts, never manufacture a trace from
        # transcript text or interpret a software opcode component as execution.
        import re
        observed = re.search(r'records=(\d+) pages=(\d+)', text)
        if not observed or int(observed[1]) <= 3 or int(observed[2]) < 2:
            raise RuntimeError('missing actual Core3/pagination coverage is FAIL')
    if hashes != {name: sha(root / name) for name in inputs} or sha(binary) != expected:
        raise RuntimeError('source/binary changed during actual qualification')
    manifest = {'scope': 'actual full Core3 Wasm callbacks/catchpoints/scripts/trace pagination with GC memory64 table64 SIMD EH and tailcall; before-instruction metadata is not committed GC/catch effects',
                'binary': str(binary), 'binary_sha256': expected,
                'qualified_build_record_sha256': sha(build_record), 'wasm_tools_sha256': sha(tool),
                'inputs': hashes, 'rows': rows}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('PASS actual Core3 Wasm events/catch/script/trace, instruction/unwind, fresh runtime/producer/host closure', flush=True)


if __name__ == '__main__':
    main()
