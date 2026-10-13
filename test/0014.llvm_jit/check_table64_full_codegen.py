#!/usr/bin/env python3
"""Inspect actual table64 fixture objects for native calls and independent unwind."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import struct
from check_wasm3_native_frame_codegen import decode_object
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('cache',type=Path);p.add_argument('output',type=Path);p.add_argument('--ros',action='store_true')
p.add_argument('--llvm',type=Path,default=Path('/toolchain/bin'));a=p.parse_args()
root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
a.output.mkdir(parents=True,exist_ok=False);rows=[];policies={}
for policy in ['instruction','unwind']:
 files=sorted((a.cache/policy).rglob('*.uwvm-ljc'));assert len(files)==8,(policy,len(files))
 functions={}
 for i,file in enumerate(files):
  blob=file.read_bytes();obj=a.output/(policy+f'-{i}.o');obj.write_bytes(decode_object(blob,a.ros))
  header=struct.unpack('<8sIIIIQQQQQ',blob[:64]);context=blob[64+header[7]:64+header[7]+header[8]]
  hashes=re.findall(rb'\x10module-wasm-hash@([0-9a-f]{64})',context);assert len(hashes)==1
  wasm_hash=hashes[0].decode()
  sections=subprocess.check_output([str(a.llvm/'llvm-objdump'),'-h',str(obj)],text=True)
  assert '.eh_frame' in sections,'missing CFI';obj.with_suffix('.sections').write_text(sections)
  assembly=subprocess.check_output([str(a.llvm/'llvm-objdump'),'-dr',str(obj)],text=True)
  assert 'file format elf64-x86-64' in assembly;obj.with_suffix('.s').write_text(assembly)
  chunks=re.split(r'(?m)^([0-9a-f]+) <(.+)>:\n',assembly)
  for n in range(1,len(chunks),3):
   name,body=chunks[n+1:n+3];match=re.fullmatch(r'uwvm_m_[0-9a-f]+_func_(\d+)',name)
   if not match:continue
   index=int(match[1]);assert index<9
   calls=len(re.findall(r'\bcallq?\s',body));key=(wasm_hash,name);assert key not in functions;functions[key]=calls
   # Reference-null is a direct tag comparison. Only table get remains; set/fill may also need ref.func.
   allowed={1,2} if index in (3,4) else {1}
   expected={x+(2 if policy=='instruction' else 0) for x in allowed}
   assert calls in expected,(policy,name,calls,body)
   rows.append(dict(policy=policy,module_wasm_sha256=wasm_hash,symbol=name,calls=calls,semantic_calls=calls-(2 if policy=='instruction' else 0),
                    object_sha256=hashlib.file_digest(obj.open('rb'),'sha256').hexdigest()))
 assert len(functions)==72,(policy,len(functions));policies[policy]=functions
assert policies['instruction'].keys()==policies['unwind'].keys()
assert all(policies['instruction'][name]==calls+2 for name,calls in policies['unwind'].items())
(a.output/'summary.json').write_text(json.dumps(dict(passed=True,scope='actual x86-64 table64 fixture objects; semantic bridges retained, unwind omits both frame-maintenance calls',functions=rows),indent=2)+'\n')
print('PASS 144 actual table functions: semantic bridges, instruction adds two frame calls, unwind omits both and retains CFI')
