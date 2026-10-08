#!/usr/bin/env python3
"""Pure EH contract checks; no subprocess, proc, native or guest execution."""
import ast
import copy
import hashlib
import json
from pathlib import Path
import sys
import types

HERE = Path(__file__).parent
source = HERE/'run_current_EH_P0_measurement.py'
m = types.ModuleType('_eh_measurement_contract')
m.__file__ = str(source)
exec(compile(source.read_bytes(), str(source), 'exec'), m.__dict__)
order, r5, sampler, host = m.dependencies()
parent = order.parent()
checks = 0


def check(ok):
    global checks
    assert ok
    checks += 1


def rejects(callback):
    global checks
    try:
        callback()
    except (RuntimeError, KeyError):
        checks += 1
    else:
        raise AssertionError('Invalid contract accepted')


prefix = ['env', 'LD_LIBRARY_PATH=/canonical/lib', 'actual-clang']
original = {label.replace('_', '-'): argv for label, argv in parent.lists(prefix, ['@fixed-rsp'])['eh-cold']}
product = {'product': {'path': str(parent.D/'main')}}
observed = {label: 'native' if label.startswith('plain-normal-') or label.endswith('native-unwind') else 'r2-phase'
            for label in order.ORDER}
ordered = order.order(original, product, parent.EH, observed)
plan = {'schema': 'uwvm-current-R3c-EH-P0-order-v1', 'formal_acceptance': False,
        'temperature_policy': 'observation_only', 'actual_P0_plain_passed': False,
        'actual_HW_passed': False, 'actual_VTune_passed': False, 'product': product,
        'original_parent': {'sha256': order.PARENT_SHA}, 'source_helper': {'sha256': m.ORDER_SHA},
        'cold_receipts': {k: {'sha256': v} for k, v in order.PINS.items()},
        'commands': {name: {'path': name, 'sha256': 'pinned'} for name in ordered}}
contents = {name: json.dumps(rows).encode() for name, rows in ordered.items()}


class FakePath:
    def __init__(self, key):
        self.key = str(key)
    def read_bytes(self):
        return contents[self.key]
    def read_text(self, **kwargs):
        return text[0]


real_path = m.Path
m.Path = FakePath
fake_parent = types.SimpleNamespace(**dict(parent.__dict__, parent_product=lambda: (prefix, ['@fixed-rsp'], product),
                                         pin=lambda path: plan['commands'][path]))
try:
    check(m.validate_order(plan, order, fake_parent) == ordered)
    for key, value in (('schema', 'old-r9'), ('formal_acceptance', True),
                       ('temperature_policy', 'reject90'), ('actual_HW_passed', True),
                       ('actual_P0_plain_passed', True), ('actual_VTune_passed', True)):
        bad = copy.deepcopy(plan)
        bad[key] = value
        rejects(lambda: m.validate_order(bad, order, fake_parent))
    for key in ('product', 'original_parent', 'source_helper', 'cold_receipts'):
        bad = copy.deepcopy(plan)
        bad[key] = {}
        rejects(lambda: m.validate_order(bad, order, fake_parent))
    for family, mutations in (('plain', ('missing', 'extra', 'cpu', 'trace', 'dispatch', 'cache', 'env', 'order', 'expected')),
                              ('pure_hw', ('missing', 'extra', 'cpu', 'trace', 'env'))):
        for mutation in mutations:
            bad = copy.deepcopy(ordered)
            rows = bad[family]
            if mutation == 'missing':
                rows.pop()
            elif mutation == 'extra':
                rows.append(copy.deepcopy(rows[0]))
            elif mutation == 'cpu':
                rows[0]['argv'][2] = '16'
            elif mutation == 'trace':
                rows[0]['argv'][rows[0]['argv'].index('-Rllvm-call-stack')+1] = 'lazy'
            elif mutation == 'dispatch':
                rows[0]['cold_observed_pending_plan'] = 'r2-phase'
            elif mutation == 'cache':
                rows[0]['argv'][rows[0]['argv'].index('-Rllvm-cache-path')+1] = 'enable'
            elif mutation == 'env':
                rows[0]['environment_delta']['UWVM_TEST_CPUSET'] = '0-31'
            elif mutation == 'order':
                rows.reverse()
            else:
                rows[0]['expected_self_check']['catches'] += 1
            contents.update({name: json.dumps(value).encode() for name, value in bad.items()})
            rejects(lambda: m.validate_order(plan, order, fake_parent))
    contents.update({name: json.dumps(value).encode() for name, value in ordered.items()})
    text = ['']
    for item in ordered['plain'][:12]:
        good = ('\x1b[32m[llvm-jit-full] owning-source=yes pending-plan='+item['cold_observed_pending_plan']+
                ' object-cache=disabled body-fallback=no\x1b[0m\n'
                'Total WASM execution time: 0.050000001s.\nTotal process time: 0.075000002s.\n')
        text[0] = good
        receipt = m.semantic_receipt(item, 'fake-log', 0)
        check(receipt['passed'] and receipt['guest_execution_ns'] == 50000001 and
              receipt['whole_process_reported_ns'] == 75000002 and receipt['observed_pending_plan'] == item['cold_observed_pending_plan'])
        for code in (1, -9, None, True):
            check(not m.semantic_receipt(item, 'fake-log', code)['passed'])
        for replacement in (good.replace('owning-source=yes', 'owning-source=no'),
                            good.replace('object-cache=disabled', 'object-cache=hit'),
                            good.replace('body-fallback=no', 'body-fallback=yes'),
                            good.replace('pending-plan='+item['cold_observed_pending_plan'], 'pending-plan=unknown'),
                            good.replace('pending-plan='+item['cold_observed_pending_plan'],
                                         'pending-plan='+('native' if item['cold_observed_pending_plan']=='r2-phase' else 'r2-phase')),
                            good+good, good.replace('0.050000001', '0'),
                            good.replace('0.050000001', '0.0000000001'),
                            good.replace('0.075000002', '0.025000001'),
                            good.replace('Total WASM execution time', 'Missing timer')):
            text[0] = replacement
            check(not m.semantic_receipt(item, 'fake-log', 0)['passed'])
        text[0] = good+'[gc-managed] allocations=0 collections=0\n'
        check(m.semantic_receipt(item, 'fake-log', 0)['passed'])  # EH is not a GC qualification contract.
        text[0] = good.replace('body-fallback=no', 'body-fallback=no module=0 actual-epoch=17')+'[llvm-jit-full] optimize-end module="fixed"\n'
        qualified = m.semantic_receipt(item, 'fake-log', 0)
        check(qualified['passed'] and qualified['reported_module_id'] == 0 and qualified['reported_epoch'] == 17)
        for suffix in (' module=0', ' unknown=1', ' module=-1 actual-epoch=17'):
            text[0] = good.replace('body-fallback=no', 'body-fallback=no'+suffix)
            check(not m.semantic_receipt(item, 'fake-log', 0)['passed'])
finally:
    m.Path = real_path

check(m.R5_SHA == 'd96e7eb2795d020ca5bf29800500b98b947db901eb7bf262eff1130da7b863cb')
check(hashlib.sha256((HERE/'run_current_host_pcore_hw_counting.py').read_bytes()).hexdigest() == m.HOST_SHA)
check(hashlib.sha256((HERE/'run_current_pcore_hw_counting.py').read_bytes()).hexdigest() == m.HW_SHA)
check(hashlib.sha256((HERE/'run_current_pcore_diagnostic.py').read_bytes()).hexdigest() == m.BASE_SHA)
check(hashlib.sha256((HERE/'run_current_general_gc.py').read_bytes()).hexdigest() == m.SAMPLER_SHA)
check(m.ORDER_PLAN_SHA == '700f5c02679e8c922abeb05d70ab9a5dd6b1492ac478150bf7041416d5834992')
tree = ast.parse(source.read_bytes())
check(not any(isinstance(n, ast.FunctionDef) and n.name in ('spawn_stopped', 'plain_sample', 'check', 'measure') for n in ast.walk(tree)))
# Optional real prepared order bytes: read-only metadata, no current remote
# source/object/proc validation and no guest execution is implied by this check.
if len(sys.argv) == 2:
    actual = Path(sys.argv[1])
    check(hashlib.sha256((actual/'plan.json').read_bytes()).hexdigest() == m.ORDER_PLAN_SHA)
    plan_actual = json.loads((actual/'plan.json').read_bytes())
    contents_actual = {}
    for name, pin in plan_actual['commands'].items():
        raw = (actual/(name+'-commands.json')).read_bytes()
        check(len(raw) == pin['bytes'] and hashlib.sha256(raw).hexdigest() == pin['sha256'])
        contents_actual[pin['path']] = raw
    old_contents = contents
    contents = contents_actual
    first = json.loads(contents_actual[plan_actual['commands']['plain']['path']])[0]
    actual_prefix = ['env', 'LD_LIBRARY_PATH='+first['environment_delta']['LD_LIBRARY_PATH'], 'actual-clang']
    actual_parent = types.SimpleNamespace(**dict(parent.__dict__,
        parent_product=lambda: (actual_prefix, ['@fixed-rsp'], plan_actual['product']),
        pin=lambda path: next(pin for pin in plan_actual['commands'].values() if pin['path'] == path)))
    m.Path = FakePath
    try:
        actual_order = m.validate_order(plan_actual, order, actual_parent)
        check(len(actual_order['plain']) == 16 and len(actual_order['pure_hw']) == 4 and len(actual_order['vtune']) == 3)
    finally:
        m.Path = real_path
        contents = old_contents
print('PASS pure EH P0 measurement contracts', checks, 'native_executed=false')
