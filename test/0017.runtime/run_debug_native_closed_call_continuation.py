#!/usr/bin/env python3
"""Keeper-only public NI and abandonment witness; never a local build runner.

The sole Linux keeper must first independently qualify the fresh runtime,
initializer, host fixture and real target/publisher/SDK closure. Binary/source
hashes identify inputs and are not proof of compilation or native permissions.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import shlex
import subprocess
import sys


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def child_limits() -> None:
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    resource.setrlimit(resource.RLIMIT_FSIZE, (16 * 1024**2, 16 * 1024**2))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--binary-sha256', required=True)
    parser.add_argument('--qualified-build-record', type=Path, required=True,
                        help='Externally reviewed keeper build/link provenance; a digest alone is not build proof')
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'linux':
        raise RuntimeError('sole Linux keeper/original 64GiB cgroup only')
    root = args.source_root.resolve(strict=True)
    relative = 'test/0017.runtime/run_debug_native_closed_call_continuation.py'
    if Path(__file__).resolve(strict=True) != root / relative:
        raise RuntimeError('runner must be precisely in the qualified source cut')
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True, timeout=20)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    binary = args.binary.resolve(strict=True)
    expected = args.binary_sha256.lower()
    if not re.fullmatch('[0-9a-f]{64}', expected) or sha(binary) != expected:
        raise RuntimeError('actual fixture binary identification mismatch')
    record = args.qualified_build_record.resolve(strict=True)
    if not record.is_file() or not 0 < record.stat().st_size <= 16 * 1024**2:
        raise RuntimeError('bounded actual build provenance required; missing provenance is not PASS')
    tool = args.wasm_tools.resolve(strict=True)
    record_hash, tool_hash = sha(record), sha(tool)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    inputs = [relative, 'test/0017.runtime/debug_native_closed_call_continuation_runtime.cc',
              'test/0017.runtime/debug_native_closed_call_continuation_runtime.wat',
              'src/uwvm2/uwvm/debugger/controller.h', 'src/uwvm2/uwvm/debugger/controller.cppm',
              'src/uwvm2/uwvm/debugger/command.h', 'src/uwvm2/uwvm/debugger/console.h',
              'src/uwvm2/uwvm/debugger/native_step.h', 'src/uwvm2/uwvm/debugger/native_registers.h',
              'src/uwvm2/uwvm/debugger/native_continuation_linux.h',
              'src/uwvm2/uwvm/debugger/native_owned_instruction_semantics.h',
              'src/uwvm2/uwvm/debugger/native_target_metadata.h',
              'src/uwvm2/uwvm/debugger/native_wasm_step_boundary.h',
              'src/uwvm2/uwvm/debugger/native_disassembly.h',
              'src/uwvm2/uwvm/debugger/native_disassembly_window.h',
              'src/uwvm2/uwvm/debugger/native_next_policy.h',
              'src/uwvm2/utils/thread/cooperative_pause_domain.h',
              'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp', 'src/uwvm2/runtime/lib/uwvm_runtime.h',
              'src/uwvm2/runtime/lib/uwvm_runtime_debug_activation_capture.h',
              'src/uwvm2/runtime/lib/uwvm_runtime_debug_native_activation_api.h',
              'src/uwvm2/runtime/lib/uwvm_runtime_debug_native_code_api.h',
              'src/uwvm2/runtime/lib/uwvm_runtime_debug_native_call_continuation_api.h',
              'src/uwvm2/runtime/lib/uwvm_runtime_native_target_metadata.h',
              'src/uwvm2/runtime/lib/uwvm_runtime_native_owner_debug_permission.h',
              'src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_emit.h']
    hashes = {name: sha(root / name) for name in inputs}
    (out / 'source-inputs.json').write_text(json.dumps(hashes, indent=2) + '\n')
    (out / 'qualified-build-record.copy').write_bytes(record.read_bytes())
    wasm = out / 'fixture.wasm'
    rows = []

    def run(name: str, argv: list[str]) -> str:
        (out / (name + '.command')).write_text(shlex.join(argv) + '\n')
        with (out / (name + '.log')).open('wb') as log:
            try:
                completed = subprocess.run(argv, cwd=root, stdout=log, stderr=subprocess.STDOUT,
                                           timeout=75, preexec_fn=child_limits)
                row = {'name': name, 'argv': argv, 'exit_code': completed.returncode}
            except subprocess.TimeoutExpired:
                rows.append({'name': name, 'argv': argv, 'timeout_seconds': 75})
                (out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
                raise RuntimeError('bounded real execution timeout is FAIL') from None
        rows.append(row)
        (out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
        if completed.returncode == 77:
            raise RuntimeError('unqualified/denied kernel/backend/producer is not product PASS')
        if completed.returncode != 0:
            raise RuntimeError((out / (name + '.log')).read_text(errors='replace')[-10000:])
        return (out / (name + '.log')).read_text()

    run('official-parse', [str(tool), 'parse', str(root / inputs[2]), '-o', str(wasm)])
    run('official-validate-wasm3', [str(tool), 'validate', '--features=wasm3', str(wasm)])
    pattern = (r'returning-NI=(\d+) call-current-proved=(\d+) call-stale-proved=(\d+) '
               r'ordinary=(\d+) hidden-bridge=(\d+) SI-retained=(\d+) NI-unavailable=(\d+) '
               r'(?:integer-FP-carriers=\d+ )?'
               r'actual-NI-Wasm-abandon=(\d+) abandon-stale-proved=(\d+) result=(\d+)')
    for policy in ('instruction', 'unwind'):
        for scenario in ('return', 'exception'):
            text = run(policy + '-' + scenario, [str(binary), str(wasm), policy, scenario])
            marker = 'debug_native_closed_call_continuation_runtime: PASS policy=' + policy + ' scenario=' + scenario
            observed = re.search(pattern, text)
            if marker not in text or not observed or 'Core3-GC-memory64-return_call-try_table=yes' not in text:
                raise RuntimeError('actual current NI/Core3/abandonment witness missing')
            values = [int(observed[index]) for index in range(1, 11)]
            calls, current, stale, ordinary, hidden, retained, unavailable, abandoned, abandon_stale, result = values
            if calls < (2 if scenario == 'return' else 1) or current != calls or stale != calls or ordinary < 1 or hidden < 1 or retained < 1:
                raise RuntimeError('real NI plus hidden bridge, current-owner and old-stop proof is mandatory')
            # Event/target denial is checked for intact stop state in the fixture;
            # its count cannot substitute for a successful hardware continuation.
            if scenario == 'exception' and (abandoned < 1 or abandon_stale != abandoned):
                raise RuntimeError('genuine NI abandonment and cooperative stale-stop refusal are mandatory')
            if result != (106 if scenario == 'return' else 4294967294):
                raise RuntimeError('actual guest result mismatch')
    if (hashes != {name: sha(root / name) for name in inputs} or sha(binary) != expected or
            sha(record) != record_hash or sha(tool) != tool_hash):
        raise RuntimeError('source/binary/build-record/tool changed during qualification')
    manifest = {'scope': 'actual NI under both stack policies; hidden bridge privacy, current caller/generation/epoch and stale-stop proof; NI abandonment during exception workload, reset/worker ACK, modern Core3 execution',
                'call_operand_class_qualified': False,
                'exact_VM_entry_negative_matrix_qualified': False,
                'exact_EH_triggered_abandonment_qualified': False,
                'finish_cross_owner_other_platforms_qualified': False,
                'binary': str(binary), 'binary_sha256': expected,
                'qualified_build_record_sha256': record_hash, 'wasm_tools_sha256': tool_hash,
                'wasm_sha256': sha(wasm), 'inputs': hashes, 'rows': rows}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('PASS actual public NI/abandonment instruction+unwind; call operand class, exact EH cause and other-platform matrix unqualified', flush=True)


if __name__ == '__main__':
    main()
