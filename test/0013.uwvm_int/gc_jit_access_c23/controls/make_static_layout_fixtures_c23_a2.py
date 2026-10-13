from pathlib import Path
import sys,json,ctypes
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
D=R/'source/static-layout-fixtures-c23-a2';D.mkdir(exist_ok=False)
class Vec(ctypes.Structure):_fields_=[('size',ctypes.c_size_t),('data',ctypes.c_void_p)]
L=ctypes.CDLL('/tmp/uwvm-gc-perf-20261011-r73/source/wasmtime-capi/lib/libwasmtime.so')
L.wasmtime_wat2wasm.argtypes=[ctypes.c_char_p,ctypes.c_size_t,ctypes.POINTER(Vec)];L.wasmtime_wat2wasm.restype=ctypes.c_void_p;L.wasm_byte_vec_delete.argtypes=[ctypes.POINTER(Vec)]
cs=[]
with N.lease():
 N.check()
 for ty in ['i8','i16','i32','f32']:
  pattern=0xff80ff81;expected=pattern & ((1 << (8 if ty=='i8' else 16 if ty=='i16' else 32))-1)
  value='i32.const '+str(pattern)+(' f32.reinterpret_i32' if ty=='f32' else '')
  read='array.get_u $A' if ty in ('i8','i16') else 'array.get $A'+(' i32.reinterpret_f32' if ty=='f32' else '')
  for length in [0,1,15,16,511,512,2048,2049,8192,8193]:
   for mutable in [False,True]:
    name=ty+'-'+('mutable' if mutable else 'immutable')+'-len'+str(length);storage='(mut '+ty+')' if mutable else ty
    text='(module (type $A (array '+storage+')) (func (export "_start") (local $a (ref null $A)) '+value+' i32.const '+str(length)+' array.new $A local.set $a '
    if length==0:text+='local.get $a i32.const 0 '+read+' drop'
    else:
     for index in [0,length-1]:
      if mutable:text+='local.get $a i32.const '+str(index)+' '+value+' array.set $A '
      text+='local.get $a i32.const '+str(index)+' '+read+' i32.const '+str(expected)+' i32.ne if unreachable end '
     text+='local.get $a array.len i32.const '+str(length)+' i32.ne if unreachable end '
    text+='))';wat=D/(name+'.wat');wasm=D/(name+'.wasm');wat.write_text(text+'\n');out=Vec();b=text.encode();error=L.wasmtime_wat2wasm(b,len(b),ctypes.byref(out));assert not error,(name,error);wasm.write_bytes(ctypes.string_at(out.data,out.size));L.wasm_byte_vec_delete(ctypes.byref(out));N.persist(wat);N.persist(wasm)
    cs.append(dict(name=name,wat=str(wat),wasm=str(wasm),wasm_sha=N.sha(wasm),trap='bounds' if length==0 else None,length=length,mutable=mutable,element_type=ty))
 N.save(D/'cases.json',dict(cases=cs,scope='80 independent assertion modules: packed numeric raw bits, mutable/immutable, empty-array traps and allocator class/large-array boundary lengths. New code generation is eligible only normal full JIT.'))
 print('C23_FIXTURES',len(cs),flush=True)
