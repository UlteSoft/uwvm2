"""All 20 relaxed SIMD opcodes, using inputs with a unique permitted result.

No NaN, signed-zero tie, overflow conversion, out-of-range swizzle index,
partial lane mask or exceptional dot/Q15 input is used. Exact vector bytes
remain a valid expectation even on targets with different relaxed choices.
"""
import struct


def relaxed_simd_examples(mark):
    prefix = ['i64 = 1000']; code = []; expected = []; coverage = []

    def vector(kind, values):
        return 'v128.const ' + kind + ' ' + ' '.join(map(str, values))

    def packed(kind, values):
        formats = {'i8x16': '<16b', 'i16x8': '<8h', 'i32x4': '<4i',
                   'i64x2': '<2q', 'f32x4': '<4f', 'f64x2': '<2d'}
        return struct.pack(formats[kind], *values)

    def check(opcode, inputs, result):
        for kind, values in inputs:
            code.append(vector(kind, values))
        before = ['v128 = bytes=' + packed(kind, values).hex() for kind, values in inputs]
        code.append('nop'); expected.append(mark(0, len(expected), prefix + before))
        code.extend([opcode, 'nop'])
        expected.append(mark(0, len(expected), prefix + ['v128 = bytes=' + result.hex()]))
        code.append('drop'); coverage.append(opcode)

    data = list(range(16)); reverse = list(reversed(data))
    check('i8x16.relaxed_swizzle', [('i8x16', data), ('i8x16', reverse)], bytes(reverse))
    for source, signed, values in [('f32x4', True, [1, -2, 3, 4]),
                                    ('f32x4', False, [1, 2, 3, 4]),
                                    ('f64x2', True, [-2, 3]), ('f64x2', False, [2, 3])]:
        opcode = 'i32x4.relaxed_trunc_' + source + ('_s' if signed else '_u')
        if source == 'f64x2': opcode += '_zero'
        result = list(values) + ([0, 0] if source == 'f64x2' else [])
        check(opcode, [(source, values)], packed('i32x4', result))
    for kind, values in [('f32x4', [1, 2, 3, 4]), ('f64x2', [1, 2])]:
        for negative in (False, True):
            inputs = [(kind, values), (kind, [2] * len(values)), (kind, [1] * len(values))]
            result = [(-2 if negative else 2) * x + 1 for x in values]
            check(kind + ('.relaxed_nmadd' if negative else '.relaxed_madd'), inputs, packed(kind, result))
    for kind, bits in [('i8x16', 8), ('i16x8', 16), ('i32x4', 32), ('i64x2', 64)]:
        count = 128 // bits; width = bits // 8
        left = list(range(1, count + 1)); right = list(range(17, 17 + count))
        masks = [-1 if i % 2 == 0 else 0 for i in range(count)]
        result = [left[i] if masks[i] else right[i] for i in range(count)]
        check(kind + '.relaxed_laneselect', [(kind, left), (kind, right), (kind, masks)], packed(kind, result))
    for kind, left, right in [('f32x4', [-3, 2, 4, -7], [-1, 4, 6, -5]),
                               ('f64x2', [-3, 2], [-1, 4])]:
        for maximum in (False, True):
            operation = max if maximum else min
            check(kind + ('.relaxed_max' if maximum else '.relaxed_min'),
                  [(kind, left), (kind, right)], packed(kind, [operation(a, b) for a, b in zip(left, right)]))
    left = [16384] * 8; right = [0, 16384, -16384, 8192, -8192, 32767, -32768, 1]
    check('i16x8.relaxed_q15mulr_s', [('i16x8', left), ('i16x8', right)],
          packed('i16x8', [(a * b + 16384) >> 15 for a, b in zip(left, right)]))
    left = list(range(1, 17)); right = [2] * 16
    check('i16x8.relaxed_dot_i8x16_i7x16_s', [('i8x16', left), ('i8x16', right)],
          packed('i16x8', [sum(left[i:i + 2]) * 2 for i in range(0, 16, 2)]))
    addends = [100, 200, 300, 400]
    check('i32x4.relaxed_dot_i8x16_i7x16_add_s',
          [('i8x16', left), ('i8x16', right), ('i32x4', addends)],
          packed('i32x4', [sum(left[4 * i:4 * i + 4]) * 2 + addends[i] for i in range(4)]))
    code.extend(['nop', 'drop', 'nop'])
    expected.extend([mark(0, len(expected), prefix), mark(0, len(expected) + 1, [])])
    return [dict(name='relaxed-simd-all-values',
        wat='(module (func (export "_start") i64.const 1000 ' + ' '.join(code) + '))',
        expected=expected, relaxed_simd_opcodes=coverage)]
