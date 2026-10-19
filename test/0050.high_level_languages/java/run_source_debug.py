#!/usr/bin/env python3
"""Check real producer source break/step positions in the existing Linux cgroup.

A successful source breakpoint does not certify Java/AS/MoonBit variables.
Locals and print capabilities are recorded separately, including refusals.
No module, debug section, source map or producer source is rewritten.
"""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import time


ANSI = re.compile(rb'\x1b\[[0-?]*[ -/]*[@-~]')


def clean(reply):
    return ANSI.sub(b'', reply).decode(errors='replace').replace('\r', '')


def one_line(value):
    if any(character in value for character in '\r\n\0'):
        raise ValueError('debugger arguments must be a single line')
    return value


def variable_result(reply):
    """Distinguish a reply containing names from an actual captured value."""
    unavailable = bool(re.search(r'(?im)^error:|unavailable|not supported|not captured', reply))
    error = bool(re.search(r'(?im)^error:', reply))
    values = [line.strip() for line in reply.splitlines()
              if re.match(r'^\s*[^=\n]+\s=\s', line)
              and '(uwvm-debug)' not in line and '<unavailable' not in line]
    empty = 'No source variables in the current scope.' in reply
    return {'available': bool(values) and not error, 'value_rows': values,
            'explicit_refusal_or_unavailable': unavailable, 'empty_scope': empty,
            'reply': reply}


def position(reply, names, expected_source, line_count, require_line=None):
    current = re.findall(
        r'^thread (\d+) module=(\d+) function=(\d+) byte-offset=(\d+) generation=(\d+)$',
        reply, re.M)
    stops = re.findall(r'^stop-id (\d+)$', reply, re.M)
    sources = re.findall(r'^  source (.+):(\d+):(\d+)$', reply, re.M)
    assert len(current) == len(stops) == len(sources) == 1, reply
    thread, module, function, offset, generation = map(int, current[0])
    source, line, column = sources[0]
    line, column = int(line), int(column)
    assert module == 0 and generation > 0 and int(stops[0]) > 0, reply
    assert os.path.normpath(source) == os.path.normpath(expected_source), (source, expected_source, reply)
    assert 1 <= line <= line_count, reply
    if require_line is not None:
        assert line == require_line, ('breakpoint must reach the requested source line', require_line, reply)
    frames = re.findall(r'^  #0 module=(\d+) function=(\d+) \[[^\n]*\] ([^\n]*)$', reply, re.M)
    assert len(frames) == 1 and (int(frames[0][0]), int(frames[0][1])) == (module, function), reply
    original_name = names.get(function)
    assert original_name, ('stopped function has no name in original module name section', reply)
    return {'thread': thread, 'module': module, 'function': function, 'byte_offset': offset,
            'generation': generation, 'stop_id': int(stops[0]), 'file': source,
            'line': line, 'column': column, 'name': original_name,
            'displayed_name': frames[0][2], 'function_name_display_pass': frames[0][2] == original_name}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'wasm', 'source', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--line', type=int, required=True)
    parser.add_argument('--ros', action='store_true', help='Use the ROS/stable -Raot backend selector')
    parser.add_argument('--expected-source', help='Exact producer-declared source path; defaults to --source resolved path')
    parser.add_argument('--expected-name', help='Optional exact producer function name expected at the source breakpoint')
    parser.add_argument('--expression', default='checksum', help='Source expression to probe without assuming support')
    parser.add_argument('--expected-value', help='Optional exact scalar value; absence never certifies value correctness')
    parser.add_argument('--expected-after-step-value', help='Exact scalar value required after the source step')
    parser.add_argument('--expected-after-step-line', type=int, help='Exact source line required after the source step')
    parser.add_argument('--require-values', action='store_true', help='Fail unless both locals and print expose verified source scalar values')
    parser.add_argument('--arg', action='append', default=[], help='Guest command argument; repeat as needed')
    parser.add_argument('--expected-stdout', type=Path, help='Optional complete expected guest stdout lines')
    parser.add_argument('--helpers', type=Path, default=Path(__file__).resolve().parents[1],
                        help='Directory containing run_matrix.py and build_products.py')
    parser.add_argument('--timeout', type=int, default=240)
    parser.add_argument('--cpu', type=int, default=21)
    parser.add_argument('--compile-threads', type=int, choices=range(8), default=0,
                        help='extra native compiler workers; zero bounds cold compilation concurrency')
    parser.add_argument('--load-arg', action='append', default=[],
                        help='explicit runtime option before --run; repeat and use --load-arg=VALUE')
    args = parser.parse_args()
    assert sys.platform == 'linux', 'execute only inside the authorized SSH Linux cgroup'
    assert 1 <= args.timeout <= 600
    helpers = args.helpers.resolve(strict=True)
    sys.path[:0] = [str(helpers), str(helpers.parent / '0017.runtime')]
    from build_products import admission, digest
    from inspect_wasm import inspect
    from run_matrix import FEATURES, compile_affinity, output_matches, send, target_affinity
    from run_debug_run_cli import Console

    before = admission()
    membership = Path('/proc/self/cgroup').read_text().strip()[3:]
    effective = (Path('/sys/fs/cgroup') / membership.lstrip('/') / 'cpuset.cpus.effective').read_text().strip()
    allowed = set()
    for item in effective.split(','):
        first, _, last = item.partition('-')
        allowed.update(range(int(first), int(last or first) + 1))
    assert args.cpu in allowed, 'execution CPU must belong to the admitted cgroup'
    os.sched_setaffinity(0, {args.cpu})
    execution_cpus = os.sched_getaffinity(0)
    compile_cpus = compile_affinity()
    os.environ.update(TERM='dumb', NO_COLOR='1')
    binary, wasm, source = [path.resolve(strict=True) for path in (args.binary, args.wasm, args.source)]
    lines = source.read_text().splitlines()
    assert 1 <= args.line <= len(lines), 'source line outside the actual source file'
    expected_source = one_line(args.expected_source or str(source))
    expression = one_line(args.expression)
    expected_name = one_line(args.expected_name) if args.expected_name is not None else None
    expected_value = one_line(args.expected_value) if args.expected_value is not None else None
    after_step_value = one_line(args.expected_after_step_value) if args.expected_after_step_value is not None else None
    names = inspect(wasm)['functions']
    assert names, 'original module must retain its producer function-name section'
    mode = ['-Raot'] if args.ros else ['-Rcm', 'full', '-Rcc', 'jit']
    argv = [str(binary), '-Rdbg', *mode, '-Rct', str(args.compile_threads), '-Rllvm-call-stack', 'instruction',
            '-Rllvm-cache-path', 'disable', *['-WFE-' + feature for feature in FEATURES], *[one_line(value) for value in args.load_arg], '--run', str(wasm), *args.arg]
    paths = [binary, wasm, source, Path(__file__).resolve(), helpers / 'run_matrix.py',
             helpers / 'build_products.py', helpers / 'inspect_wasm.py',
             helpers.parent / '0017.runtime/run_debug_run_cli.py']
    if args.expected_stdout:
        paths.append(args.expected_stdout.resolve(strict=True))
    pins = {str(path): digest(path) for path in paths}
    args.out.mkdir(parents=True, exist_ok=False)
    record = {'passed': False, 'scope': 'real producer source breakpoint, original symbol, source step and normal shutdown',
              'typed_language_values_claim': False, 'source_breakpoint_pass': False,
              'source_step_pass': False, 'source_variable_value_verified': False,
              'function_name_display_pass': False,
              'admission_before': before, 'inputs_before': pins, 'argv': argv,
              'cold_compile_cpus': sorted(compile_cpus), 'execution_cpus': sorted(execution_cpus),
              'requested': {'source': str(source), 'expected_source': expected_source,
                            'line': args.line, 'source_text': lines[args.line - 1],
                            'expected_name': expected_name, 'expression': expression,
                            'expected_value': expected_value, 'expected_after_step_value': after_step_value,
                            'expected_after_step_line': args.expected_after_step_line,
                            'require_values': args.require_values, 'guest_arguments': args.arg},
              'actions': [], 'positions': []}
    console = None

    def save():
        (args.out / 'report.json').write_text(json.dumps(record, indent=2) + '\n')

    def action(command, timeout=None):
        reply = clean(send(console, command, args.timeout if timeout is None else timeout))
        record['actions'].append({'command': command, 'reply': reply})
        save()
        return reply

    def settle(reply, required):
        deadline = time.monotonic() + args.timeout
        while True:
            assert not re.search(r'(?im)^error:', reply), reply
            if required in reply:
                return reply
            assert 'Program exited' not in reply, ('guest exited before the required stop', required, reply)
            assert time.monotonic() < deadline, ('no actual debugger completion', required, reply)
            reply = action('wait', max(1, int(deadline - time.monotonic())))

    def current(thread, required_line=None):
        result = position(action(f'bt {thread}'), names, expected_source, len(lines), required_line)
        record['positions'].append(result)
        save()
        return result

    def verified_scalar(reply, expected):
        values = re.findall(r'^\s*' + re.escape(expression) + r'\s*=\s*(?:[iuf]\d+=)?([^\n]+)$', reply, re.M)
        return values == [expected], values

    save()
    try:
        os.sched_setaffinity(0, compile_cpus)
        try:
            console = Console(argv)
        finally:
            os.sched_setaffinity(0, execution_cpus)
        console.wait(lambda reply: b'(uwvm-debug) ' in reply, 0, 600)
        assert Path(f'/proc/{console.pid}/cgroup').read_text() == Path('/proc/self/cgroup').read_text()
        prepared = action('status')
        assert 'prepared; no Wasm instruction executed' in prepared, prepared
        action('info modules')
        action('bs 0')
        breakpoint = action(f'bs 0 {expected_source}:{args.line}')
        match = re.search(r'breakpoint (\d+)', breakpoint, re.I)
        assert match and not re.search(r'(?im)^error:', breakpoint), breakpoint
        identifier = int(match[1])
        # The prepared console precedes cold ORC materialization. Keep admitted
        # compilation CPUs until startw has reached a real Wasm entry stop.
        entry = action('startw', 600)
        assert re.search(r'thread \d+ module=0 function=\d+ byte-offset=0\b', entry), entry
        record['cold_compile_entry_stop'] = True
        target_affinity(console.pid, execution_cpus)
        assert os.sched_getaffinity(console.pid) == execution_cpus
        stopped = settle(action('continue'), 'stopped: breakpoint')
        target_affinity(console.pid, execution_cpus)
        threads = re.findall(r'thread (\d+) module=0 function=', stopped)
        assert len(threads) == 1, stopped
        thread = int(threads[0])
        first = current(thread, args.line)
        if expected_name is not None:
            assert first['name'] == expected_name, ('source breakpoint reached an unrelated producer function', expected_name, first)
        record['source_breakpoint_pass'] = True
        record['function_name_display_pass'] = first['function_name_display_pass']
        record['locals'] = variable_result(action('info locals'))
        printed = action('print ' + expression)
        record['print'] = variable_result(printed)
        if expected_value is not None and record['print']['available']:
            # GDB-style scalar output, with optional explicit scalar-kind prefix.
            verified, values = verified_scalar(printed, expected_value)
            record['source_variable_value_verified'] = verified
            if not verified:
                record['print']['value_mismatch'] = {'expected': expected_value, 'actual_rows': values}
        # Explicit unsupported/empty-variable replies are evidence, not a source
        # break/step failure and never a successful managed-locals claim.
        record['typed_language_values_claim'] = record['source_variable_value_verified']
        action(f'delete {identifier}')
        target_affinity(console.pid, execution_cpus)
        stepped = settle(action(f'step source {thread} over'), 'stopped:')
        target_affinity(console.pid, execution_cpus)
        assert 'selected participant step' in stepped, stepped
        second = current(thread, args.expected_after_step_line)
        assert second['stop_id'] > first['stop_id'], (first, second)
        assert (second['function'], second['byte_offset']) != (first['function'], first['byte_offset']), (first, second)
        assert (second['file'], second['line'], second['column']) != (first['file'], first['line'], first['column']), (first, second)
        record['source_step_pass'] = True
        record['function_name_display_pass'] &= second['function_name_display_pass']
        if after_step_value is not None:
            after_locals = action('info locals')
            after_print = action('print ' + expression)
            record['locals_after_step'] = variable_result(after_locals)
            record['print_after_step'] = variable_result(after_print)
            record['source_variable_after_step_verified'], values = verified_scalar(after_print, after_step_value)
            if not record['source_variable_after_step_verified']:
                record['print_after_step']['value_mismatch'] = {'expected': after_step_value, 'actual_rows': values}
            record['locals_after_step_value_verified'], _ = verified_scalar(after_locals, after_step_value)
        if expected_value is not None:
            record['locals_value_verified'], _ = verified_scalar(record['locals']['reply'], expected_value)
        values_pass = (not args.require_values or (
            record['source_variable_value_verified'] and record.get('locals_value_verified', False)
            and (after_step_value is None or (record.get('source_variable_after_step_verified', False)
                                             and record.get('locals_after_step_value_verified', False)))))
        record['typed_language_values_claim'] = record['source_variable_value_verified'] and values_pass
        action('disable')
        target_affinity(console.pid, execution_cpus)
        finished = settle(action('continue'), 'Program exited normally.')
        target_affinity(console.pid, execution_cpus)
        if args.expected_stdout:
            transcript = clean(bytes(console.log))
            assert output_matches(transcript.encode(), args.expected_stdout.read_text()), transcript[-5000:]
            record['expected_guest_output_verified'] = True
        console.finish()
        record['normal_quit_and_terminal_restore'] = True
        record['passed'] = ('value_mismatch' not in record['print'] and record['function_name_display_pass']
                            and values_pass and 'value_mismatch' not in record.get('print_after_step', {}))
        if not values_pass:
            record['error'] = 'Required source scalar values were unavailable or did not match the source/JVM oracle'
        if not record['function_name_display_pass']:
            record['error'] = 'The actual bt frame name does not match the original producer name; source/variable/step evidence is recorded separately'
    except Exception as error:
        record['error'] = repr(error)
    finally:
        if console is not None:
            (args.out / 'console.log').write_bytes(console.log)
            console.close()
        record['inputs_after'] = {path: digest(path) for path in pins}
        record['admission_after'] = admission()
        if record['inputs_after'] != pins:
            record.update(passed=False, inputs_changed=True)
        if record['admission_after']['memory_events'] != before['memory_events']:
            record.update(passed=False, memory_events_changed=True)
        save()
    print(json.dumps({'passed': record['passed'], 'source_breakpoint_pass': record['source_breakpoint_pass'],
                      'source_step_pass': record['source_step_pass'],
                      'function_name_display_pass': record['function_name_display_pass'],
                      'source_variable_value_verified': record['source_variable_value_verified'],
                      'error': record.get('error'), 'report': str(args.out / 'report.json')}), flush=True)
    raise SystemExit(0 if record['passed'] else 1)


if __name__ == '__main__':
    main()
