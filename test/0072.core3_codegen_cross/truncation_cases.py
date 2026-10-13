"""Saturating scalar/SIMD conversion seams with rational/integer expectations."""
from fractions import Fraction
from ieee_oracle import IEEE
from numeric_cases import criterion, inputs


def values(f, iw):
    result = inputs(f)
    for candidate in (Fraction(-3, 2), Fraction(-1, 2), Fraction(1, 2), Fraction(3, 2)):
        result.append(f.pack(candidate))
    for boundary in (0, -(1 << (iw - 1)), (1 << (iw - 1)) - 1, (1 << iw) - 1):
        raw = f.pack(boundary)
        result += [raw, (raw - 1) % (1 << f.width), (raw + 1) % (1 << f.width)]
        result += [f.pack(Fraction(boundary) + delta) for delta in
                   (Fraction(-3, 2), Fraction(-1, 2), Fraction(1, 2), Fraction(3, 2))]
    return sorted(set(result))


def cases():
    result = []
    for fw in (32, 64):
        f = IEEE(fw)
        for iw in (32, 64):
            for signed in (True, False):
                suffix = 's' if signed else 'u'
                checks = [f'i{fw}.const {raw} global.set $arg call $op '
                          f'{criterion(f"i{iw}", f.trunc_integer(raw, iw, signed, True))} if unreachable end'
                          for raw in values(f, iw)]
                wat = f'''(module (global $arg (mut i{fw}) (i{fw}.const 0))
                  (func $op (export "op") (result i{iw}) global.get $arg f{fw}.reinterpret_i{fw}
                    i{iw}.trunc_sat_f{fw}_{suffix})
                  (func (export "_start") {' '.join(checks)}))'''
                result.append((f'i{iw}-trunc-sat-f{fw}-{suffix}', wat, len(checks)))
        lanes = 128 // fw
        vals = values(f, 32)
        for signed in (True, False):
            suffix = 's' if signed else 'u'
            opcode = f'i32x4.trunc_sat_f{fw}x{lanes}_{suffix}' + ('_zero' if fw == 64 else '')
            checks = []
            for index in range(len(vals)):
                lane_bits = [vals[(index + lane * 7) % len(vals)] for lane in range(lanes)]
                expected = [f.trunc_integer(raw, 32, signed, True) for raw in lane_bits] + [0] * (4 - lanes)
                checks.append(f'v128.const i{fw}x{lanes} {" ".join(map(str, lane_bits))} global.set $arg '
                              f'call $op local.set $r')
                checks += [f'local.get $r i32x4.extract_lane {lane} {criterion("i32", integer)} if unreachable end'
                           for lane, integer in enumerate(expected)]
            wat = f'''(module (global $arg (mut v128) (v128.const i32x4 0 0 0 0))
              (func $op (export "op") (result v128) global.get $arg {opcode})
              (func (export "_start") (local $r v128) {' '.join(checks)}))'''
            result.append((opcode.replace('.', '-').replace('_', '-'), wat, 4 * len(vals)))
    return result
