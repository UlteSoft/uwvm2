import sys,pathlib,json,ctypes,os
sys.path.insert(0,'/tmp/uwvm-gc-perf-20261011-r74/control');import native as N
D=N.ROOT/'source/wasmtime-codegen-analysis-a1';F=D/'fixtures';F.mkdir(exist_ok=False)
L=ctypes.CDLL('/tmp/uwvm-gc-perf-20261011-r73/source/wasmtime-capi/lib/libwasmtime.so')
class Vec(ctypes.Structure):_fields_=[('size',ctypes.c_size_t),('data',ctypes.c_void_p)]
L.wasmtime_wat2wasm.argtypes=[ctypes.c_char_p,ctypes.c_size_t,ctypes.POINTER(Vec)];L.wasmtime_wat2wasm.restype=ctypes.c_void_p
L.wasm_byte_vec_delete.argtypes=[ctypes.POINTER(Vec)];L.wasmtime_error_message.argtypes=[ctypes.c_void_p,ctypes.POINTER(Vec)];L.wasmtime_error_delete.argtypes=[ctypes.c_void_p]
cases=[]
common='(type $S (struct (field i32))) (type $A (array (mut (ref $S))))'
for name in ['reference-read','reference-write-same','reference-write-alternate']:
 alternate=name.endswith('alternate');write='write' in name
 init='i32.const 12345 struct.new $S local.set $s '
 if alternate:init+='i32.const 6789 struct.new $S local.set $t '
 init+='local.get $s i32.const 1024 array.new $A local.set $a '
 body=''
 if write:
  body+='local.get $a local.get $n i32.const 1023 i32.and '
  body+=('local.get $n i32.const 1 i32.and if (result (ref $S)) local.get $t ref.as_non_null else local.get $s ref.as_non_null end ' if alternate else 'local.get $s ref.as_non_null ')
  body+='array.set $A '
 body+='local.get $sum local.get $a local.get $n i32.const 1023 i32.and array.get $A struct.get $S 0 i32.add local.set $sum '
 oracle=(65536*(12345+6789) if alternate else 131072*12345)&0xffffffff
 final=''
 if write:
  final+='i32.const 0 local.set $n (loop $V local.get $a local.get $n array.get $A struct.get $S 0 '
  final+=('local.get $n i32.const 1 i32.and if (result i32) i32.const 6789 else i32.const 12345 end ' if alternate else 'i32.const 12345 ')
  final+='i32.ne if unreachable end local.get $n i32.const 1 i32.add local.tee $n i32.const 1024 i32.lt_u br_if $V) '
 wat='(module '+common+' (func (export "_start") (local $s (ref null $S)) (local $t (ref null $S)) (local $a (ref null $A)) (local $n i32) (local $sum i32) '+init.replace('local.get $s i32.const 1024','local.get $s ref.as_non_null i32.const 1024')+' (loop $L '+body+'local.get $n i32.const 1 i32.add local.tee $n i32.const 131072 i32.lt_u br_if $L) local.get $sum i32.const '+str(oracle)+' i32.ne if unreachable end '+final+'))'
 cases.append({'name':name,'wat_text':wat,'oracle':oracle,'reads_per_start':131072+(1024 if write else 0),'writes_per_start':131072 if write else 0,'scope':'Typed reference array and struct access; checks checksum and final1024slots after writes. No forced collection, no cycle garbage, no parallel mutator.'})
wat='''(module (type $A (array (mut i32))) (func (export "_start") (local $a (ref null $A)) (local $n i32) (local $sum i32)
 i32.const 0 i32.const 1024 array.new $A local.set $a
 (loop $L local.get $a local.get $n i32.const 1023 i32.and local.get $n array.set $A
 local.get $sum local.get $a local.get $n i32.const 1023 i32.and array.get $A i32.add local.set $sum
 local.get $n i32.const 1 i32.add local.tee $n i32.const 131072 i32.lt_u br_if $L)
 local.get $sum i32.const 4294901760 i32.ne if unreachable end
 i32.const 0 local.set $n
 (loop $V local.get $a local.get $n array.get $A local.get $n i32.const 130048 i32.add i32.ne if unreachable end
 local.get $n i32.const 1 i32.add local.tee $n i32.const 1024 i32.lt_u br_if $V)))'''
cases.append({'name':'numeric-i32-write-read','wat_text':wat,'oracle':sum(range(131072))&0xffffffff,'reads_per_start':132096,'writes_per_start':131072,'scope':'Typed numeric array writes and reads, checksum plus final1024slots. Access test, not collector throughput.'})
for c in cases:
 wat=F/(c['name']+'.wat');wasm=F/(c['name']+'.wasm');wat.write_text(c.pop('wat_text')+'\n');data=wat.read_bytes();v=Vec();e=L.wasmtime_wat2wasm(data,len(data),ctypes.byref(v))
 if e:
  msg=Vec();L.wasmtime_error_message(e,ctypes.byref(msg));m=ctypes.string_at(msg.data,msg.size).decode();L.wasm_byte_vec_delete(ctypes.byref(msg));L.wasmtime_error_delete(e);raise RuntimeError(m)
 wasm.write_bytes(ctypes.string_at(v.data,v.size));L.wasm_byte_vec_delete(ctypes.byref(v));N.persist(wat);N.persist(wasm);c.update(wat=str(wat),wasm=str(wasm),wat_sha=N.sha(wat),wasm_sha=N.sha(wasm))
N.save(F/'cases.json',{'cases':cases,'converter_dso_sha':N.sha(pathlib.Path('/tmp/uwvm-gc-perf-20261011-r73/source/wasmtime-capi/lib/libwasmtime.so')),'cgroup':pathlib.Path('/proc/self/cgroup').read_text(),'affinity':sorted(os.sched_getaffinity(0))})
print(json.dumps(cases,indent=2))
