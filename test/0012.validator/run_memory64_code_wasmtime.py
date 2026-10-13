#!/usr/bin/env python3
"""Compare full Core 3 instruction typing with Wasmtime using mixed memory contexts.

The UWVM side validates complete bodies against a parsed context whose declared
address types are supplied by the fixture. This is not a binary-parser or VM
execution qualification. Wasmtime receives actual memory64 module declarations.
"""
import argparse,json,resource,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--validator',type=Path,required=True)
p.add_argument('--wasmtime',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out.mkdir(parents=True,exist_ok=False)
def leb(n):
 out=bytearray()
 while True:
  low=n&127;n>>=7;out.append(low|(128 if n else 0))
  if not n:return bytes(out)
def sec(n,b):return bytes([n])+leb(len(b))+b
def const(t):return {'i32':b'\x41\0','i64':b'\x42\0','f32':b'\x43'+bytes(4),'f64':b'\x44'+bytes(8),'v128':b'\xfd\x0c'+bytes(16)}[t]
def flip(t):return 'i32' if t=='i64' else 'i64'
def addr(mask,index):return 'i64' if mask&(1<<index) else 'i32'
def memarg(index,align,offset):return leb(64+align)+leb(index)+leb(offset)
rows=[]
def add(label,mask,body,valid,disabled=0,reference=True):
 rows.append(dict(label=label,mask=mask,body=body.hex(),expected=valid,disabled=disabled,reference=reference))
def operation(label,mask,operands,op,result,gate=0):
 suffix=(b'\x1a' if result else b'')+b'\x0b'
 prefix=b''.join(map(const,operands));add(label,mask,prefix+op+suffix,True)
 for i,t in enumerate(operands):
  wrong=list(operands);wrong[i]=flip(t)
  add(label+f'-wrong-operand-{i}',mask,b''.join(map(const,wrong))+op+suffix,False)
 if operands:
  add(label+'-underflow',mask,op+suffix,False)
  add(label+'-unreachable',mask,b'\0'+op+suffix,True)
  # A concrete value above the polymorphic floor still has to match its type.
  add(label+'-unreachable-wrong-top',mask,b'\0'+const(flip(operands[-1]))+op+suffix,False)
 if gate:add(label+'-disabled',mask,prefix+op+suffix,False,gate,False)
# Scalar memory opcodes, in binary opcode order; signs do not alter stack types.
loads=[('i32',2),('i64',3),('f32',2),('f64',3),('i32',0),('i32',0),('i32',1),('i32',1),('i64',0),('i64',0),('i64',1),('i64',1),('i64',2),('i64',2)]
stores=[('i32',2),('i64',3),('f32',2),('f64',3),('i32',0),('i32',1),('i64',0),('i64',1),('i64',2)]
for mask in [1,2]:
 for index in [0,1]:
  at=addr(mask,index)
  for code,(value,align) in enumerate(loads+stores,0x28):
   store=code>=0x36;operands=[at,value] if store else [at];op=bytes([code])+memarg(index,align,0)
   operation(f'scalar-{code:02x}-m{index}',mask,operands,op,not store)
   for off in [1<<32,(1<<64)-1]:
    add(f'scalar-{code:02x}-m{index}-offset-{off}',mask,b''.join(map(const,operands))+bytes([code])+memarg(index,align,off)+(b'' if store else b'\x1a')+b'\x0b',at=='i64')
  for code in [0x3f,0x40]:
   prefix=const(at) if code==0x40 else b''
   # Force the result type with eqz, instead of discarding an untyped result.
   body=prefix+bytes([code])+leb(index)+bytes([0x50 if at=='i64' else 0x45])+b'\x1a\x0b'
   add(f'pages-{code}-m{index}',mask,body,True)
   add(f'pages-{code}-m{index}-wrong-result',mask,prefix+bytes([code])+leb(index)+bytes([0x45 if at=='i64' else 0x50])+b'\x1a\x0b',False)
   if code==0x40:add(f'grow-m{index}-wrong-delta',mask,const(flip(at))+bytes([code])+leb(index)+b'\x1a\x0b',False)
  # Every SIMD memory encoding, including extended/splat/zero/lane forms.
  for code,align in [(0,4),*[(n,3) for n in range(1,7)],(7,0),(8,1),(9,2),(10,3),(11,4),*[(84+n,n%4) for n in range(8)],(92,2),(93,3)]:
   lane=84<=code<=91;store=code==11 or 88<=code<=91;operands=[at,'v128'] if lane or store else [at]
   op=b'\xfd'+leb(code)+memarg(index,align,0)+(b'\0' if lane else b'')
   operation(f'simd-{code}-m{index}',mask,operands,op,not store,4)
   if lane:add(f'simd-{code}-m{index}-invalid-lane',mask,b''.join(map(const,operands))+op[:-1]+bytes([16>>align])+(b'' if store else b'\x1a')+b'\x0b',False)
  # Every threads instruction; value width and address width are independent.
  for code in [0,1,2,*range(0x10,0x4f)]:
   if code<3:
    align=3 if code==2 else 2;operands=[at,'i32'] if code==0 else [at,'i64' if code==2 else 'i32','i64'];result=True
   else:
    family=(code-0x10)//7;variant=(code-0x10)%7;align=[2,3,0,1,0,1,2][variant]
    value='i64' if variant==1 or variant>=4 else 'i32';operands=[at]+([value] if family else [])+([value] if family==8 else []);result=family!=1
   op=b'\xfe'+leb(code)+memarg(index,align,0)
   operation(f'atomic-{code:02x}-m{index}',mask,operands,op,result,1)
  operation(f'init-m{index}',mask,[at,'i32','i32'],b'\xfc\x08\0'+leb(index),False,8)
  operation(f'fill-m{index}',mask,[at,'i32',at],b'\xfc\x0b'+leb(index),False,8)
 for destination in [0,1]:
  for source in [0,1]:
   dt,st=addr(mask,destination),addr(mask,source);lt='i64' if dt==st=='i64' else 'i32'
   operation(f'copy-{destination}-{source}',mask,[dt,st,lt],b'\xfc\x0a'+leb(destination)+leb(source),False,8)
# No memory lookup is allowed for fence. Index validation precedes metadata access.
operation('fence',3,[],b'\xfe\x03\0',False,1)
for body in [b'\x42\0\x28'+memarg(2,2,0)+b'\x1a\x0b',b'\x3f\x02\x1a\x0b',b'\0\xfe\x10'+memarg(2,2,0)+b'\x1a\x0b']:
 add('invalid-index',3,body,False)
# Guarded truncations end immediately before an inaccessible host page.
body=const('i64')+b'\x28'+memarg(1,2,(1<<64)-1)+b'\x1a\x0b'
for n in range(3,len(body)-2):add(f'truncated-wide-memarg-{n}',2,body[:n],False)
request=''.join(f"{r['mask']} {r['disabled']} {r['body']}\n" for r in rows)
(a.out/'request.txt').write_text(request)
own=subprocess.run([str(a.validator)],input=request,text=True,capture_output=True,timeout=90)
(a.out/'validator.stdout').write_text(own.stdout);(a.out/'validator.stderr').write_text(own.stderr)
assert own.returncode==0,own.stderr[-4000:]
answers=own.stdout.splitlines();assert len(answers)==len(rows),(len(answers),len(rows))
for n,(r,answer) in enumerate(zip(rows,answers)):
 r['validator']=int(answer.split()[0]);r['diagnostic']=answer.split()[1:]
 if bool(r['validator'])!=r['expected']:
  (a.out/'failed.json').write_text(json.dumps(r,indent=2));raise RuntimeError(f'validator mismatch {n}: {r}')
 # Feature gates are also tested against an already-validated context, where
 # disabling multiple memories in the *module* would be a different rule.
 if not r['reference']:continue
 mask=r['mask'];body=b'\0'+bytes.fromhex(r['body']);import_type=bytes([7 if mask&1 else 3,1,2]);local_type=bytes([7 if mask&2 else 3,1,2])
 wasm=b'\0asm\x01\0\0\0'+sec(1,b'\x01\x60\0\0')+sec(2,b'\x01\x01m\x01m\x02'+import_type)+sec(3,b'\x01\0')+sec(5,b'\x01'+local_type)+sec(12,b'\x01')+sec(10,b'\x01'+leb(len(body))+body)+sec(11,b'\x01\x01\0')
 path=a.out/f'{n:04d}.wasm';path.write_bytes(wasm)
 command=[str(a.wasmtime),'compile','-W','memory64=y,threads=y','-C','cache=n',str(path),'-o',str(a.out/'last.cwasm')]
 ref=subprocess.run(command,capture_output=True,timeout=30);r['wasmtime']=ref.returncode
 (a.out/f'{n:04d}.wasmtime.log').write_bytes(ref.stdout+ref.stderr)
 if (ref.returncode==0)!=r['expected']:
  (a.out/'failed.json').write_text(json.dumps(r,indent=2));raise RuntimeError(f'Wasmtime mismatch {n}: {r}')
(a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
print(f'PASS {len(rows)} Core 3 typed-memory code checks, {sum(r["reference"] for r in rows)} complete Wasmtime modules; scalar/SIMD/67 atomic opcodes/bulk/size/grow, imported/local address types, polymorphism, feature gates, guarded truncations')
