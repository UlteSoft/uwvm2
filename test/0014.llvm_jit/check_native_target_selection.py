#!/usr/bin/env python3
"""Exercise production target-selection bodies with ELF --wrap triple injection.

This small unit avoids rebuilding a whole foreign LLVM library merely to check
the selection contract. It preserves exact production bodies and their hashes;
container adapters are test-local. Native VM/CFI/module tests remain mandatory.
Use an existing successful all-target LLVM fixture build command as the template
(a JSON list of records with command/status), never a system llvm-config.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source-root',type=Path,required=True)
p.add_argument('--build-template',type=Path,required=True)
p.add_argument('--build-cwd',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=False)
fixture=Path(__file__).parent/'fixtures/native_target_selection.cpp'
runtime=a.source_root/'src/uwvm2/runtime/lib'
lazy=a.source_root/'src/uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/translate.h'
paths={'runtime':runtime/'uwvm_runtime.default.cpp'}
if lazy.exists(): paths['lazy']=lazy

def body(text,name):
    definitions=[]
    for match in re.finditer(r'\b'+name+r'\(',text):
        begin=text.rfind('\n\n',0,match.start())+2
        opening=text.find('{',match.end())
        if opening<0 or ';' in text[match.end():opening]: continue
        depth=1; end=opening+1
        while depth:
            depth += (text[end]=='{')-(text[end]=='}')
            end+=1
        definitions.append(text[begin:end])
    assert len(definitions)==1,(name,len(definitions))
    return definitions[0]

def uncomment(text):
    return re.sub(r'//[^\n]*|/\*.*?\*/','',text,flags=re.S)

# Both live probes must exercise the same native CPU/features/ABI selection.
for name in ('posix','win64'):
    text=uncomment((runtime/f'uwvm_runtime_{name}_unwind_probe.h').read_text())
    assert not re.search(r'\.selectTarget\(\s*\)',text)
    assert re.search(r'select_runtime_llvm_jit_target\(\s*target_builder,\s*get_llvm_jit_host_cpu_name_storage\(\),\s*get_llvm_jit_host_target_attribute_storage\(\)\)',text)

template=next(x['command'] for x in json.loads(a.build_template.read_text())
              if x['name']=='simd_direct_lowering')
source_index=next(i for i,x in enumerate(template) if x.endswith('/simd_direct_lowering.cc'))
rows=[]
profiles=[(arch+'-'+env,'generic','',32) for arch in ('mips64','mips64el','mipsisa64r6','mipsisa64r6el')
          for env in ('linux-gnuabin32','linux-muslabin32')]
profiles += [('x86_64-linux-gnux32','x86-64','+sse2',32),
             ('x86_64-linux-muslx32','x86-64','+sse2',32),
             ('aarch64_32-apple-watchos','generic','+neon',32)]
# Passing the rejection test is NOT a platform execution/codegen PASS. Keep
# these targets in the separate, unfiltered assembly/object inventory.
profiles += [(triple,cpu,features,None) for triple,cpu,features in (
    ('ve-unknown-linux-gnu','generic','-vpu'), ('sparc-linux-gnu','v8',''),
    ('sparcv9-linux-gnu','v9',''), ('xcore-unknown-unknown','generic',''),
    ('bpfel-unknown-none','generic',''), ('nvptx64-nvidia-cuda','sm_50',''),
    ('amdgcn-amd-amdhsa','gfx900',''), ('wasm32-unknown-unknown','generic','+simd128'),
    ('powerpc64-ibm-aix7.2','pwr8',''), ('powerpc64-apple-darwin9','g5',''),
    ('s390x-ibm-zos','z13',''),
    ('aarch64-linux-gnu_ilp32','generic','+neon'),
    ('aarch64_be-linux-gnu_ilp32','generic','+neon'),
    ('aarch64_32-unknown-linux-gnu','generic','+neon'),
    ('powerpc-linux-gnu','generic',''), ('powerpcle-linux-gnu','generic',''),
    ('thumbv7-linux-gnueabihf','cortex-a9','+vfp3,+neon'),
    ('thumbebv7-linux-gnueabihf','cortex-a9','+vfp3,+neon'),
    ('armebv7-linux-gnueabihf','cortex-a9','+vfp3,+neon'))]
profiles += [('x86_64-pc-windows-msvc','x86-64','+sse2',64),
             ('aarch64-pc-windows-msvc','generic','+neon',64),
             ('x86_64-apple-macosx12.0','x86-64','+sse2',64),
             ('aarch64-apple-macosx12.0','generic','+neon',64),
             ('powerpc64-linux-gnu','pwr8','+altivec,+vsx',64),
             ('armv7-linux-gnueabihf','cortex-a9','+vfp3,+neon',32),
             ('thumbv7-pc-windows-msvc','cortex-a9','+vfp3,+neon',32)]
# ROS's documented RuntimeDyld fix is not implicitly installed in ordinary
# UWVM's external LLVM. The latter must reject the unsafe BE call-stub path.
profiles += [('aarch64_be-linux-gnu','generic','+neon',None if 'lazy' in paths else 64)]
profiles += [('riscv32-linux-gnu','generic-rv32','+m,+a,+f,+d',32),
             ('riscv64-linux-gnu','generic-rv64','+m,+a,+f,+d',64),
             ('loongarch64-linux-gnu','generic-la64','+f,+d',64)]
profiles += [('mips-linux-gnu','mips32r2','',32),
             ('mipsel-linux-gnu','mips32r2','',32),
             ('mips64-linux-gnuabi64','mips64r2','',64),
             ('mips64el-linux-gnuabi64','mips64r2','',64),
             ('mipsel-unknown-linux-gnu','mips32r2','+micromips',None),
             ('mipsisa32r6el-linux-gnu','mips32r6','+micromips',None),
             ('mips-unknown-linux-gnu','mips32r2','+mips16',None)]
for mode,path in paths.items():
    text=path.read_text(encoding='utf-8-sig')
    assert not re.search(r'\.selectTarget\(\s*\)',uncomment(text)),path
    names=(['append_llvm_jit_host_target_attribute_strings'] if mode=='lazy' or
           'append_llvm_jit_host_target_attribute_strings(' in text else [])
    names += (['get_runtime_llvm_jit_mcjit_target_triple','select_runtime_llvm_jit_target'] if mode=='runtime'
              else ['get_llvm_jit_mcjit_target_triple','select_llvm_jit_target'])
    directory=a.out/mode; directory.mkdir()
    extracted='\n\n'.join(body(text,name) for name in names)+'\n'
    (directory/'native_target_functions.inc').write_text(extracted)
    cmd=template.copy(); cmd[source_index]=str(fixture.resolve())
    cmd.insert(1,'-I'+str(a.source_root/'src'))
    cmd[cmd.index('-o')+1]=str(directory/'probe')
    cmd += ['-I'+str(directory),'-Wno-return-type-c-linkage',
            '-Wl,--wrap=_ZN4llvm3sys22getDefaultTargetTripleEv',
            '-Wl,--wrap=_ZN4llvm3sys16getProcessTripleEv',
            '-Wl,--wrap=_ZN4llvm3sys22getDefaultTargetTripleB5cxx11Ev',
            '-Wl,--wrap=_ZN4llvm3sys16getProcessTripleB5cxx11Ev']
    if mode=='lazy': cmd+=['-DUWVM_TEST_NATIVE_TARGET_LAZY']
    with (directory/'build.log').open('wb') as log:
        status=subprocess.run(cmd,cwd=a.build_cwd,stdout=log,stderr=subprocess.STDOUT,timeout=300).returncode
    row=dict(mode=mode,source=str(path),source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
             functions_sha256=hashlib.sha256(extracted.encode()).hexdigest(),command=cmd,status=status,probes=[])
    row['object_format_gate_sha256']=hashlib.sha256((a.source_root/'src/uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h').read_bytes()).hexdigest()
    rows.append(row)
    (a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
    if status: raise SystemExit(status)
    for triple,cpu,features,bits in profiles:
        kinds = ('native-rejected',) if bits is None else ('explicit','implicit32') if triple.startswith('mips') and 'abin32' in triple else ('explicit',)
        for kind in kinds:
            prefix=directory/(triple+'-'+kind)
            command=[str(directory/'probe'),triple,cpu,features,str(prefix)+'.o',kind]
            result=subprocess.run(command,capture_output=True,text=True,timeout=120)
            prefix.with_suffix('.log').write_text(result.stdout+result.stderr)
            passed=(result.returncode!=0 and '64-bit code requested on a subtarget' in result.stderr) if kind=='implicit32' else (
                result.returncode==0 and f'pointer_bits={bits}\n' in result.stdout and Path(str(prefix)+'.o').stat().st_size>0)
            if kind=='native-rejected':
                passed=result.returncode==4 and not Path(str(prefix)+'.o').exists()
            elif kind=='explicit':
                # Ordinary UWVM repairs unpatched external RuntimeDyld by
                # retaining unique temporary names; ROS repairs its loader.
                unique = 'lazy' in paths and triple.startswith(('riscv32-', 'riscv64-', 'loongarch64-'))
                passed &= f'unique_temp_labels={int(unique)}\n' in result.stdout
                if triple.startswith('mips'):
                    passed &= '+long-calls' in result.stdout and '+noabicalls' in result.stdout
            row['probes'].append(dict(triple=triple,kind=kind,command=command,status=result.returncode,passed=passed))
            (a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
            print(mode,triple,kind,passed,flush=True)
raise SystemExit(any(not probe['passed'] for row in rows for probe in row['probes']))
