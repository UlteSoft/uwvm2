#!/usr/bin/env python3
"""Check actual size/grow musttail dispatch, and the call-free mmap size path."""
import argparse,hashlib,json,re,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'test/0014.llvm_jit'))
from check_wasm3_tail_codegen import success_path_calls
p=argparse.ArgumentParser(description=__doc__);p.add_argument('binary',type=Path);p.add_argument('out',type=Path);p.add_argument('--llvm',type=Path,required=True)
a=p.parse_args();subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
a.out.mkdir(parents=True,exist_ok=False)
assembly=subprocess.check_output([str(a.llvm/'llvm-objdump'),'-d','--demangle',str(a.binary)],text=True)
assert 'file format elf64-x86-64' in assembly
chunks=re.split(r'(?m)^([0-9a-f]+) <(.+)>:\n',assembly);rows=[]
for i in range(1,len(chunks),3):
 name,body=chunks[i+1:i+3]
 if 'uwvmint_memory64_pages<' not in name or 'uwvm_interpreter_translate_option_t{true' not in name:continue
 assert re.search(r'\bjmpq?\s+\*',body),(name,'missing musttail dispatch')
 assert not re.search(r'\bretq?\b',body),(name,'non-tail return')
 grow='uwvmint_memory64_pages<true' in name
 if not grow:assert success_path_calls(body)==0,(name,'memory.size hot call')
 (a.out/f'{len(rows)}.s').write_text(name+'\n'+body)
 rows.append(dict(symbol=name,grow=grow,musttail=True))
assert any(r['grow'] for r in rows) and any(not r['grow'] for r in rows)
(a.out/'summary.json').write_text(json.dumps(dict(passed=True,scope='internal native x86-64 memory64 size/grow opfuncs',
 sha256=hashlib.file_digest(a.binary.open('rb'),'sha256').hexdigest(),handlers=rows),indent=2)+'\n')
print(f'PASS {len(rows)} memory64 size/grow opfuncs: musttail jumps; memory.size success paths have no calls')
