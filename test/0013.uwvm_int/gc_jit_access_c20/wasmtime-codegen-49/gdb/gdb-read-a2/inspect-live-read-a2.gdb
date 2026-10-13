set pagination off
set confirm off
set debuginfod enabled off
set auto-load off
set startup-with-shell off
set disable-randomization off
set breakpoint pending on
break wasmtime_func_call
run
python
import gdb,json,pathlib,hashlib,struct,os
D=pathlib.Path("/tmp/uwvm-gc-perf-20261011-r74/source/wasmtime-codegen-analysis-a1/gdb-read-a2")
inf=gdb.selected_inferior();pid=inf.pid;assert sorted(os.sched_getaffinity(pid))==[2]
cg=pathlib.Path('/proc/'+str(pid)+'/cgroup').read_text().strip();assert cg=='0::/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
pattern=(D/'immutable-f32-hotloop-pattern.bin').read_bytes();found=[];mappings=[]
for line in pathlib.Path('/proc/'+str(pid)+'/maps').read_text().splitlines():
 w=line.split()
 if 'x' not in w[1] or w[4]!='0' or len(w)>5:continue
 lo,hi=[int(x,16) for x in w[0].split('-')];assert hi-lo<=65536
 data=bytes(inf.read_memory(lo,hi-lo));p=D/('gdb-before-rx-'+hex(lo)+'.bin');p.write_bytes(data);mappings.append({'lo':lo,'hi':hi,'sha':hashlib.sha256(data).hexdigest(),'file':str(p)})
 pos=data.find(pattern)
 if pos>=0:assert data.find(pattern,pos+1)<0;found.append(lo+pos)
assert len(found)==1,found
loop=found[0];load=loop+(0x11a-0xbd);gdb.set_convenience_variable('read_load',gdb.Value(load));gdb.set_convenience_variable('read_loop',gdb.Value(loop))
(D/'gdb-live-code-binding.json').write_text(json.dumps({'pid':pid,'cgroup':cg,'pattern_sha':hashlib.sha256(pattern).hexdigest(),'pattern_length':len(pattern),'loop':loop,'load':load,'mappings':mappings,'scope':'Live CAPI49JIT bytes match previously PMU-qualified immutablef32loop exactly; debugger run,notperformance.'},indent=2)+'\n')
print('LIVE_JIT_PATTERN_MATCH',hex(loop),hex(load),pid,cg)
end
disable 1
tbreak *$read_load
continue
x/12i $read_loop
x/8i $read_load
info registers rcx rdx r8 r9 r10 rdi rsi
python
inf=gdb.selected_inferior();reg=lambda n:int(gdb.parse_and_eval('$'+n));base=reg('rcx');off=reg('rdx');idx=reg('rdi')&1023;addr=reg('r10');length=struct.unpack('<I',bytes(inf.read_memory(base+off+24,4)))[0];bits=struct.unpack('<I',bytes(inf.read_memory(addr,4)))[0]
assert length==1024 and idx==0 and addr==base+off+28+idx*4 and bits==0x7f800001,(length,idx,hex(addr),hex(base),hex(off),hex(bits))
(D/'gdb-live-read-proof.json').write_text(json.dumps({'heap_base':base,'compressed_object_offset':off,'index':idx,'effective_address':addr,'length':length,'bits':bits,'address_formula':'heap_base+compressed_object_offset+28+index*4','pass':True,'scope':'Stopped immediately beforeactualJITvmovss,inspectedonlyownfixtureobject;single-step debug,notbenchmark.'},indent=2)+'\n')
print('LIVE_TYPED_LOAD_PROVED',length,idx,hex(bits),hex(addr))
end
stepi
info registers xmm0
continue
python
assert gdb.selected_inferior().pid==0,'inferior did not finish'
print('GDB_TYPED_READ_COMPLETED')
end
