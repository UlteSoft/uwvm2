#!/usr/bin/env python3
"""Compare retained vendor bytes to the exact official archive, without extracting it.

Materialized in-tree symlinks are compared to their original target bytes.
Only the fifty-five documented compiler edits, one exported configuration
header template, two host build policies, and twenty-five regression
files may differ.
This is a content/provenance check, not a proof of compiler correctness.
"""
import argparse
import hashlib
from pathlib import Path
import tarfile

ARCHIVE_SHA256 = 'ebe9be46fe8756d58c5b198ffad0fa2a766257add81a4dc52179bfacc7888ee6'
CHANGED = {'llvm/lib/Target/Mips/MipsISelLowering.cpp',
           'llvm/lib/Object/RelocationResolver.cpp',
           'llvm/lib/Target/Sparc/SparcISelLowering.cpp',
           'llvm/lib/Target/PowerPC/PPCISelLowering.cpp',
           'llvm/lib/Target/PowerPC/PPCFastISel.cpp',
           'llvm/lib/Target/PowerPC/PPCISelLowering.h',
           'llvm/include/llvm/ExecutionEngine/RuntimeDyld.h',
           'llvm/include/llvm/Config/llvm-config.h.cmake',
           'llvm/lib/Target/X86/X86ISelLoweringCall.cpp',
           'llvm/lib/Target/Sparc/TargetInfo/SparcTargetInfo.cpp',
           'llvm/lib/CodeGen/AsmPrinter/AsmPrinter.cpp',
           'llvm/lib/Target/SystemZ/SystemZElimCompare.cpp',
           'llvm/lib/Target/RISCV/RISCVISelLowering.cpp',
           'llvm/lib/Target/RISCV/RISCVCallingConv.cpp',
           'llvm/lib/Target/RISCV/RISCVInstrInfo.td',
           'llvm/lib/Target/RISCV/RISCVFrameLowering.cpp',
           'llvm/lib/Target/RISCV/RISCVMachineFunctionInfo.h',
           'llvm/lib/Target/RISCV/RISCVMachineFunctionInfo.cpp',
           'llvm/lib/Target/Mips/MipsSEInstrInfo.cpp', 'llvm/lib/Target/Mips/MipsRegisterInfo.td',
           'llvm/lib/Target/VE/VEInstrInfo.cpp', 'llvm/lib/Target/AVR/AVRShiftExpand.cpp',
           'llvm/lib/Target/MSP430/MSP430ISelLowering.cpp',
           'llvm/lib/CodeGen/SelectionDAG/LegalizeDAG.cpp',
           'llvm/lib/Target/Mips/MipsSEISelLowering.cpp',
           'llvm/lib/Target/Mips/Mips32r6InstrInfo.td',
           'llvm/lib/Target/Mips/MicroMips32r6InstrInfo.td',
           'llvm/lib/Target/Mips/Mips16HardFloat.cpp',
           'llvm/lib/Target/Mips/Mips16ISelLowering.cpp',
           'llvm/lib/Target/Mips/MicroMipsInstrFPU.td',
           'llvm/lib/Target/Mips/MipsInstrInfo.cpp',
           'llvm/lib/Target/Mips/MicroMipsInstrInfo.td',
           'llvm/lib/Target/Mips/MCTargetDesc/MipsMCCodeEmitter.cpp',
           'llvm/lib/ExecutionEngine/RuntimeDyld/RuntimeDyld.cpp',
           'llvm/lib/ExecutionEngine/RuntimeDyld/RuntimeDyldELF.cpp',
           'llvm/lib/ExecutionEngine/RuntimeDyld/RuntimeDyldELF.h',
           'llvm/lib/ExecutionEngine/RuntimeDyld/RuntimeDyldImpl.h'}
CHANGED |= {'llvm/include/llvm/IR/IRBuilder.h', 'llvm/lib/Target/PowerPC/PPCAsmPrinter.cpp',
            'llvm/lib/Target/Mips/MipsBranchExpansion.cpp', 'llvm/lib/Target/ARM/ARMFastISel.cpp'}
CHANGED |= {
    'llvm/lib/Target/LoongArch/LoongArchExpandPseudoInsts.cpp',
    'llvm/lib/Target/LoongArch/LoongArchFrameLowering.cpp',
    'llvm/lib/Target/LoongArch/LoongArchISelLowering.cpp',
    'llvm/lib/Target/LoongArch/LoongArchInstrInfo.td',
    'llvm/lib/Target/LoongArch/LoongArchMachineFunctionInfo.h',
    'llvm/lib/Target/LoongArch/MCTargetDesc/LoongArchMCTargetDesc.cpp',
    'llvm/lib/Target/Mips/Mips64InstrInfo.td',
    'llvm/lib/Target/Mips/MipsDelaySlotFiller.cpp',
    'llvm/lib/Target/Mips/MipsFrameLowering.cpp',
    'llvm/lib/Target/Mips/MipsFrameLowering.h',
    'llvm/lib/Target/Mips/MipsISelLowering.h',
    'llvm/lib/Target/Mips/MipsInstrInfo.td',
    'llvm/lib/Target/Mips/MipsMachineFunction.h',
    'llvm/lib/Target/Mips/MipsSEFrameLowering.cpp',
    'llvm/lib/Target/Mips/MipsScheduleGeneric.td',
}
BUILD_POLICY_CHANGED = {'llvm/lib/CodeGen/CMakeLists.txt', 'llvm/cmake/config-ix.cmake'}
ADDED = {'llvm/test/CodeGen/SystemZ/load-trap-motion.ll',
         'llvm/test/CodeGen/RISCV/rtdyld-call-range.s',
         'llvm/test/CodeGen/RISCV/rtdyld-call-range.check',
         'llvm/test/CodeGen/RISCV/Inputs/rtdyld-call-range.cpp',
         'llvm/test/CodeGen/Mips/r6-condition-copy.mir',
         'llvm/test/CodeGen/Mips/r6-fgr64cc-spill.mir',
         'llvm/test/CodeGen/Mips/r6-shared-select-condition.ll',
         'llvm/test/CodeGen/VE/extend-stack-liveins.ll',
         'llvm/test/CodeGen/AVR/vector-variable-shifts.ll',
         'llvm/test/CodeGen/MSP430/select-call-frame.ll',
         'llvm/test/CodeGen/Mips/r6-fp-predicate-inversion.ll',
         'llvm/test/CodeGen/Mips/mips16-callframe.ll',
         'llvm/test/CodeGen/Mips/mips16-constrained-intrinsic.ll',
         'llvm/test/CodeGen/Mips/micromips-strict-arithmetic.ll',
         'llvm/test/CodeGen/Mips/micromips-r6-minmax.ll',
         'llvm/test/CodeGen/Mips/micromips-r6-compact-control.ll',
         'llvm/test/CodeGen/Mips/micromips-r6-physical-copies.ll',
         'llvm/test/CodeGen/AArch64/rtdyld-stub-byte-order.ll',
         'llvm/test/CodeGen/AArch64/rtdyld-stub-le.check',
         'llvm/test/CodeGen/AArch64/rtdyld-stub-be.check',
         'llvm/test/CodeGen/AArch64/rtdyld-pcrel-full-width.ll',
         'llvm/test/CodeGen/AArch64/rtdyld-pcrel-le.check',
         'llvm/test/CodeGen/AArch64/rtdyld-pcrel-be.check',
         'llvm/test/CodeGen/RISCV/rtdyld-local-symbols.ll',
         'llvm/test/CodeGen/RISCV/rtdyld-local-symbols.check'}
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('archive', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
with args.archive.open('rb') as stream:
    actual = hashlib.file_digest(stream, 'sha256').hexdigest()
if actual != ARCHIVE_SHA256:
    raise SystemExit('Not the pinned official source archive')
expected = dict(line.split('  ', 1)[::-1] for line in (root/'sources.sha256').read_text().splitlines())
digests, links = {}, {}
prefix = 'llvm-project-23.1.1.src/'
with tarfile.open(args.archive, mode='r|xz') as archive:
    for entry in archive:
        if not entry.name.startswith(prefix):
            continue
        relative = entry.name[len(prefix):]
        if entry.isfile():
            # Hash all regular members: a retained symlink can point at a
            # target appearing earlier or later, without extracting any path.
            digests[relative] = hashlib.file_digest(archive.extractfile(entry), 'sha256').hexdigest()
        elif entry.issym():
            import posixpath
            links[relative] = posixpath.normpath(posixpath.join(posixpath.dirname(relative), entry.linkname))
        elif entry.islnk():
            links[relative] = entry.linkname.removeprefix(prefix)
changed, added = set(), set()
for relative, digest in expected.items():
    current = hashlib.sha256((root/relative).read_bytes()).hexdigest()
    if current != digest:
        raise SystemExit('Vendor/manifest mismatch: ' + relative)
    target, seen = relative, set()
    while target in links:
        if target in seen:
            raise SystemExit('Archive symlink cycle: ' + relative)
        seen.add(target)
        target = links[target]
    if target not in digests:
        added.add(relative)
    elif digests[target] != digest:
        changed.add(relative)
if changed != CHANGED | BUILD_POLICY_CHANGED or added != ADDED:
    raise SystemExit(f'Unexpected downstream changes: edited={sorted(changed)}, added={sorted(added)}')
print(f'PASS: {len(expected)} retained files; exactly {len(CHANGED)} documented compiler/config edits, {len(BUILD_POLICY_CHANGED)} host build policies, and {len(added)} new tests')
