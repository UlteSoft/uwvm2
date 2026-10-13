from pathlib import Path
import sys,json
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));sys.path.insert(0,str(R/'source/call-ref-identity-c10'));import native as N
from fixture_converter import wat_to_wasm
N.check();D=R/'source/typed-array-store-c21';D.mkdir(exist_ok=False);cases=[]
def add(name,wat,trap=None):
 p=D/(name+'.wat');p.write_text(wat+'\n');w=p.with_suffix('.wasm');wat_to_wasm(p,w);N.persist(p);N.persist(w);cases.append(dict(name=name,wat=str(p),wasm=str(w),wasm_sha=N.sha(w),trap=trap))
for ty,width in [('i8',1),('i16',2),('i32',4),('f32',4)]:
 types='(type $A (sub (array (mut '+ty+')))) (type $B (sub $A (array (mut '+ty+')))) (type $J (struct (field i64))) (tag $E (param i32))'
 def value(x):return 'i32.const '+str(x)+(' f32.reinterpret_i32' if ty=='f32' else '')
 get=('array.get_u' if width<4 else 'array.get')+' $A'+(' i32.reinterpret_f32' if ty=='f32' else '')
 mask=(1<<(width*8))-1
 def read(idx,expect,arr='$a'):
  return 'local.get '+arr+' i32.const '+str(idx)+' '+get+' i32.const '+str(expect&mask)+' i32.ne if unreachable end '
 def module(body):return '(module '+types+' (func (export "_start") (local $a (ref null $A)) (local $b (ref null $A)) (local $n i32) '+body+'))'
 for bits in [0,1,0xffffffff,0x80000000,0x7f800001,0xffc00001,0x1234fedc]:
  body=value(0)+' i32.const 257 array.new $A local.tee $a local.set $b local.get $a i32.const 256 '+value(bits)+' array.set $A '+read(256,bits,'$b')+read(255,0)
  if width<4:
   signed=(bits&mask);signed=signed-(1<<(width*8)) if signed&(1<<(width*8-1)) else signed
   body+='local.get $a i32.const 256 array.get_s $A i32.const '+str(signed)+' i32.ne if unreachable end '
  add(ty+'-bits-'+hex(bits)[2:],module(body))
 for length in [1,2,257]:
  for ctor in ['new','default','fixed']:
   init=value(0)+' i32.const '+str(length)+' array.new $A' if ctor=='new' else 'i32.const '+str(length)+' array.new_default $A' if ctor=='default' else ' '.join(value(0) for _ in range(length))+' array.new_fixed $A '+str(length)
   pattern='local.get $n i32.const 2654435761 i32.mul i32.const 2139095041 i32.xor'
   body=init+' local.tee $a local.set $b (loop $W local.get $a local.get $n '+pattern+(' f32.reinterpret_i32' if ty=='f32' else '')+' array.set $A local.get $n i32.const 1 i32.add local.tee $n i32.const '+str(length)+' i32.lt_u br_if $W) i32.const 0 local.set $n (loop $V local.get $b local.get $n '+get+' '+pattern+' i32.const '+str(mask)+' i32.and i32.ne if unreachable end local.get $n i32.const 1 i32.add local.tee $n i32.const '+str(length)+' i32.lt_u br_if $V)'
   add(ty+'-'+ctor+'-all-'+str(length),module(body))
 body=value(0)+' i32.const 2 array.new $B local.set $a local.get $a i32.const 1 '+value(0xff800001)+' array.set $A '+read(1,0xff800001)
 add(ty+'-subtype-owner-layout',module(body))
 body=value(0)+' i32.const 257 array.new $A local.tee $a local.set $b local.get $a i32.const 256 '+value(0x7f800001)+' array.set $A local.get $b i32.const 255 local.get $a i32.const 256 i32.const 1 array.copy $A $A '+read(255,0x7f800001)+' local.get $a i32.const 0 '+value(0xdeadbeef)+' i32.const 2 array.fill $A '+read(0,0xdeadbeef)+read(1,0xdeadbeef)+read(256,0x7f800001)
 add(ty+'-alias-copy-fill',module(body))
 body=value(0)+' i32.const 2 array.new $A local.set $a (try_table (result i32) (catch $E 0) local.get $a i32.const 1 '+value(0xffc00001)+' array.set $A i32.const 99 throw $E) i32.const 99 i32.ne if unreachable end '+read(1,0xffc00001)+' local.get $a i32.const 0 '+value(23)+' array.set $A '+read(0,23)
 add(ty+'-exception-after-write',module(body))
 body=value(0)+' i32.const 2 array.new $A local.set $a local.get $a i32.const 1 '+value(0xff800001)+' array.set $A i32.const 50000 local.set $n (loop $G i64.const 9 struct.new $J drop local.get $n i32.const 1 i32.sub local.tee $n br_if $G) '+read(1,0xff800001)+' local.get $a i32.const 0 '+value(17)+' array.set $A '+read(0,17)
 add(ty+'-root-survives-collection',module(body))
 for name,length,index in [('empty',0,0),('at-length',2,2),('UINTMAX',2,0xffffffff)]:
  body=value(0)+' i32.const '+str(length)+' array.new $A local.set $a local.get $a i32.const '+str(index)+' '+value(1)+' array.set $A'
  add(ty+'-trap-'+name,module(body),'bounds')
 for index in [0,0xffffffff]:add(ty+'-null-'+str(index),module('ref.null $A i32.const '+str(index)+' '+value(1)+' array.set $A'),'null')
N.save(D/'cases.json',{'cases':cases,'scope':'New typed stores: full bit-patterns, NaNs, packed signed/unsigned truncation, lengths1/2/257 with three constructors, aliases/copy/fill, subtypes, EH after write, GC root survival, null and unsigned bounds; independent Wasm assertions.'})
print('C21_STORE_FIXTURES',len(cases),flush=True)
