#!/usr/bin/env python3
"""Audit actual x86-64 memory64 load/store opfuncs, preserving cold trap calls."""
import argparse,hashlib,json,re,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'test/0014.llvm_jit'))
from check_wasm3_tail_codegen import success_path_calls
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('binary',type=Path);p.add_argument('out',type=Path);p.add_argument('--llvm',type=Path,required=True)
p.add_argument('--require-sign-loads',action='store_true')
p.add_argument('--atomic',action='store_true')
p.add_argument('--rmw',action='store_true')
p.add_argument('--simd',action='store_true')
a=p.parse_args();subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
a.out.mkdir(parents=True,exist_ok=False)
assembly=subprocess.check_output([str(a.llvm/'llvm-objdump'),'-d','--demangle',str(a.binary)],text=True)
assert 'file format elf64-x86-64' in assembly
chunks=re.split(r'(?m)^([0-9a-f]+) <(.+)>:\n',assembly);rows=[]
for i in range(1,len(chunks),3):
 name,body=chunks[i+1:i+3]
 if ('uwvmint_memory64_simd<' if a.simd else 'uwvmint_memory64_rmw<' if a.rmw else 'uwvmint_memory64_atomic<' if a.atomic else 'uwvmint_memory64_load_store<') not in name or 'uwvm_interpreter_translate_option_t{true' not in name:continue
 assert re.search(r'\bjmpq?\s+\*',body),(name,'missing tail dispatch')
 assert not re.search(r'\bretq?\b',body),(name,'non-tail return')
 # Both overloads are explicitly [[noreturn]] in optable/memory.h.
 calls=success_path_calls(body,noreturn_symbols=('::details::memory_oob_terminate(', '::details::memory64::unaligned_atomic('))
 assert calls==0,(name,'hot-path helper call',body)
 if a.atomic or a.rmw:
  assert not re.search(r'\b[mfls]fence\b',body),(name,'redundant fence')
  if a.rmw:assert re.search(r'\block\s+|\bxchg[bwlq]\s+[^\n]*\(',body),(name,'missing native atomic RMW')
  if re.search(r'uwvmint_memory64_atomic<(?:int|long), [1248]ul, true,',name):
   assert len(re.findall(r'\bxchg[bwlq]\s+[^\n]*\(',body))==1,(name,'missing single sequentially consistent store')
 signed=re.search(r'uwvmint_memory64_load_store<(int|long), ([124])ul, true, false,',name)
 sign_load=False
 if signed and int(signed[2]) < (4 if signed[1]=='int' else 8):
  sign_load=bool(re.search(r'\bmovs(?:b[ql]|w[ql]|lq)\s+[^\n]*\(',body))
  assert sign_load,(name,'missing single sign-extending memory load')
 (a.out/f'{len(rows)}.s').write_text(name+'\n'+body)
 rows.append(dict(symbol=name,success_calls=calls,musttail=True,sign_extending_load=sign_load))
assert rows
if a.require_sign_loads:assert any(r['sign_extending_load'] for r in rows)
(a.out/'summary.json').write_text(json.dumps(dict(passed=True,scope='native x86-64 memory64 opfuncs in the supplied executable; codegen only, no throughput claim',
 sha256=hashlib.file_digest(a.binary.open('rb'),'sha256').hexdigest(),handlers=rows),indent=2)+'\n')
print(f'PASS {len(rows)} memory64 {"RMW" if a.rmw else "atomic load/store" if a.atomic else "load/store"} opfuncs: musttail jumps, no calls on successful access paths')
