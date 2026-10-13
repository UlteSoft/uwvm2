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
D=pathlib.Path("/tmp/uwvm-gc-perf-20261011-r74/source/wasmtime-codegen-analysis-a1/gdb-reference-read-a1")
inf=gdb.selected_inferior();pid=inf.pid;assert sorted(os.sched_getaffinity(pid))==[2]
cg=pathlib.Path('/proc/'+str(pid)+'/cgroup').read_text().strip();assert cg=='0::/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
pattern=(D/'reference-read-barrier-pattern.bin').read_bytes();found=[];mappings=[]
for line in pathlib.Path('/proc/'+str(pid)+'/maps').read_text().splitlines():
 w=line.split()
 if 'x' not in w[1] or w[4]!='0' or len(w)>5:continue
 lo,hi=[int(x,16) for x in w[0].split('-')];assert hi-lo<=65536
 data=bytes(inf.read_memory(lo,hi-lo));p=D/('gdb-before-rx-'+hex(lo)+'.bin');p.write_bytes(data);mappings.append({'lo':lo,'hi':hi,'sha':hashlib.sha256(data).hexdigest(),'file':str(p)})
 pos=data.find(pattern)
 if pos>=0:assert data.find(pattern,pos+1)<0;found.append(lo+pos)
assert len(found)==1,found
loop=found[0];load=loop+(0x172-0x169);gdb.set_convenience_variable('read_load',gdb.Value(load));gdb.set_convenience_variable('read_loop',gdb.Value(loop))
(D/'gdb-live-code-binding.json').write_text(json.dumps({'pid':pid,'cgroup':cg,'pattern_sha':hashlib.sha256(pattern).hexdigest(),'pattern_length':len(pattern),'loop':loop,'load':load,'mappings':mappings,'scope':'Live CAPI49 reference read block matches same-versionCLI 20-byte reference barrier block; no PMU qualification for this new fixture; debugger run,notperformance.'},indent=2)+'\n')
print('LIVE_JIT_PATTERN_MATCH',hex(loop),hex(load),pid,cg)
end
disable 1
tbreak *$read_load
continue
x/14i $read_load-0x9
info registers rbx rdx rax r13 r14
python
reg=lambda n:int(gdb.parse_and_eval('$'+n));child=reg('rbx')+reg('rdx');read64=lambda a:struct.unpack('<Q',bytes(inf.read_memory(a,8)))[0]
header=struct.unpack('<I',bytes(inf.read_memory(child,4)))[0];before=read64(child+8)
assert header&2 and before>=1024,(header,before)
proof={'child_offset':reg('rdx'),'child_native_address':child,'header':header,'in_oasr_bit':header&2,'reference_count_before':before,'jump_destination':loop+(0x1b8-0x169),'scope':'Actual CAPI49JIT reference read; own fixture child, header membership test bypasses intrusive-list insertion and RC increment when already listed. Debugger run not timings.'}
end
stepi
stepi
python
after=read64(child+8);assert after==before and reg('pc')==proof['jump_destination'],(before,after,hex(reg('pc')),hex(proof['jump_destination']))
proof.update(reference_count_after=after,fastpath_proved=True)
(D/'gdb-reference-read-proof.json').write_text(json.dumps(proof,indent=2)+'\n');print('REFERENCE_READ_BARRIER_FASTPATH_PROVED',before,after,hex(header))
end
continue
python
assert gdb.selected_inferior().pid==0
print('REFERENCE_READ_DEBUG_COMPLETED')
end
