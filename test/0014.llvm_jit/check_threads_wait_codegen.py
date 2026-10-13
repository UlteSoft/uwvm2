#!/usr/bin/env python3
"""Inspect optimized x86 wait/notify code and separation of the three bridge targets."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--llvm-bin',type=Path,required=True);p.add_argument('root',type=Path);a=p.parse_args()
subprocess.run(['bash',str(Path(__file__).resolve().parents[2]/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
out=a.root/'codegen';out.mkdir(exist_ok=True);rows=[];targets={}
for opcode in range(3):
 for indexed in [False,True]:
  for policy in ['instruction','unwind']:
   stem=f'wait-{opcode}-'+('indexed-' if indexed else 'legacy-')+policy;source=a.root/(stem+'.ll');optimized=out/(stem+'.opt.ll');assembly=out/(stem+'.s')
   commands=[[str(a.llvm_bin/'opt'),'-passes=default<O3>','-S',str(source),'-o',str(optimized)],
    [str(a.llvm_bin/'llc'),'-O3','-mtriple=x86_64-unknown-linux-gnu','-mcpu=raptorlake',str(optimized),'-o',str(assembly)]]
   for command in commands:subprocess.run(command,check=True)
   ir=optimized.read_text();asm=assembly.read_text()
   match=re.search(r'(?ms)^define i32 @(uwvm_m_[a-f0-9]+_func_0)\([^\n]*\).*?^}',ir);assert match,stem
   body=match[0];callee=re.findall(r'call i(?:32|64) @(uwvm_bridge_\w+)\(',body);assert len(callee)==1,(stem,body)
   targets[(opcode,indexed,policy)]=callee[0]
   calls=re.findall(r'\bcall\b',body);assert len(calls)==(1 if policy=='unwind' else 3),(stem,calls)
   machine=re.search(r'(?ms)^'+match[1]+r':.*?^\.Lfunc_end\d+:',asm)[0]
   machine_calls=re.findall(r'(?m)^\s+callq?\s',machine)
   assert len(machine_calls)==len(calls),(stem,machine)
   assert '.cfi_startproc' in machine,stem
   assert not re.search(r'(?m)^\s+(lock|mfence|lfence|sfence)\b',machine),stem
   rows.append(dict(input=str(source),sha256=hashlib.file_digest(source.open('rb'),'sha256').hexdigest(),commands=commands,bridge=callee[0],calls=len(calls),passed=True))
for policy in ['instruction','unwind']:
 for indexed in [False,True]:assert len({targets[(op,indexed,policy)] for op in range(3)})==3
(out/'summary.json').write_text(json.dumps(dict(passed=True,rows=rows),indent=2)+'\n')
print('PASS 12 wait/notify codegen cases: distinct bridges; unwind one semantic call, instruction three calls; native CFI; no inline fences')
