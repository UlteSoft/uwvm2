#!/usr/bin/env python3
"""Check actual x86 interpreter wait/notify dispatch after blocking helper calls."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('object',type=Path);p.add_argument('output',type=Path);p.add_argument('--memory64',action='store_true');a=p.parse_args()
subprocess.run(['bash',str(Path(__file__).resolve().parents[2]/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
text=subprocess.check_output(['objdump','-d','--no-show-raw-insn',str(a.object)],text=True)
parts=re.split(r'(?m)^([0-9a-f]+) <([^>]+)>:\n',text);rows=[]
for i in range(1,len(parts),3):
 name,body=parts[i+1:i+3]
 if ('uwvmint_memory64_wait_notifyI' if a.memory64 else 'uwvmint_atomic_wait_notifyI') not in name or 'Lb1E' not in name:continue
 instructions=[]
 for line in body.splitlines():
  m=re.match(r'\s*[0-9a-f]+:\s*(\S+)\s*(.*)',line)
  if m:instructions.append(m[1]+' '+m[2].split('#')[0].strip())
 jumps=[x for x in instructions if re.match(r'jmp\w* \*',x)]
 assert jumps and not any(re.match(r'ret\w*\b',x) for x in instructions),(name,instructions)
 rows.append(dict(symbol=name,instructions=instructions,passed=True))
assert len(rows)>=3,len(rows)
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(dict(passed=True,input=str(a.object),sha256=hashlib.file_digest(a.object.open('rb'),'sha256').hexdigest(),handlers=len(rows),rows=rows),indent=2)+'\n')
print(f'PASS {len(rows)} wait/notify handlers: indirect musttail dispatch after the blocking helper, no return instruction')
