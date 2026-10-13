from pathlib import Path
import os,sys,time,json,struct,subprocess,selectors,signal
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
s=json.loads(Path(sys.argv[1]).read_text());assert N.sha(__file__)==s['worker_sha_expected'];D=Path(s['dir']);D.mkdir(exist_ok=False,parents=True)
perf='/opt/intel/oneapi/vtune/2026.4/bin64/amplxe-perf';target=counter=None
fds=[]
def read(fd,n,deadline,proc):
 b=b''
 while len(b)<n:
  assert proc.poll() is None,('process exited',proc.returncode)
  assert time.monotonic()<deadline,('protocol timeout',b)
  with selectors.DefaultSelector() as sel:
   sel.register(fd,selectors.EVENT_READ);ev=sel.select(.1)
  if ev:
   a=os.read(fd,n-len(b));assert a;b+=a
 return b
def ident(p):return {'pid':p,'cgroup':Path('/proc/'+str(p)+'/cgroup').read_text().strip(),'cpus':sorted(os.sched_getaffinity(p))}
def cp():return {'monotonic_ns':time.clock_gettime_ns(time.CLOCK_MONOTONIC),'raw_ns':time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)}
try:
 assert N.sha(s['elf'])==s['elf_sha'] and N.sha(s['wasm'])==s['wasm_sha']
 env=dict(os.environ,WASM_PERF_NATIVE_PROFILE='1',WASM_PERF_NATIVE_REPS=str(s['reps']))
 for k in ['WASM_PERF_NATIVE_CLI','UWVM_GC_ARRAY_BUDGET_MIB']:env.pop(k,None)
 env['LD_LIBRARY_PATH']='/tmp/uwvm-gc-perf-20261011-r73/source/wasmtime-capi/lib:'+env.get('LD_LIBRARY_PATH','')
 if s['variant']=='wasmtime-drc':
  dep=json.loads((R/'source/wasmtime-dynamic-library-binding-c20.json').read_text());before_sha=N.sha(dep['path']);assert before_sha==dep['sha256'];st=Path(dep['path']).stat();assert st.st_ino==dep['inode']
 te=open(D/'target.stderr','wb')
 target=subprocess.Popen(s['cmd'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=te,env=env,preexec_fn=lambda:os.sched_setaffinity(0,{2}))
 ti=ident(target.pid);assert ti['cgroup']=='0::'+N.CG and ti['cpus']==[2]
 assert read(target.stdout.fileno(),8,time.monotonic()+90,target)==b'READY1\0\0'
 maps=Path('/proc/'+str(target.pid)+'/maps').read_text();(D/'ready-maps.txt').write_text(maps)
 if s['variant']=='wasmtime-drc':assert any(l.split()[-1]==dep['path'] and int(l.split()[4])==st.st_ino and l.split()[3]==dep['device'] for l in maps.splitlines())
 cr,cw=os.pipe();ar,aw=os.pipe();fds=[cr,cw,ar,aw]
 out=open(D/'counter.stdout','wb');err=open(D/'counter.stderr','wb')
 argv=[perf,'stat','--json-output','--no-scale','--no-inherit','-t',str(target.pid),'-C','2','-e','{cpu_core/cycles/u,cpu_core/instructions/u,cpu_core/ref-cycles/u}','-D','-1','--control','fd:'+str(cr)+','+str(aw)]
 counter=subprocess.Popen(argv,stdout=out,stderr=err,pass_fds=(cr,aw),stdin=subprocess.DEVNULL)
 ci=ident(counter.pid);assert ci['cgroup']=='0::'+N.CG
 def ctl(command):
  os.write(cw,command.encode()+b'\n');ack=read(ar,5,time.monotonic()+15,counter);assert ack==b'ack\n\x00',ack
 ctl('enable');roi_start=cp();target.stdin.write(b'G');target.stdin.flush()
 protocol=read(target.stdout.fileno(),16,time.monotonic()+45,target);roi_end=cp();ctl('disable')
 elapsed=struct.unpack('<Q',protocol[:8])[0];assert protocol[8:]==b'DONE1\0\0\0' and elapsed>=500_000_000,(protocol,elapsed)
 if s['variant']=='wasmtime-drc':assert N.sha(dep['path'])==before_sha and Path(dep['path']).stat().st_ino==st.st_ino
 target.stdin.write(b'A');target.stdin.flush();assert target.wait(timeout=10)==0;tail=target.stdout.read();assert tail==b'\x00' or not tail,tail
 assert counter.wait(timeout=10)==0,(counter.returncode,(D/'counter.stderr').read_text())
 te.close();out.close();err.close()
 text=(D/'counter.stderr').read_text();items=[]
 try:
  q=json.loads(text);items=q if isinstance(q,list) else [q]
 except json.JSONDecodeError:
  for l in text.splitlines():
   if l.lstrip().startswith('{'):items.append(json.loads(l.strip().rstrip(',')))
 assert len(items)==3,(text,items)
 parsed={}
 for x in items:
  ev=x['event'];assert any(k in ev for k in ['cycles','instructions']),ev
  v=x['counter-value'];assert v not in ['<not counted>','<not supported>'];value=float(str(v).replace(',',''));assert value>0
  pc=float(x['pcnt-running']);assert pc>=99.99,(ev,pc)
  key='instructions' if 'instructions' in ev else 'ref_cycles' if 'ref-cycles' in ev else 'cycles';assert key not in parsed
  parsed[key]={'value':value,'per_rep':value/s['reps'],'pcnt_running':pc,'event_runtime':x.get('event-runtime'),'event':ev}
 assert set(parsed)=={'instructions','cycles','ref_cycles'}
 lines=(D/'target.stderr').read_text().splitlines()
 if s['variant']!='wasmtime-drc':assert lines and all(l.startswith('UWVM-GC-FUNC-PUB ') for l in lines),lines
 else:assert not lines,lines
 N.save(D/'fixed-counter-capture.json',dict(s,target=ti,counter=ci,argv=argv,elapsed_ns=elapsed,ns_per_rep=elapsed/s['reps'],roi_start=roi_start,roi_end=roi_end,counts=parsed,raw_sha=N.sha(D/'counter.stderr'),boot=N.BOOT,scope='Fixed hardware counters grouped, unscaled, user mode, exact target TID pinned CPU2. Disabled until READY after JIT and warmup; enable ACK before GO, disable ACK after DONE. Small target user protocol serialization outside the internal guest timer is included; blocking time produces no user counts. VM transitions remain. Reject multiplexing below99.99%.'))
 print('FIXED_COUNTER_QUALIFIED',s['label'],elapsed/s['reps'],{k:v['per_rep'] for k,v in parsed.items()},flush=True)
finally:
 for p in [counter,target]:
  if p is not None and p.poll() is None:
   try:p.terminate();p.wait(timeout=5)
   except Exception:
    try:p.kill();p.wait(timeout=5)
    except Exception:pass
 for fd in fds:
  try:os.close(fd)
  except OSError:pass
 for f in D.rglob('*'):
  if f.is_file():N.persist(f)
