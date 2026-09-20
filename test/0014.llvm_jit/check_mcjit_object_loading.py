#!/usr/bin/env python3
"""Exercise RuntimeDyld relocation separately from asm/object emission.

Use the bundled LLVM's llvm-rtdyld -verify (NEVER -execute) on every successfully
encoded inventory object. External symbols receive explicitly recorded synthetic
addresses. This checks loader acceptance, not their implementations, actual
relocated instruction semantics, executable mappings, VM behavior or live CFI.
Unavailable objects and unsupported loaders remain visible and fail the default
gate. --inventory-only records them without claiming a passing regression run.
Run under the same aggregate memory/CPU limits as the rest of the audit.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--inventory',type=Path,required=True)
p.add_argument('--rtdyld',type=Path,required=True)
p.add_argument('--nm',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--inventory-only',action='store_true')
a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=False)
def digest(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream,'sha256').hexdigest()
result=dict(scope=__doc__,inventory_sha256=digest(a.inventory/'results.json'),
            tools={str(tool):digest(tool) for tool in (a.rtdyld,a.nm)},rows=[])
for target in json.loads((a.inventory/'results.json').read_text()):
    row=dict(target=target['target'],triple=target['triple'],probes={})
    result['rows'].append(row)
    directory=a.out/target['target']; directory.mkdir()
    for name,probe in target['probes'].items():
        record=dict(relocation_accepted=False,execution_tested=False)
        row['probes'][name]=record
        if not probe['object_passed']:
            record['unavailable']='upstream object emission failed'
            continue
        obj=a.inventory/target['target']/(name+'.o')
        assert digest(obj)==probe['object_sha256'],obj
        record['object_sha256']=probe['object_sha256']
        nm_cmd=[str(a.nm),'--undefined-only','--format=posix',str(obj)]
        nm=subprocess.run(nm_cmd,capture_output=True,text=True,timeout=30)
        (directory/(name+'-undefined.log')).write_text(nm.stdout+nm.stderr)
        record['nm_command']=nm_cmd; record['nm_status']=nm.returncode
        if nm.returncode: continue
        symbols=sorted(set(line.split()[0] for line in nm.stdout.splitlines() if line.strip()))
        mappings=[f'{symbol}={0x100000+index*256}' for index,symbol in enumerate(symbols)]
        cmd=[str(a.rtdyld),'-verify','-triple='+target['triple'],'-mcpu='+target['cpu'],
             '-target-addr-start=4096','-target-addr-end=16777215',
             *['-dummy-extern='+mapping for mapping in mappings],str(obj)]
        with (directory/(name+'-load.log')).open('wb') as log:
            try: status=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=60).returncode
            except subprocess.TimeoutExpired: status='timeout'
        record.update(command=cmd,status=status,relocation_accepted=status==0)
    (a.out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(row['target'],{name:probe['relocation_accepted'] for name,probe in row['probes'].items()},flush=True)
failed=[row['target'] for row in result['rows'] if not all(probe['relocation_accepted'] for probe in row['probes'].values())]
print('Loader inventory finished; failures:',', '.join(failed),flush=True)
raise SystemExit(bool(failed) and not a.inventory_only)
