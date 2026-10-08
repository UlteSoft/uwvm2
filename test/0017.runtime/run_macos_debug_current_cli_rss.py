#!/usr/bin/env python3
"""Actual current Mac arm64 source/Wasm/native stepping and replacement acceptance.

All native children use the new serial no-fork owner. Official C5 O1 DWARF
staging and the fresh complete fused source/build closure are required first.
Physical-memory observations and actual per-process peaks qualify fixed small
cases; no aggregate kernel cap or virtual-address limit is claimed.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import sys
import time
import macos_owned_debug_process_rss as owner
import run_debug_source_step_cli as source_cli
import macos_debug_current_build_contract as build_contract


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(value: bool, message: str, detail: object = None) -> None:
    if not value:
        raise AssertionError((message, detail))


def read_json(path: Path, limit: int = 32 << 20) -> dict:
    require(path.is_file() and path.stat().st_size <= limit, 'bounded actual receipt required')
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


class Console:
    def __init__(self, lease: owner.Lease, command: list[str], log: Path) -> None:
        self.command, self.logpath = command, log
        self.child = lease.start(command, log.with_suffix('.owned'))
        self.stream = self.child.stdout_path.open('rb')
        self.pending, self.transcript = bytearray(), bytearray()
        try:
            self.prompt()
        except BaseException:
            self.child.abort(); self._close_log()
            raise

    def prompt(self) -> bytes:
        marker = b'(uwvm-debug) '
        deadline = time.monotonic() + 40
        while marker not in self.pending:
            info = self.child.check()
            chunk = self.stream.read(65536)
            if chunk:
                self.pending.extend(chunk); self.transcript.extend(chunk)
                require(len(self.transcript) <= owner.OUTPUT_LIMIT, 'bounded actual console transcript')
            else:
                require(info.status != 5 and time.monotonic() < deadline, 'actual console exited or prompt deadline elapsed',
                        (bytes(self.transcript)[-4000:], self.child.stderr_path.read_bytes()[-4000:]))
                time.sleep(owner.POLL_SECONDS)
        end = self.pending.index(marker) + len(marker)
        result = bytes(self.pending[:end]); del self.pending[:end]
        return result

    def send(self, command: str) -> bytes:
        self.child.send(command)
        return self.prompt()

    def finish(self) -> None:
        try:
            self.child.send('quit')
            require(self.child.finish() == 0, 'actual console quit returned nonzero')
        finally:
            if not self.child.closed:
                self.child.abort()
            self._close_log()

    def _close_log(self) -> None:
        if not self.stream.closed:
            self.transcript.extend(self.stream.read()); self.stream.close()
        self.logpath.write_bytes(self.transcript)


class Session(source_cli.Session):
    def __init__(self, lease: owner.Lease, command: list[str], log: Path, expressions: dict[int, int],
                 sequences: list[list[dict]], source: Path) -> None:
        # Reuse the pinned actual protocol/oracle methods, replacing only their
        # raw Popen transport with the owned no-fork regular-file transport.
        self.console = Console(lease, command, log)
        self.expressions, self.sequences, self.source = expressions, sequences, source
        self.expression_sizes = expression_sizes(Path(command[-1]), expressions)
        self.thread = self.breakpoint = self.last_step_stop = 0
        self.actions, self.positions = [], []


    def position(self) -> dict:
        value = super().position()
        require(type(value['offset']) is int and 0 <= value['offset'] < self.expression_sizes[value['function']],
                'actual function-relative source offset is outside its official Code body', value)
        return value


def expression_sizes(wasm: Path, expressions: dict[int, int]) -> dict[int, int]:
    rows = [payload for kind, payload in source_cli.metadata_cli.sections(wasm) if kind == 10]
    require(len(rows) == 1, 'one actual official Code payload required')
    payload = rows[0]
    count, cursor = source_cli.metadata_cli.u32(payload, 0, len(payload))
    result = {}
    for index in range(count):
        size, body = source_cli.metadata_cli.u32(payload, cursor, len(payload))
        require(size <= len(payload) - body and index in expressions, 'bounded official Code body required')
        end = body + size
        require(body <= expressions[index] < end, 'official expression start not inside body')
        result[index] = end - expressions[index]
        cursor = end
    require(cursor == len(payload) and set(result) == set(expressions), 'entire official Code payload required')
    return result


def actual_stop(console: Console, participant: int) -> dict:
    raw = console.send(f'bt {participant}')
    clean = re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', raw)
    threads = re.findall(rb'(?m)^thread (\d+) module=(\d+) function=(\d+) byte-offset=(\d+) generation=(\d+)\r?$', clean)
    stop = re.findall(rb'(?m)^stop-id (\d+)\r?$', clean)
    require(len(threads) == 1 and int(threads[0][0]) == participant and len(stop) == 1 and int(stop[0]) > 0,
            'one real selected participant and nonzero stop label required', clean)
    return {'thread': int(threads[0][0]), 'module': int(threads[0][1]), 'function': int(threads[0][2]),
            'offset': int(threads[0][3]), 'code_generation': int(threads[0][4]), 'stop_id': int(stop[0]), 'raw': clean.decode()}


def begin(console: Console, target: int, offset: int = 0) -> int:
    require(b'prepared; no Wasm instruction executed' in console.send('status'), 'real prepared debug-full console required')
    reply = console.send(f'break 0 {target} {offset}')
    point = re.search(rb'breakpoint (\d+)', reply)
    require(point is not None, 'actual emitted breakpoint missing', reply)
    console.send('continue')
    for _ in range(20):
        stopped = console.send('wait')
        if b'stopped: breakpoint' in stopped:
            break
    else:
        raise AssertionError('actual breakpoint did not stop guest')
    match = re.search(rb'thread (\d+) module=0 function=' + str(target).encode() + rb' byte-offset=' + str(offset).encode() + rb' ', stopped)
    require(match is not None, 'actual selected participant/location missing', stopped)
    console.send('delete ' + point.group(1).decode())
    return int(match.group(1))


def exit_guest(console: Console) -> None:
    console.send('continue')
    for _ in range(20):
        reply = console.send('wait')
        if b'guest exited:' in reply:
            require(b'guest exited: 0' in reply, 'real fixture computation trapped/failed', reply)
            return
    raise AssertionError('actual guest exit not observed')


def replacement_offset(wasm: Path) -> tuple[int, int]:
    target, body = source_cli.metadata_cli.function(wasm, '_start')
    groups, expression = source_cli.metadata_cli.u32(body, 0, len(body))
    require(groups == 0, 'focused replacement requires no declared locals')
    code = body[expression:]
    # The officially validated fixed WAT has exactly two i32.const 5 call
    # sequences. This is a bounded fixture oracle, not an instruction parser
    # or source/runtime capability. Assert the entire numeric body pattern.
    leaf, _ = source_cli.metadata_cli.function(wasm, 'numeric_leaf')
    call = b'\x41\x05\x10' + source_cli.metadata_cli.leb(leaf)
    expected = call + b'\x41\x0c\x47\x04\x40\x00\x0b' + call + b'\x41\x0d\x47\x04\x40\x00\x0b\x0b'
    require(code == expected, 'actual official fixed replacement expression differs; do not guess an offset')
    return target, len(call) + 7


def run_case(lease: owner.Lease, prefix: list[str], wasm: Path, log: Path, kind: str,
             source: Path | None = None, sequences: list[list[dict]] | None = None, markers: dict | None = None,
             replacement: Path | None = None, badbody: Path | None = None) -> dict:
    row = {'case': kind, 'passed': False, 'actions': [], 'positions': []}
    console = None
    try:
        if kind == 'source-finish':
            session = Session(lease, [*prefix, '--run', str(wasm)], log, source_cli.code_expressions(wasm), sequences, source)
            console = session.console
            row['actions'], row['positions'] = session.actions, session.positions
            leaf = source_cli.metadata_cli.function(wasm, 'source_step_leaf')[0]
            outer = source_cli.metadata_cli.function(wasm, 'source_step_outer')[0]
            session.begin(leaf)
            session.seek(lambda p: p['function'] == leaf and p['line'] == markers['STEP_LEAF_ENTRY'] and p['is_statement'], 'real C5 O1 leaf statement')
            after = session.source_step('out')
            require(after['function'] == outer and not after['inline'], 'source finish did not reach actual caller statement', after)
            session.finish_guest()
        else:
            console = Console(lease, [*prefix, '--run', str(wasm)], log)
            if kind == 'wasm-native':
                leaf = source_cli.metadata_cli.function(wasm, 'numeric_leaf')[0]
                participant = begin(console, leaf)
                before = actual_stop(console, participant)
                stepped = console.send(f'step wasm {participant}')
                require(b'stopped: selected participant step' in stepped, 'actual Wasm step rejected', stepped)
                after = actual_stop(console, participant)
                require(after['stop_id'] > before['stop_id'] and (after['function'], after['offset']) != (before['function'], before['offset']), 'real Wasm position/stop label did not advance')
                row['positions'].extend((before, after))
                previous_to = None
                for _ in range(2):
                    reply = console.send(f'step asm {participant}')
                    matches = re.findall(rb'native instruction 0x([0-9a-fA-F]+) bytes=([0-9a-fA-F ]+)  ([^\r\n]+?) -> 0x([0-9a-fA-F]+)', reply)
                    require(len(matches) == 1 and bool(matches[0][1].strip()) and bool(matches[0][2].strip()), 'actual decoded native instruction missing', reply)
                    start, end = int(matches[0][0], 16), int(matches[0][3], 16)
                    require(start != 0 and end != 0 and (previous_to is None or start == previous_to), 'consecutive real native PCs disagree', reply)
                    current = actual_stop(console, participant)
                    pcs = re.findall(r'  native-pc=0x([0-9a-fA-F]+)', current['raw'])
                    require(len(pcs) == 1 and int(pcs[0], 16) == end and current['stop_id'] > after['stop_id'], 'native trap does not own this actual PC/stop', current)
                    require('native trap has no current Wasm source position' in current['raw'] and '\n  source ' not in current['raw'], 'native trap reused a stale Wasm/source position', current)
                    row['positions'].append(current); after, previous_to = current, end
                exit_guest(console)
            elif kind == 'replacement':
                target, offset = replacement_offset(wasm)
                participant = begin(console, target, offset)
                leaf = source_cli.metadata_cli.function(wasm, 'numeric_leaf')[0]
                before = actual_stop(console, participant)
                rejected = console.send(f'replace 0 {leaf} 1 {badbody}')
                require(b'control request rejected' in rejected and b'function replaced;' not in rejected, 'malformed replacement did not fail closed', rejected)
                failed = actual_stop(console, participant)
                require(failed['stop_id'] == before['stop_id'] and failed['code_generation'] == before['code_generation'], 'failed replacement changed current stop/publication', (before, failed))
                good = console.send(f'replace 0 {leaf} 1 {replacement}')
                require(b'function replaced; generation 2' in good, 'valid same-ABI replacement did not retain generation 1 after failure', good)
                after = actual_stop(console, participant)
                require(after['stop_id'] > before['stop_id'] and after['code_generation'] == before['code_generation'], 'replacement stop label changed without preserving the current caller runtime epoch')
                row['replacement_function_generation'] = 2
                row['positions'].extend((before, failed, after))
                exit_guest(console)  # first result 12, post-replacement result 13
            else:
                raise AssertionError('unknown bounded case')
        row.update(actual_argv=console.command, wasm_sha256=sha(wasm), passed=True)
    except BaseException as error:
        row['error'] = repr(error)
        raise
    finally:
        try:
            if console is not None:
                console.finish()
        except BaseException as error:
            row.update(passed=False, finish_error=repr(error))
            raise
        finally:
            if log.is_file():
                row.update(log=str(log), log_sha256=sha(log))
            log.with_suffix('.case.json').write_text(json.dumps(row, indent=2) + '\n')
    return row


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'product', 'product-qualification', 'fixture-receipt', 'wasm', 'replace-wasm', 'replacement-wasm', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--qualification-sha256', required=True)
    parser.add_argument('--as-failure-receipt', type=Path, required=True)
    parser.add_argument('--as-failure-sha256', required=True)
    parser.add_argument('--fixture-receipt-sha256', required=True)
    parser.add_argument('--repository', choices=('ordinary', 'ros'), required=True)
    parser.add_argument('--subset', choices=('all', 'no-memory', 'source'), default='all')
    args = parser.parse_args()
    require(sys.platform == 'darwin' and os.uname().machine == 'arm64', 'actual current macOS arm64 only')
    require(not any(name.startswith('DYLD_') for name in os.environ), 'unqualified loader injection/search overrides refused')
    root = args.source_root.resolve(strict=True)
    require(Path(__file__).resolve() == root / 'test/0017.runtime/run_macos_debug_current_cli_rss.py' and
            Path(owner.__file__).resolve() == root / 'test/0017.runtime/macos_owned_debug_process_rss.py' and
            Path(source_cli.__file__).resolve() == root / 'test/0017.runtime/run_debug_source_step_cli.py' and
            Path(source_cli.metadata_cli.__file__).resolve() == root / 'test/0017.runtime/run_debug_source_inline_metadata_cli.py' and
            Path(build_contract.__file__).resolve() == root / 'test/0017.runtime/macos_debug_current_build_contract.py', 'actual runner/owner/oracle source paths mismatch')
    qpath, fpath, binary = (getattr(args, name).resolve(strict=True) for name in ('product_qualification', 'fixture_receipt', 'product'))
    require(sha(qpath) == args.qualification_sha256 and sha(fpath) == args.fixture_receipt_sha256, 'actual keeper-pinned qualification packet changed')
    qualified, fixture = read_json(qpath), read_json(fpath)
    require(qualified.get('schema') == 1 and qualified.get('purpose') == 'actual-current-macos-arm64-debug-full-rss-qualified' and
            qualified.get('passed') is True and qualified.get('inputs_before') == qualified.get('inputs_after') and qualified.get('repository') == args.repository and sha(binary) == qualified['product_sha256'], 'fresh complete fused Mach-O qualification absent')
    require(qualified.get('rpaths') == [] and type(qualified.get('actual_system_loads')) is list and
            bool(qualified.get('actual_main_runtime_layout_contract')) and qualified.get('actual_sdk_sysroot') == qualified.get('sdk_root') and
            type(qualified.get('actual_linker')) is dict, 'actual r2 layout/SDK/Mach-O closure fields absent')
    require(build_contract.system_paths(qualified['actual_system_loads']) == qualified['actual_system_loads'],
            'qualified packet contains an unadmitted actual parsed system load')
    entries = qualified['source_files']
    require(current_entries(root) == entries and qualified['source_id'] == 'sha256:' + hashlib.sha256(json.dumps(entries, sort_keys=True, separators=(',', ':')).encode()).hexdigest(), 'Mac actual source/dependency closure differs from fresh build')
    require(all(sha(root / path) == value for path, value in qualified['production_pins'].items()), 'actual current fused producer/joint source pins changed')
    require(fixture.get('schema') == 1 and fixture.get('purpose') == 'actual-official-c5-O1-fixture-for-current-macos' and fixture.get('passed') is True and
            fixture['inputs_before'] == fixture['inputs_after'] and fixture.get('dwarf_version') == 5 and fixture.get('optimization') == 'O1', 'official C5 O1 fixture/oracle packet absent')
    package = fpath.parent
    wasm, oracle = package / 'c5-O1.wasm', package / 'oracle.log'
    source = root / 'test/0017.runtime/fixtures/debug_source_step_c.c'
    require(sha(wasm) == fixture['wasm_sha256'] and sha(oracle) == fixture['oracle_sha256'] and sha(source) == fixture['source_sha256'], 'actual transferred official source/Wasm/oracle changed')
    source_cli.code_expressions(wasm)
    sequences = source_cli.line_sequences(oracle.read_text())
    numeric, replace, new = (getattr(args, name).resolve(strict=True) for name in ('wasm', 'replace_wasm', 'replacement_wasm'))
    # Each minimal no-memory module is also officially encoded/validated by
    # the staging fixture receipt; arbitrary binary fixtures are not admitted.
    records = fixture.get('numeric_modules', {})
    for name, path in (('numeric', numeric), ('replace', replace), ('replacement', new)):
        require(records.get(name, {}).get('wasm_sha256') == sha(path), 'official minimal numeric module pin absent')
        fixture_wat = root / 'test/0017.runtime/fixtures' / ('debug_current_macos_numeric.wat' if name == 'numeric' else 'debug_current_macos_replace.wat' if name == 'replace' else 'debug_current_macos_replace_new.wat')
        require(sha(fixture_wat) == records[name]['wat_sha256'], 'actual current WAT source differs from officially encoded fixture')
        rows = source_cli.metadata_cli.sections(path)
        require(not any(kind in (2, 5, 11) for kind, _ in rows), 'minimal probe must have no imports/linear memory/data')
        source_cli.code_expressions(path)
    failure = args.as_failure_receipt.resolve(strict=True)
    as_failure = owner.prior_as_failure(failure, args.as_failure_sha256)
    immutable = {failure, binary, qpath, fpath, source, wasm, oracle, numeric, replace, new, Path(__file__).resolve(), Path(owner.__file__).resolve(),
                 Path(source_cli.__file__).resolve(), Path(source_cli.metadata_cli.__file__).resolve(), Path(build_contract.__file__).resolve(),
                 Path(build_contract.build_helpers.__file__).resolve()}
    before = {str(path): sha(path) for path in immutable}
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=False)
    summary = {'passed': False, 'scope': 'current arm64 single-module C5 finish; Wasm/native two-trap; same-ABI replacement; actual no-fork owner',
               'source_id': qualified['source_id'], 'repository': args.repository, 'subset': args.subset, 'inputs_before': before, 'cases': [],
               'limits': 'observed whole no-fork tree+supervisor RSS stop512MiB plus reported final-task counters below4GiB; no full-PID lifetime upper bound or aggregate kernel cap',
               'prior_actual_AS_failure': as_failure}
    try:
        allowed = []
        for policy in ('instruction', 'unwind'):
            mode = ['-Raot'] if args.repository == 'ros' else ['-Rcc', 'jit', '-Rcm', 'full']
            prefix = [str(binary), '-m', 'debug-jit', *mode, '-Rct', '0', '-Rllvm-call-stack', policy,
                      '-Rllvm-exception-dispatch', 'native-unwind', '-Rllvm-cache-path', 'disable']
            for kind, module in (('wasm-native', numeric), ('replacement', replace), ('source-finish', wasm)):
                if (args.subset == 'no-memory' and kind == 'source-finish') or (args.subset == 'source' and kind != 'source-finish'):
                    continue
                allowed.append(tuple([*prefix, '--run', str(module)]))
        reject_mode = ['-Rcc', 'jit', '-Rcm', 'lazy'] if args.repository == 'ordinary' else ['-Rint']
        allowed.append(tuple([str(binary), '-m', 'debug-jit', *reject_mode, '--run', str(numeric)]))
        with owner.Lease(tuple(allowed), as_failure=as_failure) as lease:
            probe = lease.start(['/usr/bin/true'], out / 'actual-owner-orphan-probe', orphan=True)
            require(probe.finish() == 0 and probe.receipt['group_retired'] and probe.receipt['root_reaped'] and
                    probe.receipt.get('orphan_after_root_exit'), 'actual no-fork/post-exit orphan/kernel peak ownership probe failed')
            summary['owner_probe'] = probe.receipt
            body = source_cli.metadata_cli.function(new, 'numeric_leaf')[1]
            replacement = out / 'official-replacement.body'; replacement.write_bytes(body)
            badbody = out / 'invalid-replacement.body'; badbody.write_bytes(b'\0\xff\x0b')
            before.update({str(replacement): sha(replacement), str(badbody): sha(badbody)})
            for policy in ('instruction', 'unwind'):
                mode = ['-Raot'] if args.repository == 'ros' else ['-Rcc', 'jit', '-Rcm', 'full']
                prefix = [str(binary), '-m', 'debug-jit', *mode, '-Rct', '0', '-Rllvm-call-stack', policy,
                          '-Rllvm-exception-dispatch', 'native-unwind', '-Rllvm-cache-path', 'disable']
                # Fixed no-memory paths execute first. Product mapping protection
                # remains unchanged; no virtual-address limit is imposed.
                for kind, module in (('wasm-native', numeric), ('replacement', replace), ('source-finish', wasm)):
                    if (args.subset == 'no-memory' and kind == 'source-finish') or (args.subset == 'source' and kind != 'source-finish'):
                        continue
                    row = run_case(lease, prefix, module, out / f'{policy}-{kind}.log', kind,
                                   source, sequences, fixture['markers'], replacement, badbody)
                    row['diagnostic_policy'] = policy; summary['cases'].append(row)
            reject_mode = ['-Rcc', 'jit', '-Rcm', 'lazy'] if args.repository == 'ordinary' else ['-Rint']
            rejected = lease.start([str(binary), '-m', 'debug-jit', *reject_mode, '--run', str(numeric)], out / 'unsupported-mode')
            result = rejected.finish()
            clean = re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', rejected.stdout_path.read_bytes() + rejected.stderr_path.read_bytes())
            expected = b'llvm-jit/lazy' if args.repository == 'ordinary' else b'uwvm-int/full'
            require(0 < result <= 255 and b'[fatal]' in clean and b'unsupported in the current mode: ' + expected in clean, 'crash/loader/parser failure is not unsupported-mode PASS', clean[-4000:])
            summary['cases'].append({'case': 'unsupported-mode', 'passed': True, 'actual_argv': rejected.argv, 'returncode': result})
            summary['swap_before'], summary['swap_after'] = lease.swap_before, lease.native.swap()
        summary['inputs_after'] = {str(path): sha(path) for path in before}
        require(summary['inputs_after'] == before and current_entries(root) == entries, 'actual source/product/packet/input changed during qualification')
        receipts = [read_json(path) for path in sorted(out.rglob('owner.receipt.json'))]
        require(bool(receipts) and all(row.get('passed') is True for row in receipts), 'every exact owned case/probe must qualify')
        root_peak = max(row['wait4_root_maxrss_bytes'] for row in receipts)
        witness_peak = max(row.get('witness_stopped_kernel_maxrss_bytes', 0) for row in receipts)
        supervisor_peak = int(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
        summary['actual_reported_task_peak_evidence'] = {'root_serial_maxrss_bytes': root_peak, 'witness_serial_maxrss_bytes': witness_peak,
                                              'supervisor_maxrss_bytes': supervisor_peak,
                                              'sum_reported_bytes': owner.peak_upper(root_peak, supervisor_peak, witness_peak),
                                              'scope': 'reported final-task counters, not a full-PID lifetime upper bound'}
        summary['max_of_recorded_budget_evidence_bytes'] = max(
            summary['actual_reported_task_peak_evidence']['sum_reported_bytes'],
            *(row['max_of_recorded_budget_evidence_bytes'] for row in receipts))
        require(summary['max_of_recorded_budget_evidence_bytes'] < owner.PHYSICAL_LIMIT, 'recorded budget evidence reached4GiB')
        summary['passed'] = True
    except BaseException as error:
        summary['error'] = repr(error)
        summary['qualification_limit_note'] = 'An accounting/name-port/no-fork/RSS/swap qualification failure is not a VM correctness failure. Preserve actual logs and return unavailable; never count this run as PASS.'
        raise
    finally:
        summary['owner_receipts'] = [{'path': str(path), 'sha256': sha(path)} for path in sorted(out.rglob('owner.receipt.json'))]
        (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f"PASS current actual Mac arm64 {args.subset}: {len(summary['cases'])} cases plus owned orphan/reported task-peak proof; only named case scope, no server/midrun/source-variable/other-language claim")


if __name__ == '__main__':
    main()
