#!/usr/bin/env python3
"""Qualify the optional Java environ producer on three frozen native-budget VMs."""
import argparse
import json
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import time

from build_args_env import ARGUMENTS, VALUES, expected


PINS = {
    'uwvm2': ('sha256:c8f683fcbe9a56ab03c12a96b93e50ddc551fb1ede1a92ccce9e971dfb57fd80',
              '5953cb03d5a15805d26dfcf9edef5d9cb9da87b9f10c78ca828c161d8471474e'),
    'uwvm2-stable': ('sha256:04fbb20f066e9d74b5e945593f2c8d20f17d6b29be07a413dd055c6a44b183f4',
                     'e0c9461ea749788d3d97495dac7c575853ab5e849c3956a12019777570e7fc0b'),
    'uwvm2-ros': ('sha256:e5729fb928ab7356300fd455bb58996877472cb02fb29050b735b3839289ef38',
                  'd0390dba6da583506cd12ea5acf2210a4b02753a76137874710a1bccd0668b86'),
}
NOINHERIT = '--wasip1-global-noinherit-system-environment'
ADD_ENV = '--wasip1-global-add-or-replace-environment'


def exited(fd):
    poller = select.poll()
    poller.register(fd, select.POLLIN)
    return bool(poller.poll(0))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, required=True)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cpu', type=int, default=24)
    args = parser.parse_args()
    assert sys.platform == 'linux'
    helpers = args.arena / 'uwvm2-ros/test/0050.high_level_languages'
    sys.path.insert(0, str(helpers.resolve(strict=True)))
    sys.path.insert(0, str((args.arena / 'uwvm2-ros/test/0017.runtime').resolve(strict=True)))
    from build_products import admission, digest
    from run_matrix import argv_for, debug_run
    from bench_engines import process_identity, check_admission
    from owned_process import remember_process, terminate_group, owned_process_record
    from run_debug_run_cli import Console
    guard = admission()
    assert args.cpu in os.sched_getaffinity(0)
    os.sched_setaffinity(0, {args.cpu})
    os.environ.update(TERM='dumb', NO_COLOR='1', LC_ALL='C.UTF-8', LANG='C.UTF-8')
    args.out.mkdir(parents=True, exist_ok=False)
    reference_path = args.project / 'args-env-build-v3/receipt.json'
    reference = json.loads(reference_path.read_text())
    assert reference['passed'] and reference['argv_provider_source_unchanged']
    artifact = reference['artifacts']['adapted']
    original = reference['artifacts']['original']
    assert artifact['sha256'] == '78b83d7e22d2fad87af236deab9fb82bb5c16c9e1725f0ba15369374fb1b3847'
    assert original['sha256'] == '170a27b2e330643af237315178cbd31dfb728e0bdec438bb5df75190a94f86a7'
    assert all(digest(item['path']) == item['sha256'] for item in [artifact, original])
    helper_files = [Path(__file__), Path(__file__).with_name('build_args_env.py'),
                    helpers / 'run_matrix.py', helpers / 'bench_engines.py', helpers / 'owned_process.py',
                    helpers / 'build_products.py', args.arena / 'uwvm2-ros/test/0017.runtime/run_debug_run_cli.py']
    report = {'passed': False, 'scope': 'ordinary and Wasm DBG functionality; not source-level debugging or performance',
              'cpu': args.cpu, 'reference_receipt': str(reference_path), 'reference_sha256': digest(reference_path),
              'admission_before': guard, 'helper_sha256': {str(path): digest(path) for path in helper_files},
              'products': {}, 'runs': [], 'owned_retirements': []}
    cgroup = Path('/sys/fs/cgroup') / guard['membership'][3:].lstrip('/')

    def save():
        (args.out / 'receipt.json').write_text(json.dumps(report, indent=2) + '\n')

    def case_for(row, negative=False):
        environment = VALUES if negative else row['guest_environment']
        load_args = [NOINHERIT]
        for key, value in environment.items():
            load_args.extend([ADD_ENV, key, value])
        return {'name': 'original-stub-negative' if negative else row['name'],
                'wasm': original['path'] if negative else artifact['path'],
                'args': ARGUMENTS[1] if negative else row['args'], 'load_args': load_args,
                'expected_stdout': expected(ARGUMENTS[1], {}, False) if negative else row['strict_stdout'],
                'initial_prompt_timeout': 1200, 'cold_command_timeout': 1200,
                'debug_command_timeout': 240, 'debug_compile_threads': 7}

    def ordinary(binary, repo, case, folder):
        folder.mkdir(parents=True)
        command = argv_for(binary, repo, case, False)
        row = {'passed': False, 'argv': command, 'kind': 'ordinary', 'case': case['name'],
               'repository': repo, 'expected_stdout': case['expected_stdout'], 'observations': []}
        child = None
        started = time.monotonic()
        try:
            with (folder / 'stdout.log').open('wb') as output, (folder / 'stderr.log').open('wb') as error:
                child = subprocess.Popen(command, stdout=output, stderr=error, start_new_session=True)
                remember_process(child, force=True)
                row['pid'] = child.pid
                last = float('-inf')
                while True:
                    remember_process(child)
                    now = time.monotonic()
                    assert now - started < 1200, 'ordinary cold/guest timeout'
                    if now - last >= .5:
                        try:
                            snapshot = process_identity(child.pid)
                        except (FileNotFoundError, ProcessLookupError):
                            code = child.poll()
                            if code is not None:
                                break
                            raise
                        rss = re.search(r'^VmRSS:\s+(\d+)\s+kB$', snapshot['status'], re.M)
                        if rss is None:
                            code = child.poll()
                            if code is not None:
                                break
                        anon = int(dict(line.split() for line in (cgroup / 'memory.stat').read_text().splitlines())['anon'])
                        assert rss and int(rss[1]) * 1024 < 56 << 30 and anon < 60 << 30
                        assert snapshot['cgroup'] == guard['membership']
                        assert snapshot['tasks'] and all(item['affinity'] == [args.cpu]
                                                         and item['cgroup'] == guard['membership']
                                                         for item in snapshot['tasks'].values())
                        assert (cgroup / 'memory.events').read_text() == guard['memory_events']
                        row['observations'].append({'seconds': now - started, 'rss_bytes': int(rss[1]) * 1024,
                                                    'cgroup_anon_bytes': anon, 'tasks': snapshot['tasks']})
                        last = now
                    code = child.poll()
                    if code is not None:
                        break
                    time.sleep(.05)
            row['exit'] = code
            row['stdout'] = (folder / 'stdout.log').read_text()
            row['stderr'] = (folder / 'stderr.log').read_text()
            assert code == 0 and row['stdout'] == case['expected_stdout'], (code, row['stdout'], row['stderr'][-4000:])
            prefixes = ('[llvm-jit-full] ', '[llvm-jit-cache] ', '[gc-managed] ')
            unknown = [line for line in row['stderr'].splitlines() if not line.startswith(prefixes)]
            row['accepted_engine_diagnostic_prefixes'] = list(prefixes)
            assert not unknown, unknown
            row['passed'] = True
        except Exception as error:
            row['error'] = repr(error)
        finally:
            if child is not None:
                try:
                    terminate_group(child)
                    row['ownership'] = owned_process_record(child)
                    row['retired'] = not Path(f'/proc/{child.pid}').exists()
                    assert row['retired']
                except Exception as error:
                    row.update(passed=False, cleanup_error=repr(error))
            row['seconds_functional_only'] = time.monotonic() - started
        return row

    class TracedConsole(Console):
        def __init__(self, command):
            super().__init__(command)
            self.witness = os.dup(self.owner)
            self.trace = {'pid': self.pid, 'initial_cgroup': Path(f'/proc/{self.pid}/cgroup').read_text().strip(),
                          'initial_affinity': sorted(os.sched_getaffinity(self.pid))}
            report['owned_retirements'].append(self.trace)

        def close(self):
            try:
                super().close()
            finally:
                self.trace.update(pidfd_retired=exited(self.witness), proc_gone=not Path(f'/proc/{self.pid}').exists(),
                                  reaped_by_normal_finish=self.reaped)
                os.close(self.witness)
                assert self.trace['pidfd_retired'] and self.trace['proc_gone'], self.trace

    try:
        for repo, (source_id, binary_sha) in PINS.items():
            binary = args.arena / 'products' / repo / 'uwvm'
            identity = json.loads((binary.parent / 'identity.json').read_text())
            assert identity['source_id'] == source_id and identity['binary_sha256'] == binary_sha
            assert digest(binary) == binary_sha, 'not the current frozen native-budget product'
            report['products'][repo] = identity
            for source in reference['reference_runs']:
                case = case_for(source)
                print(json.dumps({'repository': repo, 'case': case['name'], 'kind': 'ordinary', 'state': 'start'}), flush=True)
                row = ordinary(binary, repo, case, args.out / repo / case['name'])
                report['runs'].append(row)
                save()
                print(json.dumps({key: row.get(key) for key in ['repository', 'case', 'passed', 'error']}), flush=True)
            case = case_for(None, True)
            row = ordinary(binary, repo, case, args.out / repo / case['name'])
            row['expected_producer_gap'] = 'unpatched TeaVM getenv(String) returns NULL for present variables'
            report['runs'].append(row)
            save()
            case = case_for(reference['reference_runs'][-1])
            folder = args.out / repo / 'wasm-debug-unicode-environment'
            folder.mkdir(parents=True)
            print(json.dumps({'repository': repo, 'kind': 'wasm-debug', 'state': 'start'}), flush=True)
            row = debug_run(TracedConsole, binary, repo, case, folder)
            row.update(repository=repo, case='wasm-debug-unicode-environment', kind='wasm-debug')
            if row['passed']:
                continuation = next(action['reply'] for action in row['actions'] if action['command'] == 'continue')
                lines = continuation.replace('\r', '').splitlines()
                guest = [line for line in lines if line.startswith(('java args/env:', 'arg[', 'env[', 'java env map:'))]
                row['semantic_stdout'] = '\n'.join(guest) + '\n'
                row['strict_semantic_stdout'] = row['semantic_stdout'] == case['expected_stdout']
                row['passed'] = row['passed'] and row['strict_semantic_stdout']
            report['runs'].append(row)
            save()
            print(json.dumps({'repository': repo, 'kind': 'wasm-debug', 'passed': row['passed'], 'error': row.get('error')}), flush=True)
            assert digest(binary) == binary_sha
        assert all(digest(item['path']) == item['sha256'] for item in [artifact, original])
        assert all(digest(path) == value for path, value in report['helper_sha256'].items())
    except Exception as error:
        report['error'] = repr(error)
    finally:
        report['admission_after'] = admission()
        check_admission(guard, report['admission_after'])
        report['passed'] = ('error' not in report and len(report['runs']) == 18
                            and all(row['passed'] for row in report['runs'])
                            and len(report['owned_retirements']) == 3
                            and all(row['pidfd_retired'] and row['proc_gone'] for row in report['owned_retirements']))
        save()
    print(json.dumps({'passed': report['passed'], 'runs': len(report['runs']),
                      'receipt': str(args.out / 'receipt.json'), 'error': report.get('error')}), flush=True)
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
