#!/usr/bin/env python3
"""Serial strict Java source checks; run only after the exclusive CPU21 release.

The source runner, fixtures, scalar assertions and VM's built-in two-second
pause/step deadline are unchanged. This driver adds frozen-product admission
and retained pidfd retirement evidence around the existing Console protocol.
"""
import argparse
import json
import os
from pathlib import Path
import select
import sys
import time

from run_args_env import PINS


FIXTURES = [
    {
        'name': 'semantics',
        'wasm': 'target/wasi-debug-dwarf-name-typed-fixed/java-semantics.wasm',
        'wasm_sha256': '7ea197652bc9c95c4fa52aab85000141ced275c9e5c56f668b245b10277841aa',
        'source': 'src/main/java/uwvm/hll/Semantics.java',
        'source_sha256': '0d3942b90cd6ad77ce21862f05e075c0c4be671a522e7cd9f5e20b1da27ce471',
        'expected_stdout': 'expected.stdout',
        'expected_stdout_sha256': 'ff839ff63c66f6b4ad2778693646b25d963d379d115acce90bd617ae48956ef8',
        'expected_source': 'uwvm/hll/Semantics.java',
        'expected_name': 'meth_uh_Semantics_main',
        'line': 105, 'after_line': 106, 'expression': 'checksum',
        'before': '-2080261270', 'after': '-2080261270', 'args': [],
    },
    {
        'name': 'typed-update',
        'wasm': 'target/typed-debug-fixed-loop-v2/java-typed-debug.wasm',
        'wasm_sha256': '3d98851a9d3ed0b52a3849847370104d4ab86e7c4f09934119ce5a26a0e7d23f',
        'source': 'src/main/java/uwvm/hll/TypedDebug.java',
        'source_sha256': 'de66a3cd64e8641dbb7602c1bf3a9a4b357343c4a748f71ad49dac435dc80c05',
        'expected_stdout': 'target/typed-debug-fixed-loop-v2/expected.stdout',
        'expected_stdout_sha256': '9c8717f9bbb851bef4c460954ddb42d459415103a1dc08ca57cf90fa7e00a120',
        'expected_source': 'uwvm/hll/TypedDebug.java',
        'expected_name': 'meth_uh_TypedDebug_main',
        'line': 10, 'after_line': 11, 'expression': 'counter',
        'before': '17', 'after': '24', 'args': ['17'],
    },
]
STRICT_CHECKS = [
    'passed', 'source_breakpoint_pass', 'source_step_pass', 'function_name_display_pass',
    'source_variable_value_verified', 'source_variable_after_step_verified',
    'locals_value_verified', 'locals_after_step_value_verified',
    'expected_guest_output_verified', 'normal_quit_and_terminal_restore',
]
SOURCE_RUNNER_SHA256 = '38e472320e42eaffe08aed5fc319a0ad0983c4c72a25635aef756092f6af3bdd'


def is_retired(fd):
    poller = select.poll()
    poller.register(fd, select.POLLIN)
    return bool(poller.poll(0))


def command_for(project, helpers, binary, repository, fixture, folder):
    command = [str(project / 'run_source_debug.py'), '--binary', str(binary),
               '--wasm', str(project / fixture['wasm']), '--source', str(project / fixture['source']),
               '--out', str(folder), '--helpers', str(helpers), '--cpu', '21', '--timeout', '240',
               '--line', str(fixture['line']), '--expected-source', fixture['expected_source'],
               '--expected-name', fixture['expected_name'], '--expression', fixture['expression'],
               '--expected-value', fixture['before'], '--expected-after-step-value', fixture['after'],
               '--expected-after-step-line', str(fixture['after_line']), '--require-values',
               '--expected-stdout', str(project / fixture['expected_stdout'])]
    if repository != 'uwvm2':
        command.append('--ros')
    for argument in fixture['args']:
        command.extend(['--arg', argument])
    return command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, required=True)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--exclusive-cpu21-released', action='store_true', required=True,
                        help='Pass only after root explicitly releases the exclusive CPU21 window')
    args = parser.parse_args()
    assert sys.platform == 'linux', 'all execution belongs in the authorized SSH Linux cgroup'
    project = args.project.resolve(strict=True)
    helpers = (args.arena / 'uwvm2-ros/test/0050.high_level_languages').resolve(strict=True)
    sys.path[:0] = [str(helpers), str(helpers.parent / '0017.runtime'), str(project)]
    from build_products import admission, digest
    from bench_engines import check_admission
    import run_debug_run_cli
    import run_source_debug

    before = admission()
    assert 21 in os.sched_getaffinity(0)
    os.sched_setaffinity(0, {21})
    os.environ.update(TERM='dumb', NO_COLOR='1', LC_ALL='C.UTF-8', LANG='C.UTF-8')
    assert digest(project / 'run_source_debug.py') == SOURCE_RUNNER_SHA256, 'strict source runner changed'
    inputs = {str(project / 'run_source_debug.py'): SOURCE_RUNNER_SHA256}
    identities = {}
    for repository, (source_id, binary_sha256) in PINS.items():
        binary = (args.arena / 'products' / repository / 'uwvm').resolve(strict=True)
        identity = json.loads((binary.parent / 'identity.json').read_text())
        assert identity['source_id'] == source_id and identity['binary_sha256'] == binary_sha256
        assert digest(binary) == binary_sha256, 'requires current native-budget product'
        identities[repository] = identity
        inputs[str(binary)] = binary_sha256
    for fixture in FIXTURES:
        for key in ['wasm', 'source', 'expected_stdout']:
            path = (project / fixture[key]).resolve(strict=True)
            assert digest(path) == fixture[key + '_sha256'], ('historical fixture/oracle changed', path)
            inputs[str(path)] = fixture[key + '_sha256']
    for path in [Path(__file__).resolve(), project / 'run_args_env.py',
                 project / 'build_args_env.py', helpers / 'run_matrix.py',
                 helpers / 'bench_engines.py', helpers / 'build_products.py',
                 helpers / 'inspect_wasm.py', helpers.parent / '0017.runtime/run_debug_run_cli.py']:
        inputs[str(path)] = digest(path)
    args.out.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'execution_cpus': [21], 'serial': True,
              'exclusive_release_acknowledged': True, 'products': identities,
              'scope': 'unchanged finite Java source/scalar fixtures against native-budget products',
              'source_pause_step_deadline_seconds': 2,
              'deadline_policy': 'unchanged built-in controller execute/launch defaults; no CLI override',
              'cli_response_timeout_seconds': 240, 'cold_prompt_startw_timeout_seconds': 600,
              'typed_object_or_array_claim': False, 'performance_claim': False,
              'admission_before': before, 'inputs_before': inputs,
              'runs': [], 'owned_retirements': []}

    def save():
        (args.out / 'receipt.json').write_text(json.dumps(report, indent=2) + '\n')

    original_console = run_debug_run_cli.Console
    active = []

    class TracedConsole(original_console):
        def __init__(self, argv):
            super().__init__(argv)
            self.witness = os.dup(self.owner)
            self.closed = False
            self.trace = {'repository': current_repository, 'fixture': current_fixture,
                          'pid': self.pid, 'cgroup': Path(f'/proc/{self.pid}/cgroup').read_text().strip(),
                          'initial_affinity': sorted(os.sched_getaffinity(self.pid))}
            report['owned_retirements'].append(self.trace)
            active.append(self)

        def close(self):
            if self.closed:
                return
            try:
                super().close()
            finally:
                self.trace.update(pidfd_retired=is_retired(self.witness),
                                  proc_gone=not Path(f'/proc/{self.pid}').exists(),
                                  reaped_by_normal_finish=self.reaped)
                os.close(self.witness)
                self.closed = True
                save()
                assert self.trace['pidfd_retired'] and self.trace['proc_gone'], self.trace

    save()
    run_debug_run_cli.Console = TracedConsole
    previous_argv = sys.argv
    try:
        for current_repository in PINS:
            binary = (args.arena / 'products' / current_repository / 'uwvm').resolve(strict=True)
            for fixture in FIXTURES:
                current_fixture = fixture['name']
                folder = args.out / (current_repository + '-' + current_fixture)
                command = command_for(project, helpers, binary, current_repository, fixture, folder)
                row = {'repository': current_repository, 'fixture': current_fixture, 'passed': False,
                       'command': [sys.executable, *command], 'strict_checks': STRICT_CHECKS,
                       'report': str(folder / 'report.json')}
                report['runs'].append(row)
                save()
                print(json.dumps({'repository': current_repository, 'fixture': current_fixture, 'state': 'start'}), flush=True)
                started = time.monotonic()
                try:
                    sys.argv = command
                    run_source_debug.main()
                except SystemExit as error:
                    row['exit'] = error.code
                except Exception as error:
                    row['driver_error'] = repr(error)
                finally:
                    sys.argv = previous_argv
                    for console in active:
                        if not console.closed:
                            console.close()
                    row['seconds_functional_only'] = time.monotonic() - started
                if (folder / 'report.json').exists():
                    result = json.loads((folder / 'report.json').read_text())
                    row.update(checks={key: result.get(key, False) for key in STRICT_CHECKS},
                               positions=result.get('positions', []), error=result.get('error'))
                    row['passed'] = row.get('exit') == 0 and 'driver_error' not in row and all(row['checks'].values())
                retirement = [item for item in report['owned_retirements']
                              if item['repository'] == current_repository and item['fixture'] == current_fixture]
                row['passed'] &= (len(retirement) == 1 and retirement[0].get('pidfd_retired', False)
                                  and retirement[0].get('proc_gone', False)
                                  and retirement[0].get('reaped_by_normal_finish', False))
                save()
                print(json.dumps({key: row.get(key) for key in ['repository', 'fixture', 'passed', 'error', 'driver_error']}), flush=True)
                assert all(item.get('pidfd_retired') and item.get('proc_gone')
                           for item in report['owned_retirements']), 'do not start another VM with an owned survivor'
                check_admission(before, admission())
    except Exception as error:
        report['error'] = repr(error)
    finally:
        run_debug_run_cli.Console = original_console
        sys.argv = previous_argv
        for console in active:
            if not console.closed:
                console.close()
        report['inputs_after'] = {path: digest(path) for path in inputs}
        report['admission_after'] = admission()
        try:
            check_admission(before, report['admission_after'])
            assert report['inputs_after'] == inputs, 'frozen inputs changed during execution'
        except Exception as error:
            report['error'] = repr(error)
        report['passed'] = ('error' not in report and len(report['runs']) == 6
                            and all(row['passed'] for row in report['runs'])
                            and len(report['owned_retirements']) == 6
                            and all(item.get('pidfd_retired') and item.get('proc_gone')
                                    and item.get('reaped_by_normal_finish')
                                    for item in report['owned_retirements']))
        save()
    print(json.dumps({'passed': report['passed'], 'runs': len(report['runs']),
                      'receipt': str(args.out / 'receipt.json'), 'error': report.get('error')}), flush=True)
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
