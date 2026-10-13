#!/usr/bin/env python3
"""Qualify actual Wasm safe-point IR and all executable sections/relocations.

This emits native objects with LLVM O3 but does not execute generated guests;
the runtime cooperative-pause suite qualifies actual execution independently.
An optional probe built against the frozen pre-change emitter establishes a
real before/after baseline, not just two settings of the new implementation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess
from elf_executable_sections import (read_executable_object, compare_executable_images,
                                     normalize_relocations, write_executable_evidence)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--baseline-probe', type=Path)
    p.add_argument('--baseline-objects', type=Path, help='retained qualified baseline-policy objects from a previous probe run')
    p.add_argument('--guard', type=Path, required=True)
    p.add_argument('--llvm', type=Path, default=Path('/toolchain/bin'))
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    if a.baseline_probe and a.baseline_objects:
        p.error('select one baseline source')
    subprocess.run(['bash', str(a.guard)], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    a.out.mkdir(parents=True, exist_ok=False)
    commands, rows = [], []

    def run(command, destination):
        command = list(map(str, command))
        result = subprocess.run(command, capture_output=True, timeout=180)
        destination.write_bytes(result.stdout)
        destination.with_suffix(destination.suffix + '.stderr').write_bytes(result.stderr)
        commands.append(dict(command=command, exit=result.returncode))
        (a.out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        result.check_returncode()
        return result.stdout.decode()

    def materialize(ir):
        name = ir.with_suffix('')
        optimized = name.with_suffix('.optimized.ll')
        obj = name.with_suffix('.o')
        run([a.llvm / 'opt', '-passes=default<O3>', '-S', ir, '-o', optimized], name.with_suffix('.opt.log'))
        run([a.llvm / 'llc', '-O3', '-filetype=obj', '-code-model=large', '-relocation-model=pic', optimized, '-o', obj], name.with_suffix('.llc.log'))
        relocations = run([a.llvm / 'llvm-readobj', '--relocations', obj], name.with_suffix('.relocations.txt'))
        run([a.llvm / 'llvm-objdump', '-dr', obj], name.with_suffix('.assembly.txt'))
        sections = obj.parent / (obj.stem + '-sections')
        sections.mkdir()
        return write_executable_evidence(obj, sections), normalize_relocations(relocations)

    for policy in ['instruction', 'unwind']:
        current = a.out / policy
        current.mkdir()
        run([a.probe, current, policy], current / 'probe.log')
        images = {name: materialize(current / (name + '.ll')) for name in ['off-default', 'off-full', 'on', 'instructions']}
        compare_executable_images(images['off-default'][0], images['off-full'][0])
        if images['off-default'][1] != images['off-full'][1]:
            raise RuntimeError('disabled full provenance changes complete relocations')
        before_after = False
        if a.baseline_probe:
            baseline = a.out / ('baseline-' + policy)
            baseline.mkdir()
            run([a.baseline_probe, baseline, policy], baseline / 'probe.log')
            old_image, old_relocations = materialize(baseline / 'off-default.ll')
            compare_executable_images(old_image, images['off-default'][0])
            if old_relocations != images['off-default'][1]:
                raise RuntimeError('disabled current emitter changes baseline relocations')
            before_after = True
        elif a.baseline_objects:
            previous = a.baseline_objects / ('baseline-' + policy)
            if previous.joinpath('fixture.wasm').read_bytes() != current.joinpath('fixture.wasm').read_bytes():
                raise RuntimeError('retained baseline Wasm differs from the current fixture')
            baseline = a.out / ('baseline-' + policy)
            baseline.mkdir()
            for name in ['fixture.wasm', 'off-default.ll', 'off-default.optimized.ll', 'off-default.o']:
                (baseline / name).write_bytes((previous / name).read_bytes())
            old_image = write_executable_evidence(baseline / 'off-default.o', baseline)
            old_relocations = run([a.llvm / 'llvm-readobj', '--relocations', baseline / 'off-default.o'], baseline / 'off-default.relocations.txt')
            compare_executable_images(old_image, images['off-default'][0])
            if normalize_relocations(old_relocations) != images['off-default'][1]:
                raise RuntimeError('disabled current emitter changes retained baseline relocations')
            (baseline / 'origin.json').write_text(json.dumps(dict(path=str(previous),
                object_sha256=hashlib.sha256((previous / 'off-default.o').read_bytes()).hexdigest()), indent=2) + '\n')
            before_after = True
        off_ir, on_ir = [(current / (name + '.ll')).read_text() for name in ['off-default', 'on']]
        off_symbols = set(re.findall(r'^declare [^\n]*@(uwvm_bridge_[A-Za-z0-9_]+)\(', off_ir, re.M))
        on_symbols = set(re.findall(r'^declare [^\n]*@(uwvm_bridge_[A-Za-z0-9_]+)\(', on_ir, re.M))
        added = on_symbols - off_symbols
        if len(added) != 1:
            raise RuntimeError('expected exactly one new host safe-point declaration')
        symbol = added.pop()
        if symbol in images['off-default'][1] or symbol not in images['on'][1] or symbol not in images['instructions'][1]:
            raise RuntimeError('safe-point executable relocation presence is incorrect')
        rows.append(dict(policy=policy, disabled_provenance_identical=True,
                         before_after_identical=before_after, bridge=symbol,
                         disabled=images['off-default'][0].summary(), enabled=images['on'][0].summary(),
                         instruction_debug=images['instructions'][0].summary()))
    summary = dict(passed=True, probe_sha256=hashlib.sha256(a.probe.read_bytes()).hexdigest(),
                   checks=rows, scope='actual Wasm translation and LLVM O3 object generation; no generated guest execution')
    if a.baseline_probe:
        summary['baseline_probe_sha256'] = hashlib.sha256(a.baseline_probe.read_bytes()).hexdigest()
    (a.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('PASS entry/loop/musttail/mode IR; disabled complete executable sections and relocations')


if __name__ == '__main__':
    main()
