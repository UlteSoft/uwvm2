#!/usr/bin/env python3
"""Generate checked Core 3 workloads; never run a VM or claim a benchmark.

Run generation/assembly/validation under the same Linux resource controls as
the eventual measurement. The independent oracle uses modular affine algebra;
it does not invoke, emulate, or import an uwvm allocator or collector.
"""
import argparse
import hashlib
import json
from pathlib import Path

SEED = 123456789
MASK = (1 << 32) - 1
MULTIPLIER = 1664525
INCREMENT = 1013904223
KINDS = (
    'newborn32', 'older32', 'mutable32', 'packed-array', 'cyclic-graph',
    'reference-array', 'exception-reference', 'memory64',
)


def compose(outer, inner):
    # (s,z) -> (a*s+c, z+u*s+v), all in Z/(2^32). The second
    # coordinate is a checksum, independent of the guest's root accesses.
    a, c, u, v = outer
    b, d, x, y = inner
    return (a*b & MASK, (a*d+c) & MASK,
            (x+u*b) & MASK, (y+u*d+v) & MASK)


def sequence(count):
    result = (1, 0, 0, 0)
    power = (MULTIPLIER, INCREMENT, MULTIPLIER, INCREMENT)
    while count:
        if count & 1:
            result = compose(power, result)
        power = compose(power, power)
        count >>= 1
    a, c, u, v = result
    return (a*SEED+c) & MASK, (u*SEED+v) & MASK


def fixture(kind, iterations, roots):
    if kind not in KINDS or iterations < 1 or iterations > (1 << 63)-1:
        raise ValueError('explicit kind and positive signed i64 step count required')
    if roots < 1 or roots > 65536 or roots & (roots-1):
        raise ValueError('root count must be a power of two in [1,65536]')
    endpoint, unused = sequence(iterations)
    del unused
    offset = roots//2
    distance = offset or roots
    seed_reads = min(iterations, distance)
    old_sum = (seed_reads*SEED + sequence(iterations-seed_reads)[1]) & MASK
    mask = roots-1
    declarations = '(type $box (struct (field i32)))'
    extra_locals = ''
    seed_object = 'i32.const 123456789 struct.new $box'
    new_object = 'local.get $state struct.new $box'
    read = 'ref.cast (ref $box) struct.get $box 0'
    mutating = ''
    allocations = 1
    features = ['gc', 'function-references', 'reference-types']
    if kind == 'mutable32':
        declarations = '(type $box (struct (field (mut i32))))'
        new_object = 'i32.const 0 struct.new $box local.tee $boxref'
        extra_locals = '(local $boxref (ref null $box))'
        mutating = '''local.get $boxref local.get $state struct.set $box 0'''
    elif kind == 'packed-array':
        declarations = '(type $box (array (mut i8)))'
        seed_object = 'i32.const 123456789 i32.const 64 array.new $box'
        new_object = 'local.get $state i32.const 64 array.new $box'
        read = 'ref.cast (ref $box) i32.const 63 array.get_u $box'
        # Keep full-width values in a second numeric field of a real wrapper,
        # so the independent checksum still observes the exact LCG sequence.
        declarations += '\n(type $wrapper (struct (field (ref $box)) (field i32)))'
        seed_object += ' i32.const 123456789 struct.new $wrapper'
        new_object += ' local.get $state struct.new $wrapper'
        read = '''ref.cast (ref $wrapper) local.tee $wrapperref
            struct.get $wrapper 0 i32.const 63 array.get_u $box
            local.get $wrapperref struct.get $wrapper 1 i32.const 255 i32.and
            i32.ne if unreachable end
            local.get $wrapperref struct.get $wrapper 1'''
        extra_locals = '(local $wrapperref (ref null $wrapper))'
        allocations = 2
    elif kind in ('cyclic-graph', 'reference-array'):
        declarations = '''(rec (type $node
            (struct (field (mut (ref null $node))) (field i32))))'''
        extra_locals = '(local $a (ref null $node)) (local $b (ref null $node))'
        def pair(value):
            return f'''ref.null $node {value} struct.new $node local.set $a
                ref.null $node {value} struct.new $node local.set $b
                local.get $a local.get $b struct.set $node 0
                local.get $b local.get $a struct.set $node 0
                local.get $a'''
        seed_object = pair('i32.const 123456789')
        new_object = pair('local.get $state')
        read = '''ref.cast (ref $node) local.tee $a struct.get $node 0
            ref.as_non_null local.tee $b struct.get $node 0 local.get $a
            ref.eq i32.eqz if unreachable end
            local.get $b struct.get $node 1 local.get $a struct.get $node 1
            i32.ne if unreachable end
            local.get $a struct.get $node 1'''
        allocations = 2
        if kind == 'reference-array':
            declarations += '\n(type $refs (array (mut (ref null $node))))'
            seed_object += ''' i32.const 8 array.new $refs local.tee $refsref
                i32.const 7 local.get $b array.set $refs
                local.get $refsref'''
            new_object += ''' i32.const 8 array.new $refs local.tee $refsref
                i32.const 7 local.get $b array.set $refs
                local.get $refsref'''
            # The array.set consumes the tee result. The final local.get
            # restores one initialized non-null array reference for table.set.
            extra_locals += ' (local $refsref (ref null $refs))'
            # Both seed/new arrays store b at slot 7. Observe that mutation
            # directly before the original cycle/value oracle, including seeds.
            read = '''ref.cast (ref $refs) local.tee $refsref
                i32.const 0 array.get $refs local.tee $a struct.get $node 0
                local.get $refsref i32.const 7 array.get $refs
                ref.eq i32.eqz if unreachable end
                local.get $a
                ''' + read
            allocations = 3
    elif kind == 'exception-reference':
        declarations += '''\n(tag $box_tag (param (ref $box)))
            (func $throw_box (param $value i32)
                local.get $value struct.new $box throw $box_tag)'''
        new_object = '''block $caught (result (ref $box))
                try_table (catch $box_tag $caught)
                    local.get $state call $throw_box
                end
                unreachable
            end'''
        features.append('exceptions')
    elif kind == 'memory64':
        declarations = '(memory (export "linear") i64 1 1)'
        operations = '''local.get $remaining i64.const 16383 i64.and
            i64.const 2 i64.shl local.tee $address local.get $state i32.store
            local.get $address i32.load local.get $state i32.ne if unreachable end'''
        source = f'''(module {declarations}
            (func (export "_start") (local $state i32) (local $remaining i64)
                (local $address i64)
                i32.const {SEED} local.set $state
                i64.const {iterations} local.set $remaining
                loop $again
                    local.get $state i32.const {MULTIPLIER} i32.mul
                    i32.const {INCREMENT} i32.add local.set $state
                    {operations}
                    local.get $remaining i64.const 1 i64.sub local.tee $remaining
                    i64.const 0 i64.ne br_if $again
                end
                local.get $state i32.const {endpoint} i32.ne if unreachable end))\n'''
        return source, dict(kind=kind, steps=iterations, roots=0,
                            endpoint=endpoint, checksum=None,
                            aggregate_allocations_per_step=0, setup_allocations=0,
                            throws_per_step=0, runtime_exception_allocations=None,
                            features=['memory64'])
    if kind == 'newborn32':
        # Preserve the original immediate set/get comparison as a separate
        # workload; older32 deliberately reads a previous allocation instead.
        checksum = None
        probe = ''
        readback = f'''local.get $slot table.get $roots {read}
            local.get $state i32.ne if unreachable end'''
        check = ''
    else:
        checksum = old_sum
        probe = f'''local.get $sum
            local.get $slot i32.const {offset} i32.add i32.const {mask} i32.and
            table.get $roots {read} i32.add local.set $sum'''
        readback = ''
        check = f'local.get $sum i32.const {checksum} i32.ne if unreachable end'
    source = f'''(module {declarations}
        (table $roots (export "roots") {roots} {roots} anyref)
        (func (export "_start") (local $state i32) (local $remaining i64)
            (local $slot i32) (local $sum i32) (local $fill i32) {extra_locals}
            ;; Initialize every real root. Setup allocation is accounted separately.
            loop $initialize
                local.get $fill {seed_object} table.set $roots
                local.get $fill i32.const 1 i32.add local.tee $fill
                i32.const {roots} i32.lt_u br_if $initialize
            end
            i32.const {SEED} local.set $state
            i64.const {iterations} local.set $remaining
            loop $again
                local.get $state i32.const {MULTIPLIER} i32.mul
                i32.const {INCREMENT} i32.add local.set $state
                local.get $remaining i32.wrap_i64 i32.const {mask} i32.and local.set $slot
                {probe}
                local.get $slot {new_object} table.set $roots
                {mutating}
                {readback}
                local.get $remaining i64.const 1 i64.sub local.tee $remaining
                i64.const 0 i64.ne br_if $again
            end
            local.get $state i32.const {endpoint} i32.ne if unreachable end
            {check}))\n'''
    return source, dict(kind=kind, steps=iterations, roots=roots,
                        endpoint=endpoint, checksum=checksum,
                        previous_value_distance=None if checksum is None else distance,
                        aggregate_allocations_per_step=allocations,
                        throws_per_step=1 if kind == 'exception-reference' else 0,
                        runtime_exception_allocations=None,
                        setup_allocations=roots*allocations, features=features)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--steps', type=int, default=65536)
    parser.add_argument('--roots', type=int, action='append')
    parser.add_argument('--kind', choices=KINDS, action='append')
    args = parser.parse_args()
    roots = sorted(set(args.roots or (1, 64, 1024, 65536)))
    kinds = list(dict.fromkeys(args.kind or KINDS))
    # Validate all parameters before creating any output. Generation grants no
    # execution, P-core, thermal or memory admission to the eventual caller.
    for kind in kinds:
        for count in roots:
            fixture(kind, args.steps, count)
    args.out.mkdir(parents=True, exist_ok=False)
    cases = []
    for kind in kinds:
        for count in (roots[:1] if kind == 'memory64' else roots):
            source, row = fixture(kind, args.steps, count)
            path = args.out / (kind+'-'+str(count)+'-'+str(args.steps)+'.wat')
            data = source.encode()
            path.write_bytes(data)
            row.update(file=path.name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest(),
                       actual_assembly=None, actual_reference_execution=None,
                       actual_uwvm_execution=None, actual_GC=None, actual_P=None)
            cases.append(row)
    (args.out/'oracles.json').write_text(json.dumps(dict(
        schema='uwvm-core3-survival-workload-SOURCE-v1', cases=cases,
        generated_SOURCE_only=True, performance_qualified=False), indent=2)+'\n')


if __name__ == '__main__':
    main()
