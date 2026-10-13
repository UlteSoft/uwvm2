#!/usr/bin/env python3
"""Inspect optimized native table64 register-ring dispatch (not sanitizer code)."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'test/0014.llvm_jit'))
from check_wasm3_tail_codegen import success_path_calls
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('binary',type=Path);p.add_argument('output',type=Path);p.add_argument('--llvm',type=Path,required=True)
a=p.parse_args();subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
a.output.mkdir(parents=True,exist_ok=False)
assembly=subprocess.check_output([str(a.llvm/'llvm-objdump'),'-d','--demangle',str(a.binary)],text=True)
assert 'file format elf64-x86-64' in assembly
chunks=re.split(r'(?m)^([0-9a-f]+) <(.+)>:\n',assembly);rows=[]
for i in range(1,len(chunks),3):
 name,body=chunks[i+1:i+3]
 if 'uwvmint_table64<' not in name or 'uwvm_interpreter_translate_option_t{true' not in name:continue
 op=re.search(r'table64_operation\)([0-6])',name);assert op,name
 assert re.search(r'\bjmpq?\s+\*',body),(name,'missing tail dispatch')
 assert not re.search(r'\bretq?\b',body),(name,'ordinary return in musttail handler')
 calls=success_path_calls(body,noreturn_symbols=('::wasm1p1_details::table_oob_terminate(',))
 if int(op[1]) in (0,5):assert calls==0,(name,'table.get/size successful-path helper call',body)
 (a.output/f'{len(rows)}.s').write_text(name+'\n'+body)
 rows.append(dict(symbol=name,operation=int(op[1]),musttail=True,success_calls=calls))
assert {r['operation'] for r in rows}==set(range(7)), 'missing table operations'
(a.output/'summary.json').write_text(json.dumps(dict(passed=True,scope='x86-64 actual interpreter handlers; no throughput claim',
 binary_sha256=hashlib.file_digest(a.binary.open('rb'),'sha256').hexdigest(),handlers=rows),indent=2)+'\n')
print('PASS',len(rows),'table64 handlers: all seven operations retain musttail; get/size have no success-path helper calls')
