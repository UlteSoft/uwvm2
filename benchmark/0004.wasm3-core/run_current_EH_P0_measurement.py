#!/usr/bin/env python3
"""Current Ordinary EH binding; immutable plain/PIDFD/HW protocols are reused.

No SSH, compilation, admin action, software counters or new guardian. Actual
source-bound twelve-cell cold and the approved fixed order precede this runner.
"""
import argparse
import decimal
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import signal
import types

HERE = Path(__file__).resolve().parent
ORDER_SHA = '06c7762ba429e0630efdcc1f67e40cbe6f12abff010217c55bb4d990aa693e43'
ORDER_PLAN_SHA = '700f5c02679e8c922abeb05d70ab9a5dd6b1492ac478150bf7041416d5834992'
R5_SHA = 'd96e7eb2795d020ca5bf29800500b98b947db901eb7bf262eff1130da7b863cb'
SAMPLER_SHA = '91ee4854dcf7f176427f5943895bd9f933a5bfa2819c263c30e78774c8beada8'
HOST_SHA = '673d961bb4fd37bd6ed98cc6c486cac3d2dc7fe256230795468ea58db7b42cd5'
HW_SHA = '266a56b3389ca5e120a757bd2a7a9b101b834810e5ba53f053b50f536aa425f8'
BASE_SHA = '0ab44317fea5c7cddcf236d79be232e24237386d17795684fa2f0fa51c487f89'
CAPTURE = 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT'


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load(name, path, expected):
    data = Path(path).read_bytes()
    require(hashlib.sha256(data).hexdigest() == expected, 'Frozen source changed: ' + str(path))
    module = types.ModuleType(name)
    module.__file__ = str(path)
    exec(compile(data, str(path), 'exec'), module.__dict__)
    return module


def dependencies():
    # Import guarded modules as source; their main/build/prepare never runs.
    return (load('_eh_order', HERE/'prepare_current_EH_P0_order.py', ORDER_SHA),
            load('_eh_r5_adapters', HERE/'run_general_gc_R5_nativeTLS.py', R5_SHA),
            load('_eh_plain_sampler', HERE/'run_current_general_gc.py', SAMPLER_SHA),
            load('_eh_host_counter', HERE/'run_current_host_pcore_hw_counting.py', HOST_SHA))


def validate_order(plan, order, parent):
    require(plan['schema'] == 'uwvm-current-R3c-EH-P0-order-v1' and
            plan['formal_acceptance'] is False and plan['temperature_policy'] == 'observation_only' and
            all(plan[key] is False for key in ('actual_P0_plain_passed', 'actual_HW_passed', 'actual_VTune_passed')),
            'Only fixed functional-cold order, not inferred performance permission')
    prefix, tail, actual_product = parent.parent_product()
    require(plan['product'] == actual_product, 'Current complete R3c parent product differs')
    require(plan['original_parent']['sha256'] == order.PARENT_SHA and
            plan['source_helper']['sha256'] == ORDER_SHA and set(plan['commands']) == {'plain', 'pure_hw', 'vtune'} and
            set(plan['cold_receipts']) == set(order.PINS), 'Order source/cold/command contract differs')
    for name, expected in order.PINS.items():
        require(plan['cold_receipts'][name]['sha256'] == expected, 'Cold receipt identity differs')
    original = {label.replace('_', '-'): argv for label, argv in parent.lists(prefix, tail)['eh-cold']}
    observed = {label: ('native' if label.startswith('plain-normal-') or label.endswith('native-unwind')
                        else 'r2-phase') for label in order.ORDER}
    expected = order.order(original, actual_product, parent.EH, observed)
    actual = {}
    for name, pin in plan['commands'].items():
        require(parent.pin(pin['path']) == pin, 'Fixed command file changed')
        actual[name] = json.loads(Path(pin['path']).read_bytes())
    require(actual == expected, 'Actual taskset/trace/dispatch/environment/order differs')
    return actual


def semantic_receipt(item, log, exit_code):
    # Exit zero is the exact pinned fixture's own scalar checksum/catch check;
    # these expected constants are not invented dynamic runtime counters.
    definition = item['expected_self_check']
    receipt = {'passed': False, 'exit_zero': type(exit_code) is int and exit_code == 0,
               'fixture': item['fixture'], 'trace': item['profile'].split('/')[1],
               'dispatch': item['profile'].split('/')[2], 'expected': definition,
               'self_check_proof': 'Pinned self-checking _start exits0; checksum/catches are fixture properties.',
               'observed_pending_plan': None, 'guest_execution_ns': None,
               'whole_process_reported_ns': None, 'failures': [], 'formal_acceptance': False}
    try:
        require(receipt['exit_zero'], 'Actual self-checking fixture did not exit0')
        text = re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]', '', Path(log).read_text(errors='replace'))
        materializations = re.findall(r'^\[llvm-jit-full\] (owning-source=[^\r\n]+)$', text, re.M)
        require(len(materializations) == 1, 'Exactly one actual full compiler materialization receipt required')
        match = re.fullmatch(r'owning-source=yes pending-plan=(native|r2-phase) object-cache=disabled body-fallback=no'
                             r'(?: module=([0-9]+) actual-epoch=([0-9]+))?',
                             materializations[0])
        require(match is not None, 'Actual owner/cache/body fallback receipt differs')
        receipt['observed_pending_plan'] = match[1]
        receipt['reported_module_id'] = int(match[2]) if match[2] is not None else None
        receipt['reported_epoch'] = int(match[3]) if match[3] is not None else None  # Observation only, never authority.
        require(match[1] == item['cold_observed_pending_plan'], 'Actual dispatch differs from fixed cold/command policy')
        for field, label in (('guest_execution_ns', 'Total WASM execution time'),
                             ('whole_process_reported_ns', 'Total process time')):
            values = re.findall(re.escape(label) + r': ([0-9]+(?:\.[0-9]+)?)s\.', text)
            require(len(values) == 1, 'Missing/ambiguous actual ' + label)
            ns = decimal.Decimal(values[0]) * 1000000000
            require(ns > 0 and ns == ns.to_integral_value(), 'Nonpositive/non-nanosecond actual timer')
            receipt[field] = int(ns)
        require(receipt['whole_process_reported_ns'] >= receipt['guest_execution_ns'], 'Process timer smaller than Wasm timer')
        receipt['passed'] = True
    except (RuntimeError, OSError, decimal.InvalidOperation) as error:
        receipt['failures'].append(type(error).__name__ + ': ' + str(error))
    return receipt


def closure(plan, args, order, parent, sampler, host, libraries, out, stage):
    require(digest(args.plan) == ORDER_PLAN_SHA, 'Actual approved order plan changed')
    commands = validate_order(plan, order, parent)
    for pin in [*plan['cold_receipts'].values(), *plan['commands'].values(),
                plan['original_parent'], plan['source_helper']]:
        sampler.verify_pin(pin)
    cold_after = json.loads(Path(plan['cold_receipts']['cold-after.json']['path']).read_bytes())
    require(cold_after['passed'] is True and cold_after['product'] == plan['product'] and
            cold_after['actual_EH_cells'] == 12, 'Actual closed EH functional cold missing')
    for proof in cold_after['actual_cell_proofs'].values():
        require(proof['original_PIDFD_retired'] is True and proof['actual_root_returncode'] == 0,
                'Original cold child not completed')
        sampler.verify_pin(proof['raw_log'])
    for name, definition in parent.EH.items():
        require(digest(parent.F/(name+'.wasm')) == definition['sha256'], 'Exact self-checking Wasm changed')
    require(host.host_source_id(str(parent.S), out/('source-'+stage+'.json')) == parent.SID,
            'Current canonical source fingerprint changed')
    environment = commands['plain'][0]['environment_delta']
    require(all(os.environ.get(key) == value for key, value in environment.items()) and
            environment['LD_LIBRARY_PATH'] == libraries['LD_LIBRARY_PATH'] and
            not any(os.environ.get(key) for key in ('LD_PRELOAD', 'LD_AUDIT', 'LD_PROFILE')) and
            not any(key.startswith(('VTUNE_', 'INTEL_LIBITTNOTIFY')) for key in os.environ),
            'Actual loader/thread/profiler environment differs')
    for pin in libraries['files']:
        sampler.verify_pin(pin)
        require(str(Path(pin['path']).resolve(strict=True)) == pin['real_path'], 'Actual perf library path changed')
    return commands


def hardware_sample(hw, host, base, aux, guard, item, perf, out):
    signals = []
    globals_ = dict(hw.measure.__globals__, EVENT_GROUP=host.RAW_GROUP, spawn_stopped=hw.spawn_stopped,
                    parse_stat=lambda stat, debug='': host.normalized_math(hw, stat, debug),
                    signal_owned=lambda entry, sig: host.submit_owned_signal(hw, base, guard, entry, sig, signals))
    measure = types.FunctionType(hw.measure.__code__, globals_, '_unchanged_eh_hw_measure')
    row = measure(base, aux, guard, item, perf, out)
    row['owned_perf_sigint_receipts'] = signals[:]
    log = out/(row['label']+'.perf.log')
    attrs = host.final_attributes(log.read_text(errors='replace'), row['guest_admission']['pid']) if row['guest_admission'] else {
        'passed': False, 'failures': ['No actual guest']}
    counts = row['hardware_counting']
    prior = counts['failures'][:]
    counts['final_guest_attributes'] = attrs
    counts['failures'] = [v for v in prior if not v.startswith('Awaiting actual final-guest')] + attrs['failures']
    completion = host.perf_completion(row)
    row['actual_perf_completion'] = completion
    counts['counter_quality_passed'] = counts.get('math_quality_passed', False) and attrs['passed'] and completion['passed']
    counts['failures'] += completion['failures']
    row['quality_failures'] = [v for v in row['quality_failures'] if v not in prior] + counts['failures']
    return row


def run(args):
    order, r5, sampler, host = dependencies()
    parent = order.parent()
    require(args.plan == order.OUT/'plan.json' and digest(args.plan) == ORDER_PLAN_SHA, 'Only actual fixed approved order path/SHA')
    plan = json.loads(args.plan.read_bytes())
    commands = validate_order(plan, order, parent)
    admission = json.loads(args.admission.read_bytes())
    libraries = json.loads(args.perf_libraries.read_bytes())
    host.validate_raw_inventory(admission, libraries)
    require(digest(admission['trusted_wrapper']['path']) == admission['trusted_wrapper']['sha256'], 'Actual scope wrapper changed')
    hw, hw_path = host.fixed_module('_eh_owned_hw', 'run_current_pcore_hw_counting.py', HW_SHA)
    original, base_path = host.fixed_module('_eh_owned_base', 'run_current_pcore_diagnostic.py', BASE_SHA)
    inputs = [Path(__file__), HERE/'prepare_current_EH_P0_order.py', HERE/'prepare_current_EH_thread_commands.py',
              HERE/'run_general_gc_R5_nativeTLS.py', HERE/'run_current_general_gc.py',
              HERE/'run_current_host_pcore_hw_counting.py', hw_path, base_path, args.plan, args.admission,
              args.perf_libraries, Path(admission['trusted_wrapper']['path'])]
    input_sha = {str(path): digest(path) for path in inputs}
    require(args.execute and not args.out.exists(), 'Explicit --execute and fresh output required')
    args.out.mkdir(parents=True)
    for name, value in (('plan.json', plan), ('admission.json', admission), ('perf-libraries.json', libraries)):
        r5.save(args.out/name, value)
    for path in inputs[:8]:
        r5.atomic(args.out/path.name, path.read_bytes())
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    def cancel(signum, frame):
        raise KeyboardInterrupt('Controlled cancellation ' + str(signum))
    signal.signal(signal.SIGTERM, cancel)
    # No reference guest is admissible here. The unused reference path is a
    # sentinel; the product-only terminal proof remains the original function.
    guard_plan = {'product': plan['product']['product'], 'wasmtime': {'path': '__no_reference_guest__'}}
    guard = r5.r5_terminal_guard(sampler, host, original, admission, args.out, guard_plan)
    adapted = host.HostBase(original, guard.root)
    base = types.SimpleNamespace(**original.__dict__)
    base.telemetry = adapted.telemetry
    base.effective_argv = lambda item: list(item['argv'])
    base.semantic_receipt = semantic_receipt
    aux = types.SimpleNamespace(semantic_receipt=semantic_receipt)
    rows, error = [], None
    before_ok = after_ok = complete = False
    pmu_before = pmu_after = None
    try:
        selected = closure(plan, args, order, parent, sampler, host, libraries, args.out, 'before')[args.measurement]
        guard.check()
        before_ok = True
        if args.measurement == 'pure_hw':
            pmu_before = host.pmu_receipt()
            r5.save(args.out/'pmu-before.json', pmu_before)
            perf = Path(admission['host_perf']['path']).resolve(strict=True)
            require(perf.read_bytes()[:4] == b'\x7fELF' and digest(perf) == admission['host_perf']['sha256'] and
                    any(v['real_path'] == str(perf) and v['sha256'] == digest(perf) for v in libraries['files']),
                    'Actual direct perf ELF/closure differs')
        for index, item in enumerate(selected):
            out = args.out/(str(index)+'-'+item['label'])
            out.mkdir()
            environment_receipts = []
            current_hw = r5.stopped_environment_view(hw, environment_receipts)
            row = (sampler.plain_sample(base, current_hw, guard, item, out, semantic_receipt) if args.measurement == 'plain'
                   else hardware_sample(current_hw, host, base, aux, guard, item, perf, out))
            row.update(order_index=index, fixed_order_label=item['label'], measurement_family=args.measurement,
                       source_id_external=parent.SID, product_sha256=plan['product']['product']['sha256'],
                       child_environment_receipts=environment_receipts, input_sha256=input_sha,
                       host_smt_noise='unknown without independent actual observer', formal_acceptance=False)
            # Short auto samples remain diagnostic; their actual null exec /
            # frequency evidence is retained by the immutable sampler.
            row['timing_identity_quality_passed'] = (not row['hard_failures'] and not row['quality_failures'] and
                                                     row['semantic_receipt']['passed'])
            r5.save(out/'qualified.json', row)
            rows.append(row)
            with (args.out/'rows.jsonl').open('a') as stream:
                stream.write(json.dumps(row, allow_nan=False)+'\n')
            require(not row['hard_failures'] and row['semantic_receipt']['passed'], 'Actual owned guest or EH semantic failure')
            require(environment_receipts and all(v.get('capture_present') is False and
                    v.get('stopped_before_guest_GO') is True for v in environment_receipts), 'Actual stopped capture-clear witness absent')
            if args.measurement == 'pure_hw':
                require(row['actual_perf_completion']['passed'] and row['hardware_counting']['counter_quality_passed'],
                        'Actual perf ACK/FD/group/raw enabled-running/completion failed')
        closure(plan, args, order, parent, sampler, host, libraries, args.out, 'after')
        guard.check()
        if args.measurement == 'pure_hw':
            pmu_after = host.pmu_receipt()
            r5.save(args.out/'pmu-after.json', pmu_after)
            require(pmu_before == pmu_after, 'Actual PMU changed')
        require(all(digest(path) == value for path, value in input_sha.items()), 'Immutable execution input changed')
        after_ok = complete = True
    except BaseException as failure:
        error = type(failure).__name__ + ': ' + str(failure)
        raise
    finally:
        r5.save(args.out/'summary.json', {'schema': 'uwvm-current-R3c-EH-P0-measurement-v1',
                'measurement_family': args.measurement, 'rows': len(rows), 'complete': complete,
                'closure_before_ok': before_ok, 'closure_after_ok': after_ok, 'execution_error': error,
                'semantic_pass_count': sum(row['semantic_receipt']['passed'] for row in rows),
                'timing_identity_quality_pass_count': sum(row['timing_identity_quality_passed'] for row in rows),
                'hardware_quality_pass_count': sum(row.get('hardware_counting', {}).get('counter_quality_passed', False) for row in rows),
                'source_id_external': parent.SID, 'input_sha256': input_sha,
                'formal_acceptance': False, 'temperature_policy': 'observation_only',
                'limitations': ['Internal Wasm timer excludes startup/JIT; includes all EH loop/checksum, not isolated throw latency.',
                    'Parent wait4 CPU/RSS includes the stopped bootstrap; GO-to-reap wall is recorded separately.',
                    'Pure HW counts whole guest single TID including startup/JIT; not internal Wasm ROI.',
                    'Frequency is sysfs snapshots, not effective ROI GHz; host SMT noise remains unknown.',
                    'Short auto samples may lack exec/window proof and remain unqualified; no software fallback.',
                    'VTune hardware sampling/MUX/environment qualification is a separate three-command family.']})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('plan', 'admission', 'perf-libraries', 'out'):
        parser.add_argument('--'+key, type=Path, required=True)
    parser.add_argument('--measurement', choices=('plain', 'pure_hw'), required=True)
    parser.add_argument('--execute', action='store_true')
    run(parser.parse_args())


if __name__ == '__main__':
    main()
