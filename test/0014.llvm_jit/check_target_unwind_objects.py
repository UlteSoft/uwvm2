#!/usr/bin/env python3
"""Inspect real contract-probe unwind records, not assembly directives alone.

Every inventory row is retained, including missing objects/metadata. Decoding
two function records (or the Windows non-leaf caller) is structural evidence,
NOT a live unwinder, registration, recursive-trap or cache-replay test. This
does not assert that decoded instructions match every prologue state.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--inventory',type=Path,required=True)
p.add_argument('--llvm-tools',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--inventory-only',action='store_true')
a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=False)
def digest(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream,'sha256').hexdigest()
rows=[]
for target in json.loads((a.inventory/'results.json').read_text()):
    row=dict(target=target['target'],triple=target['triple'],structural_records_present=False,live_unwind_tested=False)
    rows.append(row)
    probe=target['probes']['contract']
    if not probe['object_passed']:
        row['unavailable']='contract object emission failed'
    else:
        obj=a.inventory/target['target']/'contract.o'
        assert digest(obj)==probe['object_sha256']
        row['object_sha256']=probe['object_sha256']
        directory=a.out/target['target']; directory.mkdir()
        arm_elf=target['triple'].startswith(('arm','thumb')) and 'linux' in target['triple'] and not target['triple'].startswith('arm64')
        windows='windows' in target['triple']
        macho=any(os in target['triple'] for os in ('darwin','macos','watchos'))
        tool='llvm-objdump' if macho else 'llvm-readobj' if arm_elf or windows else 'llvm-dwarfdump'
        options=['--macho','--unwind-info'] if macho else ['--unwind' if tool=='llvm-readobj' else '--eh-frame']
        cmd=[str(a.llvm_tools/tool),*options,str(obj)]
        proc=subprocess.run(cmd,capture_output=True,text=True,timeout=60)
        (directory/'unwind.txt').write_text(proc.stdout)
        (directory/'unwind.stderr').write_text(proc.stderr)
        row.update(command=cmd,status=proc.returncode,tool_sha256=digest(a.llvm_tools/tool))
        if macho:
            # A leaf may be represented only by compact unwind. Requiring two
            # DWARF FDEs would incorrectly reject valid Apple ARM64 objects.
            row['record_kind']='macho-compact-unwind'
            row['compact_count']=proc.stdout.count('compact encoding:')
            dwarf_cmd=[str(a.llvm_tools/'llvm-dwarfdump'),'--eh-frame',str(obj)]
            dwarf=subprocess.run(dwarf_cmd,capture_output=True,text=True,timeout=30)
            (directory/'dwarf.txt').write_text(dwarf.stdout)
            (directory/'dwarf.stderr').write_text(dwarf.stderr)
            row.update(dwarf_command=dwarf_cmd,dwarf_status=dwarf.returncode,
                       fde_count=len(re.findall(r'\bFDE cie=',dwarf.stdout)))
            # x86 may retain the leaf only in DWARF while arm64 retains it only
            # in compact unwind. Do not sum counts: the two tables can overlap.
            present=(row['compact_count']==2 and '_uwvm_contract_caller' in proc.stdout) or (
                dwarf.returncode==0 and not dwarf.stderr.strip() and row['fde_count']==2)
        elif windows:
            row['record_kind']='windows-runtime-function'
            present='uwvm_contract_caller' in proc.stdout and 'RuntimeFunction {' in proc.stdout
        elif arm_elf:
            row['record_kind']='arm-ehabi'
            # Thumb's symbol value carries an ISA bit; readobj prints the
            # untagged code address and may omit FunctionName. Match addresses
            # against the real symbol table instead of requiring that label.
            nm_cmd=[str(a.llvm_tools/'llvm-nm'),'--defined-only','--format=posix',str(obj)]
            nm=subprocess.run(nm_cmd,capture_output=True,text=True,timeout=30)
            (directory/'symbols.txt').write_text(nm.stdout+nm.stderr)
            row['nm_command']=nm_cmd; row['nm_status']=nm.returncode
            addresses={int(fields[2],16)&~1 for line in nm.stdout.splitlines()
                       if len(fields:=line.split())>=3 and fields[0] in ('uwvm_contract_caller','uwvm_contract_leaf')}
            entries={int(value,16) for value in re.findall(r'FunctionAddress: (0x[0-9A-Fa-f]+)',proc.stdout)}
            present=nm.returncode==0 and len(addresses)==2 and entries==addresses
        else:
            row['record_kind']='dwarf-fde'
            row['fde_count']=len(re.findall(r'\bFDE cie=',proc.stdout))
            present=row['fde_count']==2
        row['structural_records_present']=proc.returncode==0 and not proc.stderr.strip() and present
    (a.out/'results.json').write_text(json.dumps(dict(scope=__doc__,rows=rows),indent=2)+'\n')
    print(row['target'],row['structural_records_present'],flush=True)
failed=[row['target'] for row in rows if not row['structural_records_present']]
print('Missing/unverified structural unwind coverage:',', '.join(failed),flush=True)
raise SystemExit(bool(failed) and not a.inventory_only)
