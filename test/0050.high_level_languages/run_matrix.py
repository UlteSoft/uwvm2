#!/usr/bin/env python3
"""Actual compiler-produced guest execution and debugger checks, Linux only.

This checks the Wasm debugger. Source-language/PDB support is reported
separately; stopping in a Mono/QuickJS interpreter does not prove managed locals.
"""
import argparse
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import select
import signal
import subprocess
import sys
import termios
import time

from build_products import admission, digest
from inspect_wasm import inspect


FEATURES = ['gc', 'exceptions', 'tail-call', 'function-references', 'reference-types',
            'simd', 'relaxed-simd', 'extended-const', 'multi-memory', 'memory64', 'table64']


def argv_for(binary, repository, case, debug=False):
    repositories = {'uwvm2', 'uwvm2-stable', 'uwvm2-ros'}
    if repository not in repositories:
        raise ValueError('unknown repository: ' + str(repository))
    common = case.get('load_args', [])
    specific = case.get('load_args_by_repository', {})
    if not isinstance(common, list) or not all(isinstance(arg, str) for arg in common):
        raise ValueError('load_args must be a list of strings')
    if not isinstance(specific, dict) or any(repo not in repositories for repo in specific):
        raise ValueError('load_args_by_repository requires known repository keys')
    if any(not isinstance(args, list) or not all(isinstance(arg, str) for arg in args)
           for args in specific.values()):
        raise ValueError('repository load arguments must be lists of strings')
    mode = ['-Rcm', 'full', '-Rcc', 'jit'] if repository == 'uwvm2' else ['-Raot']
    return [str(binary), *(['-Rdbg'] if debug else []), *mode, '-Rct', str(case.get('debug_compile_threads', 7) if debug else 0),
            '-Rllvm-cache-path', 'disable', '-Rclog', 'err',
            *['-WFE-' + f for f in case.get('features', FEATURES)],
            *common, *specific.get(repository, []), '--run', case['wasm'], *case.get('args', [])]


def process_memory(pid):
    fields = dict(line.split(':', 1) for line in Path(f'/proc/{pid}/status').read_text().splitlines() if ':' in line)
    return {key: int(fields[key].split()[0]) * 1024 for key in ['VmRSS', 'VmHWM']}


def checked_wait(c, predicate, at, timeout, progress_log=None):
    last = 0
    cg = Path('/sys/fs/cgroup') / Path('/proc/self/cgroup').read_text().strip()[3:].lstrip('/')
    def checked(output):
        nonlocal last
        if time.monotonic() - last >= .5:
            last = time.monotonic()
            memory = process_memory(c.pid)
            anon = dict(line.split() for line in (cg / 'memory.stat').read_text().splitlines())['anon']
            assert memory['VmRSS'] < 56 << 30 and int(anon) < 60 << 30, 'cold compiler memory budget exceeded'
            if progress_log is not None:
                progress_log.write_bytes(c.log)
        return predicate(output)
    return c.wait(checked, at, timeout)


def send(c, command, timeout=240, progress_log=None):
    at = len(c.log)
    os.write(c.fd, command.encode() + b'\n')
    return checked_wait(c, lambda b: re.search(rb'(?:^|\n)\(uwvm-debug\) $', b), at, timeout, progress_log)


def output_matches(reply, expected):
    text = reply.decode(errors='replace').replace('\r', '')
    lines = iter(text.splitlines())
    return all(any(actual == line for actual in lines) for line in expected.splitlines())


def strict_debug_stdout(reply, expected):
    """Check the complete continue payload; retain every other reply byte.

    PTY CRLF is the only normalization. A unique pair of complete status lines
    separates the guest payload from editor echo/prompts. Prefix filtering is
    deliberately finite and cannot distinguish guest text imitating diagnostics;
    the fixed expected output must contain none of those prefix literals.
    """
    prefixes = (b'[llvm-jit-full] ', b'[llvm-jit-cache] ', b'[gc-managed] ')
    expected_bytes = expected.encode('utf-8')
    normalized = reply.replace(b'\r\n', b'\n')
    starts = list(re.finditer(rb'(?m)^Continuing\.\n', normalized))
    ends = list(re.finditer(rb'(?m)^Program exited normally\.\n', normalized))
    result = {
        'passed': False, 'scope': 'complete guest stdout payload of continue reply',
        'normalization': 'PTY CRLF -> LF only; every other byte is unchanged',
        'raw_reply_sha256': hashlib.sha256(reply).hexdigest(),
        'normalized_reply_sha256': hashlib.sha256(normalized).hexdigest(),
        'expected_guest_stdout_sha256': hashlib.sha256(expected_bytes).hexdigest(),
        'whole_line_diagnostic_prefixes': [p.decode('ascii') for p in prefixes],
        'diagnostic_complete_lf_required': True, 'diagnostic_lone_cr_rejected': True,
        'expected_contains_diagnostic_prefix_literal': any(p in expected_bytes for p in prefixes),
        'marker_counts': {'continuing': len(starts), 'normal_exit': len(ends)},
        'guest_stdout_sha256': None, 'internal_diagnostics_sha256': None,
        'guest_stdout_bytes': None, 'internal_diagnostics_bytes': None,
        'prefix_limitation': 'guest bytes deliberately imitating a diagnostic prefix are not distinguishable in this PTY stream',
    }
    if result['expected_contains_diagnostic_prefix_literal']:
        result['error'] = 'expected output contains a diagnostic prefix literal'
        return result
    if len(starts) != 1 or len(ends) != 1 or starts[0].end() > ends[0].start():
        result['error'] = 'unique ordered complete Continuing/normal-exit lines required'
        return result
    payload = normalized[starts[0].end():ends[0].start()]
    guest, diagnostics = [], []
    # Split only on LF. Bare CR and other control bytes must not become line
    # boundaries or disappear through splitlines/strip/decode replacement.
    pieces = payload.split(b'\n')
    for index, piece in enumerate(pieces):
        line = piece + (b'\n' if index != len(pieces) - 1 else b'')
        if line.endswith(b'\n') and b'\r' not in line and line.startswith(prefixes):
            diagnostics.append(line)
        else:
            guest.append(line)
    guest_bytes, diagnostic_bytes = b''.join(guest), b''.join(diagnostics)
    result.update(guest_stdout_sha256=hashlib.sha256(guest_bytes).hexdigest(),
                  internal_diagnostics_sha256=hashlib.sha256(diagnostic_bytes).hexdigest(),
                  guest_stdout_bytes=len(guest_bytes), internal_diagnostics_bytes=len(diagnostic_bytes),
                  passed=guest_bytes == expected_bytes)
    if not result['passed']:
        result['error'] = 'complete guest stdout bytes differ from expected UTF-8'
    return result


def compile_affinity():
    """Use admitted E cores for cold JIT; each execution is pinned separately."""
    membership = Path('/proc/self/cgroup').read_text().strip()[3:]
    text = (Path('/sys/fs/cgroup') / membership.lstrip('/') / 'cpuset.cpus.effective').read_text().strip()
    allowed = set()
    for part in text.split(','):
        bounds = [int(x) for x in part.split('-')]
        allowed.update(range(bounds[0], bounds[-1] + 1))
    cores = allowed & set(range(16, 32))
    assert cores
    return cores


def target_affinity(pid, cpus):
    for task in Path(f'/proc/{pid}/task').iterdir():
        try:
            os.sched_setaffinity(int(task.name), cpus)
        except ProcessLookupError:
            pass


def finish_large_console(c, timeout, progress_log):
    """Check normal CLI retirement after large compiler-state destruction.

    This separate host teardown allowance does not change any VM stop, step or
    managed shutdown deadline. The usual Console.finish remains the default.
    """
    assert 5 < timeout <= 60
    at = len(c.log)
    os.write(c.fd, b'quit\n')
    started = time.monotonic()
    slave_closed = False
    exit_code = None
    while time.monotonic() - started < timeout:
        if slave_closed:
            time.sleep(.02)
        elif select.select([c.fd], [], [], .02)[0]:
            try:
                chunk = os.read(c.fd, 65536)
            except OSError as error:
                if error.errno != errno.EIO:
                    raise
                chunk = b''
            if chunk:
                c.log.extend(chunk)
                assert len(c.log) <= 2 << 20
                progress_log.write_bytes(c.log)
            else:
                slave_closed = True
        if exit_code is None:
            got, status = os.waitpid(c.pid, os.WNOHANG)
            if got:
                c.reaped = True
                exit_code = os.waitstatus_to_exitcode(status)
        if exit_code is not None and slave_closed:
            assert exit_code == 0, 'CLI did not exit normally'
            assert b'managed shutdown complete' not in c.log[at:], 'successful quit is quiet'
            restored, expected = termios.tcgetattr(c.fd), c.baseline.copy()
            if hasattr(termios, 'PENDIN'):
                restored[3] &= ~termios.PENDIN
                expected[3] &= ~termios.PENDIN
            assert restored == expected, 'terminal restored after restarts'
            return {'seconds': time.monotonic() - started, 'limit_seconds': timeout,
                    'exit': 0, 'terminal_restored': True,
                    'scope': 'host CLI compiler-state teardown after normal guest exit'}
    raise TimeoutError('large CLI teardown deadline')


def debug_run(Console, binary, repo, case, folder):
    row = {'passed': False, 'argv': argv_for(binary, repo, case, True), 'actions': []}
    execution_cpus = os.sched_getaffinity(0)
    os.sched_setaffinity(0, compile_affinity())
    try:
        c = Console(row['argv'])
    finally:
        os.sched_setaffinity(0, execution_cpus)
    def action(command):
        timeout = case.get('cold_command_timeout', 600) if command in ['startw', 'run'] else case.get('debug_command_timeout', 240)
        started = time.monotonic()
        reply = send(c, command, timeout, folder / 'debug-console.progress.log')
        row['actions'].append({'command': command, 'seconds': time.monotonic() - started,
                               'process_memory': process_memory(c.pid),
                               'reply': reply.decode(errors='replace')})
        return reply
    try:
        checked_wait(c, lambda b: b'(uwvm-debug) ' in b, 0, case.get('initial_prompt_timeout', 600),
                     folder / 'debug-console.progress.log')
        row['initial_prompt_process_memory'] = process_memory(c.pid)
        assert Path(f'/proc/{c.pid}/cgroup').read_text() == Path('/proc/self/cgroup').read_text()
        entry = action('startw')
        target_affinity(c.pid, execution_cpus)
        assert os.sched_getaffinity(c.pid) == execution_cpus
        match = re.search(rb'thread (\d+) module=0 function=(\d+) byte-offset=(\d+)', entry)
        assert match is not None, entry[-3000:]
        thread, function, offset = map(int, match.groups())
        assert offset == 0, entry[-3000:]
        status = action('status')
        assert b'stopped:' in status and b'stop-id ' in status, status
        old_stop = int(re.search(rb'stop-id (\d+)', status)[1])
        bt = action(f'bt {thread}')
        assert f'function={function} byte-offset=0 '.encode() in bt, bt
        locals_reply = action(f'locals wasm {thread} 0 0 8')
        assert b'error:' not in locals_reply, locals_reply
        action('info modules')
        action('bs 0')
        stepped = action(f'step wasm {thread} into')
        assert b'error:' not in stepped and b'stopped:' in stepped, stepped
        status = action('status')
        new_stop = int(re.search(rb'stop-id (\d+)', status)[1])
        assert new_stop != old_stop
        bp = action(f'bw 0 {function} 0')
        assert re.search(rb'[Bb]reakpoint \d+', bp), bp
        target_affinity(c.pid, compile_affinity())
        restarted = action('run')
        target_affinity(c.pid, execution_cpus)
        assert b'stopped: breakpoint' in restarted, restarted
        assert f'function={function} byte-offset=0 '.encode() in restarted, restarted
        action('disable')
        at = time.monotonic()
        reply = action('continue')
        row['guest_seconds_including_debug_io'] = time.monotonic() - at
        assert b'Program exited normally.' in reply, reply[-5000:]
        strict_output = case.get('strict_debug_stdout', False)
        assert isinstance(strict_output, bool), 'strict_debug_stdout must be boolean'
        if strict_output:
            row['strict_debug_stdout'] = strict_debug_stdout(reply, case['expected_stdout'])
            assert row['strict_debug_stdout']['passed'], row['strict_debug_stdout']
            row['stdout_check_scope'] = 'complete continue payload byte equality'
        else:
            assert output_matches(reply, case['expected_stdout']), reply[-5000:]
            row['stdout_check_scope'] = 'legacy expected-line subsequence'
        if case.get('require_parallel_objects'):
            assert b'object-emit-end ' in c.log and b'objects=8 ' in c.log, 'parallel object generation was not exercised'
            assert b'object-emit-fallback ' not in c.log, 'parallel emission fell back to serial codegen'
        if case.get('debug_quit_timeout', 5) > 5:
            row['cli_retirement'] = finish_large_console(c, case['debug_quit_timeout'],
                                                         folder / 'debug-console.progress.log')
        else:
            c.finish()
        row.update(passed=True, entry_function=function, stop_ids=[old_stop, new_stop],
                   source_language_claim=False, scope='Wasm entry/step/state/breakpoint/restart/output/shutdown')
    except Exception as error:
        row['error'] = repr(error)
    finally:
        (folder / 'debug-console.log').write_bytes(c.log)
        c.close()
    return row


def plain_run(argv, case, log, timeout):
    start = time.monotonic()
    stderr_log = log.with_suffix('.stderr.log')
    with log.open('wb') as f, stderr_log.open('wb') as errors:
        child = subprocess.Popen(argv, stdout=f, stderr=errors, start_new_session=True)
        try:
            code = child.wait(timeout=timeout)
        except BaseException:
            os.killpg(child.pid, signal.SIGKILL)
            child.wait()
            raise
    assert log.stat().st_size <= 2 << 20 and stderr_log.stat().st_size <= 2 << 20
    text = log.read_bytes() + stderr_log.read_bytes()
    expected_exit = case.get('expected_exit', 0)
    exit_ok = code != 0 if expected_exit == 'nonzero' else code == expected_exit
    pattern = case.get('expected_pattern')
    return {'exit': code, 'process_seconds': time.monotonic() - start,
            'passed': exit_ok and log.read_text() == case.get('expected_stdout', '') and
                      ('expected_stderr' not in case or case['expected_stderr'] in stderr_log.read_text()) and
                      (not pattern or re.search(pattern, text.decode(errors='replace'), re.I) is not None),
            'log_sha256': digest(log)}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--arena', type=Path, required=True)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--repository', action='append', choices=['uwvm2', 'uwvm2-stable', 'uwvm2-ros'])
    p.add_argument('--case', action='append')
    p.add_argument('--samples', type=int, default=3)
    p.add_argument('--cpu', type=int, default=21)
    p.add_argument('--skip-debug', action='store_true')
    a = p.parse_args()
    guard = admission()
    assert 1 <= a.samples <= 10
    assert a.cpu in os.sched_getaffinity(0)
    os.sched_setaffinity(0, {a.cpu})
    os.environ.update(TERM='dumb', NO_COLOR='1')
    sys.path.insert(0, str(a.arena / 'uwvm2-ros/test/0017.runtime'))
    from run_debug_run_cli import Console
    cases = json.loads(a.manifest.read_text())
    a.out.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'admission_before': guard, 'manifest_sha256': digest(a.manifest),
              'script_sha256': digest(__file__), 'runs': []}
    for repo in a.repository or ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']:
        binary = a.arena / 'products' / repo / 'uwvm'
        identity = json.loads((binary.parent / 'identity.json').read_text())
        assert identity['binary_sha256'] == digest(binary)
        for case in cases:
            if a.case and case['name'] not in a.case:
                continue
            folder = a.out / repo / case['name']
            folder.mkdir(parents=True)
            wasm_hash = digest(case['wasm'])
            assert 'wasm_sha256' not in case or wasm_hash == case['wasm_sha256'], 'producer artifact changed after manifest preparation'
            row = {'repository': repo, 'case': case['name'], 'wasm_sha256': wasm_hash,
                   'product': identity, 'module': inspect(case['wasm']), 'ordinary': [], 'passed': False}
            report['runs'].append(row)
            print(json.dumps({'repository': repo, 'case': case['name'], 'state': 'started'}), flush=True)
            try:
                argv = argv_for(binary, repo, case)
                row['argv'] = argv
                for i in range(a.samples):
                    result = plain_run(argv, case, folder / f'ordinary-{i}.log', case.get('ordinary_timeout', 300))
                    row['ordinary'].append(result)
                    assert result['passed'], (folder / f'ordinary-{i}.log').read_text(errors='replace')[-5000:]
                if not a.skip_debug and not case.get('skip_debug', False):
                    row['debug'] = debug_run(Console, binary, repo, case, folder)
                    assert row['debug']['passed'], row['debug'].get('error')
                assert digest(binary) == identity['binary_sha256'] and digest(case['wasm']) == wasm_hash
                row['passed'] = True
            except Exception as error:
                row['error'] = repr(error)
            (a.out / 'report.json').write_text(json.dumps(report, indent=2))
            print(json.dumps({'repository': repo, 'case': case['name'], 'passed': row['passed'], 'error': row.get('error')}), flush=True)
    report['admission_after'] = admission()
    events = lambda text: {k: int(v) for k, v in (line.split() for line in text.splitlines())}
    before_events, after_events = events(report['admission_before']['memory_events']), events(report['admission_after']['memory_events'])
    assert all(before_events[key] == after_events[key] for key in ['oom', 'oom_kill', 'oom_group_kill'])
    report['passed'] = bool(report['runs']) and all(row['passed'] for row in report['runs'])
    (a.out / 'report.json').write_text(json.dumps(report, indent=2))
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
