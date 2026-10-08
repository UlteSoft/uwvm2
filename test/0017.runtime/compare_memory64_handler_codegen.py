#!/usr/bin/env python3
"""Compare saved actual scalar handler success prefixes across a refactor.

Only branch target addresses and objdump annotations are normalized. Register
choices, memory operands, immediates, instruction order and dispatch are kept.
The complete control-flow call audit must also pass on both inputs.
"""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('baseline',type=Path);p.add_argument('current',type=Path);p.add_argument('output',type=Path)
a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
def load(folder):
 summary=json.loads((folder/'summary.json').read_text());result={}
 for i,row in enumerate(summary['handlers']):
  body=(folder/f'{i}.s').read_text();instructions=[]
  for line in body.splitlines()[1:]:
   m=re.match(r'\s*[0-9a-f]+:\s+(?:(?:[0-9a-f]{2})\s+)+\s*(\w+)\s*(.*)',line)
   if not m:continue
   op,operand=m.groups();operand=operand.split('#')[0].strip()
   if op.startswith('j') and not operand.startswith('*'):operand='<branch>'
   instructions.append((op,operand))
   if op.startswith('jmp') and operand.startswith('*'):break
  assert instructions and instructions[-1][0].startswith('jmp') and instructions[-1][1].startswith('*')
  result[row['symbol']]=instructions
 return summary['sha256'],result
oldhash,before=load(a.baseline);newhash,after=load(a.current)
rows=[dict(symbol=s,same=before.get(s)==v,before=before.get(s),after=v) for s,v in after.items()]
assert len(rows)>=500
a.output.mkdir(parents=True,exist_ok=False)
(a.output/'comparison.json').write_text(json.dumps(dict(baseline_sha256=oldhash,current_sha256=newhash,rows=rows),indent=2)+'\n')
different=[r for r in rows if not r['same']]
if different:print(json.dumps(different[0],indent=2))
assert not different,(len(different),len(rows))
print(f'PASS {len(rows)} ordinary memory64 load/store success prefixes unchanged')
