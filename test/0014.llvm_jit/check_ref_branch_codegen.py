#!/usr/bin/env python3
"""Audit actual reference-branch JIT objects and interpreter handlers on x86-64."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
from check_wasm3_native_frame_codegen import decode_object
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--cache',type=Path,required=True);p.add_argument('--int-binary',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--ros',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);a.out.mkdir(parents=True,exist_ok=False)
objdump='/toolchain/bin/llvm-objdump';rows=[]
for policy in ('instruction','unwind'):
 files=list((a.cache/policy).rglob('*.uwvm-ljc'));assert len(files)==1
 obj=a.out/(policy+'.o');obj.write_bytes(decode_object(files[0].read_bytes(),a.ros));assembly=subprocess.check_output([objdump,'-dr',str(obj)],text=True);obj.with_suffix('.s').write_text(assembly)
 sections=subprocess.check_output([objdump,'-h',str(obj)],text=True);obj.with_suffix('.sections').write_text(sections);assert '.eh_frame' in sections
 chunks=re.split(r'(?m)^([0-9a-f]+) <(.+)>:\n',assembly);found=0
 for n in range(1,len(chunks),3):
  name,body=chunks[n+1:n+3]
  if not re.fullmatch(r'uwvm_m_[0-9a-f]+_func_[0-7]',name):continue
  found+=1;calls=len(re.findall(r'\bcallq?\s',body));assert calls==(2 if policy=='instruction' else 0),(name,body)
  assert '\ttestl\t' in body and not re.search(r'\bsh[lr][lq]?\s',body),(name,body)
  if policy=='unwind':assert not re.search(r'\b(?:pushq|popq)\s',body),body
  rows.append(dict(kind='jit',policy=policy,symbol=name,calls=calls,sha256=hashlib.sha256(obj.read_bytes()).hexdigest()))
 assert found==8
symbols=subprocess.check_output(['/toolchain/bin/llvm-nm','-S','--defined-only','--demangle',str(a.int_binary)],text=True);count=0;tails=0
for line in symbols.splitlines():
 if 'uwvmint_br_on_reference<' not in line:continue
 start,size,*_=line.split();address=int(start,16);end=address+int(size,16);tail='translate_option_t{true,' in line
 assembly=subprocess.check_output([objdump,'-d',f'--start-address={address}',f'--stop-address={end}',str(a.int_binary)],text=True);(a.out/('int-'+str(count)+'.s')).write_text(assembly)
 instructions=['\t'.join(x.split('\t')[1:]) for x in assembly.splitlines() if re.match(r'\s+[0-9a-f]+:',x)]
 assert not any(re.match(r'(?:callq?|pushq|popq)\b',x) for x in instructions),instructions
 assert sum(x.startswith('cmpl') and '-0x8(' in x for x in instructions)==1,instructions
 if tail:
  assert int(size,16)<=22 and sum(bool(re.match(r'jmpq?\s+\*',x)) for x in instructions)==2,instructions;tails+=1
 else:assert sum(bool(re.match(r'retq?\b',x)) for x in instructions)==2,instructions
 rows.append(dict(kind='int',tail=tail,address=start,size=int(size,16),instructions=instructions));count+=1
assert count==12 and tails==8
(a.out/'summary.json').write_text(json.dumps(dict(passed=True,jit_functions=16,int_handlers=count,checks=rows),indent=2)+'\n')
print('PASS: 16 JIT reference-branch functions; 12 interpreter handlers, 8 mandatory tail variants <=22 bytes; no reference helper calls or spills')
