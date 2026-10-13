#!/usr/bin/env python3
"""Small native GC argv/semantic adapter for the keeper's existing P0 protocol.

Importing this file launches nothing. The keeper owns actual compilation, cold
receipts, source/provider/ELF closure, admission and before/after retirement.
No new guardian, profiler, event, process launcher or timing loop is defined.
"""
import hashlib
import json
import os
from pathlib import Path
import types

HERE = Path(__file__).resolve().parent
PINS = {
    'run_current_EH_P0_measurement.py': '7ddaa492a182879653ffe2619275c811a82e2aa1960d85b8b7400ae83fa24e6b',
    'run_current_general_gc.py': '91ee4854dcf7f176427f5943895bd9f933a5bfa2819c263c30e78774c8beada8',
    'run_general_gc_R5_nativeTLS.py': 'd96e7eb2795d020ca5bf29800500b98b947db901eb7bf262eff1130da7b863cb',
    'run_current_host_pcore_hw_counting.py': '673d961bb4fd37bd6ed98cc6c486cac3d2dc7fe256230795468ea58db7b42cd5',
    'run_current_pcore_hw_counting.py': '266a56b3389ca5e120a757bd2a7a9b101b834810e5ba53f053b50f536aa425f8',
    'run_current_pcore_diagnostic.py': '0ab44317fea5c7cddcf236d79be232e24237386d17795684fa2f0fa51c487f89',
    'prepare_gc_native_api_costs_dense.py': '2dd27a221547103d7c7616d6ae0ff4760d0495fb930993e9ed4cd687ffc74720',
}
FIXTURE_SHA = '88ccd08bac0ef5e3725cd927c153233b98c041127d8e6f32cc48e118c2d81d08'
SOURCE_ID = '0ab1df058a30a5a3e4e562c046386ce6b2e9f9cfe05c8e861f2a14c54a3f5f0b'
PROFILES = {
    'baseline': [],
    'publication-only': ['UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION=1'],
    'C': ['UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA=1'],
    'D': ['UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA=1',
          'UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA=1'],
}


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def load(filename):
    path = HERE / filename
    blob = path.read_bytes()
    require(hashlib.sha256(blob).hexdigest() == PINS[filename], 'Frozen protocol changed: ' + filename)
    module = types.ModuleType('_native_gc_' + path.stem)
    module.__file__ = str(path)
    exec(compile(blob, str(path), 'exec'), module.__dict__)
    return module


def dependencies():
    # Only source imports. None of the imported main/build/prepare routines run.
    return types.SimpleNamespace(eh=load('run_current_EH_P0_measurement.py'),
        sampler=load('run_current_general_gc.py'), r5=load('run_general_gc_R5_nativeTLS.py'),
        host=load('run_current_host_pcore_hw_counting.py'),
        hw=load('run_current_pcore_hw_counting.py'), base=load('run_current_pcore_diagnostic.py'),
        oracle=load('prepare_gc_native_api_costs_dense.py'))


def item(binary, profile, phase, units, label, oracle_module):
    require(profile in PROFILES and Path(binary).is_absolute(), 'Actual binary/profile missing')
    expected = oracle_module.oracle(phase, units)
    return dict(label=label, fixture='native-GC-' + phase + '-' + str(units),
        profile='ordinary/native-api/' + profile, native_profile=profile,
        argv=['taskset', '-c', '0', str(binary), phase, str(units),
              str(expected['step_checksum_u32']), str(expected['root_checksum_u32'])],
        phase=phase, units=units, expected=expected, component_source_sha256=FIXTURE_SHA,
        source_id_external=SOURCE_ID, component_only=True,
        vm_qualified=False, same_wasm_comparison=False)


def semantic_receipt(command, log, code):
    result = dict(passed=False, failures=[], actual=None, guest_execution_ns=None,
                  component_only=True, vm_qualified=False, same_wasm_comparison=False,
                  formal_acceptance=False)
    try:
        require(type(code) is int and code == 0, 'Actual native component did not exit0')
        actual = json.loads(Path(log).read_bytes())
        require(isinstance(actual, dict) and actual.get('schema') == 'uwvm-gc-native-api-costs-dense-v2',
                'Wrong/ambiguous actual native GC JSON')
        require(actual.get('phase') == command['phase'] and actual.get('units') == command['units'],
                'Actual phase/count differs')
        for key, expected in command['expected'].items():
            if key != 'native_or_vm_executed':
                require(type(actual.get(key)) is type(expected) and actual[key] == expected,
                        'Actual native GC oracle differs: ' + key)
        require(actual.get('actual_exclusive_reader_protected') is True and
                actual.get('component_only') is True and actual.get('vm_qualified') is False and
                actual.get('same_wasm_comparison') is False, 'Actual scope/exclusive proof differs')
        profile = command['native_profile']
        require(actual.get('metadata_plan_present_in_layout') is (profile in ('C', 'D')) and
                actual.get('inline_metadata_enabled') is (profile == 'D'), 'Actual metadata profile differs')
        trace = command['phase'].startswith('trace')
        for key in ('construction_ns', 'timed_api_ns', 'timed_collection_ns', 'maximum_collection_ns'):
            require(type(actual.get(key)) is int and actual[key] >= 0, 'Invalid actual timer: ' + key)
        require(actual['timed_api_ns'] == 0 if trace else actual['timed_api_ns'] > 0,
                'Actual API timer differs from phase')
        require(actual['timed_collection_ns'] > 0 if trace else actual['timed_collection_ns'] == 0,
                'Actual collection timer differs from phase')
        require(0 < actual['maximum_collection_ns'] <= actual['timed_collection_ns'] if trace else
                actual['maximum_collection_ns'] == 0, 'Actual maximum collection timer differs')
        reads = 1 if command['phase'] == 'mutate-numeric' else 2 if command['phase'] == 'mutate-reference' else 0
        writes = int(command['phase'].startswith('mutate-'))
        require(actual.get('api_reads_per_mutation_unit') == reads and
                actual.get('api_writes_per_mutation_unit') == writes, 'Actual access chain differs')
        result.update(passed=True, actual=actual,
                      guest_execution_ns=actual['timed_api_ns'] + actual['timed_collection_ns'])
    except (RuntimeError, ValueError, OSError) as error:
        result['failures'].append(type(error).__name__ + ': ' + str(error))
    return result


def sample(deps, guard, command, out, measurement, perf=None):
    """Called only by the admitted keeper between its existing closure checks.

    The caller creates the original r5 terminal guard with this actual profile's
    binary pin and runs the existing actual source/provider/ELF closure. A guard
    for a different binary must not be reused across an A/B profile transition.
    """
    require(measurement in ('plain', 'pure_hw') and Path(out).is_dir(), 'Actual fresh sample directory missing')
    require(os.environ.get('RAYON_NUM_THREADS') == '1' and not os.environ.get('LD_PRELOAD') and
            not os.environ.get('LD_AUDIT'), 'Actual child environment differs')
    adapted = deps.host.HostBase(deps.base, guard.root)
    base = types.SimpleNamespace(**deps.base.__dict__)
    base.telemetry = adapted.telemetry
    base.effective_argv = lambda value: list(value['argv'])
    base.semantic_receipt = semantic_receipt
    aux = types.SimpleNamespace(semantic_receipt=semantic_receipt)
    witnesses = []
    hw = deps.r5.stopped_environment_view(deps.hw, witnesses)
    if measurement == 'plain':
        row = deps.sampler.plain_sample(base, hw, guard, command, Path(out), semantic_receipt)
    else:
        require(perf is not None, 'Actual direct perf ELF missing')
        row = deps.eh.hardware_sample(hw, deps.host, base, aux, guard, command, Path(perf), Path(out))
    row.update(component_only=True, vm_qualified=False, same_wasm_comparison=False,
               native_profile=command['native_profile'], component_source_sha256=FIXTURE_SHA,
               source_id_external=SOURCE_ID, child_environment_receipts=witnesses,
               internal_ROI_scope='API/checksum loop or actual closed collection; excludes setup/final qualification',
               counter_scope='Whole single-TID process, including setup/allocation/readback/free; not isolated internal ROI')
    internal = row['semantic_receipt'].get('guest_execution_ns')
    if internal is None or internal < 100_000_000:
        row['quality_failures'].append('Sub100ms or incomplete internal native ROI; not qualified by parent poll wall')
    row['timing_identity_quality_passed'] = (row['semantic_receipt']['passed'] and
        not row['hard_failures'] and not row['quality_failures'])
    # Existing protocol owns actual executable/PIDFD, raw enabled/running/ACK,
    # CPU/UID/cgroup/retirement and in-window sysfs frequency observations.
    # Frequency snapshots are not effective ROI GHz and temperature is observed.
    deps.r5.save(Path(out) / 'qualified-native.json', row)
    return row
