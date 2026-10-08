#!/usr/bin/env python3
"""Pure Python contract checks only; no native process and no remote cold PASS."""
import ast
from decimal import Decimal
from pathlib import Path
import hashlib
import types

SOURCE = Path(__file__).with_name('prepare_general_gc_R5_nativeTLS_long_cold.py')
ast.parse(SOURCE.read_text())
ns = types.ModuleType('_r5_long_recipe_contract')
exec(compile(SOURCE.read_text(), str(SOURCE), 'exec'), ns.__dict__)
checks = 0


def check(ok):
    global checks
    if not ok:
        raise AssertionError('contract check failed')
    checks += 1


def rejects(callback):
    global checks
    try:
        callback()
    except (RuntimeError, KeyError):
        checks += 1
    else:
        raise AssertionError('invalid contract was accepted')


cmd = ns.commands()
check(len(cmd) == 34)
check(len({label for label, _ in cmd}) == 34)
check(cmd[0][0] == 'input-before' and cmd[-1][0] == 'input-after')
for label, argv in cmd[1:-1]:
    check(argv[:3] == ['env', '-u', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT'])
    check(argv[argv.index('-Rllvm-cache-path') + 1] == 'disable')
    check(argv[argv.index('-Rclog') + 1] == 'err' and '--log-verbose' in argv)
    check(argv[5] == str(ns.PRODUCT))
    check(label.split('-ordinary-')[1] == argv[argv.index('-Rllvm-call-stack') + 1])
    check(argv[-1].endswith(('.wasm')) and str(ns.OLD / 'fixtures') in argv[-1])

NATIVE = '[llvm-jit-full] owning-source=yes pending-plan=native object-cache=disabled body-fallback=no module=0 actual-epoch=2\n'
TIMERS = 'Total WASM execution time: 0.123456789s.\nTotal process time: 0.143456789s.\n'
ALLOC = '[gc-managed] allocations=2000000 attempts=488 collections=488 reclaimed=1997823 roots_requested=1 disabled=0 reason=0\n'
MUTATE = '[gc-managed] allocations=1024 attempts=0 collections=0 reclaimed=0 roots_requested=1 disabled=0 reason=2\n'
a = ns.parse_log(NATIVE + ALLOC + TIMERS, {'guest_planned_allocations': 2000000}, 'allocate')
check(a['collector_qualified'] is True and a['field_lookup_qualified'] is False)
check(a['internal_timers']['wasm_execution_ns'] == 123456789)
check(a['internal_timers']['reported_process_ns'] == 143456789)
m = ns.parse_log(NATIVE + MUTATE + TIMERS, {'guest_planned_allocations': 1024}, 'mutate')
check(m['collector_qualified'] is False and m['field_lookup_qualified'] is True)
for old, new in (('disabled=0', 'disabled=1'), ('reason=0', 'reason=2'),
                 ('roots_requested=1', 'roots_requested=0'),
                 ('reclaimed=1997823', 'reclaimed=0'),
                 ('collections=488', 'collections=0'),
                 ('attempts=488', 'attempts=0'), ('allocations=2000000', 'allocations=1')):
    rejects(lambda old=old, new=new: ns.parse_log(NATIVE + ALLOC.replace(old, new) + TIMERS,
                                                 {'guest_planned_allocations': 2000000}, 'allocate'))
rejects(lambda: ns.parse_log(NATIVE + MUTATE.replace('reason=2', 'reason=0') + TIMERS,
                            {'guest_planned_allocations': 1024}, 'mutate'))
rejects(lambda: ns.parse_log(NATIVE + MUTATE.replace('attempts=0', 'attempts=1') + TIMERS,
                            {'guest_planned_allocations': 1024}, 'mutate'))
rejects(lambda: ns.parse_log(ALLOC + TIMERS, {'guest_planned_allocations': 2000000}, 'allocate'))
rejects(lambda: ns.parse_log(NATIVE.replace('body-fallback=no', 'body-fallback=yes') + ALLOC + TIMERS,
                            {'guest_planned_allocations': 2000000}, 'allocate'))
rejects(lambda: ns.parse_log(NATIVE + ALLOC + ALLOC + TIMERS,
                            {'guest_planned_allocations': 2000000}, 'allocate'))
rejects(lambda: ns.parse_log(NATIVE + ALLOC + TIMERS.replace('0.143456789', '0.100000000'),
                            {'guest_planned_allocations': 2000000}, 'allocate'))
rejects(lambda: ns.parse_log(NATIVE + ALLOC + TIMERS.replace('0.123456789', '0.1234567891'),
                            {'guest_planned_allocations': 2000000}, 'allocate'))
check(ns.SID.endswith('a97ff26b2da9dc6f227201bf7c36a73a371ee2885b710a188111f3d41472734a'))
check(ns.PRODUCT_SHA == '4aba5943567bbd216eb8e00382a6fee6ef5765b6d02f056f02b310abe0879632')
check('old_S6e_product_used_for_R5_qualification' in SOURCE.read_text())
check('for suffix in (\'validate\', \'roundtrip\', \'wasmtime-run\', \'wasmtime-start\')' in SOURCE.read_text())
print(f'pure Python R5 long recipe contract: {checks} checks passed; native cold/performance pending')
