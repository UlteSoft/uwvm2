#!/usr/bin/env python3
"""Focused Core 3 memory64/table64 limits against the Wasmtime compiler.

Both validators see the same binary bytes. Compilation avoids attempting to
allocate a memory/table with a very large but valid declared maximum.
"""
import argparse,json,resource,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--decoder',type=Path,required=True)
p.add_argument('--wasmtime',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out.mkdir(parents=True,exist_ok=False)
def leb(n):
 b=bytearray()
 while True:
  low=n&127;n>>=7;b.append(low|(128 if n else 0))
  if not n:return bytes(b)
def limits(flag,minimum,maximum=None):return bytes([flag])+leb(minimum)+(leb(maximum) if maximum is not None else b'')
cases=[]
for kind,max32,max64 in [('memory',1<<16,1<<48),('table',(1<<32)-1,(1<<64)-1)]:
 for name,raw,valid in [
  ('i64-unbounded',limits(4,0),True),('i64-maximum',limits(5,0,max64),True),
  ('i64-above-32',limits(5,0,max32+1),True),('i32-maximum',limits(1,0,max32),True),
  ('i32-excess',limits(1,0,max32+1),False),('descending',limits(5,2,1),False),
  ('truncated',bytes([5,0]),False),('u65',bytes([4])+b'\x80'*9+b'\x02',False)]:
  cases.append((kind,name,raw,valid))
# Threads and memory64 are independent switches; the shared maximum stays mandatory.
for name,raw,valid in [('shared64',limits(7,0,1),True),('shared64-no-max',limits(6,0),False),
                       ('i64-excess',limits(5,0,(1<<48)+1),False)]:
 cases.append(('memory',name,raw,valid))
for name,raw,valid in [('shared-table',limits(7,0,1),False)]:cases.append(('table',name,raw,valid))
rows=[]
for kind,name,raw,expected in cases:
 for imported in [False,True]:
  stem=f'{kind}-{name}-'+('import' if imported else 'local');wasm=a.out/(stem+'.wasm')
  entry=(b'\x70' if kind=='table' else b'')+raw
  if imported:entry=b'\x01m\x01x'+bytes([1 if kind=='table' else 2])+entry
  payload=b'\x01'+entry;sec=2 if imported else (4 if kind=='table' else 5)
  wasm.write_bytes(b'\0asm\x01\0\0\0'+bytes([sec])+leb(len(payload))+payload)
  command=[str(a.wasmtime),'compile','-W','memory64=y,threads=y','-C','cache=n',str(wasm),'-o',str(a.out/(stem+'.cwasm'))]
  ref=subprocess.run(command,capture_output=True,timeout=30)
  own=subprocess.run([str(a.decoder),kind,raw.hex()],capture_output=True,timeout=10)
  (a.out/(stem+'.wasmtime.log')).write_bytes(ref.stdout+ref.stderr)
  (a.out/(stem+'.decoder.log')).write_bytes(own.stdout+own.stderr)
  row=dict(kind=kind,name=name,imported=imported,expected=expected,wasmtime=ref.returncode,decoder=own.returncode,command=command)
  rows.append(row);(a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
  if (ref.returncode==0)!=expected or (own.returncode==0)!=expected:raise RuntimeError(f'limits mismatch: {row}')
print(f'PASS {len(rows)} memory64/table64 binary limits: Wasmtime compile and shared decoder agree')
