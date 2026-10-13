#!/usr/bin/env python3
"""Audit actual native mmap scalar fallback functions, including cold paths."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
from check_wasm3_tail_codegen import success_path_calls
p=argparse.ArgumentParser(description=__doc__);p.add_argument('binary',type=Path);p.add_argument('output',type=Path)
a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
llvm=Path('/toolchain/bin')
names=subprocess.check_output([str(llvm/'llvm-nm'),'--defined-only',str(a.binary)],text=True)
symbols=[line.split()[-1] for line in names.splitlines() if re.search(r'details(?:36|37)llvm_jit_memory64_scalar_(?:load|store)_bridge',line)]
assert len(symbols)==16,(len(symbols),symbols)
raw=subprocess.check_output([str(llvm/'llvm-objdump'),'-d','--disassemble-symbols='+','.join(symbols),str(a.binary)],text=True)
assembly=subprocess.run([str(llvm/'llvm-cxxfilt')],input=raw,text=True,capture_output=True,check=True).stdout
chunks=re.split(r'(?m)^([0-9a-f]+) <(.+)>:\n',assembly);rows=[]
a.output.mkdir(parents=True,exist_ok=False)
for i in range(1,len(chunks),3):
 name,body=chunks[i+1:i+3]
 assert re.search(r'\bretq?\b',body),(name,'missing successful return')
 assert success_path_calls(body)==0,(name,'successful-path helper call',body)
 assert not re.search(r'\b(?:lock|mfence|lfence|sfence)\b',body),(name,'unnecessary synchronization')
 (a.output/f'{len(rows)}.s').write_text(name+'\n'+body);rows.append(dict(name=name,successful_calls=0))
assert len(rows)==16,len(rows)
(a.output/'summary.json').write_text(json.dumps(dict(passed=True,scope='native x86-64 mmap scalar fallback bodies; no throughput claim',
 input=str(a.binary),sha256=hashlib.file_digest(a.binary.open('rb'),'sha256').hexdigest(),handlers=rows),indent=2)+'\n')
print('PASS 16 actual mmap memory64 scalar bridges: no successful-path helper calls or hardware synchronization')
