#!/usr/bin/env python3
"""Core 3 typed active-data parsing versus complete Wasmtime modules.

The UWVM probe supplies explicit memory address metadata and invokes the real
bounded data parser. Binary memory64 compiler dispatch is a separate requirement.
"""
import argparse,json,resource,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--parser',type=Path,required=True);p.add_argument('--wasmtime',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out.mkdir(parents=True,exist_ok=False)
def leb(n):
 r=bytearray()
 while True:
  low=n&127;n>>=7;r.append(low|(128 if n else 0))
  if not n:return bytes(r)
def signed(n):
 r=bytearray()
 while True:
  low=n&127;n>>=7;done=(n==0 and not low&64)or(n==-1 and low&64);r.append(low|(0 if done else 128))
  if done:return bytes(r)
def sec(n,b):return bytes([n])+leb(len(b))+b
def con(wide,n):return bytes([0x42 if wide else 0x41])+signed(n)
rows=[]
def add(label,mask,body,expected,extended=True,reference=True):
 rows.append(dict(label=label,mask=mask,body=body.hex(),expected=expected,extended=extended,reference=reference))
for mask in range(4):
 for flag,index in [(0,0),(2,0),(2,1)]:
  wide=bool(mask&(1<<index));prefix=bytes([flag])+ (leb(index) if flag==2 else b'')
  def expression(label,expr,expected=True,extended=True,reference=True):
   add(f'{label}-m{index}-flag{flag}',mask,prefix+expr+b'\x0b\x01\xab',expected,extended,reference)
  for value in [0,1,65535,65536,-1,-2147483648]:
   expression(f'const-{value}',con(wide,value))
  if wide:
   for value in [1<<32,(1<<63)-1,-(1<<63)]:expression(f'wide-{value}',con(True,value))
  expression('wrong-width',con(not wide,0),False)
  expression('import-global',b'\x23\0',wide)
  expression('local-i64-global',b'\x23\1',wide)
  expression('mutable-global',b'\x23\2',False)
  expression('local-i32-global',b'\x23\3',not wide)
  expression('missing-global',b'\x23\4',False)
  for opcode in ([0x7c,0x7d,0x7e] if wide else [0x6a,0x6b,0x6c]):
   expr=con(wide,-1)+con(wide,2)+bytes([opcode])
   expression(f'extended-{opcode}',expr)
   expression(f'extended-disabled-{opcode}',expr,False,False,False)
   expression(f'wrong-extended-operand-{opcode}',con(not wide,1)+con(wide,2)+bytes([opcode]),False)
  expression('empty',b'',False);expression('extra-result',con(wide,1)*2,False)
  expression('nonconstant',con(wide,0)+b'\x1a'+con(wide,1),False)
  expression('base-const-disabled',con(wide,1),True,False,False)
  expression('import-disabled',b'\x23\0',wide,False,False)
  expression('local-disabled',b'\x23\1',False,False,False)
  full=prefix+con(wide,-(1<<63) if wide else -(1<<31))+b'\x0b\x03abc'
  for length in range(1,len(full)):add(f'truncated-{index}-{flag}-{length}',mask,full[:length],False)
  # Binary constant encodings must fit their signed 32/64-bit carrier.
  bad=b'\x42'+b'\x80'*9+b'\x01' if wide else b'\x41'+b'\x80'*4+b'\x08'
  expression('signed-overflow',bad,False)
 add('missing-memory',mask,b'\x02\x02\x42\0\x0b\0',False)
 add('passive',mask,b'\x01\x01\xab',True)
request=''.join(f"{r['mask']} {int(r['extended'])} {r['body']}\n" for r in rows)
(a.out/'request.txt').write_text(request)
own=subprocess.run([str(a.parser),'--parse'],input=request,text=True,capture_output=True,timeout=90)
(a.out/'parser.stdout').write_text(own.stdout);(a.out/'parser.stderr').write_text(own.stderr)
assert own.returncode==0,own.stderr[-4000:]
answers=own.stdout.splitlines();assert len(answers)==len(rows),(len(answers),len(rows))
for number,(r,answer) in enumerate(zip(rows,answers)):
 r['parser']=int(answer)
 if bool(r['parser'])!=r['expected']:
  (a.out/'failed.json').write_text(json.dumps(r,indent=2));raise RuntimeError(f'parser mismatch {number}: {r}')
 if not r['reference']:continue
 mask=r['mask'];mt=lambda wide:bytes([5 if wide else 1,1,2])
 wasm=b'\0asm\x01\0\0\0'+sec(2,b'\x02\x01m\x01m\x02'+mt(mask&1)+b'\x01m\x01g\x03\x7e\0')+sec(5,b'\x01'+mt(mask&2))
 wasm+=sec(6,b'\x03\x7e\0\x42\x05\x0b\x7e\x01\x42\x09\x0b\x7f\0\x41\x07\x0b')+sec(11,b'\x01'+bytes.fromhex(r['body']))
 path=a.out/f'{number:04d}.wasm';path.write_bytes(wasm)
 ref=subprocess.run([str(a.wasmtime),'compile','-W','memory64=y','-C','cache=n',str(path),'-o',str(a.out/'last.cwasm')],capture_output=True,timeout=30)
 r['wasmtime']=ref.returncode;(a.out/f'{number:04d}.wasmtime.log').write_bytes(ref.stdout+ref.stderr)
 if (ref.returncode==0)!=r['expected']:
  (a.out/'failed.json').write_text(json.dumps(r,indent=2));raise RuntimeError(f'Wasmtime mismatch {number}: {r}')
(a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
print(f'PASS {len(rows)} guarded Core 3 data checks / {sum(r["reference"] for r in rows)} complete Wasmtime modules: mixed imported/local memory32/memory64, both active forms, i64/global/extended offsets, independent gates, signed overflow and truncations')
