#!/usr/bin/env python3
"""Compare Core 3 + threads atomic bytes with Wasmtime's whole-module compiler.

The local executable is the shared atomic decoder, not the uwvm CLI frontend.
Every module supplies correctly typed operands for its selected memory; the
matrix targets all FE families with memory64 offsets and exact atomic alignment.
"""
import argparse,json,resource,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--decoder',type=Path,required=True);p.add_argument('--wasmtime',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out.mkdir(parents=True,exist_ok=False)
def leb(n):
 b=bytearray()
 while True:
  low=n&127;n>>=7;b.append(low|(128 if n else 0))
  if not n:return bytes(b)
def section(id,payload):return bytes([id])+leb(len(payload))+payload
def constant(wide):return bytes([0x42 if wide else 0x41,0])
rows=[];opcodes=[0,1,2,3,*range(0x10,0x4f)]
for opcode in opcodes:
 if opcode==3:variants=[('fence',0,0,0,True)]
 else:
  align=(3 if opcode==2 else 2) if opcode<3 else [2,3,0,1,0,1,2][(opcode-0x10)%7]
  variants=[('wide-offset',1,align,1<<32,True),('maximum-offset',1,align,(1<<64)-1,True),
            ('memory32-wide-offset',0,align,1<<32,False),('over-aligned',1,align+1,0,False)]
 for label,index,alignment,offset,expected in variants:
  immediate=leb(opcode)+(b'\0' if opcode==3 else leb(64+alignment)+leb(index)+leb(offset))
  body=bytearray(b'\0') # zero local declarations
  if opcode!=3:
   body+=constant(index==1)
   if opcode==0:body+=constant(False)
   elif opcode in [1,2]:body+=constant(opcode==2)+constant(True)
   else:
    family=(opcode-0x10)//7;variant=(opcode-0x10)%7;wide=variant==1 or variant>=4
    if family>0:body+=constant(wide)
    if family==8:body+=constant(wide)
  body+=b'\xfe'+immediate
  if opcode!=3 and (opcode<3 or (opcode-0x10)//7!=1):body+=b'\x1a'
  body+=b'\x0b'
  # Two actual shared memories, first i32 and second i64. Both have maxima.
  module=b'\0asm\x01\0\0\0'+section(1,b'\x01\x60\0\0')+section(3,b'\x01\0')+section(5,b'\x02\x03\x01\x02\x07\x01\x02')+section(10,b'\x01'+leb(len(body))+body)
  stem=f'{opcode:02x}-{label}';wasm=a.out/(stem+'.wasm');wasm.write_bytes(module)
  command=[str(a.wasmtime),'compile','-W','memory64=y,threads=y','-C','cache=n',str(wasm),'-o',str(a.out/(stem+'.cwasm'))]
  ref=subprocess.run(command,capture_output=True,timeout=30)
  own=subprocess.run([str(a.decoder),immediate.hex()],capture_output=True,timeout=10)
  (a.out/(stem+'.wasmtime.log')).write_bytes(ref.stdout+ref.stderr);(a.out/(stem+'.decoder.log')).write_bytes(own.stdout+own.stderr)
  row=dict(opcode=opcode,case=label,expected=expected,wasmtime=ref.returncode,decoder=own.returncode,command=command)
  rows.append(row);(a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
  if (ref.returncode==0)!=expected or (own.returncode==0)!=expected:raise RuntimeError(f'atomic mismatch: {row}')
print(f'PASS {len(rows)} memory64 atomic modules: all 67 opcodes, Wasmtime compilation and shared decoding agree')
