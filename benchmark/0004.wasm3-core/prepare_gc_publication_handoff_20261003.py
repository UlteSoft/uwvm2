#!/usr/bin/env python3
"""Pure source-bound handoff/ASM test plan. Never launches a native tool or VM."""
import argparse
import hashlib
import json
from pathlib import Path

LIMIT = 262_144
PROFILES = {
    'default': [],
    'single-cas': ['UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION=1'],
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def oracle(workers, count):
    require(type(workers) is int and workers in (1, 2, 4), 'workers must be 1, 2 or 4')
    require(type(count) is int and 2 <= count <= LIMIT, 'count outside source-bounded contract')
    checksum = sum(index ^ (0xA5A50000 + worker * 0x10101)
                   for worker in range(workers) for index in range(count))
    require(checksum <= 0xFFFFFFFFFFFFFFFF, 'checksum must fit the actual native u64 accumulator')
    total = workers * count
    return dict(checksum=checksum, actual_allocations=total + 2,
                expected_reclamation=[total - workers, workers + 1, 1],
                final_live_aggregate_objects=0, semantic_root='receiving-store typed reference array only',
                native_or_vm_executed=False)


def source_record(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def plan(binary, profile):
    require(binary.is_absolute(), 'binary must be an absolute keeper-owned output path')
    require(profile in PROFILES, 'unknown exact preprocessor profile')
    here = Path(__file__).resolve().parent
    repository = here.parents[1]
    production = repository / 'src/uwvm2/uwvm/runtime/storage/gc_object.h'
    cache = repository / 'src/uwvm2/runtime/llvm_jit_cache/environment.h'
    fixture = repository / 'test/0017.runtime/gc_publication_handoff_20261003.cc'
    cells = []
    for purpose, count in (('cold', 2), ('collision-low', 16_384), ('collision-high', 131_072)):
        for workers in (1, 2, 4):
            for stores in ('same', 'split'):
                cells.append(dict(purpose=purpose, workers=workers, count=count, store_mode=stores,
                    expected=oracle(workers, count), planned_argv=[str(binary), str(workers), str(count), stores],
                    shared_stripes_required=stores == 'split' and workers > 1 and count >= 1024,
                    actual_receipt=None, performance_qualified=False))
    return dict(schema='uwvm-gc-publication-handoff-plan-v1', source_only=True,
        execute_ready=False, native_or_vm_executed=False, vm_qualified=False,
        same_wasm_comparison=False, default_enablement_accepted=False,
        profile=profile, profile_macros=PROFILES[profile], planned_binary=str(binary),
        actual_binary_hash=None, actual_macro_witness=None, actual_cpp_provider_tuple=None,
        actual_dependency_closure=None,
        production_candidate_present='UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION' in production.read_text(),
        source_inputs=[source_record(path) for path in
            (fixture, here / 'gc_publication_codegen_20261003.cc',
             here / 'gc_publication_design_20261003.md', production, cache)],
        cells=cells, scalar_oracle='Independent bounded Python arithmetic, not native timings',
        native_safety=['every actor owns a genuine admission lease',
            'per-lane release/acquire output initialization before native reads',
            'two canonical stores and two real lease-owner lists',
            'foreign payload reads while publications remain concurrent',
            'all token identities distinct, local payloads correct, packed/SIMD bytes exact',
            'one receiving-array root traces actual typed self-cycles',
            'exact stopped-cohort reclamation and local/foreign stale rejection',
            'later allocations cannot reuse any retired token'],
        resource_contract=dict(memory_max=68_719_476_736, memory_swap_max=0,
            sole_native_keeper='/root/linux_fused_resume',
            compiler_cpus=list(range(16, 32)), correctness_cpus=list(range(16, 32)),
            scaling_cpus=[0, 2, 4, 6], largest_planned_objects=4 * 131_072 + 2,
            actual_peak_rss_bytes=None, actual_cgroup_memory_peak_bytes=None,
            mac_native_allowed=False,
            mac_policy='No run before measured Linux peak plus platform overhead and watchdog prove a safe margin below 4 GiB'),
        provider_policy='Paired23 Release headers/libc++/libc++abi/libunwind, exact archive closure; no old __2 reuse or duplicate system unwind',
        correctness_builds=['fresh macro-off/on O3', 'separate ASan+UBSan', 'separate genuine TSan if supported',
            'header and named-module consumers', 'EH/no-EH where provider supports it'],
        assembly=dict(source=str(here / 'gc_publication_codegen_20261003.cc'),
            compiler_arguments=['-O3', '-S'], execute_ready=False,
            inspect=['one publication-list CAS when candidate is enabled',
                'cold token-refill/allocator CAS distinguished from publication CAS',
                'original acquire local membership and all foreign ownership checks retained',
                'macro-off byte/code baseline retained', 'instruction/register pressure and spills'],
            actual_receipt=None),
        performance_scope='Concurrent allocation + scheduling + foreign membership/payload checks; not isolated publication or Wasm execution',
        performance_requirements=['correctness before timings',
            'separate source-identical plain reversed-order runs',
            'real P-core CPU/TID affinity and in-window frequency/noise',
            'actual official VTune CLI hardware hotspots and uarch-exploration',
            'real whole-process clocks/counters separate from internal handoff timer',
            'same-artifact new-syntax Wasm cases and industry cycle-reclamation controls',
            'cross-platform QEMU macro-on/off closure before broad enablement'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--profile', choices=tuple(PROFILES), required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'Refusing to overwrite an existing plan')
    result = plan(args.binary, args.profile)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')


if __name__ == '__main__':
    main()
