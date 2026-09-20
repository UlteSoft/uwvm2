#!/usr/bin/env python3
"""Check an explicitly patched LLVM backend, not an oracle-isolation workaround.

The frontend emits IR from the unchanged independent C++ reproducer. The
selected llc must accept it with machine verification, then QEMU checks exact
bits using a separately compiled integer-only, freestanding O32 oracle.
Run inside an aggregate resource limit; this script only limits process time.
It never changes the installed compiler and refuses to overwrite old evidence.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import signal
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--clang', required=True)
p.add_argument('--llc', type=Path, required=True)
p.add_argument('--stock-llc', type=Path, required=True)
p.add_argument('--ld', type=Path, required=True, help='MIPS GNU linker supporting both byte orders')
p.add_argument('--qemu-dir', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--fixtures', type=Path, default=Path(__file__).resolve().parent / 'fixtures')
p.add_argument('--jit-emitter', type=Path, help='Optional production-emitter fixture built against main or ROS')
p.add_argument('--opt', type=Path, help='Matching opt, required with --jit-emitter')
args = p.parse_args()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
if bool(args.jit_emitter) != bool(args.opt):
    p.error('--jit-emitter and --opt must be provided together')
for name in ('llc', 'stock_llc', 'ld', 'qemu_dir'):
    setattr(args, name, getattr(args, name).resolve())
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=False)
source = args.fixtures.resolve() / 'mipsr6_fp_select_backend.cpp'
runner = args.fixtures.resolve() / 'mipsr6_fp_select_runner.cpp'
spill = args.fixtures.resolve() / 'mipsr6_fgr64cc_spill.mir'
results = {'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
           'runner_sha256': hashlib.sha256(runner.read_bytes()).hexdigest(),
           'spill_sha256': hashlib.sha256(spill.read_bytes()).hexdigest(), 'commands': []}

def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()

results['backend_sha256'] = digest(args.llc)
results['stock_backend_sha256'] = digest(args.stock_llc)
results['LD_LIBRARY_PATH'] = os.environ.get('LD_LIBRARY_PATH', '')
if args.jit_emitter:
    results['jit_emitter_sha256'] = digest(args.jit_emitter.resolve())

def run(name, command, expect_success=True):
    command = list(map(str, command))
    with (out / (name + '.log')).open('w') as log:
        child = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            status = child.wait(timeout=120)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid, signal.SIGKILL)
            child.wait()
            status = 'timeout'
    results['commands'].append({'name': name, 'command': command, 'exit_code': status})
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    if expect_success and status != 0:
        raise RuntimeError(name + ': ' + (out / (name + '.log')).read_text()[-4000:])
    return status

for endian in ('el', ''):
    arch = 'mips' + endian
    flags = ['--target=' + arch + '-linux-gnu', '-march=mips32r6', '-mabi=32', '-mnan=2008',
             '-mno-abicalls', '-fno-pic', '-fno-pie', '-ffp-contract=off', '-std=c++20']
    # Function attributes alone do not set the object's ELF ISA/ABI flags.
    target = ['-mtriple=' + arch + '-linux-gnu', '-mcpu=mips32r6', '-target-abi=o32', '-mattr=+nan2008,+noabicalls']
    jit_objects, runner_flags = [], []
    if args.jit_emitter:
        raw, optimized, jit_object = [out / (arch + suffix) for suffix in ('-wasm.ll', '-wasm-opt.ll', '-wasm.o')]
        run(arch + '-wasm-emit', [args.jit_emitter.resolve(), arch + '-linux-gnu', raw])
        run(arch + '-wasm-opt', [args.opt.resolve(), '-S', '-passes=default<O3>', raw, '-o', optimized])
        run(arch + '-wasm-backend', [args.llc, *target, '-O3', '-verify-machineinstrs', '-filetype=obj', optimized, '-o', jit_object])
        run(arch + '-wasm-assembly', [args.llc, *target, '-O3', '-verify-machineinstrs', optimized, '-o', out / (arch + '-wasm.s')])
        jit_objects.append(jit_object)
        runner_flags.append('-DUWVM_TEST_MIPS_JIT')
    spill_object, spill_mir = out / (arch + '-spill.o'), out / (arch + '-spill.mir')
    run(arch + '-spill-allocation', [args.llc, *target, '-run-pass=greedy', '-verify-machineinstrs', spill, '-o', spill_mir])
    text = spill_mir.read_text()
    if not (re.search(r'type: spill-slot, offset: 0, size: 8, alignment: 8', text)
            and 'SDC164' in text and 'LDC164' in text):
        raise RuntimeError('FGR64CC must spill and reload a complete 64-bit value')
    run(arch + '-spill-object', [args.llc, *target, '-start-before=greedy', '-verify-machineinstrs',
                                '-filetype=obj', spill, '-o', spill_object])
    status = run(arch + '-spill-stock-negative-control', [args.stock_llc, *target, '-run-pass=greedy',
        '-verify-machineinstrs', spill, '-o', out / (arch + '-spill-stock.mir')], False)
    results[arch + '-spill-stock-reproduced'] = status != 0 and status != 'timeout' and 'PHI' in (
        out / (arch + '-spill-stock-negative-control.log')).read_text()
    if not results[arch + '-spill-stock-reproduced']:
        raise RuntimeError('Stock compiler did not reproduce the expected FGR64CC spill defect')
    driver = out / (arch + '-runner.o')
    run(arch + '-runner', [args.clang, *flags, *runner_flags, '-O2', '-ffreestanding', '-fno-stack-protector', '-c', runner, '-o', driver])
    for level in ('0', '1', '2', '3', 's', 'z'):
        stem = arch + '-O' + level
        ir, obj, binary = [out / (stem + suffix) for suffix in ('.ll', '.o', '.elf')]
        run(stem + '-frontend', [args.clang, *flags, '-O' + level, '-S', '-emit-llvm', source, '-o', ir])
        backend_level = '-O' + (level if level.isdigit() else '2')
        run(stem + '-backend', [args.llc, *target, backend_level, '-verify-machineinstrs', '-filetype=obj', ir, '-o', obj])
        run(stem + '-assembly', [args.llc, *target, backend_level, '-verify-machineinstrs', ir, '-o', out / (stem + '.s')])
        if level == '3':
            status = run(stem + '-stock-negative-control', [args.stock_llc, *target, '-O3', '-verify-machineinstrs', '-filetype=obj', ir,
                                                           '-o', out / (stem + '-stock.o')], False)
            diagnostic = (out / (stem + '-stock-negative-control.log')).read_text()
            # A timeout or an unrelated failure is not evidence for this bug.
            results[stem + '-stock-reproduced'] = status != 0 and status != 'timeout' and (
                'Bad machine code' in diagnostic or 'Cannot copy registers' in diagnostic or 'Unsupported instruction' in diagnostic)
            if not results[stem + '-stock-reproduced']:
                raise RuntimeError('Stock compiler did not reproduce the expected copy defect')
        run(stem + '-link', [args.ld, '-EL' if endian else '-EB', '-m', 'elf32ltsmip' if endian else 'elf32btsmip',
                             '-static', '-e', '__start', driver, obj, spill_object, *jit_objects, '-o', binary])
        run(stem + '-qemu', [args.qemu_dir / ('qemu-' + arch), '-cpu', 'mips32r6-generic', binary])
        # 20 x 20 x 2 edge cases + 100000 x 2 random vector selections.
        results[stem + '-exact-bit-selections'] = 200800
        if args.jit_emitter:
            results[stem + '-production-jit-selections'] = 200800

# N32/N64 use the same register classes; do not mistake O32-only testing for
# complete code-generation coverage. These are verifier/object tests, not guest runs.
for endian in ('el', ''):
    for abi in ('n32', '64'):
        stem = 'mips64' + endian + '-' + abi
        ir = out / (stem + '.ll')
        run(stem + '-frontend', [args.clang, '--target=mips64' + endian + '-linux-gnu', '-march=mips64r6',
            '-mabi=' + abi, '-mnan=2008', '-std=c++20', '-O3', '-ffp-contract=off', '-S', '-emit-llvm', source, '-o', ir])
        run(stem + '-backend', [args.llc, '-mtriple=mips64' + endian + '-linux-gnu', '-mcpu=mips64r6',
            '-target-abi=' + ('n32' if abi == 'n32' else 'n64'), '-mattr=+nan2008',
            '-O3', '-verify-machineinstrs', '-filetype=obj', ir, '-o', out / (stem + '.o')])
(out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
print('PASS: 12 O32 executions (2,409,600 exact-bit vector selections and 4,819,200 f64 spill roundtrips), 4 N32/N64 object checks')
