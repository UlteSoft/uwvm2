#!/usr/bin/env python3
"""Generate deterministic raw-bit rounding cases with Python's independent math.

float32 values are exactly representable as Python float64. Rounded finite
outputs are integral and exactly representable in their original format.
NaNs are checked by class/canonical requirements by the target-side runner.
"""
import argparse
import math
import pathlib
import random
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out', type=pathlib.Path, required=True)
args = parser.parse_args()
random_bits = random.Random(0x55204E414E)
records = []
for width, integer, floating, infinity, quiet in (
    (4, '<I', '<f', 0x7f800000, 0x400000),
    (8, '<Q', '<d', 0x7ff0000000000000, 0x8000000000000),
):
    sign = 1 << (width * 8 - 1)
    special = [0, sign, 1, sign | 1, infinity, sign | infinity,
               infinity | quiet, sign | infinity | quiet]
    # Exercise every payload bit as both an sNaN and a quiet arithmetic NaN.
    for bit in range(23 if width == 4 else 52):
        for negative in (0, sign):
            special += [negative | infinity | (1 << bit),
                        negative | infinity | quiet | (1 << bit)]
    for exponent in range(1, 255 if width == 4 else 2047, 3):
        for delta in (-1, 0, 1):
            special.append((exponent << (23 if width == 4 else 52)) + delta)
            special.append(sign | special[-1])
    for record in range(5000):
        raw = [special[record * (16 // width) + lane]
               if record * (16 // width) + lane < len(special) else random_bits.getrandbits(width * 8)
               for lane in range(16 // width)]
        expected = []
        for function in (math.ceil, math.floor, math.trunc, round):
            for bits in raw:
                magnitude = bits & ~sign
                if magnitude >= infinity:
                    wanted = bits | quiet if magnitude > infinity else bits
                else:
                    value = struct.unpack(floating, struct.pack(integer, bits))[0]
                    rounded = function(value)
                    wanted = (bits & sign) if rounded == 0 else struct.unpack(integer, struct.pack(floating, rounded))[0]
                expected.append(struct.pack(integer, wanted))
        records.append(bytes([width]) + b''.join(struct.pack(integer, bits) for bits in raw) + b''.join(expected))
with args.out.open('xb') as stream:
    stream.write(b'SIMDRND1' + struct.pack('<I', len(records)))
    for record in records:
        assert len(record) == 81
        stream.write(record)
print(f'generated {len(records)} records, 120000 lane checks')
