#!/usr/bin/env python3
"""Freeze helpers and checked producer inputs without executing any guest/tool.

Run inside the original admitted SSH Linux container. Only files and proc/sysfs
metadata are read; this script never launches a subprocess or collects samples.
"""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import time

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from build_products import admission, digest
from bench_engines import select_cases
from vtune_reports import durable_json, require


ROOT_HELPERS = (
    'archive_evidence.py', 'bench_engines.py', 'build_products.py',
    'capture_jit_symbols.py', 'inspect_wasm.py', 'merge_timed_cases.py',
    'owned_process.py', 'prepare_cases.py', 'prepare_profile_libraries.py',
    'record_environment.py', 'run_matrix.py', 'run_vtune.py',
    'run_vtune_matrix.py', 'run_vtune_ordinary.py', 'summarize_benchmark.py',
    'vtune_reports.py',
)
ENGINE_HELPERS = ('join_jit_samples.py', 'summarize_hardware.py', 'freeze_performance_harness.py',
                  'analyze_wat_controls.py', 'check_initial_zombie.py', 'check_unreaped_leader.py')
RUNTIME_HELPERS = ('debug_sampling_safety.py', 'run_debug_run_cli.py')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('arena', 'source', 'base-harness', 'python-timed', 'python-gated',
                 'profile-libraries', 'environment', 'out'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--ownership-proof', type=Path,
                        help='passed real native probe bound to the current owned_process.py')
    parser.add_argument('--evidence', type=Path, action='append', default=[],
                        help='additional immutable small receipts to snapshot; never executed')
    args = parser.parse_args()
    before = admission()
    boot = Path('/proc/sys/kernel/random/boot_id').read_text().strip()
    args.out.mkdir(parents=True, exist_ok=False)
    record = {'passed': False, 'purpose': 'immutable seven-route ordinary benchmark and hardware helpers',
              'qualification_scope': 'file/schema/identity preparation only; no new calibration or hardware run',
              'starts_subprocess_or_guest_or_collection': False, 'boot': boot,
              'admission_before': before, 'started_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
              'source': str(args.source), 'base_harness': str(args.base_harness),
              'files': {}, 'products': {}, 'inputs': {}, 'external_runtime_files': {}}
    input_hashes = {}

    def checked(path, expected=None):
        path = Path(path)
        value = digest(path)
        require(expected is None or value == expected, 'input SHA mismatch: ' + str(path))
        previous = input_hashes.setdefault(str(path), value)
        require(previous == value, 'input changed during preparation: ' + str(path))
        return value

    def snapshot(path, relative=None, expected=None):
        path = Path(path)
        value = checked(path, expected)
        if relative is None:
            relative = 'inputs/' + value + '-' + path.name
        destination = args.out / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists():
            require(digest(destination) == value, 'snapshot name collision: ' + relative)
        else:
            shutil.copyfile(path, destination)
        require(digest(destination) == value, 'snapshot copy differs: ' + relative)
        record['inputs'][str(path)] = {'sha256': value, 'snapshot': relative}
        return json.loads(path.read_text()) if path.suffix == '.json' else None

    def validate_case(case):
        checked(case['wasm'], case['wasm_sha256'])
        snapshot(case['generated_source'], expected=case.get('generated_source_sha256'))
        oracle = snapshot(case['oracle'], expected=case.get('oracle_sha256'))
        require(oracle.get('passed') is True or oracle.get('status') == 'passed'
                or (oracle.get('exit') == 0 and oracle.get('module_sha256') == case['wasm_sha256']
                    and oracle.get('rounds') == case['rounds']
                    and oracle.get('checksum') == case['expected_checksum']),
                'producer oracle is not passed: ' + case['name'])
        if 'producer_receipt' in case:
            producer = snapshot(case['producer_receipt'], expected=case['producer_receipt_sha256'])
            require(producer['passed'], 'producer build did not pass')
        if 'module_inspection' in case:
            snapshot(case['module_inspection'], expected=case['module_inspection_sha256'])

    try:
        old = snapshot(args.base_harness / 'freeze.json', 'inputs/base-freeze.json')
        require(old['passed'], 'base harness is not passed')
        for name, value in old['files'].items():
            checked(args.base_harness / name, value)
        old_timed = snapshot(args.base_harness / 'manifest-timed.json', 'inputs/base-manifest-timed.json')
        old_gated = snapshot(args.base_harness / 'manifest-gated.json', 'inputs/base-manifest-gated.json')
        require(len(old_timed) == 13 and len(old_gated) == 6, 'expected original 13 timed / 6 gated routes')
        python_timed = snapshot(args.python_timed, 'inputs/python-manifest-timed.json')
        python_gated = snapshot(args.python_gated, 'inputs/python-manifest-gated.json')
        require(len(python_timed) == len(python_gated) == 1, 'one Python route of each purpose required')
        for row, purpose in ((python_timed[0], 'ordinary-long'), (python_gated[0], 'hardware-gated')):
            require(row['language'] == 'python' and row['purpose'] == purpose
                    and row['rounds'] == row['iterations'] == 200000 and row['args'] == ['200000']
                    and row['expected_checksum'] == 673761
                    and row['load_args'] == ['--wasip1-noinherit-system-environment'],
                    'Python 200k/noinherit recipe differs')
            require(row['preopens'] == row['external_runtime_assets'] == [], 'Python external assets/preopens unexpected')
        timed, gated = old_timed + python_timed, old_gated + python_gated
        select_cases(timed)
        select_cases(gated)
        require(len(select_cases(timed, purpose='ordinary-long')) == len(gated) == 7, 'expected seven performance routes')
        require(timed[:13] == old_timed and gated[:6] == old_gated, 'original routes changed')
        for row in timed + gated:
            validate_case(row)
        for row in (python_timed[0], python_gated[0]):
            oracle = json.loads(Path(row['oracle']).read_text())
            entry = 'timed' if row['purpose'] == 'ordinary-long' else 'profile'
            matches = [item for item in oracle['cases'] if item['entry'] == entry]
            require(oracle['passed'] and oracle['rounds'] == row['rounds']
                    and oracle['independent_checksum'] == row['expected_checksum']
                    and {item['engine'] for item in matches} == {'native', 'wasmtime'}
                    and len(matches) == 2 and all(item['passed'] for item in matches)
                    and all(item['stable_stdout'].splitlines() == row['checks'] for item in matches)
                    and any(item['artifact_sha256'] == row['wasm_sha256'] for item in matches),
                    'Python same-module native/Wasmtime oracle binding failed')
            for item in matches:
                lifecycle = snapshot(item['lifecycle'])
                require(lifecycle['passed'], 'Python oracle lifecycle failed')
        durable_json(args.out / 'manifest-timed.json', timed)
        durable_json(args.out / 'manifest-gated.json', gated)
        snapshot(args.arena / 'engines/recipes.json', 'recipes.json')
        for name in ROOT_HELPERS:
            snapshot(args.source / name, name)
        for name in ENGINE_HELPERS:
            snapshot(args.source / 'engines' / name, 'engines/' + name)
        for name in RUNTIME_HELPERS:
            snapshot(args.source.parent / '0017.runtime' / name, 'runtime/' + name)
        if args.ownership_proof is not None:
            proof = snapshot(args.ownership_proof, 'inputs/ownership-proof.json')
            require(proof['passed'] and proof['all_explicit_fixture_pids_absent']
                    and proof['helper_sha256'] == checked(args.source / 'owned_process.py')
                    and proof['test_sha256'] == checked(args.source / 'engines/check_unreaped_leader.py')
                    and proof['boot'] == boot
                    and proof['admission_before'] == proof['admission_after']
                    and proof['admission_before']['membership'] == before['membership'],
                    'ownership proof failed identity/admission binding')
            require(len(proof['cases']) == 5 and all(case['passed'] for case in proof['cases'])
                    and {case['name'] for case in proof['cases']} == {
                        'initial-unreaped-false-and-wrong-start', 'live-to-z-boundary-and-poll-revocation',
                        'already-reaped-refused', 'zombie-first-same-session-helper-and-foreign',
                        'invoke-delayed-registration-remains-na'}, 'ownership probe contract incomplete')
            for name in ('lifecycle.json', 'stdout.log', 'stderr.log'):
                snapshot(args.ownership_proof.parent / 'invoke-false' / name)
            record['ownership_proof_sha256'] = checked(args.ownership_proof)
        for evidence in args.evidence:
            snapshot(evidence)
        for source in args.out.rglob('*.py'):
            ast.parse(source.read_text(), filename=str(source))
        environment = snapshot(args.environment, 'inputs/environment-recorded.json')
        libraries = snapshot(args.profile_libraries / 'receipt.json', 'inputs/profile-library-receipt.json')
        require(libraries['passed'] and libraries['environment_sha256'] == checked(args.environment),
                'public profile libraries belong to another environment')
        external = record['external_runtime_files']
        for name, executable in environment['executables'].items():
            path = executable['path']
            external[path] = {'sha256': checked(path, executable['sha256']), 'role': name}
            for path, dependency in executable['dependencies'].items():
                external[path] = {'sha256': checked(path, dependency['sha256']), 'role': name + ':dependency'}
        for repo in ('uwvm2', 'uwvm2-stable', 'uwvm2-ros'):
            binary = args.arena / 'products' / repo / 'uwvm'
            identity = snapshot(binary.parent / 'identity.json', 'inputs/products/' + repo + '-identity.json')
            checked(binary, identity['binary_sha256'])
            require(identity['binary_sha256'] == old['products'][repo]['binary_sha256'], 'base product changed')
            record['products'][repo] = identity
            row = libraries['binaries'][repo]
            external[row['public']] = {'sha256': checked(row['public'], identity['binary_sha256']), 'role': repo + ':public'}
        for row in libraries['libraries']:
            for key in ('copy', 'private_copy', 'source'):
                external[row[key]] = {'sha256': checked(row[key], row['sha256']), 'role': 'profile-library:' + key}
        for row in gated:
            public = libraries['wasms'][row['wasm_sha256']]
            require(public['sha256'] == row['wasm_sha256'], 'public Wasm binding differs')
            checked(public['public'], row['wasm_sha256'])
        record.update(profile_libraries=str(args.profile_libraries),
                      profile_library_receipt_sha256=checked(args.profile_libraries / 'receipt.json'),
                      timed_routes=len(timed), ordinary_routes=7, gated_routes=7,
                      expected_hardware_profiles=42, expected_engine_pairs=49,
                      calibration_required_before_final=True,
                      sampling={'hotspots_event_base': 20000000, 'requested_interval_ms': 10,
                                'expected_actual_multiplier': 10, 'expected_hotspots_period': 200000000,
                                'uarch_mode': 'detailed', 'safety_watch_changed': False})
        for name, value in input_hashes.items():
            require(digest(name) == value, 'input changed before freeze: ' + name)
        record['admission_after'] = admission()
        require(before['membership'] == record['admission_after']['membership']
                and before['memory_events'] == record['admission_after']['memory_events'], 'admission changed')
        require(Path('/proc/sys/kernel/random/boot_id').read_text().strip() == boot, 'boot changed')
        require(shutil.disk_usage(args.out).free >= 512 << 20, 'disk reserve below 512 MiB')
        record['files'] = {str(path.relative_to(args.out)): digest(path)
                           for path in sorted(args.out.rglob('*')) if path.is_file()}
        for path in args.out.rglob('*'):
            if path.is_file():
                with path.open('rb') as stream:
                    os.fsync(stream.fileno())
        record['passed'] = True
    except BaseException as error:
        record['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        record['ended_utc'] = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())
        durable_json(args.out / 'freeze.json', record)
    print(json.dumps({'passed': record['passed'], 'freeze_sha256': digest(args.out / 'freeze.json'),
                      'manifest_timed_sha256': digest(args.out / 'manifest-timed.json'),
                      'manifest_gated_sha256': digest(args.out / 'manifest-gated.json'),
                      'frozen_files': len(record['files'])}), flush=True)


if __name__ == '__main__':
    main()
