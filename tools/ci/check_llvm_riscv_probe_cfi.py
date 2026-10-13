#!/usr/bin/env python3
"""Reject the known split-probe CFA bug before building a ROS-derived SDK.

This checks source shape, not binary provenance or runtime correctness. The
actual managed-quit and native cleanup regressions must still be run.
"""
import sys
from pathlib import Path


def main():
    path = Path(sys.argv[1])
    source = path.read_text()
    start = source.index('void RISCVFrameLowering::allocateStack(')
    end = source.index('\nstatic bool isPush(', start)
    body = source[start:end]
    loop = body[body.index('uint64_t RoundedSize = alignDown(Offset, ProbeSize);'):]
    checks = (
        'uint64_t CFAAdjust = RealStackSize - Offset;',
        'CFIBuilder.buildDefCFA(TargetReg, RoundedSize + CFAAdjust);',
        'CFIBuilder.buildDefCFAOffset(RealStackSize);',
    )
    if not all(text in loop for text in checks) or 'buildDefCFAOffset(Offset)' in loop:
        raise SystemExit('Unqualified RISC-V split-probe CFI source: apply '
                         'tools/ci/patches/llvm23-linux-riscv-split-probe-cfi.patch '
                         'and rebuild LLVM before testing the VM')
    print('PASS RISC-V split-probe CFI source shape; execution qualification still required')


if __name__ == '__main__':
    main()
