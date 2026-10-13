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
D=pathlib.Path("/tmp/uwvm-gc-perf-20261011-r74/source/wasmtime-codegen-analysis-a1/gdb-reference-write-a1")
inf=gdb.selected_inferior();pid=inf.pid;assert sorted(os.sched_getaffinity(pid))==[2]
cg=pathlib.Path('/proc/'+str(pid)+'/cgroup').read_text().strip();assert cg=='0::/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
pattern=(D/'reference-write-increment-pattern.bin').read_bytes();found=[];mappings=[]
for line in pathlib.Path('/proc/'+str(pid)+'/maps').read_text().splitlines():
 w=line.split()
 if 'x' not in w[1] or w[4]!='0' or len(w)>5:continue
 lo,hi=[int(x,16) for x in w[0].split('-')];assert hi-lo<=65536
 data=bytes(inf.read_memory(lo,hi-lo));p=D/('gdb-before-rx-'+hex(lo)+'.bin');p.write_bytes(data);mappings.append({'lo':lo,'hi':hi,'sha':hashlib.sha256(data).hexdigest(),'file':str(p)})
 pos=data.find(pattern)
 if pos>=0:assert data.find(pattern,pos+1)<0;found.append(lo+pos)
assert len(found)==1,found
loop=found[0];load=loop+(0x194-0x18c);gdb.set_convenience_variable('read_load',gdb.Value(load));gdb.set_convenience_variable('read_loop',gdb.Value(loop))
(D/'gdb-live-code-binding.json').write_text(json.dumps({'pid':pid,'cgroup':cg,'pattern_sha':hashlib.sha256(pattern).hexdigest(),'pattern_length':len(pattern),'loop':loop,'load':load,'mappings':mappings,'scope':'Live CAPI49 same-reference write block matches same-versionCLI 14-byte increment block; debugger run,notperformance.'},indent=2)+'\n')
print('LIVE_JIT_PATTERN_MATCH',hex(loop),hex(load),pid,cg)
end
disable 1
tbreak *$read_load
continue
x/18i $read_load-0x8
info registers r12 rax rsi rcx
python
reg=lambda n:int(gdb.parse_and_eval('$'+n));new=reg('eax');old=reg('esi');child=reg('r12')+new;slot=reg('rcx');read64=lambda a:struct.unpack('<Q',bytes(inf.read_memory(a,8)))[0];read32=lambda a:struct.unpack('<I',bytes(inf.read_memory(a,4)))[0];before=read64(child+8)
assert old==new and read32(slot)==old and before==1025,(old,new,before)
proof={'old_offset':old,'new_offset':new,'child_native_address':child,'slot_address':slot,'reference_count_before':before,'scope':'Actual CAPI49 same-reference array.set, own fixture; increment then destination write then decrement. No debugger timings used.'}
gdb.set_convenience_variable('write_finished',gdb.Value(loop+(0x1bd-0x18c)))
end
stepi
python
incremented=read64(child+8);assert incremented==before+1;proof['reference_count_after_increment']=incremented
end
tbreak *$write_finished
continue
python
after=read64(child+8);assert reg('pc')==loop+(0x1bd-0x18c) and after==before and read32(slot)==new
proof.update(reference_count_after_decrement=after,slot_after=read32(slot),write_order_proved=True)
(D/'gdb-reference-write-proof.json').write_text(json.dumps(proof,indent=2)+'\n');print('REFERENCE_WRITE_ORDER_PROVED',before,incremented,after)
end
continue
python
assert gdb.selected_inferior().pid==0
print('REFERENCE_WRITE_DEBUG_COMPLETED')
end
