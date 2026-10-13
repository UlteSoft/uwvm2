#!/usr/bin/env python3
"""Inspect actual new-syntax Wasm translation, then preserve LLVM O3/native assembly."""
import argparse,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path);p.add_argument('--llvm',type=Path,required=True);p.add_argument('--triple',default='x86_64-unknown-linux-gnu');p.add_argument('--cpu',default='raptorlake');p.add_argument('--address-lowering',choices=('symbols','process-local'),default='symbols');a=p.parse_args()
requirements={'alternate-memory-selection':('memory0_begin','memory1_begin'),'copy-memory1-to-0':('memory_0_native_memory','memory_1_native_memory'),'simd-load':('memory1_begin',),'index-129-memarg':('memory129_begin',)}
rows=[]
for fixture,memories in requirements.items():
    for policy in ('instruction','unwind'):
        ir=a.directory/f'{fixture}-{policy}.ll';text=ir.read_text()
        match=re.search(r'^define [^\n]*@uwvm_m_[0-9a-f]+_func_0\([^\n]*\) [^\n]*\{\n(.*?)^}',text,re.M|re.S)
        if not match:raise RuntimeError(f'{ir}: missing actual native leaf')
        body=match[1]
        if a.address_lowering == 'symbols':
            for memory in memories:
                if not re.search(r'@uwvm_m_[0-9a-f]+_'+memory+r'\b',body):raise RuntimeError(f'{ir}: memory symbol not used in native leaf: {memory}')
        else:
            # The production RISC-V workaround embeds full-width process-local
            # pointers through inline `li`. Inspect its real target-native IR;
            # execution in the complete QEMU CLI separately checks selection.
            addresses=set(re.findall(r'asm [^\n]*"li \$0, \$1"[^\n]*\(i64 (-?\d+)\)',body))
            if len(addresses) < len(memories):raise RuntimeError(f'{ir}: missing full-width process-local pointer operands')
        calls=[line for line in body.splitlines() if re.search(r'\b(call|invoke|callbr)\b',line) and '@llvm.' not in line and not re.search(r'\b(?:tail )?call\b[^\n]*\basm\b',line)]
        expected=(2 if policy=='instruction' else 0)+(fixture=='copy-memory1-to-0')
        if len(calls)!=expected:raise RuntimeError(f'{ir}: expected {expected} calls, got {calls}')
        optimized=ir.with_suffix('.opt.ll');assembly=ir.with_suffix('.s')
        subprocess.run([str(a.llvm/'opt'),'-S','-passes=default<O3>',str(ir),'-o',str(optimized)],check=True)
        subprocess.run([str(a.llvm/'llc'),'-O3','-mtriple='+a.triple,'-mcpu='+a.cpu,str(optimized),'-o',str(assembly)],check=True)
        if policy=='unwind' and '.cfi_startproc' not in assembly.read_text():raise RuntimeError(f'{assembly}: missing unwind metadata')
        rows.append(dict(fixture=fixture,policy=policy,memory_symbols=memories if a.address_lowering=='symbols' else [],native_leaf_calls=len(calls),generated_assembly=str(assembly),target=a.triple,address_lowering=a.address_lowering))
(a.directory/'ir-results.json').write_text(json.dumps(rows,indent=2)+'\n');print('PASS actual multi-memory LLVM IR: separate objects, exact frame overhead, O3 codegen and unwind CFI')
