"""Separately budgeted memory64 witnesses with guards against 32-bit aliasing.

The high-address module needs at least 4 GiB of guest address space. Do not
silently add it to the ordinary matrix's 4 GiB process budget. Expected bytes
and random-memory state come from integer arithmetic, without running a VM.
"""

HIGH = 1 << 32
PAGE = 65536
FLAGS = ['-WFE-memory64', '-WFE-multi-memory', '-WFE-simd', '-WFE-threads']


def random_endpoint(count):
    cells = [0] * 4096
    state = 123456789
    for _ in range(count):
        state = (state * 1664525 + 1013904223) & 0xffffffff
        cells[((state >> 2) & 16380) >> 2] = state
        state ^= cells[((state >> 10) & 16380) >> 2]
    return state ^ cells[((state >> 2) & 16380) >> 2]


def positive(count=2048):
    checks = []

    def eq(expression, expected, width=32):
        checks.append(f'{expression} i{width}.const {expected} i{width}.ne if unreachable end')

    prefix = f'''(module
      (memory $wide (export "wide") i64 65538 65539 shared)
      (memory $small (export "small") 1 1)
      (data $bytes "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
      (global $base (mut i64) (i64.const 0))
      (func $random (param $count i32) (result i32)
        (local $state i32) (local $remaining i32)
        i32.const 123456789 local.set $state
        local.get $count local.set $remaining
        loop $again
          local.get $state i32.const 1664525 i32.mul i32.const 1013904223 i32.add local.set $state
          global.get $base local.get $state i32.const 2 i32.shr_u i32.const 16380 i32.and i64.extend_i32_u i64.add
          local.get $state i32.store $wide
          global.get $base local.get $state i32.const 10 i32.shr_u i32.const 16380 i32.and i64.extend_i32_u i64.add
          i32.load $wide local.get $state i32.xor local.set $state
          local.get $remaining i32.const 1 i32.sub local.tee $remaining br_if $again
        end
        global.get $base local.get $state i32.const 2 i32.shr_u i32.const 16380 i32.and i64.extend_i32_u i64.add
        i32.load $wide local.get $state i32.xor)
      (func (export "_start") (local $i i64) (local $old i64)
        i64.const 0 i32.const 165 i64.const 65536 memory.fill $wide
        i64.const {HIGH} global.set $base
    '''
    eq(f'i32.const {count} call $random', random_endpoint(count))
    # Every low alias remains a distinct marker, including untouched random
    # slots; both wrongly truncated stores and wrongly truncated loads fail.
    checks.append('''i64.const 0 local.set $i
      loop $guard
        local.get $i i32.load $wide i32.const 0xa5a5a5a5 i32.ne if unreachable end
        local.get $i i64.const 4 i64.add local.tee $i i64.const 16384 i64.lt_u br_if $guard
      end''')
    # Full-width static offset and subword sign/zero extension.
    checks.append(f'i64.const 0x4001 i64.const 0x80706050403020f1 i64.store $wide offset={HIGH} align=1')
    for op, expected, width in [('i32.load8_s', -15, 32), ('i32.load8_u', 241, 32),
                                 ('i64.load8_s', -15, 64), ('i64.load8_u', 241, 64),
                                 ('i32.load16_u', 0x20f1, 32), ('i64.load32_u', 0x403020f1, 64),
                                 ('i64.load', 0x80706050403020f1, 64)]:
        eq(f'i64.const 0x4001 {op} $wide offset={HIGH} align=1', expected, width)
    # IEEE bits survive high-address unaligned loads/stores (including NaN
    # payloads, signed zero and subnormals); these are bit-preservation tests.
    for width, bits in [(32, 0x80000000), (32, 0x7fc12345), (32, 1),
                        (64, 0x8000000000000000), (64, 0x7ff8123456789abc), (64, 1)]:
        checks.append(f'i64.const {HIGH + 0x4021} i{width}.const {bits} f{width}.reinterpret_i{width} f{width}.store $wide align=1')
        eq(f'i64.const {HIGH + 0x4021} f{width}.load $wide align=1 i{width}.reinterpret_f{width}', bits, width)
    eq('i64.const 0x4001 i64.load $wide align=1', 0xa5a5a5a5a5a5a5a5, 64)
    eq('i64.const 0x4021 i64.load $wide align=1', 0xa5a5a5a5a5a5a5a5, 64)
    # A vector straddles the 2^32 boundary. Each byte and both halves must
    # match, so a host SIMD lane/endian mismatch cannot pass by round-trip.
    checks.append(f'i64.const {HIGH - 7} v128.const i8x16 ' + ' '.join(str(i) for i in range(16)) + ' v128.store $wide align=1')
    for lane in range(16):
        eq(f'i64.const {HIGH - 7 + lane} i32.load8_u $wide', lane)
    eq(f'i64.const {HIGH - 7} v128.load $wide align=1 i64x2.extract_lane 0', 0x0706050403020100, 64)
    eq(f'i64.const {HIGH - 7} v128.load $wide align=1 i64x2.extract_lane 1', 0x0f0e0d0c0b0a0908, 64)
    # Bulk operations and mixed address widths use min(i32,i64)=i32 lengths.
    checks.append(f'i64.const {HIGH + 0x4100} i32.const 0 i32.const 16 memory.init $wide $bytes')
    checks.append(f'i64.const {HIGH + 0x4103} i64.const {HIGH + 0x4100} i64.const 13 memory.copy $wide $wide')
    for i, expected in enumerate([0, 1, 2, *range(13)]):
        eq(f'i64.const {HIGH + 0x4100 + i} i32.load8_u $wide', expected)
    checks.append(f'i32.const 128 i64.const {HIGH + 0x4103} i32.const 13 memory.copy $small $wide')
    checks.append(f'i64.const {HIGH + 0x4200} i32.const 128 i32.const 13 memory.copy $wide $small')
    for i in range(13):
        eq(f'i32.const {128 + i} i32.load8_u $small', i)
        eq(f'i64.const {HIGH + 0x4200 + i} i32.load8_u $wide', i)
    checks.append(f'i64.const {HIGH + 0x4301} i32.const 0x5a i64.const 31 memory.fill $wide')
    for i in (0, 15, 30):
        eq(f'i64.const {HIGH + 0x4301 + i} i32.load8_u $wide', 0x5a)
    eq('i64.const 0x4100 i32.load $wide', 0xa5a5a5a5)
    eq('i64.const 0x4200 i32.load $wide', 0xa5a5a5a5)
    eq('i64.const 0x4301 i32.load $wide align=1', 0xa5a5a5a5)
    # High-address atomics check old and new values plus the low alias.
    at = HIGH + 0x4400
    checks.append(f'i64.const {at} i64.const 0x0102030405060708 i64.atomic.store $wide')
    eq(f'i64.const {at} i64.const 3 i64.atomic.rmw.add $wide', 0x0102030405060708, 64)
    eq(f'i64.const {at} i64.atomic.load $wide', 0x010203040506070b, 64)
    eq(f'i64.const {at} i64.const 0x010203040506070b i64.const 99 i64.atomic.rmw.cmpxchg $wide', 0x010203040506070b, 64)
    eq(f'i64.const {at} i64.atomic.load $wide', 99, 64)
    eq(f'i64.const {at} i32.const 100 i64.const 0 memory.atomic.wait32 $wide', 1)
    eq(f'i64.const {at} i32.const 1 memory.atomic.notify $wide', 0)
    eq('i64.const 0x4400 i64.load $wide', 0xa5a5a5a5a5a5a5a5, 64)
    # Growth can legally fail despite available resources. A success checks
    # the new page and retained bytes; exceeding the declared max must fail.
    eq('memory.size $wide', 65538, 64)
    checks.append('i64.const 1 memory.grow $wide local.tee $old i64.const -1 i64.ne if')
    eq('local.get $old', 65538, 64)
    eq('memory.size $wide', 65539, 64)
    eq(f'i64.const {HIGH + 2 * PAGE} i64.load $wide', 0, 64)
    eq(f'i64.const {HIGH + 3 * PAGE - 8} i64.load $wide', 0, 64)
    checks.append(f'i64.const {HIGH + 2 * PAGE} i64.const 0x123456789abcdef i64.store $wide')
    eq(f'i64.const {HIGH + 2 * PAGE} i64.load $wide', 0x123456789abcdef, 64)
    eq(f'i64.const {at} i64.atomic.load $wide', 99, 64)
    checks.append('end')
    eq('i64.const 2 memory.grow $wide', -1, 64)
    checks.append('data.drop $bytes')
    return dict(name='memory64-high-guarded', wat=prefix + '\n'.join(checks) + '))',
                # The emitted guard is already counted once above; it executes
                # 4096 times. Six growth checks run only when growth succeeds.
                checks=sum('if unreachable end' in item for item in checks) + 4095,
                checks_note='Maximum when memory.grow succeeds; six checks are conditional',
                random_iterations=count, expected_trap=None, argv=FLAGS,
                minimum_guest_bytes=65538 * PAGE,
                scope='High random writes with low sentinels; wide offset, scalar/IEEE bits, SIMD boundary, bulk/mixed memories, shared atomics, size/grow')


def cases():
    result = [positive()]
    # Small allocations deliberately make the low 32-bit alias valid. A
    # missing 65th carry bit, truncation or wrapping range check must fail.
    operations = [
        ('load-high-address', HIGH + 64, 'i32.load $wide', 32),
        ('load-high-offset', 64, f'i32.load $wide offset={HIGH}', 32),
        ('load-offset-carry', (1 << 64) - 8, 'i64.load $wide offset=16', 64),
        ('load-width-carry', (1 << 64) - 4, 'i64.load $wide', 64),
        ('vector-high-offset', 64, f'v128.load $wide offset={HIGH}', 128),
        ('vector-width-carry', (1 << 64) - 8, 'v128.load $wide', 128),
        ('atomic-high-address', HIGH + 64, 'i64.atomic.load $wide', 64),
    ]
    for name, address, op, width in operations:
        wat = f'''(module (memory $wide i64 2 2 shared)
          (global $addr (mut i64) (i64.const 0))
          (func $load (export "load") (result {'v128' if width == 128 else 'i' + str(width)}) global.get $addr {op})
          (func (export "_start") i64.const {address} global.set $addr call $load drop unreachable))'''
        result.append(dict(name='memory64-oob-' + name, wat=wat, checks=1,
                           expected_trap='memory access out of bounds', argv=FLAGS,
                           minimum_guest_bytes=2 * PAGE))
    for name, op in [
        ('store-high-address', f'i64.const {HIGH + 64} i64.const 42 i64.store $wide'),
        ('fill-high-address', f'i64.const {HIGH + 64} i32.const 42 i64.const 16 memory.fill $wide'),
        ('copy-high-destination', f'i64.const {HIGH + 64} i64.const 0 i64.const 16 memory.copy $wide $wide'),
        ('copy-high-source', f'i64.const 64 i64.const {HIGH} i64.const 16 memory.copy $wide $wide'),
        ('fill-length-carry', 'i64.const 64 i32.const 42 i64.const -32 memory.fill $wide'),
    ]:
        result.append(dict(name='memory64-oob-' + name,
                           wat=f'(module (memory $wide i64 2 2 shared) (func (export "_start") {op} unreachable))',
                           checks=1, expected_trap='memory access out of bounds', argv=FLAGS,
                           minimum_guest_bytes=2 * PAGE))
    return result
