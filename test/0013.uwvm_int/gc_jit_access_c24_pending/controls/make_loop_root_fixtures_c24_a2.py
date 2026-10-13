from pathlib import Path
import sys,json,ctypes
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
D=R/'source/loop-root-fixtures-c24-a2';D.mkdir(exist_ok=False)
class Vec(ctypes.Structure):_fields_=[('size',ctypes.c_size_t),('data',ctypes.c_void_p)]
L=ctypes.CDLL('/tmp/uwvm-gc-perf-20261011-r73/source/wasmtime-capi/lib/libwasmtime.so');L.wasmtime_wat2wasm.argtypes=[ctypes.c_char_p,ctypes.c_size_t,ctypes.POINTER(Vec)];L.wasmtime_wat2wasm.restype=ctypes.c_void_p;L.wasm_byte_vec_delete.argtypes=[ctypes.POINTER(Vec)];L.wasmtime_error_message.argtypes=[ctypes.c_void_p,ctypes.POINTER(Vec)]
T='(type $A (array (mut i32))) (type $J (struct (field i64))) (type $F (func))'
CH='(func $churn (type $F) (local $k i32) i32.const 1024 local.set $k (loop $C i64.const 999 struct.new $J drop local.get $k i32.const 1 i32.sub local.tee $k br_if $C))'
LOC='(local $a (ref null $A)) (local $n i32)'
INIT='local.get $n i32.const 17 array.new $A local.set $a '
CHECK='local.get $a i32.const 7 array.get $A local.get $n i32.ne if unreachable end '
NEXT='local.get $n i32.const 1 i32.add local.tee $n i32.const 32 i32.lt_u br_if $L'
cases=[]
def add(name,body,types=T,funcs=CH,actual_gc=True,detail=''):
 cases.append(dict(name=name,text='(module '+types+' '+funcs+' (func (export "_start") '+body+'))',actual_gc=actual_gc,detail=detail,trap=None))
for kind,call,extra in [('direct','call $churn',''),('indirect','i32.const 0 call_indirect (type $F)','(table 1 funcref) (elem (i32.const 0) $churn)'),('ref','ref.func $churn call_ref $F','(elem declare func $churn)')]:
 add('local-rotation-'+kind,LOC+'(loop $L '+INIT+call+' '+CHECK+NEXT+')',funcs=CH+extra,detail='Local reference changes on each backedge; allocation occurs in callee and caller local is the only owning root.')
add('loop-phi-with-operand-prefix',LOC+'i32.const 777 i32.const 1 array.new $A i32.const 0 i32.const 17 array.new $A (block $E (param (ref $A)) (result (ref $A)) (loop $L (param (ref $A)) (result (ref $A)) local.set $a call $churn '+CHECK+'local.get $n i32.const 1 i32.add local.set $n local.get $a local.get $n i32.const 32 i32.eq br_if $E drop local.get $n i32.const 17 array.new $A br $L)) array.len i32.const 17 i32.ne if unreachable end i32.const 0 array.get $A i32.const 777 i32.ne if unreachable end',detail='Different loop-PHI reference each backedge; a second array is live only in the outer operand prefix across callee collection.')
add('wide-operand-prefix',LOC+''.join('i32.const '+str(100+i)+' i32.const 1 array.new $A ' for i in range(12))+'(loop $L '+INIT+'call $churn '+CHECK+NEXT+')'+''.join('i32.const 0 array.get $A i32.const '+str(100+i)+' i32.ne if unreachable end ' for i in reversed(range(12))),detail='Twelve distinct references held only in the operand prefix across every allocation/call in the loop.')
add('local-clear-and-replace',LOC+'(local $b (ref null $A)) (loop $L '+INIT+'local.get $a local.set $b ref.null $A local.set $a call $churn local.get $b i32.const 7 array.get $A local.get $n i32.ne if unreachable end local.get $b local.set $a ref.null $A local.set $b '+CHECK+NEXT+')',detail='Reference carriers move between nullable locals; each safepoint must publish current locals, including explicit nulls.')
add('nested-loops-current-local',LOC+'(local $q i32) (loop $L '+INIT+'i32.const 0 local.set $q (loop $I call $churn '+CHECK+'local.get $q i32.const 1 i32.add local.tee $q i32.const 2 i32.lt_u br_if $I) '+NEXT+')',detail='Nested loop joins and repeated safepoints with outer current local.')
add('loop-phi-with-v128',LOC+'(local $v v128) i32.const 0 i32.const 17 array.new $A v128.const i32x4 10 20 30 40 (block $E (param (ref $A) v128) (result (ref $A) v128) (loop $L (param (ref $A) v128) (result (ref $A) v128) local.set $v local.set $a call $churn '+CHECK+'local.get $v i32x4.extract_lane 2 i32.const 30 i32.ne if unreachable end local.get $n i32.const 1 i32.add local.set $n local.get $a local.get $v local.get $n i32.const 32 i32.eq br_if $E drop drop local.get $n i32.const 17 array.new $A local.get $v br $L)) drop array.len i32.const 17 i32.ne if unreachable end',detail='Mixed reference/v128 block and loop parameters: numeric SIMD bits must not become roots.')
TAIL='(func $tail (param $a (ref $A)) (result i32) call $churn local.get $a i32.const 7 array.get $A)'
for kind,ret,extra in [('direct','local.get $a return_call $tail',''),('ref','local.get $a ref.func $tail return_call_ref $G','(type $G (func (param (ref $A)) (result i32))) (elem declare func $tail)'),('indirect','local.get $a i32.const 0 return_call_indirect (type $G)','(type $G (func (param (ref $A)) (result i32))) (table 1 funcref) (elem (i32.const 0) $tail)')]:
 RUN='(func $run (result i32) '+LOC+'(loop $L '+INIT+'call $churn '+CHECK+'local.get $n i32.const 31 i32.eq if '+ret+' end local.get $n i32.const 1 i32.add local.set $n br $L) unreachable)'
 add('tail-transfer-'+kind,'call $run i32.const 31 i32.ne if unreachable end',funcs=CH+TAIL+extra+RUN,detail='Live loop-created reference transfers through actual return_call variant into allocating callee.')
NODES='(type $N (struct (field (mut (ref null $N))) (field i32))) '
add('cyclic-struct-replacement','(local $p (ref null $N)) (local $n i32) (loop $L ref.null $N local.get $n struct.new $N local.set $p local.get $p local.get $p struct.set $N 0 call $churn local.get $p struct.get $N 0 struct.get $N 1 local.get $n i32.ne if unreachable end '+NEXT+')',types=T+NODES,detail='Old unreachable cycles are collected; latest self-cycle must survive through a root local and trace edge.')
add('reference-array-child-rotation',LOC+'(local $r (ref null $R)) i32.const 4 array.new_default $R local.set $r (loop $L '+INIT+'local.get $r local.get $n i32.const 3 i32.and local.get $a array.set $R ref.null $A local.set $a call $churn local.get $r local.get $n i32.const 3 i32.and array.get $R i32.const 7 array.get $A local.get $n i32.ne if unreachable end '+NEXT+')',types=T+'(type $R (array (mut (ref null $A))))',detail='Child array survives only through a mutable reference array edge after its local is cleared.')
add('linked-struct-graph','(local $p (ref null $N)) (local $n i32) (local $q (ref null $N)) (loop $L local.get $p local.get $n struct.new $N local.set $p call $churn local.get $p struct.get $N 1 local.get $n i32.ne if unreachable end '+NEXT+') local.get $p local.set $q (loop $V local.get $n i32.const 1 i32.sub local.set $n local.get $q struct.get $N 1 local.get $n i32.ne if unreachable end local.get $q struct.get $N 0 local.set $q local.get $n br_if $V) local.get $q ref.is_null i32.eqz if unreachable end',types=T+NODES,detail='All 32 graph ancestors retained transitively and walked after repeated actual collections.')
for catch in ['catch','catch_ref']:
 exn=' exnref' if catch=='catch_ref' else '';drop='drop ' if catch=='catch_ref' else ''
 body=LOC+'(loop $L '+INIT+'(block $C (result (ref $A)'+exn+') (try_table ('+catch+' $tag $C) local.get $a throw $tag) unreachable) '+drop+'local.set $a call $churn '+CHECK+NEXT+')'
 add('exception-payload-'+catch,body,types=T+'(tag $tag (param (ref $A)))',actual_gc=False,detail='Throw/catch payload carries loop-created GC array; existing native EH collection gate9 remains unchanged, so this proves EH semantics, not active collection.')
with N.lease():
 N.check()
 rows=[]
 for c in cases:
  text=c.pop('text').replace('local.get $a ','local.get $a ref.as_non_null ');name=c['name'];
  if name=='local-rotation-indirect':text=text.replace('(elem (i32.const 0) $churn)','(elem declare func $churn)').replace('(loop $L','i32.const 0 ref.func $churn table.set 0 (loop $L',1)
  if name=='tail-transfer-indirect':text=text.replace('(elem (i32.const 0) $tail)','(elem declare func $tail)').replace('(loop $L','i32.const 0 ref.func $tail table.set 0 (loop $L',1)
  wat=D/(name+'.wat');wasm=D/(name+'.wasm');wat.write_text(text+'\n');b=text.encode();out=Vec();e=L.wasmtime_wat2wasm(b,len(b),ctypes.byref(out))
  if e:
   m=Vec();L.wasmtime_error_message(e,ctypes.byref(m));raise AssertionError((name,ctypes.string_at(m.data,m.size).decode()))
  wasm.write_bytes(ctypes.string_at(out.data,out.size));L.wasm_byte_vec_delete(ctypes.byref(out));N.persist(wat);N.persist(wasm);rows.append(dict(c,wat=str(wat),wasm=str(wasm),wasm_sha=N.sha(wasm)))
 N.save(D/'cases.json',dict(cases=rows,scope='16 independent self-checking modules for changing loop roots and actual allocation in callees; EH gate9 fixtures explicitly separated. No imports, each no-tag module executes at least32768 ephemeral allocations per invocation.'))
 print('C24_LOOP_ROOT_FIXTURES',len(rows),flush=True)
