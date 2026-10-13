#!/usr/bin/env python3
"""Target-specific llvm-config adapter for a separately qualified Windows LLVM.

This adapter deliberately has no fallback to a Linux LLVM installation.  The
qualification builder sets both environment variables and checks the complete
COFF archive closure before and after using this file.  In particular, the
consumer response file keeps CMake's library order and necessary repetitions.
"""

import json
import os
from pathlib import Path
import sys


COMPONENTS = (
    'core support analysis target linker executionengine mcjit runtimedyld '
    'passes scalaropts transformutils instcombine bitreader bitwriter object '
    'targetparser x86 x86disassembler nativecodegen'
)


def main() -> None:
    root_name = os.environ.get('UWVM_WINDOWS_LLVM_BUILD')
    source_name = os.environ.get('UWVM_WINDOWS_LLVM_SOURCE')
    if not root_name or not source_name:
        raise SystemExit('Windows LLVM build and source must be explicit')
    root = Path(root_name).resolve(strict=True)
    source = Path(source_name).resolve(strict=True)
    certificate = json.loads((root / 'qualified-llvm.json').read_text())
    if certificate.get('host_target') != 'x86_64-w64-windows-gnu':
        raise SystemExit('qualified LLVM is not Windows x64')
    if not (root / 'consumer-link.rsp').is_file():
        raise SystemExit('missing ordered Windows LLVM consumer link response')
    if not (root / 'include/llvm/Config/llvm-config.h').is_file():
        raise SystemExit('missing generated target LLVM headers')
    if not (source / 'llvm/include/llvm/IR/LLVMContext.h').is_file():
        raise SystemExit('missing external LLVM source headers')
    # Advertise the new direct component only when the actual qualified
    # consumer closure contains it. Frozen older certificates stay honest.
    components = COMPONENTS
    if 'lib/libLLVMDebugInfoDWARF.a' in certificate.get('archives', {}):
        components += ' debuginfodwarf'
    arguments = [argument for argument in sys.argv[1:]
                 if argument not in ('--link-static', '--link-shared')]
    if not arguments:
        raise SystemExit('llvm-config query required')
    query = arguments[0]
    if query in ('--libfiles', '--libs'):
        unknown = set(arguments[1:]) - set(components.split())
        if unknown:
            raise SystemExit('qualified target LLVM lacks requested components: '
                             + ', '.join(sorted(unknown)))
    answers = {
        '--version': certificate['llvm_version'],
        '--prefix': str(root),
        '--includedir': str(root / 'include'),
        '--libdir': str(root / 'lib'),
        '--host-target': certificate['host_target'],
        '--components': components,
        '--cxxflags': ' '.join(('-stdlib=libc++', '-I' + str(root / 'include'),
                              '-I' + str(source / 'llvm/include'))),
        '--libfiles': '@' + str(root / 'consumer-link.rsp'),
        '--libs': '@' + str(root / 'consumer-link.rsp'),
        '--system-libs': '',
    }
    if query not in answers:
        raise SystemExit(f'unsupported target LLVM query: {query}')
    if query not in ('--libfiles', '--libs') and len(arguments) != 1:
        raise SystemExit(f'unexpected arguments to {query}: {arguments[1:]}')
    print(answers[query])


if __name__ == '__main__':
    main()
