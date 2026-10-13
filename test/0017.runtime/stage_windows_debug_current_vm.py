#!/usr/bin/env python3
"""Stage a fresh Win64 debug-full PE, launcher and official C5 statement oracle.

Keeper-controlled Linux only. No Windows PE is executed here. The normalized
build receipt must retain actual original commands/logs/outputs, not recreate
commands from a current binary or turn an older source ID into a new one.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import shutil
import subprocess
import sys
import run_debug_source_step_cli as oracle_cli


def require(value: bool, message: str) -> None:
    if not value:
        raise ValueError(message)


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_json(path: Path, limit: int = 16 << 20) -> dict:
    require(path.is_file() and path.stat().st_size <= limit, 'bounded receipt required: ' + str(path))
    result = json.loads(path.read_text())
    require(isinstance(result, dict), 'receipt must be an object')
    return result


def current_entries(root: Path) -> list[dict]:
    result = []
    for directory in ('src', 'third-parties'):
        base = (root / directory).resolve(strict=True)
        for path in sorted(base.rglob('*')):
            if path.is_file() and path.name != '.DS_Store' and not path.name.startswith('._'):
                result.append({'path': (Path(directory) / path.relative_to(base)).as_posix(), 'sha256': sha(path)})
    return result


def resolved_argument(value: str, cwd: Path) -> Path:
    path = Path(value)
    return (path if path.is_absolute() else cwd / path).resolve(strict=True)


def verify_command(row: dict) -> tuple[Path, list[str], Path]:
    argv = row.get('argv')
    require(row.get('returncode') == 0 and isinstance(argv, list) and bool(argv) and
            all(isinstance(value, str) and '\0' not in value for value in argv), 'successful actual compiler/linker argv required')
    # This first staging contract deliberately requires a captured direct argv.
    # An unexpanded response file is not accepted as proof of consumed objects.
    require(not any(value.startswith('@') or (value.startswith('-Wl,') and '@' in value) for value in argv),
            'direct original argv required; response-file provenance needs separate qualification')
    cwd = Path(row['cwd']).resolve(strict=True)
    tool = resolved_argument(argv[0], cwd)
    primary = r'(?:clang(?:\+\+)?|lld|ld\.lld)(?:-[0-9]+(?:\.[0-9]+)*)?'
    require(re.fullmatch(primary, Path(argv[0]).name) is not None and re.fullmatch(primary, tool.name) is not None,
            'direct actual LLVM primary required; env/shell/resource wrappers are not compiler provenance')
    require(sha(tool) == row.get('tool_sha256'), 'original command tool changed')
    log = Path(row['log']).resolve(strict=True)
    require(sha(log) == row.get('log_sha256'), 'original command raw log changed')
    outputs = [argv[index + 1] for index, value in enumerate(argv[:-1]) if value == '-o']
    require(len(outputs) == 1, 'one actual original -o output required')
    output = resolved_argument(outputs[0], cwd)
    require(str(output) == str(Path(row['output']).resolve(strict=True)) and sha(output) == row.get('output_sha256'), 'original command output identity changed')
    return output, argv, cwd


def file_arguments(argv: list[str], cwd: Path) -> set[Path]:
    result = set()
    for index, value in enumerate(argv[1:], 1):
        if argv[index - 1] == '-o':
            continue
        if not value or value.startswith('-'):
            continue
        path = Path(value)
        path = path if path.is_absolute() else cwd / path
        if path.is_file():
            result.add(path.resolve(strict=True))
    return result


def contains(path: Path, needle: bytes) -> bool:
    overlap = b''
    with path.open('rb') as stream:
        while chunk := stream.read(1 << 20):
            data = overlap + chunk
            if needle in data:
                return True
            overlap = data[-(len(needle) - 1):]
    return False


def pe_x64(path: Path) -> None:
    require(path.stat().st_size <= 1 << 30, 'bounded PE required')
    with path.open('rb') as stream:
        header = stream.read(64)
        require(len(header) == 64 and header[:2] == b'MZ', 'actual DOS PE header missing')
        offset = int.from_bytes(header[60:64], 'little')
        require(offset <= path.stat().st_size - 26, 'PE header is outside actual file')
        stream.seek(offset)
        coff = stream.read(26)
    require(coff[:4] == b'PE\0\0' and int.from_bytes(coff[4:6], 'little') == 0x8664 and
            int.from_bytes(coff[24:26], 'little') == 0x20b, 'actual AMD64 PE32+ required')


def expression_sizes(wasm: Path, expressions: dict[int, int]) -> dict[int, int]:
    payloads = [payload for kind, payload in oracle_cli.metadata_cli.sections(wasm) if kind == 10]
    require(len(payloads) == 1, 'one actual Code payload')
    payload = payloads[0]
    count, cursor = oracle_cli.metadata_cli.u32(payload, 0, len(payload))
    result = {}
    for index in range(count):
        size, body = oracle_cli.metadata_cli.u32(payload, cursor, len(payload))
        require(size <= len(payload) - body and index in expressions, 'bounded actual expression size')
        end = body + size
        require(body <= expressions[index] < end, 'actual expression start is not inside its body')
        result[index] = end - expressions[index]
        cursor = end
    require(cursor == len(payload), 'complete actual Code payload required')
    return result


SYSTEM_DLLS = frozenset(name.casefold() for name in (
    'kernel32.dll', 'ntdll.dll', 'advapi32.dll', 'bcrypt.dll', 'crypt32.dll',
    'user32.dll', 'ws2_32.dll', 'secur32.dll', 'ole32.dll', 'shell32.dll',
    'shlwapi.dll', 'version.dll', 'userenv.dll', 'psapi.dll', 'iphlpapi.dll',
    'winmm.dll', 'msvcrt.dll', 'ucrtbase.dll',
))


def is_system_dll(name: str) -> bool:
    return name.casefold() in SYSTEM_DLLS or bool(re.fullmatch(r'api-ms-win-[a-z0-9-]+-l[0-9]+-[0-9]+-[0-9]+\.dll', name.casefold()))


def imports(text: str) -> list[str]:
    # llvm-readobj --coff-imports prints Name inside each actual Import or
    # DelayImport block. Delay-load dependencies are included in the closure.
    result = re.findall(r'^\s*Name:\s+([^\r\n]+?)\s*$', text, re.MULTILINE)
    require(all(re.fullmatch(r'[A-Za-z0-9_.+-]+\.dll', name, re.IGNORECASE) for name in result), 'unrecognized actual DLL import spelling')
    return sorted(set(result), key=str.casefold)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'product', 'launcher', 'build-receipt', 'out', 'wasm-clang', 'wasm-ld', 'wasm-tools', 'llvm-dwarfdump', 'llvm-readobj'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--repository', choices=('ordinary', 'ros'), required=True)
    parser.add_argument('--dll', type=Path, action='append', default=[])
    args = parser.parse_args()
    require(sys.platform == 'linux', 'keeper controlled Linux staging only')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    root = args.source_root.resolve(strict=True)
    require(Path(__file__).resolve() == root / 'test/0017.runtime/stage_windows_debug_current_vm.py' and
            Path(oracle_cli.__file__).resolve() == root / 'test/0017.runtime/run_debug_source_step_cli.py' and
            Path(oracle_cli.metadata_cli.__file__).resolve() == root / 'test/0017.runtime/run_debug_source_inline_metadata_cli.py', 'actual imported oracle sources must be this qualified source tree')
    guard = root / 'tools/ci/require_wasm3_test_cgroup.sh'
    subprocess.run(['bash', str(guard)], check=True)
    pe, launcher, build_path = (getattr(args, name).resolve(strict=True) for name in ('product', 'launcher', 'build_receipt'))
    build_digest = sha(build_path)
    build = read_json(build_path)
    require(build.get('schema') == 1 and build.get('purpose') == 'actual-current-win64-debug-full-build' and
            build.get('repository') == args.repository and build.get('target') == 'x86_64-w64-windows-gnu', 'fresh actual Win64 debug-full receipt contract required')
    require(build.get('memory_max') == str(64 << 30) and build.get('memory_swap_max') == '0' and
            build.get('cpuset') == '0,2,4,6,16-31' and build.get('oom_before') == build.get('oom_after') == 0 and
            build.get('oom_kill_before') == build.get('oom_kill_after') == 0, 'actual original build resource/OOM proof required')
    before, after = (Path(build[key]).resolve(strict=True) for key in ('source_before_file', 'source_after_file'))
    require(sha(before) == sha(after), 'actual build source/dependency fingerprints changed')
    fingerprint_digest = sha(before)
    fingerprint = read_json(before)
    entries = fingerprint.get('files')
    require(isinstance(entries, list) and bool(entries) and len({row['path'] for row in entries}) == len(entries), 'complete unique actual source/dependency fingerprint required')
    canonical = json.dumps(entries, sort_keys=True, separators=(',', ':')).encode()
    source_id = 'sha256:' + hashlib.sha256(canonical).hexdigest()
    require(fingerprint.get('source_id') == source_id and build.get('source_id') == source_id, 'source ID must hash the actual original fingerprint file list')
    expected_sources = {row['path']: row['sha256'] for row in entries if row['path'].startswith('src/')}
    actual_sources = {str(path.relative_to(root)): sha(path) for path in sorted((root / 'src').rglob('*'))
                      if path.is_file() and path.name != '.DS_Store' and not path.name.startswith('._')}
    require(actual_sources == expected_sources and current_entries(root) == entries, 'current actual full source AND bundled dependencies must equal the real original build fingerprint')
    pin_paths = {'controller': 'src/uwvm2/uwvm/debugger/controller.h',
                 'activation_api': 'src/uwvm2/runtime/lib/uwvm_runtime_debug_activation_api.h',
                 'joint_source_activation': 'src/uwvm2/runtime/lib/uwvm_runtime_debug_activation_api.h',
                 'source_api': 'src/uwvm2/runtime/lib/uwvm_runtime_debug_source_api.h',
                 'debug_host_wrapper': 'src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_debug_host_bridge.h'}
    production_pins = {name: expected_sources[path] for name, path in pin_paths.items()}
    require(build.get('production_pins') == production_pins, 'actual original receipt must record current controller/producer/joint/wrapper pins')
    require('llvm_jit_debug_query_source_activation_host_api' in (root / pin_paths['joint_source_activation']).read_text(), 'actual joint source+activation API is absent')
    product, product_argv, product_cwd = verify_command(build['product_link'])
    launched, launcher_argv, launcher_cwd = verify_command(build['launcher_build'])
    require(product == pe and launched == launcher, 'actual original command output must be the passed product/launcher')
    link_inputs = file_arguments(product_argv, product_cwd)
    compile_sources = set()
    original_commands = [build['product_link'], build['launcher_build'], *build.get('product_compiles', [])]
    for command in build.get('product_compiles', []):
        output, argv, cwd = verify_command(command)
        require('-c' in argv and output in link_inputs, 'each actual compiled product object must be consumed by the original link')
        compile_sources.update(file_arguments(argv, cwd))
    require(root / 'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp' in compile_sources and
            root / 'src/uwvm2/uwvm/main.default.cpp' in compile_sources, 'fresh actual runtime and main compile records required; old object reuse is not qualification')
    require(contains(pe, source_id.encode()), 'actual PE does not retain this original build source ID')
    launcher_source = root / 'test/0017.runtime/windows_debug_current_launcher.cc'
    require(launcher_source in file_arguments(launcher_argv, launcher_cwd) and '-municode' in launcher_argv, 'actual new launcher source/Unicode entry must be in its original build')
    dwarf_archive = Path(build['debug_info_dwarf_archive']['path']).resolve(strict=True)
    require('DebugInfoDWARF' in dwarf_archive.name and dwarf_archive in link_inputs and
            sha(dwarf_archive) == build['debug_info_dwarf_archive']['sha256'], 'actual product link must consume its qualified DWARF archive')
    inputs = build.get('actual_inputs')
    require(isinstance(inputs, list) and bool(inputs) and len({row['path'] for row in inputs}) == len(inputs), 'unique actual build input SHA records required')
    for record in inputs:
        require(sha(Path(record['path']).resolve(strict=True)) == record['sha256'], 'actual original build input changed')
    require(all(any(Path(record['path']).resolve() == path for record in inputs) for path in
                (launcher_source, dwarf_archive, *compile_sources, *link_inputs)), 'compile/link/source inputs must all have original captured hashes')
    pe_x64(pe); pe_x64(launcher)
    tools = {name: getattr(args, name).resolve(strict=True) for name in ('wasm_clang', 'wasm_ld', 'wasm_tools', 'llvm_dwarfdump', 'llvm_readobj')}
    driver_names = ('windows_debug_current_owned_process.ps1', 'run_windows_debug_acceptance_current_vm.ps1')
    fixture = root / 'test/0017.runtime/fixtures/debug_source_step_c.c'
    immutable = {pe, launcher, build_path, before, after, guard, fixture, launcher_source, Path(__file__).resolve(),
                 Path(oracle_cli.__file__).resolve(), Path(oracle_cli.metadata_cli.__file__).resolve(), *tools.values(),
                 *(root / 'test/0017.runtime' / name for name in driver_names),
                 *(Path(row['path']).resolve(strict=True) for row in inputs)}
    for command in original_commands:
        immutable.update((resolved_argument(command['argv'][0], Path(command['cwd'])),
                          Path(command['log']).resolve(strict=True), Path(command['output']).resolve(strict=True)))
    dlls = {}
    for path in args.dll:
        path = path.resolve(strict=True)
        require(re.fullmatch(r'[A-Za-z0-9_.+-]+\.dll', path.name, re.IGNORECASE) and path.name.casefold() not in dlls,
                'unique regular DLL providers required')
        pe_x64(path); dlls[path.name.casefold()] = path; immutable.add(path)
    initial_hashes = {str(path): sha(path) for path in immutable}
    require(initial_hashes[str(build_path)] == build_digest and initial_hashes[str(before)] == initial_hashes[str(after)] == fingerprint_digest, 'original receipt/fingerprint changed while checking provenance')
    for record in inputs:
        require(initial_hashes[str(Path(record['path']).resolve(strict=True))] == record['sha256'], 'original input changed before staging snapshot')
    for command in original_commands:
        require(initial_hashes[str(resolved_argument(command['argv'][0], Path(command['cwd'])))] == command['tool_sha256'] and
                initial_hashes[str(Path(command['log']).resolve(strict=True))] == command['log_sha256'] and
                initial_hashes[str(Path(command['output']).resolve(strict=True))] == command['output_sha256'], 'original command evidence changed before staging snapshot')
    out = args.out.resolve(); out.mkdir(mode=0o700, parents=True, exist_ok=False)
    evidence = out / 'host-evidence'; evidence.mkdir(mode=0o700)
    commands = []
    generated_hashes = {}

    def run(argv: list[str], name: str) -> str:
        log = evidence / (name + '.log')
        with log.open('xb') as stream:
            result = subprocess.run(argv, cwd=root, stdout=stream, stderr=subprocess.STDOUT, timeout=180, check=False)
        row = {'argv': argv, 'cwd': str(root), 'log': str(log), 'returncode': result.returncode, 'log_sha256': sha(log)}
        commands.append(row)
        generated_hashes[str(log)] = row['log_sha256']
        require(result.returncode == 0 and log.stat().st_size <= 16 << 20, 'official bounded tool failed: ' + name)
        return log.read_text(errors='strict')

    summary = {'passed': False, 'scope': 'exact source/PE/official fixture staging; no Win64 execution', 'source_id': source_id,
               'commands': commands, 'inputs_before': initial_hashes}
    try:
        versions = {name: {'path': str(path), 'sha256': sha(path), 'version': run([str(path), '--version'], 'version-' + name)} for name, path in tools.items()}
        # Recursive official import closure includes the product, launcher and
        # every non-system provider before any guest execution is permitted.
        pending, seen, import_rows = [pe, launcher], set(), []
        while pending:
            path = pending.pop()
            if path in seen:
                continue
            seen.add(path)
            names = imports(run([str(tools['llvm_readobj']), '--coff-imports', str(path)], 'imports-' + str(len(import_rows))))
            import_rows.append({'path': str(path), 'sha256': sha(path), 'imports': names})
            for name in names:
                if is_system_dll(name):
                    continue
                require(name.casefold() in dlls, 'missing actual non-system DLL provider: ' + name)
                pending.append(dlls[name.casefold()])
        require(set(dlls.values()) == seen - {pe, launcher}, 'every supplied non-system DLL must belong to the actual recursive import closure')
        wasm, obj = out / 'c-dwarf5-O1.wasm', evidence / 'c-dwarf5-O1.o'
        run([str(tools['wasm_clang']), '--target=wasm32-unknown-unknown', '-g', '-gdwarf-5', '-O1', '-std=c17', '-nostdlib', '-c', str(fixture), '-o', str(obj)], 'compile-c-dwarf5-O1')
        object_sha256 = sha(obj)
        generated_hashes[str(obj)] = object_sha256
        run([str(tools['wasm_ld']), '--no-entry', '--export-all', str(obj), '-o', str(wasm)], 'link-c-dwarf5-O1')
        require(sha(obj) == object_sha256, 'actual compiler object changed during the official Wasm link')
        emitted = sha(wasm)
        generated_hashes[str(wasm)] = emitted
        run([str(tools['wasm_tools']), 'validate', str(wasm)], 'validate-c-dwarf5-O1')
        require(sha(wasm) == emitted, 'fixture changed during official validation')
        run([str(tools['llvm_dwarfdump']), '--verify', str(wasm)], 'verify-c-dwarf5-O1')
        require(sha(wasm) == emitted, 'fixture changed during official DWARF verification')
        dump = run([str(tools['llvm_dwarfdump']), '--debug-info', '--debug-line', '--debug-ranges', '--debug-rnglists', str(wasm)], 'oracle-c-dwarf5-O1')
        require(sha(wasm) == emitted and dump.count('DW_TAG_inlined_subroutine') >= 4 and
                'source_step_inner' in dump and 'source_step_middle' in dump, 'actual nested/repeated embedded inline metadata required')
        expressions = oracle_cli.code_expressions(wasm)
        sequences = oracle_cli.line_sequences(dump)
        functions = {name: oracle_cli.metadata_cli.function(wasm, 'source_step_' + name)[0] for name in ('leaf', 'outer', 'recursive')}
        leaf_lines = [index for index, line in enumerate(fixture.read_text().splitlines(), 1) if 'STEP_LEAF_ENTRY' in line]
        require(len(leaf_lines) == 1 and any(row['line'] == leaf_lines[0] and row['is_statement'] for seq in sequences for row in seq), 'official compiler did not retain the exact leaf origin statement')
        oracle = {'schema': 1, 'wasm_sha256': emitted, 'compiler_object_sha256': object_sha256, 'source_sha256': sha(fixture), 'source_basename': fixture.name,
                  'actual_import_policy': 'reject-all', 'expressions': expressions, 'expression_sizes': expression_sizes(wasm, expressions),
                  'sequences': sequences, 'functions': functions, 'leaf_entry_line': leaf_lines[0], 'official_tools': versions,
                  'official_commands': commands}
        oracle_path = out / 'c-dwarf5-O1.oracle.json'
        with oracle_path.open('x') as stream:
            json.dump(oracle, stream, indent=2); stream.write('\n')
        origins = {'uwvm.exe': pe, 'windows_debug_current_launcher.exe': launcher, 'build-receipt.json': build_path,
                   **{name: root / 'test/0017.runtime' / name for name in driver_names},
                   **{path.name: path for path in dlls.values()}}
        generated_hashes[str(oracle_path)] = sha(oracle_path)
        files = {wasm.name: sha(wasm), oracle_path.name: sha(oracle_path)}
        for name, origin in origins.items():
            target = out / name
            with target.open('xb') as destination, origin.open('rb') as original:
                shutil.copyfileobj(original, destination)
            require(sha(target) == initial_hashes[str(origin)], 'actual artifact changed while copying: ' + name)
            files[name] = sha(target)
        qualification = {'schema': 3, 'purpose': 'current-joint-debug-full-c5-v1', 'repository': args.repository,
                         'source_id': source_id, 'source_before_sha256': sha(before), 'source_after_sha256': sha(after),
                         'production_pins': production_pins, 'files_sha256': files,
                         'non_system_dlls': [{'name': path.name, 'sha256': sha(path)} for path in sorted(dlls.values())],
                         'actual_pe_imports': import_rows, 'debug_info_dwarf_archive': build['debug_info_dwarf_archive']}
        subprocess.run(['bash', str(guard)], check=True)
        actual_after = {str(path.relative_to(root)): sha(path) for path in sorted((root / 'src').rglob('*'))
                        if path.is_file() and path.name != '.DS_Store' and not path.name.startswith('._')}
        final_hashes = {str(path): sha(path) for path in immutable}
        require(actual_after == actual_sources and current_entries(root) == entries and final_hashes == initial_hashes and sha(wasm) == emitted, 'source/dependencies/build/tool/DLL/fixture changed during staging')
        generated_after = {pathname: sha(Path(pathname)) for pathname in generated_hashes}
        require(generated_after == generated_hashes and all(sha(out / name) == value for name, value in files.items()), 'generated official object/log/oracle or copied admission artifacts changed')
        summary.update(generated_when_created=generated_hashes, generated_after=generated_after)
        # Publish the executable qualification only after every pre/post check.
        # A failed stage can leave evidence, but cannot leave this admission file.
        with (out / 'qualification.json').open('x') as stream:
            json.dump(qualification, stream, indent=2, sort_keys=True); stream.write('\n')
        summary.update(passed=True, inputs_after=final_hashes, qualification_sha256=sha(out / 'qualification.json'), files_sha256=files)
    except BaseException as error:
        summary['error'] = repr(error)
        raise
    finally:
        with (evidence / 'stage.json').open('x') as stream:
            json.dump(summary, stream, indent=2, sort_keys=True); stream.write('\n')
    print(json.dumps({'staged_only': True, 'source_id': source_id, 'qualification_sha256': sha(out / 'qualification.json'), 'out': str(out)}, sort_keys=True))


if __name__ == '__main__':
    main()
