#!/usr/bin/env python3
"""Preserve/inspect the actual x86-64 register-ring memory handlers used by new Core 3 tests.

Indexed memories are resolved while compiling Wasm; the executed scalar/SIMD
handlers receive native object pointers. Bulk copy may call memmove and trap
helpers, but successful dispatch must remain an indirect tail jump.
"""
import argparse,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('binary',type=Path);p.add_argument('output',type=Path);p.add_argument('--llvm',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
symbols=subprocess.check_output([str(a.llvm/'llvm-nm'),'-C','-S','--defined-only',str(a.binary)],text=True)
rows=[];asm=[]
for line in symbols.splitlines():
    fields=line.split(maxsplit=3)
    if len(fields)!=4:continue
    address,size,_,name=fields
    match=re.search(r'::(uwvmint_memory_copy|i32_load16|i32_storeN|u16_copy_scaled_index)<',name)
    if not match or '_t{true,' not in name:continue
    if match[1]!='uwvmint_memory_copy' and 'bounds_check_mmap_full(' not in name:continue
    start=int(address,16);end=start+int(size,16)
    dis=subprocess.check_output([str(a.llvm/'llvm-objdump'),'--disassemble','--demangle',f'--start-address={start}',f'--stop-address={end}',str(a.binary)],text=True)
    if 'file format elf64-x86-64' not in dis:raise RuntimeError('requires x86-64 ELF input')
    if not re.search(r'\bjmpq?\s+\*',dis):raise RuntimeError('missing tail dispatch: '+name+'\n'+dis)
    if match[1] in ('i32_load16','i32_storeN') and re.search(r'\bcallq?\s',dis):raise RuntimeError('unexpected helper call in direct memory ring handler: '+name)
    rows.append(dict(handler=match[1],function=name,bytes=end-start,indirect_tail_jump=True,calls=len(re.findall(r'\bcallq?\s',dis))))
    asm.append(dis)
required={'uwvmint_memory_copy','i32_load16','i32_storeN'}
if not required.issubset({r['handler'] for r in rows}):raise RuntimeError(f'missing actual memory handlers: {rows}')
(a.output/'assembly.txt').write_text('\n'.join(asm));(a.output/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
print(f'PASS {len(rows)} actual register-ring memory handlers: indirect tail dispatch; full assembly recorded')
