#!/usr/bin/env python3
"""Actual product direct numeric source parameters, official embedded DWARF only.

Requires new product/debug-DWARF/runtime closure, not a frozen Win/r11 binary.
Prior Stage1 fixtures must have passed official compiler + wasm-tools +
llvm-dwarfdump verification. Other locations remain explicitly unavailable.
This does not qualify memory/pointer/aggregate/caller/native-register variables.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import resource
import subprocess
import sys
import run_debug_source_inline_metadata_cli as metadata_cli
from run_debug_source_inline_metadata_cli import Console, function, variant, sha


def qualify(prefix: list[str], wasm: Path, export: str, expected_value: int, log: Path,
            unavailable: bytes | None = None, replacement: Path | None = None,
            native: bool = False) -> dict:
    target, body = function(wasm, export)
    if replacement is not None:
        replacement.write_bytes(body)  # exact officially valid same-signature body
    console = Console([*prefix, '--run', str(wasm)], log)
    seen_value = seen_unavailable = False
    try:
        initial = console.send('locals source 1')
        if b'source parameter ' in initial or b'source local ' in initial:
            raise AssertionError(('initial stop has no actual frame', initial))
        if replacement is not None:
            reply = console.send(f'replace 0 {target} 1 {replacement}')
            if b'function replaced; generation 2' not in reply:
                raise AssertionError(reply)
        if b'breakpoint' not in console.send(f'break 0 {target} 0'):
            raise AssertionError('actual emitted entry safe point missing')
        console.send('continue')
        stopped = b''
        for _ in range(20):
            stopped = console.send('wait')
            if b'stopped: breakpoint' in stopped:
                break
        else:
            raise AssertionError(stopped)
        match = re.search(rb'thread (\d+) module=0 function=' + str(target).encode() + rb' byte-offset=', stopped)
        if match is None:
            raise AssertionError(stopped)
        participant = int(match.group(1))
        for _ in range(128):
            trace = console.send(f'bt {participant}')
            current = re.search(rb'thread \d+ module=0 function=(\d+) byte-offset=', trace)
            if current is None or int(current.group(1)) != target:
                break
            values = console.send(f'locals source {participant}')
            # Repeating the real paused query must neither resume nor mutate the
            # copied snapshot; each query reauthenticates the captured ticket.
            if values != console.send(f'locals source {participant}'):
                raise AssertionError(('same-stop source values unstable', values))
            if unavailable is not None:
                if b'source parameter ' in values or b'source local ' in values:
                    raise AssertionError(('old/unbound source value exposed', values))
                seen_unavailable |= unavailable in values
            else:
                parameter = re.search(rb'(?m)^source parameter value type=[^\r\n]* = i32=(-?\d+)\r?$', values)
                if parameter is not None:
                    if int(parameter.group(1)) != expected_value:
                        raise AssertionError(('actual immutable formal parameter differs from fixture', values))
                    seen_value = True
            if seen_value and native:
                stepped = console.send(f'step asm {participant}')
                if b'native instruction 0x' not in stepped:
                    raise AssertionError(('native backend did not actually step', stepped))
                values = console.send(f'locals source {participant}')
                if b'native trap has no current cooperative local snapshot' not in values or b'source parameter ' in values:
                    raise AssertionError(('native trap reused old cooperative values', values))
                break
            stepped = console.send(f'step wasm {participant}')
            if b'guest exited:' in stepped:
                break
        if unavailable is None and not seen_value:
            raise AssertionError(('no actual direct formal parameter value observed; fixture may use an unsupported location', wasm,
                                  bytes(console.transcript)[-6000:]))
        if unavailable is not None and not seen_unavailable:
            raise AssertionError(('explicit unavailable reason not observed', unavailable, bytes(console.transcript)[-6000:]))
        return {'passed': True, 'wasm_sha256': sha(wasm), 'export': export, 'function_index': target,
                'expected_parameter': None if unavailable is not None else expected_value,
                'replacement': replacement is not None, 'native_step': native, 'log': str(log),
                'actual_command': console.command}
    finally:
        console.finish()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--fixture-dir', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--native-step', action='store_true')
    args = parser.parse_args()
    if sys.platform != 'linux':
        raise RuntimeError('execute only in keeper controlled Linux cgroup')
    root = args.source_root.resolve(strict=True)
    if Path(__file__).resolve(strict=True) != root / 'test/0017.runtime/run_debug_source_dwarf_values_cli.py' or \
       Path(metadata_cli.__file__).resolve(strict=True) != root / 'test/0017.runtime/run_debug_source_inline_metadata_cli.py':
        raise RuntimeError('actual runner/import must come from precisely --source-root')
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    fixture_dir = args.fixture_dir.resolve(strict=True)
    binary = args.uwvm.resolve(strict=True)
    source_names = ['src/uwvm2/uwvm/debugger/' + name for name in
                    ('source_dwarf_types.h', 'source_dwarf_index.h', 'source_dwarf_query.h', 'source_dwarf_values.h',
                     'command.h', 'controller.h', 'console.h')]
    fixture_sources = {'c': 'debug_source_meta_c.c', 'cpp': 'debug_source_meta_cpp.cc', 'rust': 'debug_source_meta_rust.rs'}
    source_names += ['test/0017.runtime/fixtures/' + name for name in fixture_sources.values()]
    source_names += ['src/uwvm2/uwvm/debugger/' + name for name in
                     ('source_dwarf_types.cppm', 'source_dwarf_index.cppm')]
    source_names += ['src/uwvm2/runtime/lib/uwvm_runtime.h', 'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp',
                     'src/uwvm2/runtime/lib/uwvm_runtime_debug_source_api.h',
                     'test/0017.runtime/run_debug_source_dwarf_values_cli.py',
                     'test/0017.runtime/run_debug_source_inline_metadata_cli.py']
    sources = {name: sha(root / name) for name in source_names}
    product_sha = sha(binary)
    fixture_receipt = fixture_dir / 'summary.json'
    fixture_receipt_sha = sha(fixture_receipt)
    wasm_tool = args.wasm_tools.resolve(strict=True)
    wasm_tool_sha = sha(wasm_tool)
    receipt = json.loads(fixture_receipt.read_text())
    if receipt.get('stage') != 'embedded-metadata-only':
        raise RuntimeError('actual Stage1 official compiler/verification receipt required')
    components = receipt.get('components', {})
    for name in ('source_dwarf_types.h', 'source_dwarf_types.cppm', 'source_dwarf_index.h', 'source_dwarf_index.cppm'):
        if components.get(name) != sha(root / 'src/uwvm2/uwvm/debugger' / name):
            raise RuntimeError('Stage1 parser component is stale: ' + name)
    verified = {row['name']: row for row in receipt['fixtures']}
    official_commands = receipt.get('commands', [])
    fixture_inputs = {}
    rows = []
    for policy in ('instruction', 'unwind'):
        mode = ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
        prefix = [str(binary), '-m', 'debug-jit', *mode, '-Rct', '0', '-Rllvm-call-stack', policy,
                  '-Rllvm-exception-dispatch', 'native-unwind', '-Rllvm-cache-path', 'disable']
        for language in ('c', 'cpp', 'rust'):
            for version in (4, 5):
                name = f'{language}-dwarf{version}-O1'
                wasm = fixture_dir / (name + '.wasm')
                if name not in verified or sha(wasm) != verified[name]['wasm_sha256'] or \
                   verified[name].get('source_sha256') != sources['test/0017.runtime/fixtures/' + fixture_sources[language]]:
                    raise RuntimeError('official fixture/source receipt/hash mismatch: ' + name)
                for expected_log in ('compile-' + name + '.log', 'wasm-validate-' + name + '.log',
                                     'dwarf-verify-' + name + '.log', 'dwarf-oracle-' + name + '.log'):
                    matched = [row for row in official_commands if row.get('log') == expected_log]
                    if len(matched) != 1 or matched[0].get('returncode') != 0 or not (fixture_dir / expected_log).is_file():
                        raise RuntimeError('missing successful actual official-tool command/log: ' + expected_log)
                    if not matched[0].get('argv'):
                        raise RuntimeError('missing actual official-tool argv: ' + expected_log)
                    fixture_inputs[str(fixture_dir / expected_log)] = sha(fixture_dir / expected_log)
                fixture_inputs[str(wasm)] = sha(wasm)
                subprocess.run([str(wasm_tool), 'validate', str(wasm)], check=True)
                rows.append(qualify(prefix, wasm, 'source_outer_' + language, 7 if language == 'rust' else 5,
                                    out / f'{policy}-{language}-v{version}.log'))
        baseline = fixture_dir / 'c-dwarf5-O1.wasm'
        rows.append(qualify(prefix, baseline, 'source_outer_c', 5, out / f'{policy}-replacement.log',
                            b'captured stop, source or function generation is stale', out / f'{policy}-same.body'))
        for kind, reason in (('missing', b'no embedded metadata bound to the live code owner'),
                             ('malformed', b'embedded DWARF metadata is invalid or unsupported'),
                             ('external', b'embedded DWARF metadata is invalid or unsupported')):
            wasm = out / (kind + '.wasm')
            if not wasm.exists():
                variant(baseline, wasm, kind)
            subprocess.run([str(wasm_tool), 'validate', str(wasm)], check=True)
            rows.append(qualify(prefix, wasm, 'source_outer_c', 5, out / f'{policy}-{kind}.log', reason))
        if args.native_step:
            rows.append(qualify(prefix, baseline, 'source_outer_c', 5, out / f'{policy}-native.log', native=True))
    after = {name: sha(root / name) for name in source_names}
    if after != sources or sha(binary) != product_sha or sha(fixture_receipt) != fixture_receipt_sha or \
       sha(wasm_tool) != wasm_tool_sha or any(sha(Path(path)) != digest for path, digest in fixture_inputs.items()):
        raise RuntimeError('source/product/tool/official receipt or fixture input changed during qualification')
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    (out / 'summary.json').write_text(json.dumps({'passed': True, 'product_sha256': product_sha, 'source_before': sources,
         'source_after': after, 'source_before_equals_after': True, 'official_fixture_receipt_sha256': fixture_receipt_sha,
        'wasm_tools_sha256': wasm_tool_sha, 'official_fixture_inputs_pre_equals_post': fixture_inputs, 'cases': rows,
        'scope': 'Actual current cooperative direct numeric formal parameter values with embedded C/C++/Rust DWARF4/5 O1.',
        'kernel_native_step_qualified': args.native_step, 'caller_or_memory_or_native_register_values': False,
        'source_build_closure_qualified_by_runner': False}, indent=2) + '\n')
    print('PASS actual direct numeric source parameters and source/stop/replacement/native-unavailable boundaries')


if __name__ == '__main__':
    main()
