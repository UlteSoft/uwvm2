#!/usr/bin/env python3
"""Independent native-component accounting/payload oracle; never runs a guest."""
import argparse, copy, json, pathlib

MASK = (1 << 32) - 1
ROOTS = 1024
SEED = 123456789

def require(condition, reason):
    if not condition:
        raise ValueError(reason)

def jump(state, count):
    # Affine exponentiation skips unused LCG values; the C++ fixture instead
    # allocates every value. No production checksum or collector is reused.
    multiplier, increment = 1, 0
    base_multiplier, base_increment = 1664525, 1013904223
    while count:
        if count & 1:
            multiplier, increment = (base_multiplier * multiplier) & MASK, (base_multiplier * increment + base_increment) & MASK
        base_multiplier, base_increment = (base_multiplier * base_multiplier) & MASK, (base_multiplier * base_increment + base_increment) & MASK
        count >>= 1
    return (multiplier * state + increment) & MASK

def expected(allocations, interval, duplicates):
    require(16384 <= allocations <= 512000000 and ROOTS <= interval <= 1000000 and allocations % interval == 0 and 0 <= duplicates <= ROOTS, 'Unsupported bounded component parameters')
    state, checksum, last_checksum = SEED, 0, 0
    for _ in range(allocations // interval):
        state = jump(state, interval - ROOTS)
        last_checksum = 0
        for _ in range(ROOTS):
            state = (state * 1664525 + 1013904223) & MASK
            last_checksum += state
        checksum += last_checksum
    return {'allocations': allocations, 'root_count': ROOTS, 'duplicate_roots': duplicates,
            'collect_every': interval, 'collections': allocations // interval + 1,
            'reclaimed': allocations, 'final_reclaimed': ROOTS, 'remaining': 0,
            'actual_compact_reads': (allocations // interval) * ROOTS,
            'seed': SEED, 'last_lcg': state, 'last_root_checksum': last_checksum,
            'readback_checksum': checksum}

def check(record, wanted):
    for key, value in wanted.items():
        require(type(record.get(key)) is int and record[key] == value, 'Independent oracle mismatch: ' + key)
    for key in ('allocation_ns', 'collection_ns', 'readback_ns', 'teardown_ns', 'component_process_ns'):
        require(type(record.get(key)) is int and record[key] >= 0, 'Missing/invalid measured region: ' + key)
    require(sum(record[key] for key in ('allocation_ns', 'collection_ns', 'readback_ns', 'teardown_ns')) <= record['component_process_ns'], 'Overlapping/impossible region accounting')
    require(type(record.get('compact_collection_directory_enabled')) is int and record['compact_collection_directory_enabled'] in (0, 1), 'Missing actual directory configuration')
    require(record.get('actual_compact_admission_proved') is True and record.get('collector_excludes_admission') is True and
            record.get('native_component_only') is True and record.get('automatic_gc') is False and record.get('vm_qualified') is False,
            'Component provenance/boundary flags missing')

def self_test():
    n, interval = 16384, 4096
    roots = [0] * ROOTS
    state, checksum, last = SEED, 0, 0
    for i in range(n):
        state = (state * 1664525 + 1013904223) & MASK
        roots[i % ROOTS] = state
        if (i + 1) % interval == 0:
            last = sum(roots)
            checksum += last
    for duplicates in (0, ROOTS):
        result = expected(n, interval, duplicates)
        require(result['last_lcg'] == state and result['last_root_checksum'] == last and result['readback_checksum'] == checksum, 'Affine/naive oracle disagreement')
    synthetic = dict(expected(n, interval, 0), allocation_ns=10, collection_ns=20, readback_ns=30,
                     teardown_ns=1, component_process_ns=100, compact_collection_directory_enabled=0,
                     actual_compact_admission_proved=True, collector_excludes_admission=True,
                     native_component_only=True, automatic_gc=False, vm_qualified=False)
    check(synthetic, expected(n, interval, 0))
    rejected = []
    for key, bad in [('root_count', 1023), ('readback_checksum', checksum ^ 1), ('remaining', True),
                     ('actual_compact_admission_proved', False), ('component_process_ns', 1),
                     ('compact_collection_directory_enabled', True)]:
        altered = copy.deepcopy(synthetic)
        altered[key] = bad
        try: check(altered, expected(n, interval, 0))
        except ValueError: rejected.append(key)
        else: raise AssertionError(key)
    print(json.dumps({'pure_math_oracle_passed': True, 'native_execution_performed': False,
                      'synthetic_invalid_records_rejected': rejected,
                      'guest_build_perf_SSH_or_Linux_guard_execution': False}))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--allocations', type=int)
    parser.add_argument('--collect-every', type=int)
    parser.add_argument('--duplicates', type=int)
    parser.add_argument('--record', type=pathlib.Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    require(all(value is not None for value in (args.allocations, args.collect_every, args.duplicates)), 'Supply actual component parameters')
    wanted = expected(args.allocations, args.collect_every, args.duplicates)
    if args.record:
        prefix = 'GC_COMPACT_RING '
        lines = [line[len(prefix):] for line in args.record.read_text().splitlines() if line.startswith(prefix)]
        require(len(lines) == 1, 'Expected one complete actual component record')
        check(json.loads(lines[0]), wanted)
        print(json.dumps({'component_record_matches_independent_oracle': True, 'vm_qualified': False,
                          'record': str(args.record), 'expected': wanted}, indent=2))
    else:
        print(json.dumps({'expected_only': True, 'native_execution_performed': False, 'expected': wanted}, indent=2))

if __name__ == '__main__':
    main()
