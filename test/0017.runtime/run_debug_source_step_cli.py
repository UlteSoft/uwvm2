#!/usr/bin/env python3
"""Fresh-product real source into/next/finish; official embedded DWARF only.

Remote keeper Linux cgroup only. This file does not qualify native stepping,
values, hot replacement, midrun attach, tail source mapping or IDE capabilities.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import subprocess
import sys
import run_debug_source_inline_metadata_cli as metadata_cli


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value: bool, message: str, detail: object = None) -> None:
    if not value:
        raise AssertionError((message, detail))


def code_expressions(wasm: Path) -> dict[int, int]:
    """Code-payload offsets for the official line oracle, never credentials.

    These focused compiler fixtures require zero actual imports and only
    numeric local declarations. Reuse the bounded fixture ULEB parser; reject
    any actual import before local Code indices can be module function indices.
    """
    sections = metadata_cli.sections(wasm)
    imports = [payload for kind, payload in sections if kind == 2]
    require(len(imports) <= 1, 'duplicate actual Import section')
    for import_payload in imports:
        import_count, cursor = metadata_cli.u32(import_payload, 0, len(import_payload))
        require(import_count == 0, 'focused Code oracle rejects any actual import; no function-index adjustment is assumed')
        require(cursor == len(import_payload), 'complete empty Import vector required')
    codes = [payload for kind, payload in sections if kind == 10]
    require(len(codes) == 1, 'one actual Code section required')
    payload = codes[0]
    count, cursor = metadata_cli.u32(payload, 0, len(payload))
    result = {}
    for index in range(count):
        size, body = metadata_cli.u32(payload, cursor, len(payload))
        require(size <= len(payload) - body, 'Code body extent')
        end = body + size
        groups, expression = metadata_cli.u32(payload, body, end)
        for _ in range(groups):
            _, expression = metadata_cli.u32(payload, expression, end)
            require(expression < end and payload[expression] in (0x7f, 0x7e, 0x7d, 0x7c, 0x7b),
                    'focused oracle rejects nonnumeric local declaration')
            expression += 1
        require(expression < end, 'nonempty compiler expression required')
        result[index] = expression
        cursor = end
    require(cursor == len(payload), 'complete Code section consumed')
    return result


def line_sequences(text: str) -> list[list[dict]]:
    """Read actual LLVM DWARFDebugLine Row::dump decimal columns/flags.

    Accept old/new LLVM tables (optional OpIndex), preserving end_sequence.
    No line/statement or discriminator is invented from a source filename.
    """
    result, pending = [], []
    for line in text.splitlines():
        fields = line.split()
        if not fields or not re.fullmatch(r'0x[0-9a-fA-F]+', fields[0]):
            continue
        numbers = []
        for field in fields[1:]:
            if not field.isdecimal():
                break
            numbers.append(int(field))
        if len(numbers) not in (5, 6):
            continue
        row = {'address': int(fields[0], 16), 'line': numbers[0], 'column': numbers[1],
               'file': numbers[2], 'isa': numbers[3], 'discriminator': numbers[4],
               'op_index': numbers[5] if len(numbers) == 6 else 0,
               'is_statement': 'is_stmt' in fields, 'end_sequence': 'end_sequence' in fields}
        require(row['op_index'] == 0, 'Wasm oracle cannot use a VLIW operation index', row)
        if pending:
            require(row['address'] >= pending[-1]['address'], 'official line sequence addresses decrease')
        pending.append(row)
        if row['end_sequence']:
            result.append(pending)
            pending = []
    require(not pending and bool(result), 'complete official line sequences required')
    return result


def line_at(sequences: list[list[dict]], pc: int) -> dict:
    matches = []
    for sequence in sequences:
        if sequence[0]['address'] <= pc < sequence[-1]['address']:
            active = [row for row in sequence if row['address'] <= pc and not row['end_sequence']]
            if active:
                matches.append(active[-1])
    require(len(matches) == 1, 'actual Code offset must have one official line row', (pc, matches))
    return matches[0]


class Session:
    def __init__(self, argv: list[str], log: Path, expressions: dict[int, int], sequences: list[list[dict]], source: Path):
        self.console = metadata_cli.Console(argv, log)
        self.expressions, self.sequences, self.source = expressions, sequences, source
        self.thread = 0
        self.actions = []
        self.positions = []
        self.breakpoint = 0
        self.last_step_stop = 0

    def send(self, command: str) -> bytes:
        reply = self.console.send(command)
        require(len(self.console.transcript) <= 8 * 1024 * 1024, 'bounded console transcript')
        self.actions.append(command)
        require(b'control request rejected' not in reply and b'source stepping unavailable' not in reply,
                'actual controller operation rejected', (command, reply[-4000:]))
        return reply

    def position(self) -> dict:
        reply = self.send(f'bt {self.thread}')
        clean = re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', reply)
        threads = re.findall(rb'(?m)^thread (\d+) module=(\d+) function=(\d+) byte-offset=(\d+) generation=(\d+)\r?$', clean)
        require(len(threads) == 1 and int(threads[0][0]) == self.thread, 'one actual fixture participant required', clean)
        thread, module, function, offset, generation = map(int, threads[0])
        require(module == 0 and function in self.expressions and generation > 0, 'actual published fixture function', clean)
        stop = re.findall(rb'(?m)^stop-id (\d+)\r?$', clean)
        require(len(stop) == 1 and int(stop[0]) > 0, 'fresh nonzero controller stop label required', clean)
        source = re.findall(rb'(?m)^  source (.+):(\d+):(\d+)\r?$', clean)
        require(len(source) == 1, 'actual current source location required', clean)
        file, line, column = source[0][0].decode(), int(source[0][1]), int(source[0][2])
        require(Path(file).name == self.source.name, 'unrelated source file cannot satisfy fixture', source)
        pc = self.expressions[function] + offset
        row = line_at(self.sequences, pc)
        require((row['line'], row['column']) == (line, column), 'product line/column disagree with official Code row', (pc, row, source))
        inline_display = [line.decode() for line in re.findall(rb'(?m)^  inline (?!metadata unavailable:)([^\r\n]+)\r?$', clean)]
        # Actual query_inline_frames/console display is inner -> outer. Keep
        # those raw lines and normalize only this owned test oracle to the
        # concrete scope policy's outer -> inner order; no identity is minted.
        inline = list(reversed(inline_display))
        physical = [int(value) for value in re.findall(rb'(?m)^  #\d+ module=0 function=(\d+) ', clean)]
        value = {'thread': thread, 'function': function, 'offset': offset, 'code_offset': pc,
                 'runtime_epoch': generation, 'stop_id': int(stop[0]), 'file': file,
                 'line': line, 'column': column, 'discriminator': row['discriminator'],
                 'is_statement': row['is_statement'], 'inline_display_inner_to_outer': inline_display,
                 'inline': inline, 'physical_functions': physical}
        self.positions.append(value)
        return value

    def source_step(self, policy: str) -> dict:
        before = self.position()
        reply = self.send(f'step source {self.thread} {policy}')
        require(b'stopped: selected participant step' in reply, 'source command did not produce an actual stopped guest', reply)
        after = self.position()
        require(after['stop_id'] > before['stop_id'] and after['stop_id'] > self.last_step_stop,
                'source step must advance the real stop label', (before, after))
        require(after['is_statement'], 'source stop must be an official statement row', after)
        self.last_step_stop = after['stop_id']
        return after

    def begin(self, function: int) -> None:
        require(b'prepared; no Wasm instruction executed' in self.send('status'), 'actual prepared debug-full console required')
        reply = self.send(f'break 0 {function} 0')
        found = re.search(rb'breakpoint (\d+)', reply)
        require(found is not None, 'actual emitted entry breakpoint missing', reply)
        self.breakpoint = int(found.group(1))
        self.send('continue')
        for _ in range(20):
            stopped = self.send('wait')
            if b'stopped: breakpoint' in stopped:
                break
        else:
            raise AssertionError(('actual breakpoint timeout', stopped))
        match = re.search(rb'thread (\d+) module=0 function=' + str(function).encode() + rb' byte-offset=', stopped)
        require(match is not None, 'actual target entry stop missing', stopped)
        self.thread = int(match.group(1))
        self.send(f'delete {self.breakpoint}')

    def seek(self, predicate, description: str, count: int = 512) -> dict:
        for _ in range(count):
            # Entry/prologue opcodes may have no source. They cannot be source
            # origins; advance using an actual Wasm step until a mapped row.
            reply = self.send(f'bt {self.thread}')
            if b'\n  source ' in reply:
                current = self.position()
                if predicate(current):
                    return current
            step = self.send(f'step wasm {self.thread}')
            require(b'guest exited:' not in step, 'fixture exited before required real origin', description)
        raise AssertionError(('bounded origin search failed', description))

    def finish_guest(self) -> None:
        self.send('continue')
        for _ in range(20):
            result = self.send('wait')
            if b'guest exited:' in result:
                require(b'guest exited: 0' in result, 'fixture computed an incorrect result or trapped', result)
                return
        raise AssertionError('actual fixture exit was not observed')


def qualify(prefix: list[str], wasm: Path, source: Path, sequences: list[list[dict]], markers: dict[str, int], kind: str, log: Path) -> dict:
    indices = {name: metadata_cli.function(wasm, 'source_step_' + name)[0] for name in ('outer', 'leaf', 'recursive')}
    session = Session([*prefix, '--run', str(wasm)], log, code_expressions(wasm), sequences, source)
    row = {'case': kind, 'actual_argv': session.console.command, 'actions': session.actions,
           'positions': session.positions, 'wasm_sha256': sha(wasm), 'source_sha256': sha(source), 'log': str(log), 'passed': False}
    try:
        origin = indices['leaf'] if kind == 'finish' else indices['recursive'] if kind == 'recursive' else indices['outer']
        session.begin(origin)
        if kind == 'into':
            session.seek(lambda p: p['function'] == origin and p['line'] == markers['STEP_BEFORE_INLINE'] and p['is_statement'], 'before nested inline')
            for _ in range(24):
                after = session.source_step('into')
                require(after['function'] == indices['outer'], 'into unexpectedly skipped to another physical function', after)
                if len(after['inline']) >= 2 and 'source_step_middle' in after['inline'][0] and 'source_step_inner' in after['inline'][-1]:
                    break
            else:
                raise AssertionError('source into never entered the actual nested inline chain')
        elif kind == 'next':
            session.seek(lambda p: p['function'] == origin and p['line'] == markers['STEP_PHYSICAL_CALL'] and p['is_statement'], 'before actual leaf call')
            for _ in range(24):
                after = session.source_step('over')
                require(after['function'] == indices['outer'] and not after['inline'], 'next stopped inside a real child', after)
                if after['line'] == markers['STEP_AFTER_CALL']:
                    break
            else:
                raise AssertionError('source next did not reach the actual after-call statement')
        elif kind == 'finish':
            session.seek(lambda p: p['function'] == origin and p['line'] == markers['STEP_LEAF_ENTRY'] and p['is_statement'], 'real physical leaf')
            after = session.source_step('out')
            require(after['function'] == indices['outer'] and not after['inline'], 'physical finish did not reach actual caller stop', after)
        elif kind == 'inline-out':
            before = session.seek(lambda p: p['function'] == origin and len(p['inline']) >= 2 and
                'source_step_inner' in p['inline'][-1] and p['is_statement'], 'real nested concrete inline instance')
            after = session.source_step('out')
            require(after['function'] == indices['outer'] and len(after['inline']) < len(before['inline']) and
                    after['inline'] == before['inline'][:len(after['inline'])], 'inline finish did not leave THAT concrete instance', (before, after))
        elif kind == 'repeated':
            sites = set()
            for _ in range(512):
                reply = session.send(f'bt {session.thread}')
                if b'\n  source ' in reply:
                    current = session.position()
                    if current['function'] == origin and len(current['inline']) >= 2 and 'source_step_middle' in current['inline'][0]:
                        for marker in ('STEP_INLINE_ONE', 'STEP_INLINE_TWO'):
                            if re.search(r':' + str(markers[marker]) + r':\d+$', current['inline'][0]):
                                sites.add(marker)
                if len(sites) == 2:
                    break
                require(b'guest exited:' not in session.send(f'step wasm {session.thread}'), 'repeated callsite fixture exited early')
            require(len(sites) == 2, 'two actual same-name inline concrete call sites required', sites)
        elif kind == 'recursive':
            before = session.seek(lambda p: p['function'] == origin and p['physical_functions'].count(origin) >= 3 and p['is_statement'],
                                  'three actual recursive physical frames')
            after = session.source_step('out')
            require(after['function'] == origin and after['physical_functions'].count(origin) == before['physical_functions'].count(origin) - 1,
                    'same-function finish did not return one real activation', (before, after))
        else:
            raise AssertionError('unsupported test case')
        session.finish_guest()
        row['passed'] = True
    except BaseException as error:
        row['error'] = repr(error)
        raise
    finally:
        try:
            session.console.finish()
        except BaseException as error:
            row['passed'] = False
            row['finish_error'] = repr(error)
            raise
        finally:
            if log.is_file():
                row['log_sha256'] = sha(log)
            log.with_suffix('.case.json').write_text(json.dumps(row, indent=2) + '\n')
    return row


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'out', 'uwvm', 'build-receipt', 'wasm-clang', 'wasm-ld', 'rustc', 'wasm-tools', 'llvm-dwarfdump'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--language', action='append', choices=('c', 'cpp', 'rust'))
    parser.add_argument('--dwarf-version', type=int, action='append', choices=(4, 5))
    parser.add_argument('--case', action='append', choices=('into', 'next', 'finish', 'inline-out', 'repeated', 'recursive'))
    args = parser.parse_args()
    for values in (args.language, args.dwarf_version, args.case):
        require(values is None or len(values) == len(set(values)), 'duplicate requested case would overwrite a fresh receipt')
    require(sys.platform == 'linux', 'keeper controlled Linux only')
    root = args.source_root.resolve(strict=True)
    require(Path(__file__).resolve(strict=True) == root / 'test/0017.runtime/run_debug_source_step_cli.py' and
            Path(metadata_cli.__file__).resolve(strict=True) == root / 'test/0017.runtime/run_debug_source_inline_metadata_cli.py', 'actual runner/import paths must match source root')
    guard = root / 'tools/ci/require_wasm3_test_cgroup.sh'
    subprocess.run(['bash', str(guard)], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    binary = args.uwvm.resolve(strict=True)
    receipt_path = args.build_receipt.resolve(strict=True)
    receipt = json.loads(receipt_path.read_text())
    require(Path(receipt['binary_path']).resolve(strict=True) == binary and receipt['binary_sha256'] == sha(binary), 'actual linked product receipt/hash mismatch')
    require(receipt['link_returncode'] == 0 and isinstance(receipt['link_argv'], list) and receipt['link_argv'], 'successful actual link argv required')
    outputs = [receipt['link_argv'][i + 1] for i, value in enumerate(receipt['link_argv'][:-1]) if value == '-o']
    link_cwd = Path(receipt['link_cwd']).resolve(strict=True)
    require(len(outputs) == 1, 'one actual linker output required')
    linked_output = Path(outputs[0])
    if not linked_output.is_absolute():
        linked_output = link_cwd / linked_output
    require(linked_output.resolve(strict=True) == binary, 'actual linker output/cwd must be the passed product')
    before_fingerprint, after_fingerprint = (Path(receipt[name]).resolve(strict=True) for name in ('source_before_file', 'source_after_file'))
    require(sha(before_fingerprint) == sha(after_fingerprint), 'actual build changed source/dependency fingerprint')
    fingerprint = json.loads(before_fingerprint.read_text())
    require(fingerprint.get('source_id') and fingerprint.get('files'), 'complete build source/dependency fingerprint required')
    entries = fingerprint['files']
    require(len({entry['path'] for entry in entries}) == len(entries), 'duplicate build fingerprint paths')
    canonical = json.dumps(entries, sort_keys=True, separators=(',', ':')).encode()
    require(fingerprint['source_id'] == 'sha256:' + hashlib.sha256(canonical).hexdigest(), 'actual fingerprint source_id must hash its real file list')
    build_sources = {entry['path']: entry['sha256'] for entry in entries}
    current = [path for path in (root / 'src').rglob('*') if path.is_file() and path.name != '.DS_Store' and not path.name.startswith('._')]
    source_before = {str(path.relative_to(root)): sha(path) for path in sorted(current)}
    require(all(build_sources.get(name) == value for name, value in source_before.items()) and
            {name for name in build_sources if name.startswith('src/')} == set(source_before), 'fresh product source fingerprint must match actual current source')
    sources = {'c': root / 'test/0017.runtime/fixtures/debug_source_step_c.c',
               'cpp': root / 'test/0017.runtime/fixtures/debug_source_step_cpp.cc',
               'rust': root / 'test/0017.runtime/fixtures/debug_source_step_rust.rs'}
    immutable = [binary, receipt_path, before_fingerprint, after_fingerprint, guard, Path(__file__).resolve(), Path(metadata_cli.__file__).resolve(), *sources.values()]
    tools = {name: getattr(args, name).resolve(strict=True) for name in ('wasm_clang', 'wasm_ld', 'rustc', 'wasm_tools', 'llvm_dwarfdump')}
    immutable += list(tools.values())
    inputs_before = {str(path): sha(path) for path in immutable}
    summary = {'passed': False, 'qualification': 'actual product source stepping; no variable/native/tail-source claim',
               'product_sha256': sha(binary), 'build_source_id': fingerprint['source_id'], 'build_receipt_sha256': sha(receipt_path),
               'inputs_before': inputs_before, 'source_before': source_before, 'tools': {}, 'commands': [], 'fixtures': [], 'cases': []}

    def run(argv: list[str], name: str) -> str:
        command = {'argv': argv, 'cwd': str(root), 'log': str(out / (name + '.log')), 'returncode': None}
        summary['commands'].append(command)
        with (out / (name + '.log')).open('wb') as log:
            completed = subprocess.run(argv, cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=180, check=False)
        command['returncode'] = completed.returncode
        command['log_sha256'] = sha(out / (name + '.log'))
        require(completed.returncode == 0, 'official tool command failed', command)
        require((out / (name + '.log')).stat().st_size <= 16 * 1024 * 1024, 'bounded official tool log')
        return (out / (name + '.log')).read_text(errors='strict')

    try:
        for name, tool in tools.items():
            summary['tools'][name] = {'path': str(tool), 'sha256': sha(tool), 'version': run([str(tool), '--version'], 'version-' + name)}
        if 'rust' in (args.language or ('c', 'cpp', 'rust')):
            libdir = Path(run([str(tools['rustc']), '--print', 'target-libdir', '--target', 'wasm32-unknown-unknown'], 'rust-target-libdir').strip()).resolve(strict=True)
            core = list(libdir.glob('libcore-*.rlib'))
            require(len(core) == 1, 'one actual installed Wasm rust core required', str(libdir))
            rust_libraries = sorted({path.resolve(strict=True) for pattern in ('libcore-*.rlib', 'libcompiler_builtins-*.rlib') for path in libdir.glob(pattern)})
            immutable.extend(rust_libraries)
            inputs_before.update({str(path): sha(path) for path in rust_libraries})
        for language in args.language or ('c', 'cpp', 'rust'):
            source = sources[language]
            markers = {}
            for marker in ('STEP_BEFORE_INLINE', 'STEP_INLINE_ONE', 'STEP_INLINE_TWO', 'STEP_PHYSICAL_CALL', 'STEP_AFTER_CALL', 'STEP_LEAF_ENTRY'):
                lines = [number for number, line in enumerate(source.read_text().splitlines(), 1) if marker in line]
                require(len(lines) == 1, 'one exact fixture source marker', marker)
                markers[marker] = lines[0]
            for version in args.dwarf_version or (4, 5):
                stem = f'{language}-dwarf{version}-O1'
                wasm = out / (stem + '.wasm')
                if language == 'rust':
                    run([str(tools['rustc']), '--edition=2021', '--crate-name', 'source_step_rust', '--target', 'wasm32-unknown-unknown',
                         '-C', 'panic=abort', '-C', 'debuginfo=2', '-C', f'dwarf-version={version}', '-C', 'split-debuginfo=off',
                         '-C', 'opt-level=1', '-C', 'codegen-units=1', '-C', 'linker=' + str(tools['wasm_ld']),
                         '-C', 'link-arg=--no-entry', str(source), '-o', str(wasm)], 'compile-' + stem)
                else:
                    obj = out / (stem + '.o')
                    run([str(tools['wasm_clang']), '--target=wasm32-unknown-unknown', '-g', f'-gdwarf-{version}', '-O1',
                         '-std=c++23' if language == 'cpp' else '-std=c17', '-nostdlib', '-c', str(source), '-o', str(obj)], 'compile-' + stem)
                    run([str(tools['wasm_ld']), '--no-entry', '--export-all', str(obj), '-o', str(wasm)], 'link-' + stem)
                emitted_sha = sha(wasm)
                immutable.append(wasm)
                inputs_before[str(wasm)] = emitted_sha
                run([str(tools['wasm_tools']), 'validate', str(wasm)], 'wasm-validate-' + stem)
                require(sha(wasm) == emitted_sha, 'emitted module changed during official validation', stem)
                run([str(tools['llvm_dwarfdump']), '--verify', str(wasm)], 'dwarf-verify-' + stem)
                require(sha(wasm) == emitted_sha, 'emitted module changed during official DWARF verification', stem)
                oracle = run([str(tools['llvm_dwarfdump']), '--debug-info', '--debug-line', '--debug-ranges', '--debug-rnglists', str(wasm)], 'dwarf-oracle-' + stem)
                require(sha(wasm) == emitted_sha, 'emitted module changed during official line oracle dump', stem)
                require(oracle.count('DW_TAG_inlined_subroutine') >= 4 and 'source_step_inner' in oracle and 'source_step_middle' in oracle,
                        'official compiler did not retain real nested/repeated inline metadata', stem)
                sequences = line_sequences(oracle)
                code_expressions(wasm)  # prove actual imports are empty before indexing any fixture stop
                summary['fixtures'].append({'name': stem, 'source_sha256': sha(source), 'wasm_sha256': sha(wasm),
                    'actual_import_policy': 'all imports rejected by bounded Code oracle',
                    'official_discriminator_nonzero': any(row['discriminator'] for sequence in sequences for row in sequence), 'markers': markers})
                for policy in ('instruction', 'unwind'):
                    mode = ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
                    prefix = [str(binary), '-m', 'debug-jit', *mode, '-Rct', '0', '-Rllvm-call-stack', policy,
                              '-Rllvm-exception-dispatch', 'native-unwind', '-Rllvm-cache-path', 'disable']
                    for kind in args.case or ('into', 'next', 'finish', 'inline-out', 'repeated', 'recursive'):
                        require(sha(wasm) == emitted_sha, 'actual product must consume the officially verified exact module', stem)
                        row = qualify(prefix, wasm, source, sequences, markers, kind, out / f'{stem}-{policy}-{kind}.log')
                        require(sha(wasm) == emitted_sha, 'module changed during actual product source-step case', (stem, kind))
                        row.update(language=language, dwarf_version=version, diagnostic_policy=policy)
                        summary['cases'].append(row)
        subprocess.run(['bash', str(guard)], check=True)
        summary['inputs_after'] = {str(path): sha(path) for path in immutable}
        current_after = [path for path in (root / 'src').rglob('*') if path.is_file() and path.name != '.DS_Store' and not path.name.startswith('._')]
        summary['source_after'] = {str(path.relative_to(root)): sha(path) for path in sorted(current_after)}
        require(summary['inputs_after'] == inputs_before and summary['source_after'] == source_before, 'source/tool/product/build/input changed during qualification')
        summary['passed'] = True
    except BaseException as error:
        summary['error'] = repr(error)
        raise
    finally:
        summary['failed_case_receipts'] = [{'path': str(path), 'sha256': sha(path)} for path in sorted(out.glob('*.case.json'))
                                            if not json.loads(path.read_text()).get('passed')]
        (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f"PASS actual official-tool source stepping: {len(summary['cases'])} console cases; {out / 'summary.json'}")


if __name__ == '__main__':
    main()
