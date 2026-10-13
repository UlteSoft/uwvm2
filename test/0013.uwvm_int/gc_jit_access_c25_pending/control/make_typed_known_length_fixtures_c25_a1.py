from pathlib import Path
import sys,json,ctypes
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
D=R/'source/typed-known-length-fixtures-c25-a1'
class Vec(ctypes.Structure):_fields_=[('size',ctypes.c_size_t),('data',ctypes.c_void_p)]
cases=[]
for name,decl,get in [('i8','i8','array.get_u'),('i16','i16','array.get_u'),('i32','i32','array.get'),('f32','f32','array.get')]:
 T='(type $A (array (mut '+decl+')))';loc='(local $a (ref null $A)) (local $b (ref null $A)) (local $n i32)'
 cast='f32.reinterpret_i32 ' if name=='f32' else '';uncast='i32.reinterpret_f32 ' if name=='f32' else ''
 def new(value,length):return 'i32.const '+str(value)+' '+cast+'i32.const '+str(length)+' array.new $A '
 def eq(value):return uncast+'i32.const '+str(value)+' i32.ne if unreachable end '
 def read(which,index,value):return 'local.get $'+which+' i32.const '+str(index)+' '+get+' $A '+eq(value)
 def add(suffix,body,funcs='',detail=''):
  cases.append(dict(name=name+'-'+suffix,text='(module '+T+' '+funcs+' (func (export "_start") '+loc+' '+body+'))',detail=detail))
 add('old-ssa-after-local-set',new(11,17)+'local.set $a local.get $a '+new(22,33)+'local.set $a i32.const 16 '+get+' $A '+eq(11)+read('a',32,22),detail='Older reference is already on operand stack when the same local is overwritten with another length. Facts must follow the complete reference SSA, not the current local.')
 add('branch-different-lengths','(loop $L local.get $n i32.const 1 i32.and if (result (ref $A)) '+new(11,17)+'else '+new(22,257)+'end local.set $a local.get $a i32.const 16 '+get+' $A '+uncast+'local.get $n i32.const 1 i32.and if (result i32) i32.const 11 else i32.const 22 end i32.ne if unreachable end local.get $n i32.const 1 i32.add local.tee $n i32.const 64 i32.lt_u br_if $L)',detail='Unknown PHI/if result must retain actual authenticated header bounds; no inferred constructor length may cross the join.')
 factory='(func $factory (result (ref $A)) '+new(99,31)+')'
 add('unknown-call-result','call $factory local.set $a '+read('a',30,99),funcs=factory,detail='The caller must not borrow a length fact from the callee code generator.')
 add('alias-null-replacement',new(11,17)+'local.set $a local.get $a local.set $b ref.null $A local.set $a '+new(77,257)+'local.set $a '+read('b',16,11)+'local.get $b i32.const 16 i32.const 99 '+cast+'array.set $A '+read('b',16,99)+read('a',256,77),detail='Aliased old reference remains valid when another local is cleared and assigned a new different-length array.')
 add('changing-runtime-constructor-length','(loop $L i32.const 63 '+cast+'local.get $n i32.const 31 i32.and i32.const 17 i32.add array.new $A local.set $a '+read('a',16,63)+'local.get $a local.get $n i32.const 31 i32.and i32.const 16 i32.add i32.const 9 '+cast+'array.set $A local.get $a local.get $n i32.const 31 i32.and i32.const 16 i32.add '+get+' $A '+eq(9)+'local.get $n i32.const 1 i32.add local.tee $n i32.const 128 i32.lt_u br_if $L)',detail='Known logical length is a changing runtime i32 value, not just a constant; index is computed from the same loop induction value.')
with N.lease():
 N.check();D.mkdir(exist_ok=False)
 L=ctypes.CDLL('/tmp/uwvm-gc-perf-20261011-r73/source/wasmtime-capi/lib/libwasmtime.so');L.wasmtime_wat2wasm.argtypes=[ctypes.c_char_p,ctypes.c_size_t,ctypes.POINTER(Vec)];L.wasmtime_wat2wasm.restype=ctypes.c_void_p;L.wasm_byte_vec_delete.argtypes=[ctypes.POINTER(Vec)];L.wasmtime_error_message.argtypes=[ctypes.c_void_p,ctypes.POINTER(Vec)]
 for c in cases:
  t=c.pop('text');wat=D/(c['name']+'.wat');wasm=D/(c['name']+'.wasm');wat.write_text(t+'\n');b=t.encode();out=Vec();e=L.wasmtime_wat2wasm(b,len(b),ctypes.byref(out))
  if e:
   m=Vec();L.wasmtime_error_message(e,ctypes.byref(m));raise AssertionError((c['name'],ctypes.string_at(m.data,m.size).decode()))
  wasm.write_bytes(ctypes.string_at(out.data,out.size));L.wasm_byte_vec_delete(ctypes.byref(out));N.persist(wat);N.persist(wasm);c.update(wat=str(wat),wasm=str(wasm),wat_sha=N.sha(wat),wasm_sha=N.sha(wasm),trap=None)
 N.save(D/'cases.json',dict(cases=cases,generator_sha=N.sha(__file__),scope='20 independent Wasm correctness fixtures for i8/i16/i32/f32 length facts, stale reference SSA, PHI, calls, null replacement, local aliasing, changing dynamic constructor length. This index is not a candidate pass claim.'))
 print('C25_LENGTH_FIXTURES',len(cases),flush=True)
