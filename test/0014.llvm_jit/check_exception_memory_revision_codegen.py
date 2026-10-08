#!/usr/bin/env python3
"""Compare real before/after full-JIT objects for the same ordinary mmap loop.

Every executable section and relocation is reported. Instruction mode may gain
cold exception cleanup, but its memory leaf and normal caller prefix must remain
identical. Native-unwind mode requires identical whole executable images and
complete relocations. No timings or ignored machine-code bytes are used.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess
from check_wasm3_native_frame_codegen import decode_object
from elf_executable_sections import write_executable_evidence, normalize_relocations, compare_executable_images, ELFCodeError


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before',type=Path,required=True)
    p.add_argument('--after',type=Path,required=True)
    p.add_argument('--fixture',type=Path,required=True)
    p.add_argument('--runtime-optimization',required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--llvm',type=Path,default=Path('/toolchain/bin'))
    p.add_argument('--ros',action='store_true')
    a=p.parse_args()
    subprocess.run(['bash',str(Path(__file__).resolve().parents[2]/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=False)
    commands=[];rows=[]
    def run(command,path):
        command=list(map(str,command));r=subprocess.run(command,capture_output=True,timeout=120)
        path.write_bytes(r.stdout+r.stderr)
        commands.append(dict(command=command,exit=r.returncode,log=str(path)))
        (a.out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
        if r.returncode:raise RuntimeError(f'{command[0]} exit={r.returncode}: {path}')
        return (r.stdout+r.stderr).decode(errors='replace')
    def emit(binary,label,policy):
        out=a.out/(label+'-'+policy);out.mkdir();cache=out/'cache';cache.mkdir()
        mode=['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full']
        run([binary,*mode,'-Rct','0','-Rllvm-full-policy','pb-o3','-Rllvm-call-stack',policy,
             '-WFD-exceptions','-Rllvm-cache-path','path',cache,'--run',a.fixture],out/'run.log')
        cached=list(cache.rglob('*.uwvm-ljc'))
        if len(cached)!=1:raise RuntimeError('expected exactly one actual cached object')
        obj=out/'native.o';obj.write_bytes(decode_object(cached[0].read_bytes(),a.ros))
        executable=write_executable_evidence(obj,out)
        reloc=run([a.llvm/'llvm-readobj','--relocations',obj],out/'relocations.txt')
        symbols=run([a.llvm/'llvm-nm','--defined-only',obj],out/'symbols.txt')
        run([a.llvm/'llvm-objdump','-dr',obj],out/'native.s')
        run([a.llvm/'llvm-dwarfdump','--eh-frame',obj],out/'eh-frame.txt')
        bodies=[]
        for index in [0,1]:
            names=re.findall(r'\b(uwvm_m_[0-9a-f]+_func_'+str(index)+r')$',symbols,re.M)
            if len(names)!=1:raise RuntimeError('fixture must contain memory leaf0/caller1')
            asm=run([a.llvm/'llvm-objdump','-dr','--disassemble-symbols='+names[0],obj],out/f'func-{index}.s')
            if 'file format elf64-x86-64' not in asm:raise RuntimeError('this explicit native instruction comparison requires x86-64')
            body=asm.split('<'+names[0]+'>:',1)[1]
            bodies.append(re.sub(r'uwvm_m_[0-9a-f]+_','uwvm_m_HASH_',body))
        return executable,normalize_relocations(reloc),bodies
    for policy in ['instruction','unwind']:
        before=emit(a.before,'before',policy);after=emit(a.after,'after',policy)
        try:compare_executable_images(before[0],after[0]);same_code=True
        except ELFCodeError:same_code=False
        same_reloc=before[1]==after[1]
        if before[2][0]!=after[2][0]:raise RuntimeError(f'{policy}: memory leaf machine code changed')
        def normal_prefix(body):
            lines=body.splitlines(keepends=True)
            for index,line in enumerate(lines):
                if re.search(r'\bretq?\b',line):return ''.join(lines[:index+1])
            raise RuntimeError('ordinary caller has no normal return')
        if normal_prefix(before[2][1])!=normal_prefix(after[2][1]):
            raise RuntimeError(f'{policy}: normal caller machine-code prefix changed; inspect retained objects')
        if policy=='unwind' and not(same_code and same_reloc):
            raise RuntimeError('native-unwind ordinary code or complete relocations changed')
        rows.append(dict(policy=policy,identical_whole_executable_images=same_code,
                         identical_complete_relocations=same_reloc,identical_memory_leaf=True,
                         identical_normal_caller_prefix=True,before=before[0].summary(),after=after[0].summary(),
                         instruction_cold_cleanup_added=policy=='instruction' and not same_code))
    report=dict(passed=True,checks=rows,runtime_optimization=a.runtime_optimization,
                generated_optimization='pb-o3',timing_benchmark=False,
                before_sha256=hashlib.sha256(a.before.read_bytes()).hexdigest(),
                after_sha256=hashlib.sha256(a.after.read_bytes()).hexdigest(),
                fixture_sha256=hashlib.sha256(a.fixture.read_bytes()).hexdigest())
    (a.out/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS before/after actual mmap loop: identical memory leaf and normal caller; complete unwind code+relocations identical')


if __name__=='__main__':main()
