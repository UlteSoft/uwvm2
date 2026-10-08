#!/usr/bin/env python3
"""Actual CLI dispatch through two aliases of one compliant native DSO."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for name in ('uwvm','plugin','wasm-tools','out','source-root'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args()
subprocess.run(['bash',str(a.source_root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
a.out.mkdir(exist_ok=False,parents=True)
fixture='''(module
 (import "permitted" "add" (func $add (param i32 i32) (result i32)))
 (import "permitted" "sizes" (func $allowed (param i32 i32) (result i32)))
 (import "restricted" "sizes" (func $denied (param i32 i32) (result i32)))
 (memory (export "memory") 1)
 (func (export "_start")
  i32.const 17 i32.const 25 call $add i32.const 42 i32.ne if unreachable end
  i32.const 0 i32.const 287454020 i32.store
  i32.const 4 i32.const 1432778632 i32.store
  i32.const 0 i32.const 4 call $allowed if unreachable end
  i32.const 0 i32.load i32.const 287454020 i32.eq if unreachable end
  i32.const 4 i32.load i32.const 1432778632 i32.eq if unreachable end
  i32.const 20 i32.const 287454020 i32.store
  i32.const 24 i32.const 1432778632 i32.store
  i32.const 20 i32.const 24 call $denied i32.const 76 i32.ne if unreachable end
  i32.const 20 i32.load i32.const 287454020 i32.ne if unreachable end
  i32.const 24 i32.load i32.const 1432778632 i32.ne if unreachable end
  i32.const 0 i32.const 4 call $allowed if unreachable end))'''
wat=a.out/'aliases.wat';wasm=a.out/'aliases.wasm';wat.write_text(fixture)
subprocess.run([str(a.wasm_tools),'parse',str(wat),'-o',str(wasm)],check=True)
subprocess.run([str(a.wasm_tools),'validate',str(wasm)],check=True)
rows=[]
for order in (('permitted','restricted'),('restricted','permitted')):
 for debug in (False,True):
  label=('-'.join(order))+'-'+('debug' if debug else 'run')
  cmd=[str(a.uwvm),'-Rdbg' if debug else '-Raot','-Rct','0','-Rllvm-cache-path','disable']
  for alias in order:cmd+=['--wasm-register-dl',str(a.plugin),alias]
  cmd+=['--wasip1-single-create','permitted','--wasip1-single-expose-host-api','permitted','--run',str(wasm)]
  r=subprocess.run(cmd,input=b'continue\nwait\nquit\n' if debug else b'',stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=40)
  log=a.out/(label+'.log');log.write_bytes(r.stdout)
  passed=r.returncode==0 and b'[fatal]' not in r.stdout and (not debug or (b'guest exited: 0' in r.stdout and b'timed out' not in r.stdout))
  rows.append(dict(label=label,argv=cmd,returncode=r.returncode,passed=passed,log_sha256=hashlib.sha256(r.stdout).hexdigest()))
  (a.out/'results.json').write_text(json.dumps(rows,indent=2))
  assert passed,(label,r.returncode,r.stdout[-6000:])
sha=lambda f:hashlib.file_digest(f.open('rb'),'sha256').hexdigest()
(a.out/'qualification.json').write_text(json.dumps(dict(passed=True,actual_VM=True,cgroup=Path('/proc/self/cgroup').read_text(),inputs={str(f):sha(f) for f in (a.uwvm,a.plugin,a.wasm_tools,wasm,Path(__file__))}),indent=2))
print('PASS actual CLI alias dispatch: permitted/restricted memory effects, both load orders, run/debug modes')
