#!/usr/bin/env python3
"""Compare actual cached JIT machine code for equivalent short/explicit Core 3 table/global encodings."""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
from check_wasm3_native_frame_codegen import decode_object
from elf_executable_sections import write_executable_evidence, normalize_relocations, compare_executable_images
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--uwvm',type=Path,required=True);p.add_argument('--fixtures',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--ros',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out.mkdir(parents=True,exist_ok=False);rows=[]
for case in ('identity','tuple','indirect','memory','if','select','select-external','loop'):
 for policy in ('instruction','unwind'):
  results=[]
  for encoding in ('short','explicit'):
   path=a.fixtures/(case+('-short' if encoding=='short' else '')+'.wasm');out=a.out/(case+'-'+policy+'-'+encoding);out.mkdir();cache=out/'cache';cache.mkdir()
   command=[str(a.uwvm)]+(['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full'])+['-WFE-function-references','-Rllvm-call-stack',policy,'-Rllvm-cache-path','path',str(cache),'--run',str(path)]
   r=subprocess.run(command,capture_output=True,timeout=60);(out/'run.log').write_bytes(r.stdout+r.stderr);(out/'run.command').write_text(json.dumps(command)+'\n');assert r.returncode==0,r.stderr
   objects=list(cache.rglob('*.uwvm-ljc'));assert len(objects)==1,objects;obj=out/'native.o';obj.write_bytes(decode_object(objects[0].read_bytes(),a.ros))
   assembly=subprocess.check_output(['/toolchain/bin/llvm-objdump','-dr',str(obj)],text=True);(out/'native.s').write_text(assembly)
   machine=write_executable_evidence(obj,out)
   relocations=subprocess.check_output(['/toolchain/bin/llvm-readobj','--relocations',str(obj)],text=True);(out/'relocations.txt').write_text(relocations)
   results.append((machine,normalize_relocations(relocations)))
  compare_executable_images(results[0][0],results[1][0])
  if results[0][1]!=results[1][1]:raise RuntimeError((case,policy,'complete object relocations differ; inspect retained objects'))
  rows.append(dict(case=case,policy=policy,**results[0][0].summary(),identical_relocations=True))
(a.out/'summary.json').write_text(json.dumps(dict(passed=True,comparisons=len(rows),checks=rows,binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest()),indent=2)+'\n')
print('PASS',len(rows),'actual JIT machine-code/relocation comparisons (short versus explicit table/global types)')
