#!/usr/bin/env python3
"""Replay the pinned r32 ROS macOS acceptance on a new source-matched arm64 VM.

The final VM must first be linked from an arm64 runtime object cross-compiled
inside the Linux 64 GiB cgroup. Every macOS execution here is watched at the
4 GiB process-tree RSS limit; this script itself never builds the product.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


LIMIT = 4 * 1024**3
BASELINE_ROWS = (
    ('newsyntax-jit', 'uwvm-r32-ros-macos-ros-newsyntax-jit/summary.json'),
    ('newsyntax-int', 'uwvm-r32-ros-macos-ros-newsyntax-int/summary.json'),
    ('core3-initializers', 'table-initializer-matrix/summary.json'),
)
SCRIPTS = (
    ('sources', 'run_native_step_macos_sources_cli.py'),
    ('debug-step', 'run_native_step_macos_debug_cli.py'),
    ('control-fd', 'run_llvm_debug_control_fd_macos.py'),
    ('server', 'run_llvm_debug_server_macos.py'),
    ('sealed', 'run_llvm_debug_sealed_macos_cli.py'),
    ('gc-typed', 'run_llvm_debug_gc_typed_replacement.py'),
    ('hotreplace-unwind', 'run_hot_replace_macos_debug_cli.py'),
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(path: Path, value: object) -> None:
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def fingerprint(source: Path, manifest: Path) -> str:
    return subprocess.check_output(
        [sys.executable, str(source / 'tools/ci/wasm3_source_fingerprint.py'),
         str(source), str(manifest)], text=True).strip()


def watch(command: list[str], *, source: Path, watcher: Path, log: Path,
          expected_exit: int) -> dict:
    wrapped = [sys.executable, str(watcher), '--', *command]
    with log.open('wb') as stream:
        completed = subprocess.run(wrapped, cwd=source, stdout=stream,
                                   stderr=subprocess.STDOUT, timeout=1800)
    text = log.read_text(errors='replace')
    limits = re.findall(r'^MACOS_RSS_LIMIT_BYTES=(\d+)$', text, re.MULTILINE)
    peaks = re.findall(r'^PEAK_PROCESS_TREE_RSS_BYTES=(\d+)$', text,
                       re.MULTILINE)
    require(len(limits) == len(peaks) == 1 and int(limits[0]) == LIMIT,
            f'watchdog did not record the 4 GiB process-tree limit: {log}')
    peak = int(peaks[0])
    require(completed.returncode == expected_exit and peak <= LIMIT,
            f'{log.name}: exit={completed.returncode} expected={expected_exit} '
            f'peak={peak}')
    return {'passed': True, 'command': command, 'watch_command': wrapped,
            'exit': completed.returncode, 'expected_exit': expected_exit,
            'peak_process_tree_rss_bytes': peak, 'log': str(log),
            'log_sha256': sha(log)}


def strip_ansi(text: str) -> str:
    return re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', text)


def expected_exit(row: dict) -> int:
    value = row.get('observed_shell_exit', row.get('observed_exit',
                                                 row.get('exit')))
    require(isinstance(value, int), f'baseline row has no exit: {row}')
    return value if value >= 0 else 256 + value


def fixture_command(row: dict, *, baseline: Path, binary: Path,
                    copied: dict[Path, Path]) -> list[str]:
    raw = [str(value) for value in row['command']]
    old_binary = baseline / 'uwvm'
    old_watcher = baseline.parents[2] / 'test/0017.runtime/macos_rss_limit.py'
    if (len(raw) >= 4 and Path(raw[0]).name.startswith('python') and
            Path(raw[1]) == old_watcher and raw[2] == '--'):
        raw = raw[3:]
    require(raw and Path(raw[0]) == old_binary,
            f'baseline row does not invoke the pinned ROS binary: {row}')
    command = [str(binary)]
    for token in raw[1:]:
        path = Path(token)
        if path.suffix == '.wasm':
            require(path in copied, f'unstaged baseline Wasm input: {path}')
            command.append(str(copied[path]))
        else:
            command.append(token)
    return command


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--build-manifest', type=Path, required=True)
    parser.add_argument('--cross-manifest', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--clang', type=Path)
    parser.add_argument('--wasm-ld', type=Path)
    parser.add_argument('--rustc', type=Path)
    args = parser.parse_args()
    require(sys.platform == 'darwin' and os.uname().machine == 'arm64',
            'ROS macOS requalification requires the native arm64 host')
    source = args.source.resolve(strict=True)
    require(Path(__file__).resolve() ==
            source / 'tools/ci/requalify_macos_ros_from_r32.py',
            'runner must execute from the selected source tree')
    binary = args.binary.resolve(strict=True)
    baseline = args.baseline.resolve(strict=True)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    watcher = source / 'test/0017.runtime/macos_rss_limit.py'
    build_path = args.build_manifest.resolve(strict=True)
    cross_path = args.cross_manifest.resolve(strict=True)
    old_path = baseline / 'manifest.json'
    build, cross, old = (json.loads(path.read_text()) for path in
                         (build_path, cross_path, old_path))
    source_id = fingerprint(source, out / 'source-before.json')
    binary_hash = sha(binary)
    linked = build.get('build', build)
    require(build.get('source_id') == linked.get('source_id') == source_id and
            build.get('binary_sha256') == linked.get('binary_sha256') == binary_hash,
            'local linked product manifest/source/binary SHA mismatch')
    cross_events = {parts[0]: int(parts[1]) for line in
                    cross.get('cgroup_events_after', cross.get('cgroup_events', '')).splitlines()
                    if len(parts := line.split()) == 2}
    cross_events_before = {parts[0]: int(parts[1]) for line in
                           cross.get('cgroup_events_before', cross.get('cgroup_events', '')).splitlines()
                           if len(parts := line.split()) == 2}
    require(cross.get('source_id') == source_id and
            cross.get('object_sha256') == linked.get('runtime_object_sha256') and
            cross.get('cgroup_memory_max') == '68719476736' and
            cross.get('cgroup_swap_max') == '0' and
            cross.get('cgroup_cpuset') == '0,2,4,6,16-31' and
            cross_events.get('oom') == 0 and cross_events.get('oom_kill') == 0 and
            cross_events_before.get('oom') == 0 and cross_events_before.get('oom_kill') == 0,
            'Linux-cgroup arm64 runtime object provenance mismatch')
    require(old.get('all_acceptance_passed') is True and
            old.get('source_id') == 'sha256:ac49101ff52e6fe7a599442461b7e56f7e3384c645d86f0e0b6373b033ccca68',
            'baseline must be the pinned passing r32 ROS macOS evidence')
    require(old.get('binary_sha256') == sha(baseline / 'uwvm'),
            'r32 baseline product bytes changed')
    for filename, record in old.get('logs', {}).items():
        require(record.get('passed') is True and
                sha(baseline / filename) == record.get('sha256'),
                f'r32 baseline acceptance log changed: {filename}')
    require(all(old.get('acceptance', {}).get(name, {}).get('passed') is True
                for name, _ in SCRIPTS),
            'r32 baseline lacks a passing debugger category')
    wasm_tools = args.wasm_tools.resolve(strict=True)
    wasmtime = args.wasmtime.resolve(strict=True)
    inputs = {'runner_sha256': sha(Path(__file__)),
              'watchdog_sha256': sha(watcher),
              'baseline_manifest_sha256': sha(old_path),
              'build_manifest_sha256': sha(build_path),
              'cross_manifest_sha256': sha(cross_path),
              'wasm_tools_sha256': sha(wasm_tools),
              'wasmtime_sha256': sha(wasmtime)}
    for name in ('clang', 'wasm_ld', 'rustc'):
        tool = getattr(args, name)
        if tool is not None:
            inputs[name + '_sha256'] = sha(tool.resolve(strict=True))

    acceptance = {}
    for name, script in SCRIPTS:
        path = source / 'test/0017.runtime' / script
        category_out = out / name
        command = [sys.executable, str(path), '--uwvm', str(binary),
                   '--out', str(category_out), '--wasm-tools', str(wasm_tools)]
        if name in ('control-fd', 'server', 'gc-typed'):
            command.append('--ros')
        if name == 'gc-typed':
            command += ['--source-root', str(source), '--wasmtime',
                        str(wasmtime), '--macos']
        if name == 'hotreplace-unwind':
            command += ['--policy', 'unwind']
        if name == 'sources':
            for option, tool in (('--clang', args.clang),
                                 ('--wasm-ld', args.wasm_ld),
                                 ('--rustc', args.rustc)):
                if tool is not None:
                    # Preserve the launcher basename: LLVM's lld selects its
                    # WebAssembly driver from argv[0] (`wasm-ld` -> `lld`).
                    require(tool.is_file(), f'missing tool: {tool}')
                    command += [option, str(tool.absolute())]
        if name == 'debug-step' and args.clang is not None:
            command += ['--clang', str(args.clang.absolute())]
        acceptance[name] = watch(command, source=source, watcher=watcher,
                                 log=out / f'{name}.log', expected_exit=0)
        acceptance[name]['script_sha256'] = sha(path)
        summary = category_out / 'summary.json'
        if summary.exists():
            contents = json.loads(summary.read_text())
            if isinstance(contents, dict):
                for field in ('binary_sha256', 'product_sha256'):
                    if field in contents:
                        require(contents[field] == binary_hash,
                                f'{name} summary used another product')
            acceptance[name]['summary'] = str(summary)
            acceptance[name]['summary_sha256'] = sha(summary)
        require(sha(binary) == binary_hash, f'product changed during {name}')
        save(out / 'progress.json', acceptance)

    for name, relative in BASELINE_ROWS:
        old_rows_path = baseline / relative
        old_rows = json.loads(old_rows_path.read_text())
        if isinstance(old_rows, dict):
            require(old_rows.get('source_id') == old['source_id'] and
                    old_rows.get('product_sha256') == old['binary_sha256'],
                    f'baseline {name} rows differ from the signed r32 product')
        rows = old_rows['rows'] if isinstance(old_rows, dict) else old_rows
        require(rows and all(row.get('passed') is True for row in rows),
                f'baseline {name} has an incomplete row')
        category_out = out / name
        category_out.mkdir()
        copied: dict[Path, Path] = {}
        for row in rows:
            for token in row['command']:
                path = Path(str(token))
                if path.suffix != '.wasm' or path in copied:
                    continue
                require(path.is_file() and path.parent == old_rows_path.parent,
                        f'baseline Wasm fixture path is not pinned: {path}')
                target = category_out / path.name
                shutil.copy2(path, target)
                require(sha(target) == sha(path), f'Wasm fixture copy changed: {path}')
                copied[path] = target
        replayed = []
        for index, row in enumerate(rows):
            command = fixture_command(row, baseline=baseline, binary=binary,
                                      copied=copied)
            case_name = row.get('profile', '') + '-' + row['case']
            case_name = case_name.lstrip('-')
            if (expected_exit(row) == 127 and '-WFD-exceptions' in command and
                    row['case'].startswith('exnref_') and '-WFE-gc' not in command):
                # The pinned r32 row disabled exceptions while GC also stayed
                # off, so it reported GC first. Enable the other required
                # features here to test the intended exceptions gate exactly.
                command[command.index('-WFD-exceptions'):command.index('-WFD-exceptions')] = [
                    '-WFE-gc', '-WFE-reference-types']
            result = watch(command, source=source, watcher=watcher,
                           log=category_out / f'{index:02d}-{case_name}.log',
                           expected_exit=expected_exit(row))
            text = strip_ansi(Path(result['log']).read_text(errors='replace'))
            disabled = [token[5:] for token in command if token.startswith('-WFD-')
                        and token != '-WFD-function-references']
            if expected_exit(row) == 127:
                require(len(disabled) == 1 and
                        f'--wasm-feature-enable-{disabled[0]}' in text,
                        f'wrong feature-off diagnostic: {result["log"]}')
            if expected_exit(row) == 251:
                require('Uncaught WebAssembly exception' in text and
                        'Wasm call stack captured at throw' in text and
                        'payload[0]' in text and 'func_idx=0' in text and
                        'func_idx=1' in text,
                        f'uncaught exception trace incomplete: {result["log"]}')
            result['case'] = case_name
            replayed.append(result)
            save(category_out / 'results.json', replayed)
        acceptance[name] = {'passed': True, 'checks': len(replayed),
                            'rows': replayed,
                            'baseline_summary_sha256': sha(old_rows_path),
                            'fixture_sha256': {str(target): sha(target)
                                               for target in copied.values()},
                            'peak_process_tree_rss_bytes':
                                max(row['peak_process_tree_rss_bytes']
                                    for row in replayed)}
        require(sha(binary) == binary_hash, f'product changed during {name}')
        save(out / 'progress.json', acceptance)

    after_id = fingerprint(source, out / 'source-after.json')
    require(after_id == source_id and sha(binary) == binary_hash and
            sha(build_path) == inputs['build_manifest_sha256'] and
            sha(cross_path) == inputs['cross_manifest_sha256'],
            'source, product, build manifest, or cross provenance changed')
    require(len(acceptance) == 10 and
            sum(acceptance[name]['checks'] for name, _ in BASELINE_ROWS) == 26,
            'missing seven debugger categories or 26 Core 3 fixture rows')
    summary = {'passed': True, 'source_id': source_id,
               'source_before_sha256': sha(out / 'source-before.json'),
               'source_after_sha256': sha(out / 'source-after.json'),
               'binary': str(binary), 'binary_sha256': binary_hash,
               'build_manifest': str(build_path),
               'cross_manifest': str(cross_path),
               'baseline': str(baseline), 'inputs': inputs,
               'limit_bytes': LIMIT, 'acceptance': acceptance}
    save(out / 'manifest.json', summary)
    print('PASS ROS macOS 7 debugger categories and 26 Core 3 rows, '
          'all under 4 GiB RSS')


if __name__ == '__main__':
    main()
