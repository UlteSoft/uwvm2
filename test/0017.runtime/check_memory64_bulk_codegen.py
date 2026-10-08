#!/usr/bin/env python3
"""Audit native mmap bulk handlers: no pin/lock calls, only libc transfer + traps.

Interpreter records must keep their actual indirect musttail dispatch. LLVM
bridge bodies may tail-jump to memmove/memset. The saved LLVM object separately
records the generated mixed-width call ABI.
"""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path);p.add_argument('--kind',choices=['int','jit'],required=True)
p.add_argument('--binary',type=Path);p.add_argument('--assembly',type=Path);p.add_argument('--init',action='store_true')
p.add_argument('--llvm',type=Path,default=Path('/toolchain/bin'))
a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
assert bool(a.binary)!=bool(a.assembly)
source=a.binary or a.assembly
if a.assembly:assembly=a.assembly.read_text()
else:
 command=[str(a.llvm/'llvm-objdump'),'-d',str(a.binary)]
 if a.kind=='jit':
  # LLVM-linked fixture executables are large. Disassemble only this bridge
  # family, preserving the entire real function rather than a handpicked path.
  names=subprocess.check_output([str(a.llvm/'llvm-nm'),'--defined-only',str(a.binary)],text=True)
  keys=('llvm_jit_memory64_init_bridge',) if a.init else ('llvm_jit_memory64_copy_bridge','llvm_jit_memory64_fill_bridge')
  symbols=[line.split()[-1] for line in names.splitlines() if any(key in line for key in keys)]
  assert symbols
  command+=['--disassemble-symbols='+','.join(symbols)]
 assembly=subprocess.check_output(command,text=True)
 # Select raw symbol names before demangling: llvm-objdump interprets its
 # symbol filter in the displayed namespace when --demangle is enabled.
 assembly=subprocess.run([str(a.llvm/'llvm-cxxfilt')],input=assembly,text=True,
                         capture_output=True,check=True).stdout
chunks=re.split(r'(?m)^([0-9a-f]+) <(.+)>:\n',assembly);rows=[]
a.output.mkdir(parents=True,exist_ok=False)
for i in range(1,len(chunks),3):
 name,body=chunks[i+1:i+3]
 if a.kind=='int':
  if ('uwvmint_memory64_init<' if a.init else 'uwvmint_memory64_bulk<') not in name or 'uwvm_interpreter_translate_option_t{true' not in name:continue
  assert re.search(r'\bjmpq?\s+\*',body),(name,'missing musttail dispatch')
  assert not re.search(r'\bretq?\b',body),(name,'ordinary return')
 else:
  if a.init:
   if not name.startswith('uwvm2::') or not re.search(r'::llvm_jit_memory64_init_bridge\(',name):continue
  elif not name.startswith('void uwvm2::') or not re.search(r'::llvm_jit_memory64_(?:copy|fill)_bridge<',name):continue
 assert not re.search(r'\b(?:lock|mfence|lfence|sfence)\b',body),(name,'unexpected hardware synchronization')
 calls=re.findall(r'^.*\bcallq?\b.*$',body,re.M)
 allowed=('memmove','memset','::details::memory_oob_terminate(',
          '::lib::llvm_jit_runtime_trap(','::lib::llvm_jit_memory_out_of_bounds_trap(')
 assert all(any(symbol in call for symbol in allowed) for call in calls),(name,calls)
 # Direct external transfers may be optimized into a tail jump; they still
 # must be libc memory operations, not an allocation or operation-pin helper.
 transfers=re.findall(r'^.*\b(?:callq?|jmpq?)\b.*\bmem(?:move|set)\b.*$',body,re.M)
 assert transfers,(name,'missing bulk transfer')
 (a.output/f'{len(rows)}.s').write_text(name+'\n'+body)
 rows.append(dict(symbol=name,transfers=transfers,calls=calls,musttail_dispatch=a.kind=='int'))
assert len(rows)==(1 if a.init else 6),(a.kind,len(rows))
(a.output/'summary.json').write_text(json.dumps(dict(passed=True,kind=a.kind,
 scope='native x86-64 mmap bulk opfuncs/bridges; no throughput or frontend claim',
 input=str(source),sha256=hashlib.file_digest(source.open('rb'),'sha256').hexdigest(),handlers=rows),indent=2)+'\n')
print('PASS',a.kind,str(len(rows))+' native mmap bulk handlers: only libc transfers and trap calls'+('; musttail dispatch' if a.kind=='int' else ''))
