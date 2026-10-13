#!/usr/bin/env python3
"""Require a real T0-to-native OSR before Core 3 direct/indirect tail transfer."""
import argparse, hashlib, itertools, json, re, resource, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('uwvm',type=Path);p.add_argument('out',type=Path)
p.add_argument('--wat2wasm',required=True);p.add_argument('--wasmtime',required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=False)
rows=[]
for indirect, tuple_result in itertools.product([False,True], [0,2,27]):
 name=('indirect' if indirect else 'direct')+('-wide' if tuple_result==27 else '-tuple' if tuple_result else '')
 declaration='(table 1 funcref) (elem (i32.const 0) $finish)' if indirect else ''
 edge='i32.const 0 return_call_indirect (type $finish_type)' if indirect else 'return_call $finish'
 wat=a.out/(name+'.wat');wasm=wat.with_suffix('.wasm')
 wat.write_text('(module (type $finish_type (func (param i32 i64) (result i64))) '+declaration+'''
 (func $finish (type $finish_type) (param $trap i32) (param $s i64) (result i64)
   local.get $trap if unreachable end local.get $s)
 (func $hot (param $trap i32) (result i64) (local $n i32) (local $s i64)
   i32.const 1000000 local.set $n
   loop $again
'''+('nop '*2000)+'''
     local.get $s i64.const 1 i64.add local.set $s
     local.get $n i32.const 1 i32.sub local.tee $n br_if $again
   end local.get $trap local.get $s 
'''+edge+''')
 (func (export "_start") (param $mode i32)
   i32.const 0 call $hot i64.const 1000000 i64.ne if unreachable end
   i32.const 0 call $hot i64.const 1000000 i64.ne if unreachable end
   local.get $mode i32.const 1 i32.eq if i32.const 1 call $hot drop end
   local.get $mode i32.const 2 i32.eq if unreachable end))
''')
 if tuple_result:
  extra_types=' '.join(['i64']*24+['v128']) if tuple_result==27 else ''
  extra_values=' '.join([f'i64.const {201+i}' for i in range(24)]+['v128.const i32x4 1 2 3 4']) if tuple_result==27 else ''
  checks='v128.const i32x4 1 2 3 4 i32x4.eq i32x4.all_true if else unreachable end '+''.join(f'i64.const {201+i} i64.ne if unreachable end ' for i in reversed(range(24))) if tuple_result==27 else ''
  source=wat.read_text().replace('(result i64)', '(result i64 i32 '+extra_types+')')
  source=source.replace('end local.get $s)', 'end local.get $s i32.const 31 '+extra_values+')')
  source=source.replace('call $hot i64.const', 'call $hot '+checks+'i32.const 31 i32.ne if unreachable end i64.const')
  source=source.replace('call $hot drop end', 'call $hot '+('drop '*tuple_result)+'end')
  wat.write_text(source)
 subprocess.run([a.wat2wasm,'--enable-tail-call' ,str(wat),'-o',str(wasm)],check=True)
 # _start takes one parameter; invoke it explicitly in the reference engine.
 for mode in [0,1,2]:
  ref=subprocess.run([a.wasmtime,'-C','cache=n','-W','tail-call=y','--invoke','_start',str(wasm),str(mode)],capture_output=True,timeout=60)
  (a.out/(name+'-reference-'+str(mode)+'.log')).write_bytes(ref.stdout+ref.stderr)
  assert (ref.returncode==0)==(mode==0)
 for policy in ['instruction','unwind']:
  for mode in [0,1,2]:
   label=name+'-'+policy+'-'+str(mode);compile_log=a.out/(label+'.compile.log')
   command=[str(a.uwvm.resolve()),'-Rtiered','-Rtiered-disable-t2','-Rct','0','-Rllvm-cache-path','disable','-Rllvm-call-stack',policy,
    '-Rclog','file',str(compile_log),'-WFE-tail-call','-Wstart','2',str(mode),'--run',str(wasm)]
   run=subprocess.run(command,capture_output=True,timeout=120)
   log=re.sub(r'\x1b\[[0-9;?]*[ -/]*[@-~]','',(run.stdout+run.stderr).decode(errors='replace'))
   (a.out/(label+'.log')).write_text(log);compilation=compile_log.read_text()
   stack=[int(n) for n in re.findall(r'func_idx=(\d+)',log)]
   expected=[] if mode==0 else [0,2] if mode==1 else [2]
   ready=[int(n) for n in re.findall(r'tiered_osr_ready=(\d+)',compilation)]
   passed=(run.returncode==0 if mode==0 else run.returncode!=0 and 'unreachable' in log) and stack==expected
   # A passing result in the interpreter is insufficient: every run must request
   # native OSR, and the successful run must confirm an actual completed reentry.
   passed=passed and 'tiered-osr-request ' in compilation and (mode!=0 or bool(ready) and max(ready)>0)
   rows.append(dict(case=label,command=command,exit=run.returncode,stack=stack,osr_ready=ready,passed=passed))
   (a.out/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
   if not passed:raise RuntimeError(label+'\n'+log+'\n'+compilation[-3000:])
(a.out/'summary.json').write_text(json.dumps(dict(passed=True,cases=len(rows),binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest()),indent=2)+'\n')
print('PASS direct/indirect tail calls after actual OSR: results, retired frames and balanced interpreter resumption')
