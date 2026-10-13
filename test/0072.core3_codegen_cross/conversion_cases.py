"""Exact rational conversions, including mixed signedness and halfway integers."""
from fractions import Fraction
from ieee_oracle import IEEE
from numeric_cases import criterion, inputs


def cases():
    result = []
    for width in (32, 64):
        f = IEEE(width)
        ft, bt = f'f{width}', f'i{width}'
        for iw in (32, 64):
            it = f'i{iw}'
            for signed in (True, False):
                # Represent integer arguments as raw bits: no host double can
                # double-round an i64 before the requested f32 conversion.
                values = [0, 1, (1 << iw) - 1, 1 << (iw - 1), (1 << (iw - 1)) - 1]
                for shift in (f.frac, f.frac + 1, min(iw - 1, f.frac + 3)):
                    if shift < iw:
                        values += [((1 << shift) + d) % (1 << iw) for d in (-3, -1, 1, 3)]
                checks = []
                for raw in sorted(set(values)):
                    value = raw - (1 << iw) if signed and raw >= 1 << (iw - 1) else raw
                    checks.append(f'{it}.const {raw} global.set $arg call $op '
                                  f'{criterion(bt, f.pack(value))} if unreachable end')
                name = f'{ft}-convert-{it}-' + ('s' if signed else 'u')
                wat = f'''(module (global $arg (mut {it}) ({it}.const 0))
                  (func $op (export "op") (result {bt}) global.get $arg
                    {ft}.convert_{it}_{'s' if signed else 'u'} {bt}.reinterpret_{ft})
                  (func (export "_start") {' '.join(checks)}))'''
                result.append((name, wat, len(checks)))
            for trunc_signed in (True, False):
                for convert_signed in (True, False):
                    candidates = [Fraction(-3, 2), Fraction(-1, 2), Fraction(0), Fraction(1, 2), Fraction(3, 2)]
                    for power in (iw - 1, iw):
                        candidates += [Fraction((1 << power) + d) for d in (-2049, -1025, -1, 0, 1)]
                    checks = []
                    for candidate in candidates:
                        raw = f.pack(candidate)
                        if f.nan(raw) or f.inf(raw):
                            continue
                        value = f.finite(raw)
                        integer = abs(value.numerator) // value.denominator
                        if value < 0:
                            integer = -integer
                        lo = -(1 << (iw - 1)) if trunc_signed else 0
                        hi = (1 << (iw - int(trunc_signed))) - 1
                        if not lo <= integer <= hi:
                            continue
                        bits = integer % (1 << iw)
                        converted = bits - (1 << iw) if convert_signed and bits >= 1 << (iw - 1) else bits
                        checks.append(f'{bt}.const {raw} global.set $arg call $op '
                                      f'{criterion(bt, f.pack(converted))} if unreachable end')
                    ts, cs = ('s' if trunc_signed else 'u'), ('s' if convert_signed else 'u')
                    name = f'{ft}-{it}-roundtrip-{ts}{cs}'
                    wat = f'''(module (global $arg (mut {bt}) ({bt}.const 0))
                      (func $op (export "op") (result {bt}) global.get $arg {ft}.reinterpret_{bt}
                        {it}.trunc_{ft}_{ts} {ft}.convert_{it}_{cs} {bt}.reinterpret_{ft})
                      (func (export "_start") {' '.join(checks)}))'''
                    result.append((name, wat, len(checks)))
    for source_width, destination_width, op in ((32, 64, 'promote'), (64, 32, 'demote')):
        source, destination = IEEE(source_width), IEEE(destination_width)
        values = inputs(source)
        if source_width == 64:
            # f32 halfway boundaries, overflow, and the normal/subnormal seam.
            tiny = Fraction(1, 1 << 149)
            values += [source.pack(x) for x in (tiny / 2, 3 * tiny / 2,
                       Fraction(1) + Fraction(1, 1 << 24), Fraction(1) + Fraction(3, 1 << 24),
                       Fraction((1 << 24) - 1) * (1 << 104),
                       Fraction(1, 1 << 126) - tiny / 2)]
        checks = []
        for raw in values:
            if source.nan(raw):
                canonical = raw & source.payload == source.quiet
                expected = destination.expected_nan([]) if canonical else (
                    destination.infinity | destination.quiet, destination.infinity | destination.quiet)
            elif source.inf(raw):
                expected = destination.infinity | (destination.sign if raw & source.sign else 0)
            else:
                expected = destination.pack(source.finite(raw), bool(raw & source.sign))
            checks.append(f'i{source_width}.const {raw} global.set $arg call $op '
                          f'{criterion(f"i{destination_width}", expected)} if unreachable end')
        wat = f'''(module (global $arg (mut i{source_width}) (i{source_width}.const 0))
          (func $op (export "op") (result i{destination_width}) global.get $arg
            f{source_width}.reinterpret_i{source_width} f{destination_width}.{op}_f{source_width}
            i{destination_width}.reinterpret_f{destination_width})
          (func (export "_start") {' '.join(checks)}))'''
        result.append((f'f{destination_width}-{op}-f{source_width}', wat, len(checks)))
    return result
