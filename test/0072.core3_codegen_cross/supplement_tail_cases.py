"""Long Wasm tail transfers with exact spilled FP/SIMD bits and live GC arguments."""


def cases():
    rounds = 1_000_002
    f32 = [0, 0x80000000, 1, 0x80000001, 0x7F7FFFFF, 0xFF7FFFFF,
           0x7F800000, 0xFF800000, 0x7FC12345, 0xFFC54321, 0x3F800001, 0xBF000001]
    f64 = [0, 0x8000000000000000, 1, 0x8000000000000001,
           0x7FEFFFFFFFFFFFFF, 0xFFEFFFFFFFFFFFFF, 0x7FF0000000000000,
           0xFFF0000000000000, 0x7FF8123456789ABC, 0xFFF8ABCDEF012345,
           0x3FF0000000000001, 0xBFE0000000000001]
    types, pushes, checks = [], [], []
    for i, bits in enumerate(f32):
        types.append(f'(param $s{i} f32)')
        pushes.append(f'i32.const 0x{bits:x} f32.reinterpret_i32')
        checks.append(f'local.get $s{i} i32.reinterpret_f32 i32.const 0x{bits:x} i32.ne if unreachable end')
    for i, bits in enumerate(f64):
        types.append(f'(param $d{i} f64)')
        pushes.append(f'i64.const 0x{bits:x} f64.reinterpret_i64')
        checks.append(f'local.get $d{i} i64.reinterpret_f64 i64.const 0x{bits:x} i64.ne if unreachable end')
    for i in range(12):
        value = f'v128.const i32x4 {101+i} {201+i} {301+i} {401+i}'
        types.append(f'(param $v{i} v128)')
        pushes.append(value)
        checks.append(f'local.get $v{i} {value} i32x4.eq i32x4.all_true i32.eqz if unreachable end')
    for i in range(24):
        types.append(f'(param $x{i} i64)')
        pushes.append(f'i64.const {501+i}')
        checks.append(f'local.get $x{i} i64.const {501+i} i64.ne if unreachable end')
    result = []
    for indirect in (False, True):
        wide_types = ' '.join(p.split()[-1].rstrip(')') for p in types)
        declarations = f'(type $ta (func (param i32) (result i64))) (type $tb (func (param i32 {wide_types}) (result i64)))'
        if indirect:
            declarations += '(table 2 funcref) (elem (i32.const 0) $a $b)'
        a_to_b = 'i32.const 1 return_call_indirect (type $tb)' if indirect else 'return_call $b'
        b_to_a = 'i32.const 0 return_call_indirect (type $ta)' if indirect else 'return_call $a'
        wat = f'''(module {declarations}
          (func $a (type $ta) (param $n i32) (result i64)
            local.get $n i32.eqz if i64.const 177 return end
            local.get $n i32.const 1 i32.sub {' '.join(pushes)} {a_to_b})
          (func $b (type $tb) (param $n i32) {' '.join(types)} (result i64)
            {' '.join(checks)} local.get $n {b_to_a})
          (func (export "_start") i32.const {rounds} call $a
            i64.const 177 i64.ne if unreachable end))'''
        name = 'tail-mixed-spills-' + ('indirect' if indirect else 'direct') + '-2m'
        result.append((name, wat, 2 * rounds))
    for indirect in (False, True):
        declarations = '''(type $s (struct (field (mut i64))))
          (type $ta (func (param i32 (ref $s)) (result (ref $s))))
          (type $tb (func (param (ref $s) i32 f64) (result (ref $s))))'''
        if indirect:
            declarations += '(table 2 funcref) (elem (i32.const 0) $a $b)'
        a_to_b = 'i32.const 1 return_call_indirect (type $tb)' if indirect else 'return_call $b'
        b_to_a = 'i32.const 0 return_call_indirect (type $ta)' if indirect else 'return_call $a'
        wat = f'''(module {declarations}
          (func $a (type $ta) (param $n i32) (param $p (ref $s)) (result (ref $s))
            local.get $n i32.eqz if local.get $p return end
            i64.const 0 struct.new $s drop
            local.get $p local.get $p struct.get $s 0 i64.const 1 i64.add struct.set $s 0
            local.get $p local.get $n i32.const 1 i32.sub
            i64.const 0x8000000000000000 f64.reinterpret_i64 {a_to_b})
          (func $b (type $tb) (param $p (ref $s)) (param $n i32) (param $z f64) (result (ref $s))
            local.get $z i64.reinterpret_f64 i64.const 0x8000000000000000 i64.ne if unreachable end
            local.get $p struct.get $s 0
            i64.const {37 + rounds} local.get $n i64.extend_i32_u i64.sub
            i64.ne if unreachable end
            local.get $n local.get $p {b_to_a})
          (func (export "_start")
            i32.const {rounds} i64.const 37 struct.new $s call $a
            struct.get $s 0 i64.const {37 + rounds} i64.ne if unreachable end))'''
        name = 'tail-live-gc-' + ('indirect' if indirect else 'direct') + '-2m'
        result.append((name, wat, 2 * rounds))
    return result
