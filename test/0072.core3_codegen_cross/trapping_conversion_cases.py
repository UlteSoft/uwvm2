"""Dynamic scalar truncation traps and valid adjacent limits, with rational oracles."""
from fractions import Fraction
from ieee_oracle import IEEE


def cases():
    result = []
    for fw in (32, 64):
        f = IEEE(fw)
        for iw in (32, 64):
            for signed in (True, False):
                suffix = 's' if signed else 'u'
                name = f'i{iw}-trunc-f{fw}-{suffix}'
                op = f'i{iw}.trunc_f{fw}_{suffix}'
                limit = f.pack(1 << (iw - int(signed)))
                lo = -(1 << (iw - 1)) if signed else 0
                # At high precision lo-1 is itself a trap. At low precision it
                # can round back to lo; advance once towards negative infinity.
                lower = f.pack(lo - 1)
                try:
                    f.trunc_integer(lower, iw, signed)
                except ValueError:
                    pass
                else:
                    lower += 1
                bad = [
                    ('qnan', f.infinity | f.quiet),
                    ('negative-qnan', f.sign | f.infinity | f.quiet | 17),
                    ('snan', f.infinity | 17),
                    ('negative-snan', f.sign | f.infinity | 17),
                    ('infinity', f.infinity),
                    ('negative-infinity', f.sign | f.infinity),
                    ('upper-limit', limit),
                    ('above-upper-limit', limit + 1),
                    ('below-lower-limit', lower),
                    ('further-below-lower-limit', lower + 1),
                ]
                prefix = f'''(module (global $arg (mut i{fw}) (i{fw}.const 0))
                  (func $op (export "op") (result i{iw}) global.get $arg
                    f{fw}.reinterpret_i{fw} {op})'''
                for label, raw in bad:
                    try:
                        f.trunc_integer(raw, iw, signed)
                    except ValueError as error:
                        expected = str(error)
                    else:
                        raise AssertionError(('expected trapping input', name, label, raw))
                    wat = prefix + f'''(func (export "_start") i{fw}.const {raw}
                      global.set $arg call $op drop))'''
                    result.append(dict(name=name + '-' + label, wat=wat, checks=1,
                                       expected_trap=expected, input_bits=raw,
                                       float_width=fw, integer_width=iw, signed=signed))
                controls = [0, f.sign, f.pack(Fraction(1, 2)),
                            f.pack(Fraction(-1, 2)), f.pack(1),
                            f.pack(lo), limit - 1, f.pack(Fraction(lo) - Fraction(1, 2))]
                checks = []
                for raw in dict.fromkeys(controls):
                    try:
                        expected = f.trunc_integer(raw, iw, signed)
                    except ValueError:
                        continue
                    checks.append(f'i{fw}.const {raw} global.set $arg call $op '
                                  f'i{iw}.const {expected} i{iw}.ne if unreachable end')
                assert len(checks) >= 6
                wat = prefix + '(func (export "_start") ' + ' '.join(checks) + '))'
                result.append(dict(name=name + '-valid-controls', wat=wat,
                                   checks=len(checks), expected_trap=None))
    assert len(result) == 88
    assert sum(row['expected_trap'] is not None for row in result) == 80
    return result
