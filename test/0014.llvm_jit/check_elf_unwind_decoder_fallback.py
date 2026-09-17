#!/usr/bin/env python3
"""Cross-check unresolved ELF unwind-inspection cases with GNU readelf.

Some LLVM object-inspection readers do not resolve SPARC/SystemZ relocations
or decode ARM BE EHABI. Preserve the LLVM result and independently report GNU
decoding. Neither inspection proves live RuntimeDyld/CFI correctness; in
particular readable ARM BE EHABI does not fix its native loader's ABS32 bug.
GNU readelf is only an inspector, never a ROS LLVM build/link dependency.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--inventory',type=Path,required=True)
p.add_argument('--llvm-inspection',type=Path,required=True)
p.add_argument('--readelf',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=False)
def digest(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream,'sha256').hexdigest()
rows=[]
prior=json.loads((a.llvm_inspection/'results.json').read_text())
result=dict(scope=__doc__,prior_sha256=digest(a.llvm_inspection/'results.json'),
            readelf_sha256=digest(a.readelf),rows=rows)
(a.out/'readelf-version.txt').write_text(subprocess.check_output([str(a.readelf),'--version'],text=True))
for old in prior['rows']:
    if old['structural_records_present']: continue
    obj=a.inventory/old['target']/'contract.o'
    if not obj.is_file() or 'object_sha256' not in old: continue
    with obj.open('rb') as stream:
        if stream.read(4)!=b'\x7fELF': continue
    assert digest(obj)==old['object_sha256']
    arm=old['triple'].startswith(('arm','thumb')) and not old['triple'].startswith('arm64')
    cmd=[str(a.readelf),'--unwind' if arm else '--debug-dump=frames','--wide',str(obj)]
    proc=subprocess.run(cmd,capture_output=True,text=True,timeout=30)
    (a.out/(old['target']+'.txt')).write_text(proc.stdout)
    (a.out/(old['target']+'.stderr')).write_text(proc.stderr)
    count=len(re.findall(r'^0x[0-9a-fA-F]+(?:\s[^\n:]*)?:\s',proc.stdout,re.M)) if arm else len(re.findall(r'\bFDE cie=',proc.stdout))
    passed=proc.returncode==0 and not proc.stderr.strip() and count==2
    rows.append(dict(target=old['target'],command=cmd,status=proc.returncode,record_count=count,
                     structural_records_present=passed,prior_llvm_inspection=old,live_unwind_tested=False))
    (a.out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(old['target'],passed,flush=True)
raise SystemExit(any(not row['structural_records_present'] for row in rows))
