#!/usr/bin/env python3
"""Audit actual full-JIT cached memory64 atomic objects, not an offline emitter."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
from check_wasm3_native_frame_codegen import decode_object
from check_wasm3_tail_codegen import success_path_calls
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('cache',type=Path);p.add_argument('output',type=Path);p.add_argument('--ros',action='store_true')
p.add_argument('--llvm',type=Path,default=Path('/toolchain/bin'))
a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
a.output.mkdir(parents=True,exist_ok=False);rows=[]
for policy in ['instruction','unwind']:
 files=list((a.cache/policy).rglob('*.uwvm-ljc'));assert len(files)==1,files
 obj=a.output/(policy+'.o');obj.write_bytes(decode_object(files[0].read_bytes(),a.ros))
 sections=subprocess.check_output([str(a.llvm/'llvm-objdump'),'-h',str(obj)],text=True)
 assert '.eh_frame' in sections,'native unwind tables missing';(a.output/(policy+'.sections')).write_text(sections)
 assembly=subprocess.check_output([str(a.llvm/'llvm-objdump'),'-dr',str(obj)],text=True)
 assert 'file format elf64-x86-64' in assembly
 (a.output/(policy+'.s')).write_text(assembly)
 chunks=re.split(r'(?m)^([0-9a-f]+) <(.+)>:\n',assembly);checked=0
 for i in range(1,len(chunks),3):
  name,body=chunks[i+1:i+3];match=re.fullmatch(r'uwvm_m_[0-9a-f]+_func_(\d+)',name)
  if not match:continue
  index=int(match[1]);relative=index%67
  if index>=134 or relative<4:continue # wait/notify/fence and deliberate u65 overflow trap
  opcode=relative+12
  calls=success_path_calls(body)
  assert calls==(2 if policy=='instruction' else 0),(policy,name,calls,body)
  assert not re.search(r'\b[mfls]fence\b',body),(name,'redundant fence')
  if opcode>=0x17:
   assert re.search(r'\block\s+|\bxchg[bwlq]\s+[^\n]*\(',body),(name,'atomic write lost')
  rows.append(dict(policy=policy,index=index,address_bits=64 if index>=67 else 32,opcode=opcode,success_calls=calls));checked+=1
 assert checked==126,(policy,checked)
summary=dict(passed=True,scope='actual x86-64 full-JIT mmap atomic objects; no throughput claim',cases=rows,
 objects={x.name:hashlib.file_digest(x.open('rb'),'sha256').hexdigest() for x in a.output.glob('*.o')})
(a.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print('PASS 252 actual full-JIT atomic functions: native atomics, independent unwind with zero successful-path bookkeeping calls, CFI')
