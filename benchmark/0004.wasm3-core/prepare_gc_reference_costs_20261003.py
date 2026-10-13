#!/usr/bin/env python3
"""Pure-data plans and oracles; never compile or launch native programs."""
import argparse
import hashlib
import json
from pathlib import Path
import random

PHASES = ('match-defined', 'get32', 'set32', 'set-reference')
RELATIONS = ('local', 'foreign-equal', 'foreign-subtype')
ROOTS = 1024
MASK = 0xFFFFFFFF


def require(condition, message):
    if not condition:
        raise ValueError(message)


def initial(index):
    return (index * 0x10101 + 0x13579BD) & MASK


def oracle(phase, units):
    require(phase in PHASES and type(units) is int and 1024 <= units <= 16_000_000,
            'Unknown phase or units outside the native contract')
    state, total = 123456789, 0
    scalars = [initial(index) for index in range(ROOTS)]
    targets = list(range(ROOTS))
    for step in range(units):
        state = (state * 1664525 + 1013904223) & MASK
        slot = (state >> 10) & (ROOTS - 1)
        if phase == 'match-defined':
            total += slot + 1
        elif phase == 'get32':
            total += scalars[slot]
        elif phase == 'set32':
            scalars[slot] = state
            total += state
        else:
            targets[step & (ROOTS - 1)] = slot
            total += slot
        total &= MASK
    kept = len(set(targets))
    return dict(checksum_u32=total, scalar_checksum_u32=sum(scalars) & MASK,
                kept_sources=kept, first_reclaimed=ROOTS - kept,
                final_reclaimed=ROOTS + kept, allocations=2 * ROOTS,
                timed_apis_per_unit=1 if phase in ('match-defined', 'get32') else 2,
                timed_collections=0, qualification_collections=2,
                native_executed=False, vm_qualified=False)


def ancestor(records, index, target_depth):
    while records[index]['depth'] > target_depth:
        node = records[index]
        jump = node['jump']
        index = jump if records[jump]['depth'] >= target_depth else node['parent']
    return index


def local_depth_jump_model(seed, count=128):
    """Inheritance algebra only; not a Core3 parser or native proof.

    Canonical keys already have valid, single-parent identity. A duplicate
    shape may appear at a different module-local index. No payload address or
    collector identity/cache is modeled or authorized by this comparison.
    """
    rng = random.Random(seed)
    interned, canonical = {}, []
    modules = []
    for module in range(2):
        nodes = []
        for index in range(count):
            parent = None if index == 0 or rng.randrange(5) == 0 else rng.randrange(index)
            kind = nodes[parent]['kind'] if parent is not None else rng.randrange(3)
            parent_id = nodes[parent]['id'] if parent is not None else None
            shape = rng.randrange(8)
            key = (kind, parent_id, shape)
            if key not in interned:
                interned[key] = len(canonical)
                canonical.append(dict(kind=kind, parent=parent_id))
            identity = interned[key]
            depth = nodes[parent]['depth'] + 1 if parent is not None else 0
            jump = ancestor(nodes, parent, depth & (depth - 1)) if parent is not None else index
            nodes.append(dict(id=identity, kind=kind, parent=parent, depth=depth, jump=jump))
        modules.append(nodes)
    comparisons = 0
    for source in modules:
        for expected in modules:
            for index, node in enumerate(source):
                for target in expected:
                    cursor = node['id']
                    original = False
                    while cursor is not None:
                        if cursor == target['id']:
                            original = True
                            break
                        cursor = canonical[cursor]['parent']
                    candidate = node['kind'] == target['kind'] and node['depth'] >= target['depth']
                    if candidate:
                        candidate = source[ancestor(source, index, target['depth'])]['id'] == target['id']
                    require(original == candidate, 'Owner-local model disagrees with canonical parent relation')
                    comparisons += 1
    return comparisons


def plan(binary):
    require(binary.is_absolute(), 'Binary must be an absolute supervisor-owned planned path')
    cases = []
    for phase in PHASES:
        for units in (1024, 32_768, 4_000_000):
            expected = oracle(phase, units)
            for relation in RELATIONS:
                cases.append(dict(phase=phase, relation=relation, units=units,
                    expected=expected, planned_argv=['taskset', '-c', '0', str(binary),
                        phase, relation, str(units), str(expected['checksum_u32']),
                        str(expected['scalar_checksum_u32'])], actual_receipt=None))
    repository = Path(__file__).resolve().parents[2]
    paths = (
        'benchmark/0004.wasm3-core/gc_reference_costs_20261003.cc',
        'benchmark/0004.wasm3-core/gc_reference_hot_path_design_20261003.md',
        'src/uwvm2/uwvm/runtime/storage/gc_object.h',
        'src/uwvm2/validation/standard/wasm3/recursive_type_registry.h',
    )
    sources = []
    for relative in paths:
        data = (repository / relative).read_bytes()
        sources.append(dict(path=relative, size=len(data), sha256=hashlib.sha256(data).hexdigest()))
    return dict(family='gc-reference-costs-20261003', status='source-only', cases=cases,
        source_records=sources, complete_header_closure=False, provider_identity=None,
        product_identity=None, execute_ready=False, native_executed=False,
        vm_qualified=False, performance_accepted=False,
        resource_contract=dict(native_owner='/root/linux_fused_resume',
            memory_max_bytes=64 * 1024**3, swap_max_bytes=0,
            actual_cgroup_receipt_required=True, build_cpus=list(range(16, 32)),
            performance_cpus=[0], native_macos_allowed=False),
        timer_scope='One native API for match/get; checked setter plus readback for set. Selection/checksum/assertions included. Setup, final readback and two real collections excluded from internal api_ns, included in process wall/whole-TID PMU.',
        cold_contract='All twelve phase/relation combinations at 1024 units before longer runs; new provider/source/artifact closure. Source-only means neither C++ syntax nor generated code is qualified.',
        relation_contract='Two genuine pinned stores and actual lease lists. Equal canonical IDs use different module indices; proper subtype has a distinct canonical ID and a declared single parent. Local mode still includes the second empty store in the complete cohort.',
        acceptance_requires=[
            'Fresh coherent all-TU macros/header/vendor/provider source and actual ELF receipt',
            'Actual entry lease + protects_shared exclusive; exact roots, reclaimed counts and stale rejection',
            'Paired unprofiled ABBA with observed UID/TID/P0/cgroup, frequency, no OOM/throttling',
            'Official VTune CLI hardware hotspots plus separately recorded uarch coverage',
            'O3 actual code-generation boundary and no synthetic removal of owner/lock checks',
            'No attribution of these component costs to the old immutable allocation ring',
        ])


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
