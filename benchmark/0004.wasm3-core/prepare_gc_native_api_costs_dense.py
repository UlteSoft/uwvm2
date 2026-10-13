#!/usr/bin/env python3
"""Pure-source native GC dense/sparse API oracle/argv plan; never launches any native tool.

Use separately bound fresh component binaries in the keeper's existing scope.
No binary/source/provider/PMU qualification can be inferred from this plan.
"""
import argparse
import hashlib
import json
from pathlib import Path

PHASES = ('allocate-numeric', 'allocate-reference', 'mutate-numeric',
          'mutate-reference', 'trace64', 'trace65', 'trace-dense64', 'trace-dense65')
MASK = 0xFFFFFFFF
ROOTS = 1024
BATCH = 4096


def require(condition, message):
    if not condition:
        raise ValueError(message)


def oracle(phase, units):
    require(phase in PHASES and type(units) is int, 'Unknown phase/integral units')
    trace = phase.startswith('trace')
    allocate = phase.startswith('allocate-')
    dense = phase.startswith('trace-dense')
    require(1 <= units <= 256 if trace else
            4096 <= units <= (1_000_000 if allocate else 16_000_000), 'Units outside bounded native contract')
    ring = [0] * ROOTS
    total = 0
    state = 123456789
    if phase == 'mutate-reference':
        for index in range(units):
            state = (state * 1664525 + 1013904223) & MASK
            target = (state >> 10) % ROOTS
            ring[index % ROOTS] = target
            total = (total + target) & MASK
    else:
        for index in range(units * BATCH if trace else units):
            state = (state * 1664525 + 1013904223) & MASK
            slot = index % ROOTS
            if phase == 'mutate-numeric':
                total = (total + ring[slot]) & MASK
            payload = state % (1 << 31) if dense else state
            ring[slot] = payload
            total = (total + payload) & MASK
    allocations = units * BATCH if trace else units if allocate else ROOTS
    return dict(step_checksum_u32=total, root_checksum_u32=sum(ring) & MASK,
                allocations=allocations, timed_collections=units if trace else 0,
                timed_reclaimed=allocations - ROOTS if trace else 0,
                qualification_reclaimed=ROOTS if trace else allocations,
                field_count=int(phase.rsplit('dense', 1)[1]) if dense else int(phase[5:]) if trace else 2 if phase.endswith('reference') else 1,
                root_count=ROOTS, dense_reference_layout=dense,
                typed_reference_fields_per_trace_object=(int(phase.rsplit('dense',1)[1]) if dense else 1) if trace else 0,
                heap_self_edges_per_trace_object=(int(phase.rsplit('dense',1)[1])-1 if dense else 1) if trace else 0,
                native_or_vm_executed=False)


def plan(binary, profile):
    require(profile in ('default', 'C', 'D'), 'Unknown exact macro profile')
    require(binary.is_absolute(), 'Planned binary must be an absolute supervisor-owned path')
    phases = {}
    for phase in PHASES:
        cold = 1 if phase.startswith('trace') else 4096
        low, high = (4, 64) if phase.startswith('trace') else (16_384, 262_144) if phase.startswith('allocate') else (32_768, 4_000_000)
        cells = []
        for purpose, units in (('cold', cold), ('low', low), ('high', high)):
            expected = oracle(phase, units)
            cells.append(dict(purpose=purpose, units=units, expected=expected,
                planned_argv=['taskset', '-c', '0', str(binary), phase, str(units),
                    str(expected['step_checksum_u32']), str(expected['root_checksum_u32'])],
                actual_receipt=None))
        phases[phase] = cells
    source = Path(__file__).with_name('gc_native_api_costs_dense_20261003.cc')
    return dict(schema='uwvm-gc-native-api-costs-dense-plan-v2', source_only=True,
        execute_ready=False, native_or_vm_executed=False, performance_accepted=False,
        profile=profile, planned_binary=str(binary), actual_binary_hash=None,
        actual_cpp_provider_tuple=None, actual_dependency_closure=None,
        component_source=dict(path=str(source), bytes=source.stat().st_size,
            sha256=hashlib.sha256(source.read_bytes()).hexdigest()),
        dense_controls='All 64/65 fields are true reference types; field0 is an authentic i31 nonheap scalar; remaining 63/64 fields are authentic self edges. The 65-field representation retains the original owned-index fallback.',
        reference_performance_status='No native or Wasm data; source-only, no default optimization acceptance.',
        profile_macros={'default':[], 'C':['UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA=1'],
            'D':['UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA=1', 'UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA=1']}[profile],
        actual_macro_witness=None, phases=phases, reversed_order_pairs_minimum=9,
        resource_contract=dict(memory_max=68719476736, memory_swap_max=0,
            cpus_effective=[0,2,4,6,*range(16,32)], compiler_cpus=list(range(16,32)),
            benchmark_cpu=0, sole_keeper='/root/linux_fused_resume'),
        provider_policy='Paired23 Release C++ headers/libc++/libc++abi/libunwind only; driver/link closure must exclude old __2 or duplicate system unwind. No LLVM SDK is linked by this native component.',
        timing_scope='API and status/checksum loop for allocate/mutate; closed admission+collector/status checks for trace. Startup/setup/readback/final reclamation excluded from internal clocks but included in process wall/PMU.',
        pmu_policy='Whole-process PMU includes all untimed setup/allocation/readback/reclamation. Never divide those counts by the internally timed collection interval as an isolated collector counter.',
        acceptance_requires=['actual fresh all-source/header/vendor and paired-provider closure',
            'all eight cold modes plus independent external oracle', 'true reader/exclusive protects_shared and exact total reclamation',
            'artifact hashes/macro witness before/after', 'quiet P0 plain ABBA+in-window frequency/noise/UID/TID/cgroup/OOM/throttling',
            'separate installed-official CLI HW VTune hotspots/uarch-exploration',
            'generated assembly before locking/authentication conclusions'],
        vm_qualified=False, same_wasm_comparison=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--profile', choices=('default', 'C', 'D'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'Refusing to replace a frozen plan')
    result = plan(args.binary, args.profile)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')


if __name__ == '__main__':
    main()
