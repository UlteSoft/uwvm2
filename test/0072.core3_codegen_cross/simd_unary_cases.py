"""SIMD float unary witnesses using exact bits and the rational IEEE oracle.

Core 3.0 lifts scalar operators to lanes:
https://webassembly.github.io/spec/core/exec/numerics.html#op-fabs
Abs/neg preserve the NaN payload exactly; rounding checks permitted NaNs.
"""
from ieee_oracle import IEEE
from numeric_cases import criterion, inputs


def cases():
    result = []
    for width in (32, 64):
        ieee = IEEE(width)
        lanes = 128 // width
        integer_shape, float_shape = f'i{width}x{lanes}', f'f{width}x{lanes}'
        values = inputs(ieee)
        # Include payloads and both signs across canonical and signalling NaNs,
        # negative subnormals, and values immediately around half/tie boundaries.
        values += [ieee.sign | (ieee.infinity | 1),
                   ieee.sign | (ieee.infinity | ieee.quiet),
                   ieee.infinity | ieee.quiet | 17,
                   (ieee.bias - 1) << ieee.frac | 1,
                   ieee.sign | (((ieee.bias - 1) << ieee.frac) | 1)]
        for operation in ('abs', 'neg', 'ceil', 'floor', 'trunc'):
            checks = []
            for index in range(len(values)):
                bits = [values[(index + lane) % len(values)] for lane in range(lanes)]
                checks.append(f'v128.const {integer_shape} {" ".join(map(str, bits))} '
                              'global.set $a call $op local.set $r')
                for lane, value in enumerate(bits):
                    expected = ieee.unary(operation, value)
                    checks.append(f'local.get $r {integer_shape}.extract_lane {lane} '
                                  f'{criterion(f"i{width}", expected)} if unreachable end')
            wat = f'''(module
              (global $a (mut v128) (v128.const i32x4 0 0 0 0))
              (func $op (export "op") (result v128)
                global.get $a {float_shape}.{operation})
              (func (export "_start") (local $r v128) {' '.join(checks)}))'''
            result.append((f'{float_shape}-{operation}-special-bits',
                           wat, len(values) * lanes))
    return result

