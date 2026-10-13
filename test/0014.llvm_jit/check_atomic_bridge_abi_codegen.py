#!/usr/bin/env python3
"""Compare actual x86 JIT atomic bridge callers after an ABI implementation change.

Both runs must use byte-identical independent semantic fixtures. Inspect emitted
code for every function and both stack policies; do not count this as a timing
benchmark or as qualification of the native C++ callee on other architectures.
"""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--current',type=Path,required=True)
p.add_argument('--llvm-bin',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args()
subprocess.run(['bash',str(Path(__file__).resolve().parents[2]/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
a.out.mkdir(parents=True,exist_ok=False)
metadata=[json.loads((root/'summary.json').read_text()) for root in [a.baseline,a.current]]
assert all(m['passed'] for m in metadata)
assert metadata[0]['test_sha256']==metadata[1]['test_sha256'],'semantic fixture changed'
rows=[]
def functions(assembly):
 result={}
 for match in re.finditer(r'(?ms)^uwvm_m_[a-f0-9]+_func_(\d+):.*?^\.Lfunc_end\d+:',assembly):
  instructions=[]
  for line in match[0].splitlines():
   line=line.split('#')[0].strip()
   if not line or line.startswith('.') or line.endswith(':'):continue
   # Bridge ABI/type hashes and module identities may legitimately change.
   # Keep opcodes, register widths, offsets, immediates and instruction order.
   line=re.sub(r'uwvm_bridge_\w+','BRIDGE',line)
   line=re.sub(r'uwvm_m_[a-f0-9]+','MODULE',line)
   instructions.append(re.sub(r'\s+',' ',line))
  result[int(match[1])]=instructions
 return result
for policy in ['instruction','unwind']:
 emitted=[]
 for label,root in [('before',a.baseline),('after',a.current)]:
  source=root/('mixed-'+policy+'.ll');ir=a.out/(label+'-'+policy+'.ll');asm=a.out/(label+'-'+policy+'.s')
  commands=[[str(a.llvm_bin/'opt'),'-passes=default<O3>','-S',str(source),'-o',str(ir)],
   [str(a.llvm_bin/'llc'),'-O3','-mtriple=x86_64-unknown-linux-gnu','-mcpu=raptorlake',str(ir),'-o',str(asm)]]
  for cmd in commands:subprocess.run(cmd,check=True)
  functions_here=functions(asm.read_text());assert set(functions_here)==set(range(66)),(label,policy)
  emitted.append(functions_here)
  rows.append(dict(policy=policy,label=label,input=str(source),sha256=hashlib.file_digest(source.open('rb'),'sha256').hexdigest(),commands=commands))
 comparisons=[dict(function=i,before=emitted[0][i],after=emitted[1][i],identical=emitted[0][i]==emitted[1][i]) for i in range(66)]
 (a.out/(policy+'-comparison.json')).write_text(json.dumps(comparisons,indent=2)+'\n')
 assert all(r['identical'] for r in comparisons),(policy,'native caller instruction sequences changed; inspect comparison')
 for body in emitted[1].values():
  assert sum(line.startswith('call') for line in body)==(3 if policy=='instruction' else 1)
  assert not any(line.split()[0] in ['lock','mfence','lfence','sfence'] for line in body)
(a.out/'summary.json').write_text(json.dumps(dict(passed=True,functions=66,policies=2,fixture_sha256=metadata[0]['test_sha256'],rows=rows),indent=2)+'\n')
print('PASS 132 atomic bridge callers: instruction-identical after symbol normalization; unwind 1 call versus instruction 3; no added fences')
