set pagination off
set confirm off
set debuginfod enabled off
set auto-load off
set startup-with-shell off
set disable-randomization off
set breakpoint pending on
break _RNvXs5_NtNtNtNtNtCs4ZcjYNh6lQY_8wasmtime7runtime2vm2gc7enabled3drcNtB5_7DrcHeapNtNtB9_10gc_runtime6GcHeap9alloc_raw
run
python
import gdb,pathlib,struct,json,os
E=pathlib.Path("/tmp/uwvm-gc-perf-20261011-r74/source/wasmtime-codegen-analysis-a1/gdb-alloc-a2")
inf=gdb.selected_inferior();pid=inf.pid;assert sorted(os.sched_getaffinity(pid))==[2]
cg=pathlib.Path('/proc/'+str(pid)+'/cgroup').read_text().strip();assert cg=="0::/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope"
reg=lambda n:int(gdb.parse_and_eval('$'+n))
entry=reg('pc');heap=reg('rsi');size=reg('r8');read32=lambda a:struct.unpack('<I',bytes(inf.read_memory(a,4)))[0]
before=read32(heap+0x40);end=read32(heap+0x44);aligned=(size+15)&~15
initially_exhausted=[]
while before+aligned>end:
 initially_exhausted.append({'bump':before,'end':end,'size':size})
 assert len(initially_exhausted)<=16
 gdb.execute('continue')
 entry=reg('pc');heap=reg('rsi');size=reg('r8');before=read32(heap+0x40);end=read32(heap+0x44);aligned=(size+15)&~15
assert before%16==0 and before>0 and size==4124 and before+aligned<=end,(before,size,end)
proof={'initially_exhausted_entries':initially_exhausted,'pid':pid,'cgroup':cg,'entry':entry,'heap_self':heap,'layout_size':size,'aligned_size':aligned,'bump_before':before,'bump_end':end,'scope':'Own stopped CAPI49 child, native alloc_raw inlined FreeList bump fastpath after lazy heap initialization. Debugger data not performance; slowpath hit count covers only after eligible fast allocation. Offsets bound to actual ELF disassembly.'}
gdb.set_convenience_variable('bump_updated',gdb.Value(entry+0x93))
slow_break=gdb.Breakpoint("_RNvMNtNtNtNtNtCs4ZcjYNh6lQY_8wasmtime7runtime2vm2gc7enabled9free_listNtB2_8FreeList10alloc_slow")
print('ALLOC_FAST_ENTRY',hex(entry),hex(heap),before,end,size,aligned)
end
disable 1
tbreak *$bump_updated
continue
x/12i $pc-0x29
info registers eax ecx r12 r14
python
assert reg('pc')==entry+0x93,hex(reg('pc'))
after=read32(heap+0x40);assert after==before+aligned and reg('eax')==before and reg('ecx')==after
assert slow_break.hit_count==0
proof.update(bump_after=after,returned_index_register=reg('eax'),fastpath_proved=True)
print('ALLOC_BUMP_UPDATE_PROVED',before,after)
end
continue
python
assert gdb.selected_inferior().pid==0,'inferior did not finish'
assert slow_break.hit_count==0,slow_break.hit_count
proof.update(slowpath_break_hits=slow_break.hit_count,normal_exit=True)
(E/'gdb-native-allocation-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
print('GDB_ALLOCATION_COMPLETED_WITHOUT_SLOWPATH')
end
