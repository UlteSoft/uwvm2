#!/usr/bin/env python3
"""Inspect actual private-cache objects emitted and executed by the target VM.

The bounded existing decoder is inspection-only; it grants no cache-execution
authority. Normal VM execution handles authentication. Counts are evidence to
review, not an assertion of hardware throughput.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import stat
import subprocess
from run_matrix import digest, save, verify, execute
from architecture_catalog import LINUX_PROFILES
from elf_function_extent import function_extent


def instruction_evidence(assembly, arch):
    profile_arches = {row['id']: row['llvm_arch'] for row in LINUX_PROFILES}
    llvm_arch = profile_arches.get(arch, arch)
    groups = {
        'x86': 'i686', 'aarch64_be': 'aarch64',
        'armeb': 'arm', 'thumb': 'arm', 'thumbeb': 'arm',
        'riscv32': 'riscv64', 'riscv32be': 'riscv64', 'riscv64be': 'riscv64',
        'ppc': 'ppc64', 'ppcle': 'ppc64', 'ppc64le': 'ppc64',
        'mips': 'mips64', 'mipsel': 'mips64', 'mips64el': 'mips64',
        'sparcv9': 'sparc64', 'sparc': 'sparc64', 'sparcel': 'sparc64',
        'loongarch32': 'loongarch64', 'systemz': 's390x',
    }
    group = groups.get(llvm_arch, llvm_arch)
    rules = {
        'x86_64': (r'\bjmpq?\s+\*', r'\bcallq?\s'),
        'i686': (r'\bjmp[lq]?\s+\*', r'\bcall[lq]?\s'),
        'aarch64': (r'\bbr\s+x(?!30\b)\d+', r'\bbl(?:r)?\s'),
        'arm': (r'\bbx\s+(?:r(?:[0-9]|1[0-2])|ip)\b|\bldr\s+pc\s*,\s*\[(?!sp\b|r13\b)', r'\bblx?\s'),
        'riscv64': (r'\bjalr\s+zero\s*,\s*(?:0x0|0)\((?!ra\b)\w+\)|\bc\.jr\s+(?!ra\b)\w+',
                    r'\b(?:jal|jalr)\s+ra\s*,|\bc\.jalr\s'),
        'ppc64': (r'\bbctr\b', r'\b(?:bl|bctrl)\b'),
        'mips64': (r'\b(?:jr|jrc|jic)\s+\$?(?:t9|25)\b|\bjalr\s+\$?(?:zero|0)\s*,\s*\$?(?:t9|25)\b',
                   r'\b(?:jal|bal|balc|jialc)\b|\bjalr\s+(?!\$?(?:zero|0)\s*,)'),
        'sparc64': (r'\bjmp\s+%(?:[gl][0-7]|[oi][0-6])|\bjmpl\s+%(?:[gl][0-7]|[oi][0-6])[^\n]*,\s*%g0\b',
                    r'\bcall\b|\bjmpl\s+[^\n]*,\s*%o7\b'),
        # LLVM prints the zero-link JIRL as JR; exclude both RA spellings.
        'loongarch64': (r'\bjirl\s+\$?(?:zero|r0)\s*,\s*\$?(?!ra\b|r1\b)\w+|\bjr\s+\$?(?!ra\b|r1\b)\w+',
                        r'\bbl\b|\bjirl\s+\$?(?:ra|r1)\b'),
        's390x': (r'\bbr\s+%r(?!14\b)\d+\b', r'\b(?:brasl|basr|bras)\s+%r14\b'),
        'hexagon': (r'\bjumpr\s+r(?!31\b)\d+\b', r'\b(?:call|callr)\s'),
        'm68k': (r'\bjmp\s+', r'\b(?:jsr|bsr)\s'),
    }
    jump, call = rules[group]
    instructions = []
    for line in assembly.splitlines():
        match = re.match(r'^\s*[0-9a-f]+:\s+([A-Za-z][A-Za-z0-9_.]*)\b', line)
        if match and not match[1].startswith('R_'):
            instructions.append(line)
    code = '\n'.join(instructions)
    direct_tail_jumps = 0
    if group == 'aarch64':
        opcodes = {int(m[1], 16): m[2] for m in re.finditer(
            r'^\s*([0-9a-f]+):\s+([A-Za-z][A-Za-z0-9_.]*)', code, re.M)}
        for match in re.finditer(
                r'^\s*([0-9a-f]+):\s+R_AARCH64_JUMP26\s+(uwvm_m_[0-9a-f]+_func_\d+)\s*$', assembly, re.M):
            assert opcodes[int(match[1], 16)] == 'b'
            direct_tail_jumps += 1
    fp_pattern = r'\b(?:v?(?:add|sub|mul|div|sqrt|round)(?:ss|sd|ps|pd)|f(?:add|sub|mul|div|sqrt|rint)[.a-z]*|frint\w*|vfmul\w*)\b'
    if group == 'mips64':
        fp_pattern = r'\b(?:add|sub|mul|div|sqrt)\.[sd]\b|\b(?:ceil|floor|round|trunc)\.[wl]\.[sd]\b'
    return dict(direct_wasm_tail_jumps=direct_tail_jumps,
                indirect_linkless_jumps=len(re.findall(jump, code)),
                link_setting_calls=len(re.findall(call, code)),
                instruction_lines=len(instructions),
                architecture_group=group,
                relocation_lines_excluded=True,
                fp_instruction_lines=[line for line in instructions if re.search(fp_pattern, line)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    parser.add_argument('output', type=Path)
    a = parser.parse_args()
    config = json.loads(a.config.read_text())
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    verify(config['pins'])
    spec = importlib.util.spec_from_file_location('bounded_private_object_decoder', config['decoder'])
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    fixtures = {f['name']: f for f in json.loads(Path(config['fixtures']).read_text())['fixtures']}
    rows = []
    for name in config['cases']:
        fixture = fixtures[name]
        assert digest(fixture['path']) == fixture['sha256']
        for level in (1, 2, 3):
            for policy in config.get('stack_policies', ['instruction', 'unwind']):
                label = f'{name}-O{level}-{policy}'
                work = out / label
                work.mkdir()
                cache = work / 'cache'
                cache.mkdir()
                mode = ['-Raot'] if config['ros'] else ['-Rcc', 'jit', '-Rcm', 'full']
                prefix = list(config.get('prefix', []))
                object_source = config.get('object_source', 'private-cache')
                obj = work / 'actual.o'
                if object_source == 'exclusive-test-capture':
                    # Only separately qualified test-macro products may emit
                    # these bytes. The ordinary product/cache policy is kept.
                    assert config['test_capture_macro_qualified'] is True
                    assert prefix and Path(prefix[0]).name.startswith('qemu-')
                    prefix += ['-U', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT',
                               '-E', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT=' + str(obj)]
                    cache_flags = ['-Rllvm-cache-path', 'disable']
                else:
                    assert object_source == 'private-cache', object_source
                    cache_flags = ['-Rllvm-cache-path', 'path', cache.name]
                argv = [*prefix, config['product'], *mode, '-Rct', '0',
                        '-Rllvm-full-policy', f'pb-o{level}', '-Rllvm-call-stack', policy,
                        *cache_flags, '-Rclog', 'file', 'compiler.log',
                        *fixture['argv'], '--run', fixture['path']]
                row = execute(argv, work / 'run.log', work, config.get('timeout', 120))
                assert row['exit'] == 0, row
                entries = list(cache.rglob('*.uwvm-ljc'))
                if object_source == 'exclusive-test-capture':
                    assert not entries, (label, entries)
                    info = obj.lstat()
                    assert stat.S_ISREG(info.st_mode) and info.st_nlink == 1
                    assert stat.S_IMODE(info.st_mode) == 0o600
                    assert obj.read_bytes()[:4] == b'\x7fELF'
                    cache_hash = None
                else:
                    assert len(entries) == 1, (label, entries)
                    obj.write_bytes(module.decode_object(entries[0].read_bytes(), config['ros']))
                    cache_hash = digest(entries[0])
                symbols = subprocess.check_output([config['llvm_nm'], '--defined-only', str(obj)], text=True)
                functions = [0, 1] if name.startswith('tail-') else [0]
                evidence = []
                for index in functions:
                    found = re.findall(r'\b(uwvm_m_[0-9a-f]+_func_' + str(index) + r')$', symbols, re.M)
                    assert len(found) == 1, (label, index, found)
                    direct_extent_arches = {'x86_64', 'i686', 'aarch64', 'aarch64_be',
                                            'riscv64', 'riscv32', 'riscv32be', 'riscv64be'}
                    if config['arch'] in direct_extent_arches:
                        section, start, size = function_extent(obj.read_bytes(), found[0])
                        command = [config['llvm_objdump'], '-dr', '--no-show-raw-insn',
                                   '--section=' + section, '--start-address=' + str(start),
                                   '--stop-address=' + str(start + size)]
                        disassembly_extent = 'bounded-ELF-function'
                    else:
                        # Descriptor/Thumb ABIs need their own entry resolution.
                        # Preserve their existing selector until separately qualified.
                        command = [config['llvm_objdump'], '-dr', '--no-show-raw-insn',
                                   '--disassemble-symbols=' + found[0]]
                        disassembly_extent = 'llvm-symbol-selector'
                    if config['arch'].startswith('riscv'):
                        command += ['-M', 'no-aliases']
                    assembly = subprocess.check_output([*command, str(obj)], text=True)
                    asm = work / f'function-{index}.asm'
                    asm.write_text(assembly)
                    counts = instruction_evidence(assembly, config['arch'])
                    if name.startswith('tail-'):
                        assert counts['indirect_linkless_jumps'] + counts['direct_wasm_tail_jumps'] > 0, (label, index, assembly)
                    evidence.append(dict(function=found[0], disassembly_extent=disassembly_extent,
                                         assembly_sha256=digest(asm), **counts))
                row.update(case=name, optimization=level, stack_policy=policy,
                           object_sha256=digest(obj), cache_sha256=cache_hash, object_source=object_source,
                           functions=evidence,
                           cold_trap_call_paths_qualified=False, hardware_efficiency_qualified=False)
                rows.append(row)
                save(out / 'checkpoint.json', dict(state='running', rows=rows))
                print('PASS executed actual object', label, flush=True)
    verify(config['pins'])
    save(out / 'summary.json', dict(passed=True, rows=rows, config_sha256=digest(a.config),
        product_sha256=digest(config['product']), scope=config['scope'],
        hardware_efficiency_qualified=False, cold_trap_call_paths_qualified=False))


if __name__ == '__main__':
    main()
