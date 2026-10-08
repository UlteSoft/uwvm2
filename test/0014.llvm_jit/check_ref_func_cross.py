#!/usr/bin/env python3
"""Cross-execute real reference construction, without relabeling this as a complete target JIT run."""
import argparse, hashlib, json, re, subprocess
from pathlib import Path
from run_wasm3_int_cross import PROFILES
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--emitter',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
p.add_argument('--only',default='armhf-neon,s390x-vector,aarch64-neon,riscv64-scalar,i386-sse2,ppc32-scalar')
a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
a.out.mkdir(parents=True,exist_ok=False);selected=set(a.only.split(','));assert selected<={x[0] for x in PROFILES}
rows=[];llvm=Path('/work/artifacts/uwvm2-ros-jit/llvm/bin');deps=Path('/work/deps')
for name,triple,cpu,features,emulator,_,flags in PROFILES:
 if name not in selected:continue
 out=a.out/name;out.mkdir();little=not name.startswith(('ppc32','ppc64-vsx','s390x','mips64-scalar','mips-scalar','sparc64'))
 width=32 if name.startswith(('i386','armhf','armel','ppc32','mipsel','mips-scalar')) else 64
 # The target-native C oracle proves the model layout independently of host LLVM emitter sizeof.
 (out/'oracle.c').write_text('''#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
struct ref {void *payload;unsigned kind;};
_Static_assert(offsetof(struct ref,kind)==sizeof(void*),"native tag offset");
_Static_assert(sizeof(struct ref)==sizeof(void*)*2,"native reference size");
extern void ref_imported(uintptr_t,void*);extern void ref_defined(uintptr_t,void*);
int main(void){unsigned n=0;for(unsigned kind=2;kind<=3;++kind)for(unsigned pattern=0;pattern<256;++pattern){
 unsigned char storage[sizeof(struct ref)+2];memset(storage,0xa5,sizeof(storage));uintptr_t payload=0;memset(&payload,pattern,sizeof(payload));
 if(kind==2)ref_imported(payload,storage+1);else ref_defined(payload,storage+1);
 uintptr_t actual;unsigned tag;memcpy(&actual,storage+1,sizeof(actual));memcpy(&tag,storage+1+offsetof(struct ref,kind),sizeof(tag));
 if(actual!=payload||tag!=kind||storage[0]!=0xa5||storage[sizeof(storage)-1]!=0xa5){fprintf(stderr,"kind=%u pattern=%u\\n",kind,pattern);return 1;}++n;}
 printf("PASS %u unaligned reference constructions with guarded endpoints\\n",n);return 0;}
''')
 gcc=deps/'usr/lib/gcc-cross'/triple/'15';sysroot=deps/'usr'/triple
 cc=['/toolchain/bin/clang',f'--target={triple}',f'--sysroot={deps}',f'--gcc-install-dir={gcc}','-isystem',str(sysroot/'include'),'-O2',*flags]
 linker=['--ld-path='+str(deps/'usr/bin'/(triple+'-ld'))] if name.startswith(('ppc32','ppc64-vsx','sparc64')) else ['-fuse-ld=lld']
 llc=[str(llvm/'llc'),'-O2','-verify-machineinstrs','-relocation-model=pic',f'-mcpu={cpu}',f'-mattr={features}']
 phases=[('emit',[str(a.emitter),triple,str(width),'little' if little else 'big',str(out/'ref.ll')]),
 ('opt',[str(llvm/'opt'),'-passes=default<O2>',str(out/'ref.ll'),'-S','-o',str(out/'ref.opt.ll')]),
 ('object',[*llc,'-filetype=obj',str(out/'ref.opt.ll'),'-o',str(out/'ref.o')]),
 ('assembly',[*llc,'-filetype=asm',str(out/'ref.opt.ll'),'-o',str(out/'ref.s')]),
 ('oracle-ir',[*cc,'-S','-emit-llvm',str(out/'oracle.c'),'-o',str(out/'oracle.ll')]),
 ('oracle-object',[*llc,'-filetype=obj',str(out/'oracle.ll'),'-o',str(out/'oracle.o')]),
 ('link',[*cc,*linker,str(out/'oracle.o'),str(out/'ref.o'),'-L'+str(sysroot/'lib'),'-o',str(out/'test')]),
 ('run',[str(deps/'usr/bin'/('qemu-'+emulator)),'-U','LD_LIBRARY_PATH','-L',str(sysroot),str(out/'test')])]
 row=dict(profile=name,width=width,endian='little' if little else 'big',phases=[])
 for phase,command in phases:
  r=subprocess.run(command,capture_output=True,timeout=120);(out/(phase+'.log')).write_bytes(r.stdout+r.stderr)
  row['phases'].append(dict(phase=phase,command=command,exit=r.returncode))
  if r.returncode:break
 row['passed']=phase=='run' and r.returncode==0
 if row['passed']:
  # A zero-call IR body is necessary but not sufficient: reject native helper calls from legalization as well.
  ir=(out/'ref.opt.ll').read_text();assembly=(out/'ref.s').read_text()
  assert not re.search(r'\b(?:call|invoke)\b',ir)
  assert not re.search(r'^\s*(?:callq?|bl|blx|brasl|basr|jal|jalr)\s',assembly,re.M),assembly
  row['object_sha256']=hashlib.file_digest((out/'ref.o').open('rb'),'sha256').hexdigest()
 rows.append(row);(a.out/'summary.json').write_text(json.dumps(dict(passed=all(x['passed'] for x in rows),results=rows),indent=2)+'\n')
 print(name,'PASS direct reference construction' if row['passed'] else f'FAIL {phase}: {r.stderr.decode(errors="replace")[-1000:]}',flush=True)
raise SystemExit(not all(x['passed'] for x in rows))
