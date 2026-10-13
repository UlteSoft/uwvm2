"""Generate self-checking Wasm using the rational oracle, not host arithmetic."""
import random
from ieee_oracle import IEEE


def criterion(integer_type, expected):
    if isinstance(expected, tuple):
        value, mask = expected
        return f'{integer_type}.const {mask} {integer_type}.and {integer_type}.const {value} {integer_type}.ne'
    return f'{integer_type}.const {expected} {integer_type}.ne'


def inputs(ieee):
    one = ieee.bias << ieee.frac
    return [0, ieee.sign, 1, ieee.sign | 1, ieee.payload, 1 << ieee.frac,
            one, one | ieee.sign, one - 1, one + 1,
            one + (1 << (ieee.frac - 1)), (ieee.bias - 1) << ieee.frac,
            ieee.infinity - 1, ieee.sign | (ieee.infinity - 1),
            ieee.infinity, ieee.sign | ieee.infinity,
            ieee.infinity | ieee.quiet, ieee.infinity | 1,
            ieee.sign | ieee.infinity | ieee.quiet | 17]


def scalar_cases():
    result = []
    for width in (32, 64):
        f = IEEE(width)
        vals = inputs(f)
        rng = random.Random(0xC032 + width)
        vals += [rng.getrandbits(width) for _ in range(24)]
        int_type, float_type = f'i{width}', f'f{width}'
        for op in ('add', 'sub', 'mul', 'div', 'min', 'max', 'copysign'):
            pairs = [(a, b) for a in vals[:19] for b in vals[:19]]
            pairs += [(a, vals[(i * 17 + 5) % len(vals)]) for i, a in enumerate(vals[19:])]
            checks = []
            for a, b in pairs:
                checks.append(f'{int_type}.const {a} global.set $a {int_type}.const {b} global.set $b '
                              f'call $op {criterion(int_type, f.binary(op, a, b))} if unreachable end')
            wat = f'''(module
              (global $a (mut {int_type}) ({int_type}.const 0))
              (global $b (mut {int_type}) ({int_type}.const 0))
              (func $op (export "op") (result {int_type})
                global.get $a {float_type}.reinterpret_{int_type}
                global.get $b {float_type}.reinterpret_{int_type}
                {float_type}.{op} {int_type}.reinterpret_{float_type})
              (func (export "_start") {' '.join(checks)}))'''
            result.append((f'{float_type}-{op}', wat, len(checks)))
        for op in ('sqrt', 'nearest', 'floor', 'ceil', 'trunc', 'abs', 'neg'):
            checks = [f'{int_type}.const {a} global.set $a call $op '
                      f'{criterion(int_type, f.unary(op, a))} if unreachable end' for a in vals]
            wat = f'''(module
              (global $a (mut {int_type}) ({int_type}.const 0))
              (func $op (export "op") (result {int_type})
                global.get $a {float_type}.reinterpret_{int_type}
                {float_type}.{op} {int_type}.reinterpret_{float_type})
              (func (export "_start") {' '.join(checks)}))'''
            result.append((f'{float_type}-{op}', wat, len(checks)))
    return result


def simd_cases():
    result = []
    for width in (32, 64):
        f = IEEE(width)
        lanes = 128 // width
        it, ft = f'i{width}x{lanes}', f'f{width}x{lanes}'
        vals = inputs(f)
        for op in ('add', 'sub', 'mul', 'div', 'min', 'max', 'sqrt', 'nearest'):
            checks = []
            for index in range(len(vals)):
                a = [vals[(index + lane) % len(vals)] for lane in range(lanes)]
                b = [vals[(index * 7 + lane + 1) % len(vals)] for lane in range(lanes)]
                checks.append(f'v128.const {it} {" ".join(map(str, a))} global.set $a '
                              f'v128.const {it} {" ".join(map(str, b))} global.set $b call $op local.set $r')
                for lane in range(lanes):
                    expected = f.unary(op, a[lane]) if op in ('sqrt', 'nearest') else f.binary(op, a[lane], b[lane])
                    checks.append(f'local.get $r {it}.extract_lane {lane} '
                                  f'{criterion(f"i{width}", expected)} if unreachable end')
            wat = f'''(module
              (global $a (mut v128) (v128.const i32x4 0 0 0 0))
              (global $b (mut v128) (v128.const i32x4 0 0 0 0))
              (func $op (export "op") (result v128)
                global.get $a {"" if op in ('sqrt', 'nearest') else "global.get $b"} {ft}.{op})
              (func (export "_start") (local $r v128) {' '.join(checks)}))'''
            result.append((f'{ft}-{op}', wat, len(vals) * lanes))
    return result


def cases():
    from conversion_cases import cases as conversions
    from truncation_cases import cases as truncations
    return scalar_cases() + simd_cases() + unfused_cases() + conversions() + truncations()


def unfused_cases():
    result = []
    for width in (32, 64):
        f = IEEE(width)
        one = f.bias << f.frac
        a, b, c = one + 1, one - 2, one | f.sign
        product = f.binary('mul', a, b)
        expected = f.binary('add', product, c)
        fused = f.pack(f.finite(a) * f.finite(b) + f.finite(c))
        assert expected != fused, 'fixture must distinguish unauthorized FMA'
        it, ft = f'i{width}', f'f{width}'
        wat = f'''(module
          (global $a (mut {it}) ({it}.const 0))
          (global $b (mut {it}) ({it}.const 0))
          (global $c (mut {it}) ({it}.const 0))
          (func $op (export "op") (result {it})
            global.get $a {ft}.reinterpret_{it} global.get $b {ft}.reinterpret_{it} {ft}.mul
            global.get $c {ft}.reinterpret_{it} {ft}.add {it}.reinterpret_{ft})
          (func (export "_start")
            {it}.const {a} global.set $a {it}.const {b} global.set $b {it}.const {c} global.set $c
            call $op {criterion(it, expected)} if unreachable end))'''
        result.append((f'{ft}-unfused-mul-add', wat, 1))
    return result
