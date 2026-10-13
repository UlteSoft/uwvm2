"""Real Wasm function transfers, distinct from interpreter opcode dispatch."""


def cases():
    count = 2_000_003
    expected = 13 + 7 * count
    result = []
    for kind in ('direct', 'indirect', 'typed-ref', 'different-prototype'):
        types = '(type $t (func (param i32 i64) (result i64)))'
        declarations = ''
        bparams = '(param $n i32) (param $s i64)'
        to_a = 'local.get $n i32.const 1 i32.sub local.get $s i64.const 7 i64.add return_call $a'
        to_b = to_a.replace('$a', '$b')
        if kind == 'indirect':
            declarations = '(table 2 funcref) (elem (i32.const 0) $a $b)'
            to_a = to_a.replace('return_call $a', 'i32.const 0 return_call_indirect (type $t)')
            to_b = to_b.replace('return_call $b', 'i32.const 1 return_call_indirect (type $t)')
        elif kind == 'typed-ref':
            declarations = '(elem declare func $a $b)'
            to_a = to_a.replace('return_call $a', 'ref.func $a return_call_ref $t')
            to_b = to_b.replace('return_call $b', 'ref.func $b return_call_ref $t')
        elif kind == 'different-prototype':
            bparams = '(param $s i64) (param $n i32)'
            to_b = 'local.get $s i64.const 7 i64.add local.get $n i32.const 1 i32.sub return_call $b'
        wat = f'''(module {types} {declarations}
          (func $a (param $n i32) (param $s i64) (result i64)
            local.get $n i32.eqz if local.get $s return end {to_b})
          (func $b {bparams} (result i64)
            local.get $n i32.eqz if local.get $s return end {to_a})
          (func (export "_start") i32.const {count} i64.const 13 call $a
            i64.const {expected} i64.ne if unreachable end))'''
        result.append((f'tail-{kind}-2m', wat, count))
    return result + abi_cases()


def abi_cases():
    """Exhaust argument/return registers and check every spilled field."""
    rounds = 1_000_002
    params = ' '.join(f'(param $v{i} i64)' for i in range(24))
    pushes = ' '.join(f'i64.const {101+i}' for i in range(24))
    checks = ' '.join(f'local.get $v{i} i64.const {101+i} i64.ne if unreachable end' for i in range(24))
    result = [('tail-stack-parameters-2m', f'''(module
      (func $a (param $n i32) (result i64)
        local.get $n i32.eqz if i64.const 177 return end
        local.get $n i32.const 1 i32.sub {pushes} return_call $b)
      (func $b (param $n i32) {params} (result i64)
        {checks} local.get $n return_call $a)
      (func (export "_start") i32.const {rounds} call $a
        i64.const 177 i64.ne if unreachable end))''', 2 * rounds)]
    result.append(('tail-vector-parameters-2m', f'''(module
      (func $a (param $n i32) (param $v v128) (result v128)
        local.get $n i32.eqz if local.get $v return end
        local.get $n i32.const 1 i32.sub local.get $v i64.const 23 return_call $b)
      (func $b (param $n i32) (param $v v128) (param $x i64) (result v128)
        local.get $x i64.const 23 i64.ne if unreachable end
        local.get $n local.get $v return_call $a)
      (func (export "_start") i32.const {rounds} v128.const i32x4 1 2 3 4 call $a
        v128.const i32x4 1 2 3 4 i32x4.eq i32x4.all_true if else unreachable end))''', 2 * rounds))
    types = ['i32', 'i64', 'f32', 'f64', 'v128', 'funcref', 'externref'] + ['i64'] * 24
    values = ['i32.const 31', 'i64.const 37', 'f32.const 2.5', 'f64.const 9.25',
              'v128.const i32x4 1 2 3 4', 'ref.null func', 'ref.null extern'] + [f'i64.const {201+i}' for i in range(24)]
    checks = ['i32.const 31 i32.ne', 'i64.const 37 i64.ne', 'f32.const 2.5 f32.ne', 'f64.const 9.25 f64.ne',
              'v128.const i32x4 1 2 3 4 i32x4.eq i32x4.all_true i32.eqz', 'ref.is_null i32.eqz', 'ref.is_null i32.eqz'] + [f'i64.const {201+i} i64.ne' for i in range(24)]
    returns = '(result ' + ' '.join(types) + ')'
    verify = ' '.join(c + ' if unreachable end' for c in reversed(checks))
    for indirect in (False, True):
        to_a = 'i32.const 0 return_call_indirect (type $ta)' if indirect else 'return_call $a'
        to_b = 'i32.const 1 return_call_indirect (type $tb)' if indirect else 'return_call $b'
        result.append(('tail-tuple-' + ('indirect' if indirect else 'direct') + '-2m', f'''(module
          (type $ta (func (param i32) {returns}))
          (type $tb (func (param i32 f64) {returns}))
          (table 2 funcref) (elem (i32.const 0) $a $b)
          (func $a (type $ta) (param $n i32) {returns}
            local.get $n i32.eqz if {' '.join(values)} return end
            local.get $n i32.const 1 i32.sub f64.const 9 {to_b})
          (func $b (type $tb) (param $n i32) (param $x f64) {returns}
            local.get $x f64.const 9 f64.ne if unreachable end local.get $n {to_a})
          (func (export "_start")
            i32.const {rounds} call $a {verify}
            i32.const {rounds} i32.const 0 call_indirect (type $ta) {verify}))''', 4 * rounds))
    # Each body exceeds lazy grouping's 8 KiB budget. New activations must
    # zero their locals even when the opposite tail target is initially cold.
    cold = f'''(module
      (func $a (param $n i32) (local $z i64) {'nop ' * 9000}
        local.get $z i64.eqz if else unreachable end i64.const 17 local.set $z
        local.get $n i32.eqz if return end
        local.get $n i32.const 1 i32.sub return_call $b)
      (func $b (param $n i32) (local $z i32) {'nop ' * 9000}
        local.get $z i32.eqz if else unreachable end i32.const 31 local.set $z
        local.get $n i32.eqz if return end
        local.get $n i32.const 1 i32.sub return_call $a)
      (func (export "_start") i32.const 2000003 call $a))'''
    result.append(('tail-cold-fresh-locals-2m', cold, 2_000_003))
    return result
