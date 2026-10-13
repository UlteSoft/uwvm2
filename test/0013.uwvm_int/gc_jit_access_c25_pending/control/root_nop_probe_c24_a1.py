from pathlib import Path
import os,sys,json,hashlib
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
def apply(pid,s,D):
 N.check();assert os.uname().machine=='x86_64' and s['variant']=='root-store-nop-probe'
 assert Path('/proc/'+str(pid)+'/cgroup').read_text().strip()=='0::'+N.CG and os.sched_getaffinity(pid)=={2}
 assert len(list(Path('/proc/'+str(pid)+'/task').iterdir()))==1
 recipes=R/'source/root-nop-probe-c24-a1/recipes.json';assert N.sha(recipes)==s['root_nop_recipe_sha']
 c=next(x for x in json.loads(recipes.read_text())['recipes'] if x['case']==s['case']);assert s['elf_sha']==c['elf_sha'] and s['wasm_sha']==c['wasm_sha'] and N.sha(c['wat'])==c['wat_sha'];assert N.sha(s['elf'])==c['elf_sha'] and N.sha(s['wasm'])==c['wasm_sha']
 maps=Path('/proc/'+str(pid)+'/maps').read_text();rx=[]
 for l in maps.splitlines():
  a=l.split(maxsplit=5);path=a[5] if len(a)==6 else ''
  if 'r' in a[1] and 'x' in a[1] and not path:
   lo,hi=[int(x,16) for x in a[0].split('-')];rx.append((lo,hi))
 assert len(rx)==1 and rx[0][1]-rx[0][0]==4096,rx
 lo,hi=rx[0];fd=os.open('/proc/'+str(pid)+'/mem',os.O_RDWR)
 try:
  before=os.pread(fd,hi-lo,lo);assert len(before)==hi-lo;expected=bytearray(before)
  # All checks finish before the first write. The owning child is blocked
  # after READY (JIT+warmup), and never receives GO until this returns.
  for x in c['pieces']:
   o=x['offset'];n=x['length'];a=bytes.fromhex(x['original']);b=bytes.fromhex(x['replacement']);assert len(a)==len(b)==n and before[o:o+n]==a,(x,before[o:o+n].hex());expected[o:o+n]=b
  for x in c['pieces']:assert os.pwrite(fd,bytes.fromhex(x['replacement']),lo+x['offset'])==x['length']
  after=os.pread(fd,hi-lo,lo);assert after==expected
 finally:os.close(fd)
 D=Path(D);orig=D/'root-nop-original-rx.bin';patched=D/'root-nop-patched-rx.bin';orig.write_bytes(before);patched.write_bytes(after);N.persist(orig);N.persist(patched)
 changed=[i for i,(a,b) in enumerate(zip(before,after)) if a!=b];assert len(changed)<=19
 N.save(D/'root-nop-intervention.json',dict(case=s['case'],target=pid,cgroup=N.CG,affinity=[2],recipe=str(recipes),recipe_sha=N.sha(recipes),helper_sha=N.sha(__file__),original_rx_sha=N.sha(orig),patched_rx_sha=N.sha(patched),lo=lo,hi=hi,pieces=c['pieces'],changed_offsets=changed,scope='Causal store-only x86_64 intervention in own paused child. Three exact root stores replaced by equal-length NOPs,3retired instructions preserved; array auth/locks and every other byte unchanged. Only two statically call-free access loops with no aggregate allocation/import/call inside loop. No runtime source/ELF/cache mutation, no compiled C24 claim. Child Wasm validates every invocation; automatic polls remain at original constructors before patched loop.'))
 return dict(before=N.sha(orig),after=N.sha(patched))
