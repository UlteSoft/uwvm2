from pathlib import Path
import sys,json,os,subprocess,struct,selectors,time
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
D=R/'source/static-layout-preview-c23-a3';D.mkdir(exist_ok=False);rows=[];tools=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin')
cs={c['name']:c for c in json.loads((R/'source/typed-array-store-bench-c21/cases.json').read_text())['cases']}
def read(p,n):
 b=b'';deadline=time.monotonic()+30
 while len(b)<n:
  assert p.poll() is None
  sel=selectors.DefaultSelector();sel.register(p.stdout,selectors.EVENT_READ);events=sel.select(.1);sel.close();assert time.monotonic()<deadline
  if events:b+=os.read(p.stdout.fileno(),n-len(b))
 return b
for name in ['i8-write-read','i32-write-read']:
 for v in ['typed-array-access-inline','typed-array-access-inline-static32']:
  c=cs[name];elf=R/('build/deferred-'+v+'/uwvm');env=dict(os.environ,WASM_PERF_NATIVE_PROFILE='1',WASM_PERF_NATIVE_REPS='1');env.pop('WASM_PERF_NATIVE_CLI',None)
  def setup():os.sched_setaffinity(0,{6})
  stderr=D/(name+'-'+v+'.stderr')
  with stderr.open('wb') as err:
   p=subprocess.Popen([elf,'-Rcc','jit','-Rcm','full','-Rct','0','-Rllvm-cache-path','disable','-WFE-gc','--run',c['wasm']],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err,preexec_fn=setup,env=env)
   try:
    assert read(p,8)==b'READY1\0\0';maps=Path('/proc/'+str(p.pid)+'/maps').read_text();out=[];fd=os.open('/proc/'+str(p.pid)+'/mem',os.O_RDONLY)
    try:
     for line in maps.splitlines():
      a=line.split(maxsplit=5);path=a[5] if len(a)==6 else ''
      if 'r' not in a[1] or 'x' not in a[1] or path:continue
      lo,hi=[int(x,16) for x in a[0].split('-')];assert hi-lo<32<<20;b=os.pread(fd,hi-lo,lo);assert len(b)==hi-lo
      f=D/(name+'-'+v+'-'+str(len(out))+'.bin');f.write_bytes(b);N.persist(f);obj=f.with_suffix('.o');asm=f.with_suffix('.asm');lab='static-layout-preview-c23-a3-'+name+'-'+v+'-'+str(len(out));N.run(lab+'-obj',[tools/'llvm-objcopy','--input-target=binary','--output-target=elf64-x86-64','--binary-architecture=i386:x86-64','--rename-section','.data=.text,alloc,load,readonly,code,contents',f,obj],cpu=20,timeout=30);N.run(lab+'-asm',[tools/'llvm-objdump','--disassemble','--section=.text','--no-show-raw-insn','--x86-asm-syntax=intel',obj],cpu=20,timeout=30);asm.write_bytes((R/'evidence'/(lab+'-asm.stdout')).read_bytes());N.persist(asm);obj.unlink();out.append(dict(sha=N.sha(f),file=str(f),asm=str(asm),lo=lo,hi=hi))
    finally:os.close(fd)
    assert out;p.stdin.write(b'G');p.stdin.flush();protocol=read(p,16);assert protocol[8:]==b'DONE1\0\0\0';p.stdin.write(b'A');p.stdin.flush();assert p.wait(timeout=10)==0;rows.append(dict(case=name,variant=v,ELF_sha=N.sha(elf),WASM_sha=c['wasm_sha'],rx=out,elapsed=struct.unpack('<Q',protocol[:8])[0],cpu=6,cgroup=Path('/proc/self/cgroup').read_text().strip()))
   finally:
    if p.poll() is None:p.terminate();p.wait(timeout=5)
 N.persist(stderr)
N.save(D/'binding.json',dict(rows=rows,scope='Actual postJIT READY RX snapshot, non-PMU CPU6 inspection only; no performance ratios from these single repetitions.'))
print('C23_PREVIEW_CAPTURED',len(rows),flush=True)
