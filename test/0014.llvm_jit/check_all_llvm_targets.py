#!/usr/bin/env python3
"""Inventory every registered LLVM backend using production emission helpers.

Both assembly printing and object encoding must succeed. In particular, XCore
and NVPTX can print text without providing an MC object emitter usable by MCJIT.
This is code generation, NOT linking, full-VM or unwinder execution. Preserve failures
for diagnosis; GPUs, bytecode and microcontrollers are not automatically ROS
native JIT hosts just because LLVM registers them. See run_simd_cross_codegen
for the separate executable/QEMU matrix. Run under an aggregate resource cap.
Any failed probe makes the default exit status nonzero. --inventory-only is an
explicit diagnostic mode: it preserves failures, but completion is not PASS.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--tools', type=Path, required=True)
p.add_argument('--llvm-tools', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--extra-profiles', type=Path, help='JSON profiles with name/triple/cpu/features and optional fp_mode/llc_flags')
p.add_argument('--extras-only', action='store_true')
p.add_argument('--inventory-only', action='store_true',
               help='finish enumeration despite failed probes; NOT a passing regression gate')
a = p.parse_args()
a.out.mkdir(parents=True, exist_ok=False)
version = subprocess.check_output([a.llvm_tools/'llc', '--version'], text=True)
(a.out/'llvm-version.txt').write_text(version)
registered = re.findall(r'^\s+(\S+)\s+- ', version.split('Registered Targets:')[1], re.M)
# Explicit triples avoid relying on the executing host's default triple/ABI.
# Alias entries are tested too and remain visible in the inventory.
profiles = {
 'aarch64': ('aarch64-linux-gnu','generic','+neon'),
 'aarch64_32': ('aarch64_32-apple-watchos','generic','+neon'),
 'aarch64_be': ('aarch64_be-linux-gnu','generic','+neon'),
 'amdgpu': ('amdgcn-amd-amdhsa','gfx900',''),
 'arm': ('armv7-linux-gnueabihf','cortex-a9','+vfp3,+neon'),
 'armeb': ('armebv7-linux-gnueabihf','cortex-a9','+vfp3,+neon'),
 'avr': ('avr-unknown-unknown','atmega328p',''),
 'bpf': ('bpf-unknown-none','generic',''),
 'bpfeb': ('bpfeb-unknown-none','generic',''),
 'bpfel': ('bpfel-unknown-none','generic',''),
 'hexagon': ('hexagon-unknown-linux-musl','hexagonv60',''),
 'lanai': ('lanai-unknown-unknown','generic',''),
 'loongarch32': ('loongarch32-linux-gnusf','generic-la32',''),
 'loongarch64': ('loongarch64-linux-gnu','generic-la64','+f,+d,+lsx'),
 'mips': ('mips-linux-gnu','mips32r6',''),
 'mipsel': ('mipsel-linux-gnu','mips32r6',''),
 'mips64': ('mips64-linux-gnuabi64','mips64r6',''),
 'mips64el': ('mips64el-linux-gnuabi64','mips64r6',''),
 'msp430': ('msp430-unknown-unknown','generic',''),
 'nvptx': ('nvptx-nvidia-cuda','sm_50',''),
 'nvptx64': ('nvptx64-nvidia-cuda','sm_50',''),
 'ppc32': ('powerpc-linux-gnu','generic',''),
 'ppc32le': ('powerpcle-linux-gnu','generic',''),
 'ppc64': ('powerpc64-linux-gnu','pwr8','+altivec,+vsx'),
 'ppc64le': ('powerpc64le-linux-gnu','pwr8','+altivec,+vsx'),
 'r600': ('r600-unknown-unknown','redwood',''),
 'riscv32': ('riscv32-linux-gnu','generic-rv32','+m,+a,+f,+d,+c'),
 'riscv32be': ('riscv32be-linux-gnu','generic-rv32','+m,+a,+f,+d,+c'),
 'riscv64': ('riscv64-linux-gnu','generic-rv64','+m,+a,+f,+d,+c'),
 'riscv64be': ('riscv64be-linux-gnu','generic-rv64','+m,+a,+f,+d,+c'),
 'sparc': ('sparc-linux-gnu','v8',''),
 'sparcel': ('sparcel-unknown-linux-gnu','v8',''),
 'sparcv9': ('sparcv9-linux-gnu','v9',''),
 'spirv': ('spirv-unknown-unknown','', ''),
 'spirv32': ('spirv32-unknown-unknown','', ''),
 'spirv64': ('spirv64-unknown-unknown','', ''),
 'systemz': ('s390x-linux-gnu','z13','+vector'),
 'thumb': ('thumbv7-linux-gnueabihf','cortex-a9','+vfp3,+neon'),
 'thumbeb': ('thumbebv7-linux-gnueabihf','cortex-a9','+vfp3,+neon'),
 # Mirrors get_llvm_jit_host_target_attribute_storage: VE needs a machine-
 # level scalar-backend fallback, not an ignored per-function -vpu attribute.
 've': ('ve-unknown-linux-gnu','generic','-vpu'),
 'wasm32': ('wasm32-unknown-unknown','generic','+simd128'),
 'wasm64': ('wasm64-unknown-unknown','generic','+simd128'),
 'x86': ('i686-linux-gnu','pentium4','+sse2'),
 'x86-64': ('x86_64-linux-gnu','x86-64','+sse2'),
 'xcore': ('xcore-unknown-unknown','generic',''),
}
for alias, target in [('arm64','aarch64'), ('arm64_32','aarch64_32'), ('amdgcn','amdgpu')]:
    profiles[alias] = profiles[target]
assert set(registered) <= profiles.keys(), 'Add newly registered targets explicitly'
extras = json.loads(a.extra_profiles.read_text()) if a.extra_profiles else []
if a.extras_only:
    assert extras, '--extras-only requires profiles'
    registered = []
overrides = {}
for profile in extras:
    name = profile['name']
    assert re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', name) and name not in profiles
    profiles[name] = (profile['triple'], profile['cpu'], profile['features'])
    overrides[name] = profile
    registered.append(name)
rows = []
for name in registered:
    triple, cpu, features = profiles[name]
    directory = a.out/name
    directory.mkdir()
    row = dict(target=name, triple=triple, cpu=cpu, features=features, execution_tested=False, probes={})
    for probe in ('contract', 'simd'):
        prefix = directory/probe
        ir = prefix.with_suffix('.ll')
        mode = overrides.get(name, {}).get('fp_mode', 'i386' if name == 'x86' else 'native-nan' if name.startswith('sparc') else 'native')
        emit = ([str(a.tools/'llvm_jit_target_contract'), triple, cpu, features] if probe == 'contract' else
                [str(a.tools/'simd_direct_lowering'), triple, cpu, features, str(prefix), '--ir-only'])
        phases = [('emit', emit),
                  ('fp', [str(a.tools/'simd_cross_finalize'), str(ir), str(prefix)+'.fp.ll', mode]),
                  ('opt', [str(a.llvm_tools/'opt'), '-passes=default<O3>', str(prefix)+'.fp.ll', '-o', str(prefix)+'.bc']),
                  ('legalize', [str(a.tools/'simd_cross_finalize'), str(prefix)+'.bc', str(prefix)+'.final.ll', 'legalize']),
                  ('asm', [str(a.llvm_tools/'llc'), '-O3', '-verify-machineinstrs', '-mtriple='+triple,
                           # Do not reappend the original features: FP lowering
                           # can deliberately add -x87 to the function contract.
                           '-mcpu='+cpu,
                           *([] if re.match(r'^(?:x86_64|i[3-6]86)-', triple) else ['-mattr='+features]),
                           *overrides.get(name, {}).get('llc_flags', []),
                           str(prefix)+'.final.ll', '-o', str(prefix)+'.s'])]
        # Run the same finalized IR through the MC encoder too. Printing '.cfi'
        # or a pseudo instruction is not evidence that relocatable bytes exist.
        # Keep the assembly result even if the later object stage fails, so a
        # missing emitter cannot be hidden in a single aggregate PASS count.
        object_command = phases[-1][1].copy()
        object_command.insert(1, '-filetype=obj')
        object_command[object_command.index('-o') + 1] = str(prefix)+'.o'
        phases.append(('object', object_command))
        record = dict(phases=[], assembly_passed=False, object_passed=False)
        row['probes'][probe] = record
        for phase, command in phases:
            output = ir if probe == 'contract' and phase == 'emit' else directory/(probe+'-'+phase+'.log')
            with output.open('wb') as stdout, (directory/(probe+'-'+phase+'.stderr')).open('wb') as stderr:
                try:
                    status = subprocess.run(command, stdout=stdout, stderr=stderr, timeout=180).returncode
                except subprocess.TimeoutExpired:
                    status = 'timeout'
            record['phases'].append(dict(phase=phase, command=command, status=status))
            if phase == 'asm':
                record['assembly_passed'] = status == 0
            elif phase == 'object':
                record['object_passed'] = status == 0
            if status != 0:
                record['failed_phase'] = phase
                break
        record['codegen_passed'] = record['assembly_passed'] and record['object_passed']
        if record['assembly_passed']:
            asm = Path(str(prefix)+'.s').read_text(errors='replace')
            record['unwind_directives_present'] = any(t in asm for t in ('.cfi_startproc', '.seh_proc', '.fnstart'))
            record['assembly_sha256'] = hashlib.sha256(asm.encode()).hexdigest()
        if record['object_passed']:
            record['object_sha256'] = hashlib.sha256(Path(str(prefix)+'.o').read_bytes()).hexdigest()
        (directory/'result.json').write_text(json.dumps(row, indent=2)+'\n')
    rows.append(row)
    (a.out/'results.json').write_text(json.dumps(rows, indent=2)+'\n')
    print(name, {k:v['codegen_passed'] for k,v in row['probes'].items()}, flush=True)
print('Inventory complete; failed probes require classification, not a blanket platform PASS.', flush=True)
failures = [row['target'] for row in rows if not all(probe['codegen_passed'] for probe in row['probes'].values())]
if failures:
    print('FAILED target probes: ' + ', '.join(failures), flush=True)
raise SystemExit(bool(failures) and not a.inventory_only)
