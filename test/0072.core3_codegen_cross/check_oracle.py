"""Known bit anchors and exact boundary invariants, executed on guarded Linux."""
from fractions import Fraction
from ieee_oracle import IEEE, matches


def check():
    for width, sqrt2 in ((32, 0x3fb504f3), (64, 0x3ff6a09e667f3bcd)):
        f = IEEE(width)
        one = f.bias << f.frac
        assert f.pack(1) == one
        assert f.pack(-1) == one | f.sign
        assert f.pack(Fraction(1) + Fraction(1, 1 << (f.frac + 1))) == one
        assert f.pack(Fraction(1) + Fraction(3, 1 << (f.frac + 1))) == one + 2
        assert f.binary('div', 1, f.pack(2)) == 0
        assert f.binary('div', 3, f.pack(2)) == 2
        assert f.binary('sub', 1 << f.frac, f.payload) == 1
        assert f.binary('mul', f.infinity - 1, f.pack(2)) == f.infinity
        assert f.binary('div', one | f.sign, f.infinity) == f.sign
        assert f.binary('add', f.sign, f.sign) == f.sign
        assert f.binary('add', one, one | f.sign) == 0
        assert f.binary('min', 0, f.sign) == f.sign
        assert f.binary('max', 0, f.sign) == 0
        assert f.sqrt(f.pack(2)) == sqrt2
        assert f.unary('nearest', f.pack(Fraction(-1, 2))) == f.sign
        assert f.unary('nearest', f.pack(Fraction(5, 2))) == f.pack(2)
        assert f.unary('nearest', f.pack(Fraction(7, 2))) == f.pack(4)
        for iw in (32, 64):
            assert f.trunc_integer(f.infinity | 17, iw, True, True) == 0
            assert f.trunc_integer(f.infinity, iw, True, True) == (1 << (iw - 1)) - 1
            assert f.trunc_integer(f.infinity | f.sign, iw, True, True) == 1 << (iw - 1)
            assert f.trunc_integer(f.infinity, iw, False, True) == (1 << iw) - 1
            assert f.trunc_integer(f.pack(Fraction(-1, 2)), iw, False) == 0
            assert f.trunc_integer(f.pack(Fraction(-3, 2)), iw, True) == (1 << iw) - 1
            assert f.trunc_integer(f.pack(-2), iw, False, True) == 0
        signaling = f.infinity | 17
        assert f.unary('neg', signaling) == signaling | f.sign
        assert matches(f.infinity | f.quiet | 27, f.binary('add', signaling, one))
        assert not matches(signaling, f.binary('add', signaling, one))
        assert not matches(f.infinity | f.quiet | 1, f.binary('add', f.infinity | f.quiet, one))
        for b in (0, f.sign, 1, f.payload, 1 << f.frac, one - 1, one, one + 1, f.infinity - 1):
            assert f.pack(f.finite(b), bool(b & f.sign)) == b
    print('PASS rational IEEE anchors: both widths, halfway/subnormal/overflow/NaN/zero', flush=True)


if __name__ == '__main__':
    check()
