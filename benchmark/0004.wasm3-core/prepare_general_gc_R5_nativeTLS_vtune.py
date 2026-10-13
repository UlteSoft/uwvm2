#!/usr/bin/env python3
"""Command-only current R5 VTune recipe. Never launches a guest or profiler."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import types

HERE = Path(__file__).resolve().parent
DRIVER_SHA = 'd96e7eb2795d020ca5bf29800500b98b947db901eb7bf262eff1130da7b863cb'
PLAN_SHA = 'c6e08fa3595470094b6c97db55c3ddd20e45440862e014a4003be4c03ba63f35'
VTUNE = '/opt/intel/oneapi/vtune/2026.4/bin64/vtune'
CELLS = ('reference-array-allocate-2000000', 'mutable-struct-mutate-2000000')
COLLECTORS = ('hotspots', 'uarch-exploration')
HISTORICAL_HELP = {
    'version': 'f77d69a0bed6e01d78c2e961f2408f0ac3592b5ebef9c8781c14f8e0b2836148',
    'hotspots': '1d4071eb7a1564b565cb2f82054b0113ad2ac39850bc63ea75fa186578c58edb',
    'uarch-exploration': '00f0fa0c7866cc326206173652b8a56d945e06542f002cd9a658ce6bc6d70d1c',
}
DEFAULT_RESULT = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/evidence/R5-nativeTLS-general-gc-vtune-20261002-r1')


def require(ok, reason):
    if not ok:
        raise RuntimeError(reason)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def driver():
    path = HERE / 'run_general_gc_R5_nativeTLS.py'
    data = path.read_bytes()
    require(sha(data) == DRIVER_SHA, 'Frozen R5 binding changed')
    module = types.ModuleType('_frozen_r5_vtune_binding')
    module.__file__ = str(path)
    exec(compile(data, str(path), 'exec'), module.__dict__)
    return module


def profiler_argv(collector, result, guest):
    require(collector in COLLECTORS and len(guest) > 4 and guest[:3] == ['taskset', '-c', '0'],
            'Wrong hardware collector/target CPU argv')
    common = ['/usr/bin/taskset', '-c', '16', VTUNE, '-collect', collector]
    if collector == 'hotspots':
        common += ['-knob', 'sampling-mode=hw', '-knob', 'enable-stack-collection=true',
                   '-knob', 'stack-size=1024', '-knob', 'enable-characterization-insights=false',
                   '-run-pass-thru=--perf-threads=none']
    else:
        common += ['-knob', 'pmu-collection-mode=summary']
    # Historical cpu-mask=0 was unsupported for this target. Actual guest
    # affinity and per-result cpuid=cpu_0 reporting provide the CPU evidence.
    return [*common, '-result-dir', str(result), '--', *guest]


def clean_controller_environment(plan, result):
    # The sole keeper supplies this explicit environment to its already-owned
    # controller/spawn protocol. This recipe does not execute env or Popen.
    # Private HOME avoids inherited VTune user configuration; no inherited
    # capture, LD_PRELOAD/AUDIT, VTune, ITT, PERFPID or runtime override survives.
    return {'HOME': str(result / 'profiler-home'), 'TMPDIR': str(result / 'profiler-tmp'),
            'USER': 'macromodel', 'LOGNAME': 'macromodel',
            'PATH': '/usr/bin:/bin:/usr/sbin:/sbin', 'LANG': 'C', 'LC_ALL': 'C',
            'LD_LIBRARY_PATH': plan['LD_LIBRARY_PATH'], 'RAYON_NUM_THREADS': '1'}


def recipe(plan, result, tool_pin, inherited):
    selected = [row for row in plan['commands']
                if row['profile'] == 'ordinary/unwind/R5-nativeTLS-default' and row['fixture'] in CELLS]
    require(len(selected) == 2 and {row['fixture'] for row in selected} == set(CELLS),
            'Missing/duplicate current R5 cell')
    environment = clean_controller_environment(plan, result)
    commands = [
        ['installed-version', ['/usr/bin/taskset', '-c', '16', VTUNE, '-version']],
        ['installed-hotspots-help', ['/usr/bin/taskset', '-c', '16', VTUNE, '-help', 'collect', 'hotspots']],
        ['installed-uarch-help', ['/usr/bin/taskset', '-c', '16', VTUNE, '-help', 'collect', 'uarch-exploration']],
        ['installed-report-help', ['/usr/bin/taskset', '-c', '16', VTUNE, '-help', 'report']],
    ]
    targets = []
    for cell in CELLS:
        row = next(v for v in selected if v['fixture'] == cell)
        for collector in COLLECTORS:
            name = cell + '-' + collector
            directory = result / name
            commands.append([name, profiler_argv(collector, directory, row['argv'])])
            targets.append({'label': name, 'fixture': cell, 'collector': collector,
                            'result_directory': str(directory), 'guest_argv': row['argv'],
                            'fixture_pin': plan['fixtures'][cell], 'actual_PID_TID': None,
                            'actual_guest_environment': None, 'actual_collection_passed': False})
    report_queries = []
    for target in targets:
        prefix = ['/usr/bin/taskset', '-c', '16', VTUNE]
        directory, name = target['result_directory'], target['label']
        report_queries.append([name + '-summary', [*prefix, '-report', 'summary', '-result-dir', directory]])
        for report in ('hotspots', 'hw-events'):
            for query in ('group-by=?', 'filter=?'):
                report_queries.append([name + '-' + report + '-' + query.split('=')[0] + '-query',
                                       [*prefix, '-report', report, '-result-dir', directory, '-' + query]])
    return {
        'schema': 'uwvm-r5-nativeTLS-vtune-command-recipe-v1',
        'current_plan_sha256': PLAN_SHA, 'product': plan['product'], 'source_id': plan['source_id'],
        'native_TLS_all_TUs': True, 'experiments': 'all omitted', 'vtune': tool_pin,
        'controller_environment': environment,
        'inherited_environment_inventory': [
            {'name': k, 'bytes': len(os.fsencode(v)), 'value_sha256': sha(os.fsencode(v)),
             'kept_exactly': environment.get(k) == v}
            for k, v in sorted(inherited.items())],
        'expected_initial_profiler_environment_sha256': sha(
            json.dumps(environment, sort_keys=True, separators=(',', ':')).encode()),
        'historical_installed_help_sha256': HISTORICAL_HELP,
        'actual_current_help_and_profiler_DSO_closure': None,
        'targets': targets, 'commands': commands, 'report_queries': report_queries,
        'required_before_after': 'unchanged R5 driver.closure with current plan/actual libraries plus actual VTune tool/DSO/helper pins',
        'required_guard': 'keeper derived r6 all-tree owned PIDFD/birth/UID/CG/TID guard; no counting roster widening',
        'required_environment_witness': 'actual stopped controller/profiler bootstrap plus actual guest proc environ; capture absent, LD matches, no preload/audit',
        'required_target_reports': 'actual per-result guest PID/TID + cpuid=cpu_0, P-Core; retain unfiltered reports',
        'temperature_policy': 'observation_only', 'formal_acceptance': False,
        'command_recipe_only': True, 'execute_ready': False,
        'actual_current_VTune_collection_passed': False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--result-root', type=Path, default=DEFAULT_RESULT)
    args = parser.parse_args()
    raw = args.plan.read_bytes()
    require(sha(raw) == PLAN_SHA, 'Wrong or changed actual R5 prepared plan')
    plan = json.loads(raw)
    current = driver()
    sampler, cold = current.dependencies()
    current.validate_plan(plan, sampler, cold)
    # Recheck direct bytes now; the unchanged full closure is still mandatory
    # before/after actual collection. A recipe is not publication authority.
    for pin in [plan['product'], *(plan['fixtures'][cell] for cell in CELLS)]:
        sampler.verify_pin(pin)
    tool = current.pin(VTUNE)
    require(args.result_root.is_absolute() and not args.result_root.exists() and not args.out.exists() and
            not args.out.with_name('commands.json').exists() and not args.out.with_name('report-queries.json').exists(),
            'Fresh absolute result/output/sidecars required')
    value = recipe(plan, args.result_root, tool, dict(os.environ))
    value['current_plan_path'] = str(args.plan.resolve(strict=True))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    current.save(args.out, value)
    current.save(args.out.with_name('commands.json'), value['commands'])
    current.save(args.out.with_name('report-queries.json'), value['report_queries'])


if __name__ == '__main__':
    main()
