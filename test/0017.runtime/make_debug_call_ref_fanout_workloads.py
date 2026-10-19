#!/usr/bin/env python3
"""Independently checked dynamic call_ref locality/cache-thrashing inputs."""
import argparse,hashlib,json,subprocess
from pathlib import Path
from make_debug_core3_feature_workloads import PRELUDE,EPILOGUE,SPEC
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--wasm-tools',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--iterations',type=int,default=1_000_000)
a=p.parse_args();assert 1<=a.iterations<=10_000_000;a.out.mkdir(parents=True,exist_ok=False);rows=[]
for count in [1,8,32,64,256]:
 n=a.iterations;cycles,rest=divmod(n,count)
 expected=(3*n*(n-1)//2+n+cycles*count*(count-1)//2+rest*(rest-1)//2)&0xffffffff
 # Independent small-scale oracle cross-check; only run in the SSH cgroup.
 small=min(n,10_000);cycles_small,rest_small=divmod(small,count)
 assert sum(3*i+1+i%count for i in range(small))&0xffffffff==(3*small*(small-1)//2+small+cycles_small*count*(count-1)//2+rest_small*(rest_small-1)//2)&0xffffffff
 names=[f'$target{i}' for i in range(count)]
 declaration='(type $unary (func (param i32) (result i32)))\n'+f'(table $targets {count} (ref null $unary))\n'
 declaration+='(elem (table $targets) (i32.const 0) (ref $unary) '+' '.join('(ref.func '+v+')' for v in names)+')\n'
 declaration+='\n'.join(f'(func {v} (type $unary) local.get 0 i32.const 3 i32.mul i32.const {i+1} i32.add)' for i,v in enumerate(names))
 kernel=f'''(func $kernel (export "kernel") (param $n i32) (result i64)
 (local $i i32) (local $sum i32) (local $target (ref null $unary)) (local $value i32)
 (loop $again
  local.get $i i32.const {count-1} i32.and table.get $targets local.set $target
  local.get $i local.get $target ref.as_non_null call_ref $unary local.set $value
  local.get $sum local.get $value i32.add local.set $sum
  local.get $i i32.const 1 i32.add local.tee $i local.get $n i32.lt_u br_if $again)
 local.get $sum i64.extend_i32_u)'''
 name=f'call_ref_fanout_{count}';wat=a.out/(name+'.wat');wasm=wat.with_suffix('.wasm')
 wat.write_text(f';; {SPEC}; dynamic targets={count}; independently modulo-checked sum\n(module\n{PRELUDE}\n(global $iterations i32 (i32.const {n})) (global $kernel_repeats i32 (i32.const 1))\n{declaration}\n{kernel}\n{EPILOGUE})\n')
 row={'name':name,'feature':'Typed reference target locality','required_features':['reference-types','function-references'],'opcode_witness':'call_ref','oracle':'sum(3*i+1+(i%targets)) modulo 2**32, independent closed form and small iterative cross-check','iterations':n,'kernel_repeats':1,'target_count':count,'expected_checksum':expected,'wat_sha256':hashlib.sha256(wat.read_bytes()).hexdigest(),'tool_runs':[]}
 for argv in [[str(a.wasm_tools),'parse',str(wat),'-o',str(wasm)],[str(a.wasm_tools),'validate','--features','all',str(wasm)],[str(a.wasm_tools),'print',str(wasm)]]:
  q=subprocess.run(argv,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60);(a.out/(name+'.'+argv[1]+'.log')).write_bytes(q.stdout);row['tool_runs'].append({'argv':argv,'exit':q.returncode});assert q.returncode==0,q.stdout[-2500:]
  if argv[1]=='print':assert b'call_ref' in q.stdout and b'table.get' in q.stdout
 row.update(wasm_sha256=hashlib.sha256(wasm.read_bytes()).hexdigest(),validation='passed');rows.append(row);(a.out/'manifest.json').write_text(json.dumps(rows,indent=2)+'\n')
print('PASS official parse/validate/print and independent fanout checksums',flush=True)
