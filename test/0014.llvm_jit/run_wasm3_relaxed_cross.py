#!/usr/bin/env python3
"""Cross-compile the production relaxed-SIMD lowering and execute 25 independent vectors.

This is generated-object testing, not a complete target CLI/JIT or unwind test.
Requires the cgroup wrapper externally, Clang/LLVM tools, and extracted Debian
cross libc/libgcc sysroots and qemu-user. C oracles are compiled with Clang.
"""
import argparse
import json
from pathlib import Path
import subprocess

# Debian's MIPS CRT objects explicitly request executable-stack ELF metadata.
# Honor that input for this fixed-data C oracle only; no VM build flags are changed.
PROFILES = [
    ('i386-sse2','i686-linux-gnu','pentium4','+sse2','i386','i386',['-msse2','-mfpmath=sse']),
    ('aarch64-neon','aarch64-linux-gnu','generic','+neon','aarch64','native',[]),
    ('aarch64-scalar','aarch64-linux-gnu','generic','-neon,-sve','aarch64','native',[]),
    ('riscv64-scalar','riscv64-linux-gnu','generic-rv64','+m,+a,+f,+d,+c','riscv64','native',['-march=rv64gc','-mabi=lp64d']),
    ('riscv64-vector','riscv64-linux-gnu','generic-rv64','+m,+a,+f,+d,+c,+v','riscv64','native',['-march=rv64gcv','-mabi=lp64d']),
    ('armhf-neon','arm-linux-gnueabihf','cortex-a15','+neon,+vfp4','arm','native',['-mcpu=cortex-a15','-mfpu=neon-vfpv4']),
    ('armhf-scalar','arm-linux-gnueabihf','cortex-a15','-neon,+vfp4','arm','native',['-mcpu=cortex-a15','-mfpu=vfpv4']),
    ('armel-soft','arm-linux-gnueabi','arm926ej-s','-neon,-vfp2','arm','native',['-mcpu=arm926ej-s','-mfloat-abi=soft']),
    ('ppc64le-vsx','powerpc64le-linux-gnu','pwr8','+vsx','ppc64le','native',['-mcpu=power8']),
    ('ppc64-vsx','powerpc64-linux-gnu','pwr8','+vsx','ppc64','native',['-mcpu=power8']),
    ('ppc32-scalar','powerpc-linux-gnu','ppc','-altivec,-vsx','ppc','native',[]),
    ('s390x-vector','s390x-linux-gnu','z14','+vector','s390x','native',['-march=z14']),
    ('mips64el-scalar','mips64el-linux-gnuabi64','mips64r2','+fp64','mips64el','native',['-march=mips64r2','-mabi=64','-Wl,-z,execstack']),
    ('mips64-scalar','mips64-linux-gnuabi64','mips64r2','+fp64','mips64','native',['-march=mips64r2','-mabi=64','-Wl,-z,execstack']),
    ('mipsel-scalar','mipsel-linux-gnu','mips32r2','+fpxx,-fp64','mipsel','native',['-march=mips32r2','-mabi=32','-Wl,-z,execstack']),
    ('mips-scalar','mips-linux-gnu','mips32r2','+fpxx,-fp64','mips','native',['-march=mips32r2','-mabi=32','-Wl,-z,execstack']),
    ('sparc64-scalar','sparc64-linux-gnu','v9','-vis','sparc64','native-nan',[]),
    ('loongarch64-lsx','loongarch64-linux-gnu','la464','+lsx','loongarch64','native',['-march=la464']),
    ('loongarch64-scalar','loongarch64-linux-gnu','la464','-lsx,-lasx','loongarch64','native',[]),
]

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('output',type=Path)
    ap.add_argument('--emitter',type=Path,required=True);ap.add_argument('--finalizer',type=Path,required=True)
    ap.add_argument('--llvm',type=Path,required=True);ap.add_argument('--deps',type=Path,required=True)
    ap.add_argument('--clang',type=Path,help='Clang executable, when LLVM tools come from a libraries-only source build')
    ap.add_argument('--only',help='comma-separated profile names');args=ap.parse_args()
    selected=set(args.only.split(',')) if args.only else {p[0] for p in PROFILES}
    if not selected <= {p[0] for p in PROFILES}:ap.error('unknown profile')
    args.output.mkdir(parents=True,exist_ok=True);rows=[]
    for name,triple,cpu,features,emulator,mode,flags in PROFILES:
        if name not in selected:continue
        directory=args.output/name;directory.mkdir(exist_ok=False);prefix=directory/'relaxed';path=lambda ext:str(prefix)+ext
        gcc=args.deps/'usr/lib/gcc-cross'/triple/'15';sysroot=args.deps/'usr'/triple
        compiler=[str(args.clang or args.llvm/'clang'),f'--target={triple}',f'--sysroot={args.deps}',f'--gcc-install-dir={gcc}',
                  '-isystem',str(sysroot/'include'),'-fuse-ld=lld','-O2',*flags]
        # These Debian CRT ABIs/relocations require GNU ld; compilation stays in Clang/LLVM.
        # PPC32 also uses the matching linker for its ELF PLT/GOT calling convention.
        linker = args.deps/'usr/bin'/(triple+'-ld')
        link_flags = ['--ld-path='+str(linker)] if triple in {
            'powerpc64-linux-gnu', 'powerpc-linux-gnu', 'sparc64-linux-gnu'} else []
        codegen=[str(args.llvm/'llc'),'-O3','-verify-machineinstrs','-relocation-model=pic',f'-mcpu={cpu}',f'-mattr={features}']
        phases=[('emit',[str(args.emitter),triple,cpu,features,str(prefix)]),
                ('fp',[str(args.finalizer),path('.ll'),path('.fp.ll'),mode]),
                ('opt',[str(args.llvm/'opt'),'-passes=default<O3>',path('.fp.ll'),'-o',path('.bc')]),
                ('legalize',[str(args.finalizer),path('.bc'),path('.final.ll'),'legalize']),
                ('object',[*codegen,'-filetype=obj',path('.final.ll'),'-o',path('.o')]),
                ('assembly',[*codegen,'-filetype=asm',path('.final.ll'),'-o',path('.s')]),
                ('oracle-ir',[*compiler,'-S','-emit-llvm',path('.c'),'-o',path('.oracle.ll')]),
                ('oracle-object',[*codegen,'-filetype=obj',path('.oracle.ll'),'-o',path('.oracle.o')]),
                ('link',[*compiler,*link_flags,path('.oracle.o'),path('.o'),'-L'+str(sysroot/'lib'),'-lm','-o',path('.test')]),
                ('run',[str(args.deps/'usr/bin'/('qemu-'+emulator)),'-U','LD_LIBRARY_PATH','-L',str(sysroot),path('.test')])]
        row={'profile':name,'triple':triple,'cpu':cpu,'features':features,'phases':[]}
        for phase,command in phases:
            try:
                run=subprocess.run(command,capture_output=True,timeout=60);status=run.returncode;output=run.stdout+run.stderr
            except subprocess.TimeoutExpired as e:status='timeout';output=(e.stdout or b'')+(e.stderr or b'')
            (directory/(phase+'.log')).write_bytes(output);row['phases'].append({'phase':phase,'command':command,'exit':status})
            if status!=0:break
        row['passed']=phase=='run' and status==0;rows.append(row)
        (directory/'result.json').write_text(json.dumps(row,indent=2)+'\n');(args.output/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
        print(name,'PASS' if row['passed'] else f'FAIL {phase}: {output.decode(errors="replace")[:700]}',flush=True)
    return not all(row['passed'] for row in rows)
if __name__=='__main__':raise SystemExit(main())
