#!/usr/bin/env python3
"""Compare all native code and relocations with/without a dead throw_ref.

This verifies zero generated-code cost, not retained exnref runtime support or
process performance. Run inside the remote Linux cgroup against a frozen CLI.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
from check_wasm3_native_frame_codegen import decode_object
from elf_executable_sections import write_executable_evidence, normalize_relocations, compare_executable_images


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--uwvm', type=Path, required=True)
    p.add_argument('--wasm-tools', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--ros', action='store_true')
    p.add_argument('--llvm', type=Path, default=Path('/toolchain/bin'))
    a = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    a.out.mkdir(parents=True, exist_ok=False)
    fixtures = {
        'prefix': '''(module (func (export "_start")
          i32.const 42 block br 0 throw_ref end
          i32.const 42 i32.ne if unreachable end))''',
        'memory-loop': '''(module (memory 1)
          (func $step (param i32) (result i32)
            block br 0 throw_ref end
            i32.const 0 local.get 0 i32.store i32.const 0 i32.load)
          (func (export "_start") (local $i i32) (local $sum i32)
            loop $again local.get $sum local.get $i call $step i32.add local.set $sum
              local.get $i i32.const 1 i32.add local.tee $i i32.const 128 i32.lt_u br_if $again
            end local.get $sum i32.const 8128 i32.ne if unreachable end))''',
    }
    rows, commands = [], []

    def run(command, output):
        command = list(map(str, command))
        r = subprocess.run(command, capture_output=True, timeout=90)
        output.write_bytes(r.stdout + r.stderr)
        commands.append(dict(command=command, exit=r.returncode))
        (a.out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        if r.returncode != 0:
            raise RuntimeError(f'command failed: {command}\n{output.read_text(errors="replace")}')
        return r.stdout.decode(errors='replace')

    for name, source in fixtures.items():
        for policy in ['instruction', 'unwind']:
            results = []
            for variant in ['without', 'with']:
                out = a.out / (name + '-' + policy + '-' + variant)
                out.mkdir()
                wat, wasm, cache = out / 'input.wat', out / 'input.wasm', out / 'cache'
                wat.write_text(source.replace('throw_ref', '') if variant == 'without' else source)
                run([a.wasm_tools, 'parse', wat, '-o', wasm], out / 'parse.log')
                cache.mkdir()
                mode = ['-Raot'] if a.ros else ['-Rcc', 'jit', '-Rcm', 'full']
                run([a.uwvm] + mode + ['-WFE-exceptions', '-Rct', '0', '-Rllvm-call-stack', policy,
                    '-Rllvm-cache-path', 'path', cache, '--run', wasm], out / 'run.log')
                objects = list(cache.rglob('*.uwvm-ljc'))
                if len(objects) != 1:
                    raise RuntimeError(f'expected one complete module object: {objects}')
                native = out / 'native.o'
                native.write_bytes(decode_object(objects[0].read_bytes(), a.ros))
                run([a.llvm / 'llvm-objdump', '-dr', native], out / 'native.s')
                machine = write_executable_evidence(native, out)
                relocations = run([a.llvm / 'llvm-readobj', '--relocations', native], out / 'relocations.txt')
                results.append((machine, normalize_relocations(relocations)))
            compare_executable_images(results[0][0], results[1][0])
            if results[0][1] != results[1][1]:
                raise RuntimeError(f'{name}/{policy}: complete object relocations differ')
            rows.append(dict(case=name, policy=policy, **results[0][0].summary(), identical_relocations=True))
    (a.out / 'summary.json').write_text(json.dumps(dict(passed=True, comparisons=len(rows), checks=rows,
        binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest()), indent=2) + '\n')
    print('PASS dead throw_ref codegen:', len(rows), 'complete executable/relocation comparisons')


if __name__ == '__main__':
    main()
