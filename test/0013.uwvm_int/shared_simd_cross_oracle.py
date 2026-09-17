#!/usr/bin/env python3
"""Redirect a simd_direct_lowering-generated C oracle to the uwvm-int evaluator.

Generate expected bytes on an independent, verified host. Compile the output as C
and link with uwvm_int_simd_cross.cc compiled with UWVM2TEST_SIMD_CROSS_ADAPTER.
No LLVM library or host-side reference implementation is linked into the target.
"""
import argparse
import pathlib
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("oracle", type=pathlib.Path)
    args = parser.parse_args()
    text = args.oracle.read_text()
    pattern = r"extern void simd_(\d+)_(\d+)\(unsigned char\*,const unsigned char\*\);"

    def wrapper(match):
        op, lane = match.groups()
        return (f"static void simd_{op}_{lane}(unsigned char*o,const unsigned char*i)"
                f"{{shared_simd({op},{lane},o,i);}}")

    text, count = re.subn(pattern, wrapper, text)
    if count != 385:
        parser.error(f"expected 385 opcode/lane declarations, found {count}")
    print("extern void shared_simd(unsigned,unsigned,unsigned char*,const unsigned char*);")
    print(text, end="")


if __name__ == "__main__":
    main()
