#!/usr/bin/env python3
"""Summarize completed ordinary profiles and join each usable same-run snapshot.

Run after all collectors have retired, inside the original admitted SSH Linux
container. This starts no VM, collector or VTune report. Attribution uses the
specified immutable harness's join implementation, not this source directory.
"""
import argparse
import csv
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import sys
import time

sys.dont_write_bytecode = True
REPOSITORIES = ('uwvm2', 'uwvm2-stable', 'uwvm2-ros')
KINDS = ('hotspots', 'uarch-detailed')


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--harness', required=True, type=Path)
    parser.add_argument('--profiles', required=True, type=Path)
    parser.add_argument('--manifest', required=True, type=Path,
                        help='the exact frozen hardware-gated cohort manifest')
    parser.add_argument('--expected-boot', help='cohort boot; defaults to current boot, never silently mix old runs')
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.harness))
    from build_products import admission
    from bench_engines import select_cases
    from vtune_reports import durable_json
    freeze_path = args.harness / 'freeze.json'
    freeze = json.loads(freeze_path.read_text())
    require(freeze['passed'], 'frozen harness is not passed')
    require(all(sha(args.harness / name) == value for name, value in freeze['files'].items()),
            'frozen helper changed')
    spec = importlib.util.spec_from_file_location('frozen_join_jit_samples', args.harness / 'engines/join_jit_samples.py')
    join_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(join_module)
    args.out.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'purpose': 'final-hardware-qualification-and-offline-attribution',
              'started_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
              'admission_before': admission(), 'helper_sha256': sha(__file__),
              'freeze_sha256': sha(freeze_path), 'harness': str(args.harness),
              'profiles': str(args.profiles), 'rows': [], 'starts_vm_or_collection_or_report': False}
    original_path = args.profiles / 'report.json'
    try:
        original_sha = sha(original_path)
        matrix = json.loads(original_path.read_text())
        manifest_sha = sha(args.manifest)
        require(freeze['files'].get('manifest-gated.json') == manifest_sha,
                'manifest is not this frozen harness hardware cohort')
        require(matrix['manifest_sha256'] == manifest_sha and matrix['ordinary_gated'],
                'matrix belongs to another manifest or execution mode')
        cases = select_cases(json.loads(args.manifest.read_text()), purpose='hardware-gated')
        case_by_name = {case['name']: case for case in cases}
        expected = {(repository, name, kind) for repository in REPOSITORIES
                    for name in case_by_name for kind in KINDS}
        actual = [(item['repository'], item['case'], item['kind']) for item in matrix['runs']]
        expected_boot = args.expected_boot or Path('/proc/sys/kernel/random/boot_id').read_text().strip()
        report.update(manifest=str(args.manifest), manifest_sha256=manifest_sha, expected_boot=expected_boot,
                      route_count=len(cases), repositories=list(REPOSITORIES), kinds=list(KINDS),
                      expected_profiles=len(expected), observed_profiles=len(actual))
        require(len(set(actual)) == len(actual), 'duplicate final profile identities')
        require(set(actual) == expected and len(actual) == len(expected),
                f'incomplete/foreign cohort: missing={sorted(expected - set(actual))}, extra={sorted(set(actual) - expected)}')
        require(matrix['passed'], 'driver did not qualify the complete requested matrix')
        require(matrix['admission_before']['membership'] == matrix['admission_after']['membership']
                == report['admission_before']['membership'], 'matrix and postprocess cgroup differ')
        for item in matrix['runs']:
            repo, case, kind = [item[key] for key in ('repository', 'case', 'kind')]
            folder = args.profiles / repo / case / kind
            receipt = folder / 'receipt.json'
            row = {'repository': repo, 'case': case, 'kind': kind, 'hardware_passed': False,
                   'receipt': str(receipt), 'attribution_status': 'not attempted'}
            report['rows'].append(row)
            try:
                d = json.loads(receipt.read_text())
                row['receipt_sha256'] = sha(receipt)
                require(item['exit'] == 0 and item['passed'] and d['passed'], 'final profile failed')
                require(item['receipt_sha256'] == sha(receipt) and item['receipt'] == d, 'matrix receipt differs')
                require((d['repository'], d['case'], d['kind']) == (repo, case, kind),
                        'profile route/repository/analysis identity differs')
                require(d['boot'] == expected_boot, 'profile belongs to an old/different boot cohort')
                require(d['manifest_sha256'] == manifest_sha
                        and d['wasm_sha256'] == case_by_name[case]['wasm_sha256'],
                        'profile belongs to another manifest/module cohort')
                require(d['admission_before']['membership'] == d['admission_after']['membership']
                        == report['admission_before']['membership'], 'profile cgroup differs from cohort')
                for name in ('run_vtune_ordinary.py', 'bench_engines.py', 'run_matrix.py', 'run_vtune.py',
                             'vtune_reports.py', 'owned_process.py', 'prepare_profile_libraries.py', 'capture_jit_symbols.py'):
                    recorded = [value for path, value in d['helper_sha256'].items() if Path(path).name == name]
                    require(recorded == [freeze['files'][name]], 'measurement helper belongs to another freeze: ' + name)
                require(d['performance_eligible'] and not d['calibration_only'] and d['mode'] == 'ordinary-gated',
                        'profile is excluded from final performance')
                require(kind in ('hotspots', 'uarch-detailed'), 'summary counting is not a final profile')
                require(not d['kernel_alerts'] and not d['cleanup_errors'], 'kernel alert or cleanup failure')
                require(d['target_exit'] == d['collector_exit'] == 0, 'target/collector did not exit normally')
                require(d['binary_sha256'] == freeze['products'][repo]['binary_sha256'], 'wrong final product')
                require(math.isfinite(d['kernel_seconds']) and 1 <= d['kernel_seconds'] <= 20
                        and 1 <= d['guest_window_seconds'] < 28, 'final hardware duration outside bounds')
                for stage in ('admission_before', 'admission_after'):
                    require(all(int(line.split()[1]) == 0 for line in d[stage]['memory_events'].splitlines()),
                            'nonzero original-cgroup memory event')
                cpu = d['hybrid_pmu']['target']
                require(cpu == 21 and d['hybrid_pmu']['companion'] == 9, 'wrong target/companion CPUs')
                for stage in ('target_at_ready', 'target_at_done'):
                    target = d[stage]
                    require(target['pid'] == d['target_pid'] and target['cgroup'] == d['admission_before']['membership'],
                            'target identity/cgroup mismatch')
                    status = dict(line.split(':', 1) for line in target['status'].splitlines() if ':' in line)
                    require(int(status['TracerPid']) == 0 and int(status['NoNewPrivs']) == 1, 'traced/privileged target')
                    for task in target['tasks'].values():
                        require(task['affinity'] == [21] and task['cgroup'] == target['cgroup']
                                and len(task['capabilities']) == 5 and all(value == 0 for value in task['capabilities'].values()),
                                'target task affinity/cgroup/capability failure')
                owners = d['process_ownership']
                require(all(owner['closed'] for owner in [owners['target'], owners['collector'], *owners['commands']]),
                        'profile retained an unretired owned process')
                qualification = folder / 'hardware-qualification.json'
                q = json.loads(qualification.read_text())
                require(q == d['hardware_qualification'] and q['passed'] and d['hardware_qualification_passed'],
                        'hardware qualification receipt mismatch')
                require(q['kind'] == kind and q['helper_sha256'] == freeze['files']['vtune_reports.py'],
                        'qualification kind/helper differs from frozen cohort')
                require(Path(q['result']).resolve() == (folder / 'result').resolve()
                        and sha(folder / 'result/config/runsa.options') == q['runsa_options_sha256'],
                        'qualification result/collector configuration changed')
                for command in q['commands']:
                    require(command['exit'] == 0
                            and sha(command['stdout']) == command['stdout_sha256']
                            and sha(command['stderr']) == command['stderr_sha256'], 'raw hardware report changed')
                if kind == 'uarch-detailed':
                    require(q.get('target_microarchitecture'), 'target VTune formula metrics missing')
                require(q['target_pid'] == d['target_pid'] and q['target_cpu'] == 21 and q['target_pmu'] == 'cpu_atom',
                        'hardware sample dimensions mismatch')
                require(all(q['target_samples'][event] >= 10 for event in ('INST_RETIRED.ANY', 'CPU_CLK_UNHALTED.REF_TSC')),
                        'actual retired/clock samples insufficient')
                row.update(hardware_passed=True, kernel_seconds=d['kernel_seconds'],
                           host_window_seconds=d['guest_window_seconds'], cold_ready_seconds=d['cold_ready_seconds'],
                           target_pid=d['target_pid'], target_cpu=21, binary_sha256=d['binary_sha256'],
                           wasm_sha256=d['wasm_sha256'], actual_retired_samples=q['target_samples']['INST_RETIRED.ANY'],
                           actual_clock_samples=q['target_samples']['CPU_CLK_UNHALTED.REF_TSC'],
                           target_microarchitecture=q.get('target_microarchitecture', []),
                           qualification_sha256=sha(qualification))
                metadata = d.get('jit_metadata', {})
                row['metadata_passed'] = bool(metadata.get('passed'))
                if metadata.get('passed'):
                    out = args.out / 'joins' / repo / case / kind
                    try:
                        j = join_module.join(receipt, folder / 'jit-metadata/receipt.json', qualification, out)
                        row.update(attribution_status='passed', join_receipt=str(out / 'receipt.json'),
                                   join_receipt_sha256=sha(out / 'receipt.json'),
                                   wasm_sample_coverage=j['wasm_sample_coverage'],
                                   sample_classification=j['sample_classification'],
                                   top_wasm_functions=[{key: value for key, value in item.items() if key != 'sample_addresses'}
                                                       for item in j['top_wasm_functions'][:5]])
                    except Exception as error:
                        row.update(attribution_status='failed', attribution_error=f'{type(error).__name__}: {error}',
                                   join_receipt=str(out / 'receipt.json'))
                else:
                    row.update(attribution_status='unavailable', attribution_error=metadata.get('error', 'no usable snapshot'))
                require(sha(receipt) == row['receipt_sha256'], 'profile changed during offline summary')
            except Exception as error:
                row['hardware_passed'] = False
                row['error'] = f'{type(error).__name__}: {error}'
            print(json.dumps({key: row.get(key) for key in ('repository', 'case', 'kind', 'hardware_passed', 'attribution_status', 'error')}, allow_nan=False), flush=True)
            durable_json(args.out / 'receipt.json', report)
        require(sha(original_path) == original_sha and sha(freeze_path) == report['freeze_sha256'], 'matrix/freeze changed')
        require(all(sha(args.harness / name) == value for name, value in freeze['files'].items()), 'frozen helper changed')
        report.update(hardware_passed=sum(row['hardware_passed'] for row in report['rows']),
                      usable_snapshots=sum(row.get('metadata_passed', False) for row in report['rows']),
                      joins_passed=sum(row['attribution_status'] == 'passed' for row in report['rows']),
                      attribution_unavailable=sum(row['attribution_status'] == 'unavailable' for row in report['rows']),
                      admission_after=admission(), original_matrix_sha256=original_sha)
        require(sha(args.manifest) == manifest_sha, 'cohort manifest changed during summary')
        report['passed'] = report['hardware_passed'] == len(expected) and report['joins_passed'] == report['usable_snapshots']
        fields = ['repository', 'case', 'kind', 'hardware_passed', 'kernel_seconds', 'host_window_seconds',
                  'cold_ready_seconds', 'actual_retired_samples', 'actual_clock_samples', 'metadata_passed',
                  'attribution_status', 'retired_wasm_coverage', 'clock_wasm_coverage', 'error']
        with (args.out / 'summary.csv').open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=fields)
            writer.writeheader()
            for row in report['rows']:
                values = {key: row.get(key, '') for key in fields}
                values['retired_wasm_coverage'] = row.get('wasm_sample_coverage', {}).get('INST_RETIRED.ANY', '')
                values['clock_wasm_coverage'] = row.get('wasm_sample_coverage', {}).get('CPU_CLK_UNHALTED.REF_TSC', '')
                writer.writerow(values)
    except BaseException as error:
        report['passed'] = False
        report['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        report['ended_utc'] = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())
        durable_json(args.out / 'receipt.json', report)
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
