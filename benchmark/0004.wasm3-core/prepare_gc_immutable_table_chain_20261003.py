#!/usr/bin/env python3
"""Pure-data oracle and keeper plan; no native tool is compiled or launched."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOTS = 1024
MASK = 0xFFFFFFFF
SEED, MULTIPLIER, INCREMENT = 123456789, 1664525, 1013904223
MODES = ('collect4096', 'deferred-collect')
ORIGINAL_WASM = {
    2_000_000: dict(state_u32=750095765,
        wasm_sha256='f1b1637cf4b7c1380ba6320065cf534818c6c154d8eabe63a7b2eafd8013c316'),
    16_000_000: dict(state_u32=493211925,
        wasm_sha256='66874f9a4a0a5977b4ace1cc702f949c6c2627311dce0bcbe87c818c55516e3e'),
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def endpoint(count):
    require(type(count) is int and count >= 0, 'Endpoint requires a nonnegative integer')
    multiplier, increment = 1, 0
    power_multiplier, power_increment = MULTIPLIER, INCREMENT
    while count:
        if count & 1:
            multiplier = multiplier * power_multiplier & MASK
            increment = (increment * power_multiplier + power_increment) & MASK
        power_increment = (power_multiplier + 1) * power_increment & MASK
        power_multiplier = power_multiplier * power_multiplier & MASK
        count >>= 1
    return (multiplier * SEED + increment) & MASK


def oracle(count, mode):
    require(type(count) is int and 1024 <= count <= 16_000_000 and mode in MODES,
            'Outside the bounded native component contract')
    roots = [endpoint(count - ((slot + ROOTS - 1) % ROOTS)) for slot in range(ROOTS)]
    collections = count // 4096 if mode == 'collect4096' else 0
    timed_reclaimed = collections * 4096 - ROOTS if collections else 0
    return dict(state_u32=endpoint(count), root_checksum_u32=sum(roots) & MASK,
        root_slot_0_u32=roots[0], root_slot_1_u32=roots[1], root_slots=ROOTS,
        allocations=count, timed_collections=collections,
        timed_reclaimed=timed_reclaimed, qualification_reclaimed=count - timed_reclaimed,
        qualification_collections=2, native_executed=False, vm_qualified=False,
        original_wasm_identity=ORIGINAL_WASM.get(count),
        original_wasm_hash_verified_by_this_plan=False)


def pure_controls():
    # Direct scalar loop plus the DECREASING remaining index verifies the
    # affine oracle and all 1024 final slot positions independently. This is
    # data arithmetic, not a fake native table or permission/GC witness.
    checks = []
    for count in (1024, 1025, 4096, 5000, 16384):
        roots, state = [0] * ROOTS, SEED
        for remaining in range(count, 0, -1):
            state = (state * MULTIPLIER + INCREMENT) & MASK
            roots[remaining & (ROOTS - 1)] = state
        exact = [endpoint(count - ((slot + ROOTS - 1) % ROOTS)) for slot in range(ROOTS)]
        require(roots == exact and state == endpoint(count), 'Original remaining-index oracle differs')
        checks.append(dict(count=count, compared_slots=ROOTS, pure_scalar_equal=True))
    original_path = Path(__file__).with_name('generate.py')
    spec = importlib.util.spec_from_file_location('original_ring_generator_20261003', original_path)
    require(spec is not None and spec.loader is not None, 'Missing original fixture generator')
    original = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(original)
    for count, record in ORIGINAL_WASM.items():
        require(endpoint(count) == record['state_u32'] == original.expected_for_case('gc-allocation-ring', count),
                'New data plan changed the ORIGINAL Wasm expectation')
    return dict(scalar_controls=checks, original_expected_for_case_matches=True,
        original_generator_sha256=hashlib.sha256(original_path.read_bytes()).hexdigest(),
        wasm_parser_executed=False, cpp_compiled=False, native_executed=False)


def plan(binary):
    require(binary.is_absolute(), 'Planned binary must be an absolute keeper-owned path')
    cases = []
    for count in (1024, 4096, 16384, 262144, 2_000_000, 16_000_000):
        for mode in MODES:
            expected = oracle(count, mode)
            cases.append(dict(count=count, mode=mode, expected=expected,
                planned_argv=['taskset', '-c', '0', str(binary), mode, str(count),
                    str(expected['state_u32']), str(expected['root_checksum_u32'])], actual_receipt=None))
    repository = Path(__file__).resolve().parents[2]
    paths = (
        'benchmark/0004.wasm3-core/gc_immutable_table_chain_20261003.cc',
        'benchmark/0004.wasm3-core/gc_reference_hot_path_design_20261003.md',
        'benchmark/0004.wasm3-core/generate.py',
        'src/uwvm2/uwvm/runtime/storage/gc_object.h',
        'src/uwvm2/uwvm/runtime/storage/wasm_module.h',
        'src/uwvm2/uwvm/runtime/storage/gc_static_roots.h',
    )
    sources = []
    for relative in paths:
        data = (repository / relative).read_bytes()
        sources.append(dict(path=relative, size=len(data), sha256=hashlib.sha256(data).hexdigest()))
    return dict(family='gc-immutable-table-chain-20261003', status='source-only',
        pure_controls=pure_controls(), source_records=sources, cases=cases,
        complete_header_closure=False, execute_ready=False, provider_identity=None,
        product_identity=None, actual_assembly=None, actual_pmu=None, actual_vtune=None,
        native_executed=False, vm_qualified=False, performance_accepted=False,
        resource_contract=dict(native_owner='/root/linux_fused_resume', memory_max_bytes=64 * 1024**3,
            swap_max_bytes=0, build_cpus=list(range(16, 32)), performance_cpus=[0],
            actual_cgroup_receipt_required=True, native_macos_allowed=False),
        timer_scope='chain_ns includes actual leaf operations, loop arithmetic/checks/checksum and timing checks; subtracts explicit closed-cohort root census/copy/collection intervals. Whole-process wall and PMU include setup, final table readback and untimed qualification. No ratio divides whole-process counters by only chain_ns.',
        exact_chain=['LCG uint32 state', 'remaining & 1023 BEFORE decrement',
            'uwvm2_gc_struct_new immutable i32', 'real table family/bounds/slot codec/table-owner retain and store',
            'real table family/bounds/slot codec/caller retain and load',
            'uwvm2_gc_reference_type_matches defined nonnullable type0', 'checked full-carrier struct_get'],
        boundary='Storage-leaf component ONLY. Actual LLVM emitter/table bridges/debug RT guard, cast/get witness fusion, guest polls/TLS/frame-map roots/native_unwind are not executed. No RT stub or fabricated initializer publication is used.',
        acceptance_requires=['all cold modes default and singleCAS on from one coherent fresh source/provider closure',
            'actual module/table/owner/type/1024 slots and actual static root visitor under genuine exclusive lease',
            'original 2M/16M state identities, independently checked scalar slots and exact reclamation/stale rejection',
            'same-codegen P0 unprofiled ABBA/frequency/UID/TID/cgroup plus separate actual HW VTune/PMU coverage',
            'actual full-JIT SAME-WASM original byte hashes before claiming a product improvement'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'Refusing to replace a frozen plan')
    result = plan(args.binary)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')


if __name__ == '__main__':
    main()
