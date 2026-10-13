set pagination off
set confirm off
set disable-randomization off
python
import gdb,json,pathlib,hashlib,os
D=pathlib.Path('/tmp/uwvm-gc-perf-20261011-r74/source/production-private-codegen-c26-a5/uwvm2-ros-global-publication-after-loop' )
class Capture(gdb.Breakpoint):
 def __init__(self):
  super().__init__('_ZN5uwvm27runtime8compiler8llvm_jit21compile_all_from_uwvm7details45llvm_jit_gc_numeric_array_index_domain_bridgeEm',internal=True);self.done=False
 def stop(self):
  if self.done:return False
  self.done=True;inf=gdb.selected_inferior();pid=inf.pid;P=pathlib.Path('/proc')/str(pid);maps=(P/'maps').read_text();(D/'maps.txt').write_text(maps);rows=[]
  for l in maps.splitlines():
   z=l.split();a,e=[int(v,16)for v in z[0].split('-')]
   if 'x' in z[1] and len(z)==5 and 0<e-a<=33554432:
    raw=bytes(inf.read_memory(a,e-a));p=D/('rx-'+hex(a)+'.bin');p.write_bytes(raw);rows.append(dict(start=a,end=e,sha=hashlib.sha256(raw).hexdigest(),file=str(p)))
  assert rows
  (D/'capture.json').write_text(json.dumps(dict(pid=pid,tid=gdb.selected_thread().ptid[1],boot=pathlib.Path('/proc/sys/kernel/random/boot_id').read_text().strip(),cgroup=(P/'cgroup').read_text().strip(),affinity=sorted(os.sched_getaffinity(pid)),ELF_sha='5f092c8d83b57c684b103e27109d49ebecf5ee99a6d8609a61a9e98c2c5f13ff',WASM_sha='a1e85e4377d0da0a7d49c8dccc190ec6236df3a6f2014af4d07b6584d899bed8',rx=rows),indent=2)+'\n');return False
Capture()
end
run
