"""Integer/rational IEEE oracle. No host float, fenv, libm or LLVM dependency.

NaN arithmetic returns a predicate, not an arbitrarily selected payload. Exact
bit operations retain signaling payloads. This models ordinary Core arithmetic,
not the optional deterministic profile or relaxed SIMD.
"""
from fractions import Fraction
from math import isqrt


class IEEE:
    def __init__(self, width):
        assert width in (32, 64)
        self.width = width
        self.frac = 23 if width == 32 else 52
        self.expbits = 8 if width == 32 else 11
        self.bias = (1 << (self.expbits - 1)) - 1
        self.sign = 1 << (width - 1)
        self.payload = (1 << self.frac) - 1
        self.infinity = ((1 << self.expbits) - 1) << self.frac
        self.quiet = 1 << (self.frac - 1)

    def nan(self, bits):
        return bits & self.infinity == self.infinity and bool(bits & self.payload)

    def inf(self, bits):
        return bits & ~self.sign == self.infinity

    def finite(self, bits):
        assert not self.nan(bits) and not self.inf(bits)
        exponent = (bits & self.infinity) >> self.frac
        mantissa = bits & self.payload
        if exponent:
            mantissa |= 1 << self.frac
        power = (exponent or 1) - self.bias - self.frac
        value = Fraction(mantissa << power, 1) if power >= 0 else Fraction(mantissa, 1 << -power)
        return -value if bits & self.sign else value

    @staticmethod
    def ties_even(numerator, denominator):
        q, r = divmod(numerator, denominator)
        return q + (2 * r > denominator or (2 * r == denominator and q & 1))

    @staticmethod
    def exponent(value):
        n, d = value.numerator, value.denominator
        e = n.bit_length() - d.bit_length()
        if (n < d << e) if e >= 0 else (n << -e < d):
            e -= 1
        return e

    def pack(self, value, negative_zero=False):
        value = Fraction(value)
        negative = value < 0 or (value == 0 and negative_zero)
        value = abs(value)
        sign = self.sign if negative else 0
        if not value:
            return sign
        exponent = self.exponent(value)
        emin = 1 - self.bias
        power = max(exponent, emin) - self.frac
        scaled = value / (1 << power) if power >= 0 else value * (1 << -power)
        mantissa = self.ties_even(scaled.numerator, scaled.denominator)
        if mantissa == 1 << (self.frac + 1):
            mantissa >>= 1
            exponent += 1
        if exponent > self.bias:
            return sign | self.infinity
        if exponent < emin and mantissa < 1 << self.frac:
            return sign | mantissa
        return sign | ((max(exponent, emin) + self.bias) << self.frac) | (mantissa & self.payload)

    def expected_nan(self, inputs):
        # Canonical operands impose canonical payload; otherwise any quiet
        # arithmetic payload is permitted. The sign is always unspecified.
        canonical = all(not self.nan(b) or b & self.payload == self.quiet for b in inputs)
        if canonical:
            return (self.infinity | self.quiet, self.sign - 1)
        return (self.infinity | self.quiet, self.infinity | self.quiet)

    def trunc_integer(self, bits, width, signed, saturating=False):
        lo = -(1 << (width - 1)) if signed else 0
        hi = (1 << (width - int(signed))) - 1
        if self.nan(bits):
            if saturating:
                return 0
            raise ValueError('invalid conversion to integer')
        if self.inf(bits):
            if saturating:
                return (lo if bits & self.sign else hi) % (1 << width)
            raise ValueError('integer overflow')
        value = self.finite(bits)
        integer = abs(value.numerator) // value.denominator
        if value < 0:
            integer = -integer
        if saturating:
            integer = min(hi, max(lo, integer))
        elif not lo <= integer <= hi:
            raise ValueError('integer overflow')
        return integer % (1 << width)

    def sqrt(self, bits):
        if self.nan(bits):
            return self.expected_nan([bits])
        if bits & self.sign and bits != self.sign:
            return self.expected_nan([])
        if self.inf(bits) or bits & ~self.sign == 0:
            return bits
        value = self.finite(bits)
        exponent = self.exponent(value) // 2
        power = max(exponent, 1 - self.bias) - self.frac
        scaled = value / (1 << (2 * power)) if power >= 0 else value * (1 << (-2 * power))
        n, d = scaled.numerator, scaled.denominator
        q = isqrt(n // d)
        comparison = 4 * n - d * (2 * q + 1) ** 2
        q += comparison > 0 or (comparison == 0 and q & 1)
        return self.pack(Fraction(q) * (1 << power) if power >= 0 else Fraction(q, 1 << -power))

    def unary(self, op, bits):
        if op == 'abs':
            return bits & ~self.sign
        if op == 'neg':
            return bits ^ self.sign
        if op == 'sqrt':
            return self.sqrt(bits)
        if self.nan(bits):
            return self.expected_nan([bits])
        if self.inf(bits):
            return bits
        value = self.finite(bits)
        if op == 'nearest':
            integer = self.ties_even(abs(value.numerator), value.denominator)
            if value < 0:
                integer = -integer
        elif op == 'floor':
            integer = value.numerator // value.denominator
        elif op == 'ceil':
            integer = -(-value.numerator // value.denominator)
        elif op == 'trunc':
            integer = abs(value.numerator) // value.denominator
            if value < 0:
                integer = -integer
        else:
            raise ValueError(op)
        return self.pack(integer, bool(bits & self.sign))

    def binary(self, op, a, b):
        if op == 'copysign':
            return (a & ~self.sign) | (b & self.sign)
        if self.nan(a) or self.nan(b):
            return self.expected_nan([a, b])
        if op in ('min', 'max'):
            if a & ~self.sign == b & ~self.sign == 0:
                return (a | b) if op == 'min' else (a & b)
            if self.inf(a):
                return a if (bool(a & self.sign) == (op == 'min')) else b
            if self.inf(b):
                return b if (bool(b & self.sign) == (op == 'min')) else a
            va, vb = self.finite(a), self.finite(b)
            return a if ((va < vb) if op == 'min' else (va > vb)) else b
        if op == 'sub':
            b ^= self.sign
            op = 'add'
        negative = bool((a ^ b) & self.sign)
        if op == 'add':
            if self.inf(a) or self.inf(b):
                if self.inf(a) and self.inf(b) and (a ^ b) & self.sign:
                    return self.expected_nan([])
                return a if self.inf(a) else b
            return self.pack(self.finite(a) + self.finite(b), bool(a & b & self.sign))
        if op == 'mul':
            if self.inf(a) or self.inf(b):
                finite = b if self.inf(a) else a
                if not self.inf(finite) and self.finite(finite) == 0:
                    return self.expected_nan([])
                return self.infinity | (self.sign if negative else 0)
            return self.pack(self.finite(a) * self.finite(b), negative)
        if op == 'div':
            if (self.inf(a) and self.inf(b)) or (a & ~self.sign == b & ~self.sign == 0):
                return self.expected_nan([])
            if self.inf(a) or b & ~self.sign == 0:
                return self.infinity | (self.sign if negative else 0)
            if self.inf(b):
                return self.sign if negative else 0
            return self.pack(self.finite(a) / self.finite(b), negative)
        raise ValueError(op)


def matches(actual, expected):
    if isinstance(expected, tuple):
        value, mask = expected
        return actual & mask == value
    return actual == expected
