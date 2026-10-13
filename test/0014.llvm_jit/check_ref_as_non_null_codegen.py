#!/usr/bin/env python3
"""Check actual x86-64 reference-check handlers and signed JIT objects, not a surrogate IR function."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
from check_wasm3_native_frame_codegen import decode_object
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--cache',type=Path,required=True);p.add_argument('--int-binary',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--ros',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);a.out.mkdir(parents=True,exist_ok=False)
objdump='/toolchain/bin/llvm-objdump';nm='/toolchain/bin/llvm-nm';rows=[]
for policy in ('instruction','unwind'):
 files=list((a.cache/policy).rglob('*.uwvm-ljc'));assert len(files)==1
 obj=a.out/(policy+'.o');obj.write_bytes(decode_object(files[0].read_bytes(),a.ros));assembly=subprocess.check_output([objdump,'-dr',str(obj)],text=True);obj.with_suffix('.s').write_text(assembly)
 sections=subprocess.check_output([objdump,'-h',str(obj)],text=True);obj.with_suffix('.sections').write_text(sections);assert '.eh_frame' in sections
 chunks=re.split(r'(?m)^([0-9a-f]+) <(.+)>:\n',assembly);found=0
 for n in range(1,len(chunks),3):
  name,body=chunks[n+1:n+3]
  if not re.fullmatch(r'uwvm_m_[0-9a-f]+_func_[0-5]',name):continue
  found+=1;calls=len(re.findall(r'\bcallq?\s',body));assert calls==(3 if policy=='instruction' else 1),(name,body)
  # The null trap is out of line after the successful return. Successful unwind paths have no calls or frame spills.
  hot=body.split('\tretq')[0];hot_calls=len(re.findall(r'\bcallq?\s',hot));assert hot_calls==(2 if policy=='instruction' else 0)
  assert '\ttestl\t' in hot and not re.search(r'\bsh[lr][lq]?\s',hot),(name,body)
  if policy=='unwind':assert not re.search(r'\b(?:pushq|popq)\s',hot),body
  rows.append(dict(kind='jit',policy=policy,symbol=name,calls=calls,success_calls=hot_calls,object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest()))
 assert found==6
symbols=subprocess.check_output([nm,'-S','--defined-only','--demangle',str(a.int_binary)],text=True);count=0;tail_count=0
for line in symbols.splitlines():
 if 'uwvmint_ref_as_non_null<' not in line:continue
 start,size,*_=line.split();address=int(start,16);end=address+int(size,16);tail='translate_option_t{true,' in line
 assembly=subprocess.check_output([objdump,'-d',f'--start-address={address}',f'--stop-address={end}',str(a.int_binary)],text=True)
 (a.out/('int-'+str(count)+'.s')).write_text(assembly)
 ins=[x.split('\t')[1:] for x in assembly.splitlines() if re.match(r'\s+[0-9a-f]+:',x)]
 ins=['\t'.join(x) for x in ins];stop=next(i for i,x in enumerate(ins) if re.match(r'(?:jmpq?\s+\*|retq?\b)',x));hot=ins[:stop+1]
 assert not any(re.match(r'(?:callq?|pushq|popq)\b',x) for x in hot),hot
 assert sum(x.startswith('cmpl') for x in hot)==1,hot
 assert '-0x8(' in hot[0 if tail else 1],hot
 if tail:assert len(hot)==5 and hot[-1].startswith('jmp'),hot;tail_count+=1
 else:assert len(hot)==5 and hot[-1].startswith('ret'),hot
 rows.append(dict(kind='int',tail=tail,address=start,hot_instructions=hot,binary_sha256=hashlib.sha256(a.int_binary.read_bytes()).hexdigest()));count+=1
assert count==6 and tail_count==4
(a.out/'summary.json').write_text(json.dumps(dict(passed=True,jit_functions=12,int_handlers=count,checks=rows),indent=2)+'\n')
print('PASS: 12 JIT functions (independent unwind, direct tag test); 6 interpreter handlers (4 mandatory tail jumps, no hot calls/spills)')
