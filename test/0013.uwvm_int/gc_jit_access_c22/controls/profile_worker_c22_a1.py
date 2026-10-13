import pathlib,sys,json,os,time,subprocess,selectors,struct,hashlib,signal
R=pathlib.Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
spec=json.loads(pathlib.Path(sys.argv[1]).read_text());assert N.sha(__file__)==spec['worker_sha_expected'];D=pathlib.Path(spec['dir']);D.mkdir(parents=True,exist_ok=False)
V='/opt/intel/oneapi/vtune/2026.4/bin64/vtune'
def identity(pid):
 return {'pid':pid,'cgroup':pathlib.Path('/proc/'+str(pid)+'/cgroup').read_text().strip(),'cpus':sorted(os.sched_getaffinity(pid))}
def readyread(p,n,deadline):
 b=b''
 while len(b)<n:
  if p.poll() is not None:raise RuntimeError(('target died',p.returncode,b))
  sel=selectors.DefaultSelector();sel.register(p.stdout,selectors.EVENT_READ)
  events=sel.select(min(.2,max(0,deadline-time.monotonic())));sel.close()
  assert time.monotonic()<deadline,('protocol timeout',b)
  if events:
   t=os.read(p.stdout.fileno(),n-len(b));assert t;b+=t
 return b
def process_counters(pid):
 s=pathlib.Path('/proc/'+str(pid)+'/stat').read_text();a=s[s.rfind(')')+2:].split()
 return {'minflt':int(a[7]),'majflt':int(a[9]),'utime_ticks':int(a[11]),'stime_ticks':int(a[12]),'clock_ticks_per_second':os.sysconf('SC_CLK_TCK')}
def clockpair():return {'monotonic_ns':time.clock_gettime_ns(time.CLOCK_MONOTONIC),'raw_ns':time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)}
def flush_archive():
 files=[p for p in D.rglob('*') if p.is_file()]
 total=sum(p.stat().st_size for p in files);assert total<256<<20,('profile-directory-budget',total)
 for p in files:N.persist(p)
 return {'files':len(files),'bytes':total}
def jitcapture(pid,phase):
 smaps=pathlib.Path('/proc/'+str(pid)+'/smaps_rollup').read_text();q=D/(phase+'-whole-process-smaps-rollup.txt');q.write_text(smaps);N.persist(q)
 maps=pathlib.Path('/proc/'+str(pid)+'/maps').read_text();p=D/(phase+'-maps.txt');p.write_text(maps);N.persist(p);rows=[];total=0
 fd=os.open('/proc/'+str(pid)+'/mem',os.O_RDONLY)
 try:
  for l in maps.splitlines():
   a=l.split(maxsplit=5);path=a[5] if len(a)==6 else '';perms=a[1]
   if 'x' not in perms or 'r' not in perms or not (not path or path.startswith('/memfd:') or path.startswith('[anon:')):continue
   lo,hi=[int(v,16) for v in a[0].split('-')];n=hi-lo;total+=n;assert total<=32<<20
   b=os.pread(fd,n,lo);assert len(b)==n
   f=D/(phase+'-rx-'+hex(lo)+'.bin');f.write_bytes(b);N.persist(f)
   rows.append({'lo':lo,'hi':hi,'sha':hashlib.sha256(b).hexdigest(),'path':path,'perms':perms,'file':str(f)})
 finally:os.close(fd)
 assert rows,'no live executable JIT mapping'
 N.save(D/(phase+'-rx-index.json'),rows)
 return rows
target=collector=None
try:
 assert N.sha(spec['elf'])==spec['elf_sha'] and N.sha(spec['wasm'])==spec['wasm_sha']
 env=dict(os.environ,WASM_PERF_NATIVE_PROFILE='1',WASM_PERF_NATIVE_REPS=str(spec['reps']))
 env.pop('WASM_PERF_NATIVE_CLI',None)
 env.pop('UWVM_GC_ARRAY_BUDGET_MIB',None)
 config=spec.get('native_experiment_env',{});assert isinstance(config,dict) and set(config)<= {'UWVM_GC_ARRAY_BUDGET_MIB'}
 if config:assert config['UWVM_GC_ARRAY_BUDGET_MIB'].isdigit() and 1<=int(config['UWVM_GC_ARRAY_BUDGET_MIB'])<=64
 env.update(config)
 spec['worker_sha']=N.sha(__file__)
 spec['whole_process_smaps_scope']='whole process only; not a pure GC payload/heap-size metric'
 env['LD_LIBRARY_PATH']='/tmp/uwvm-gc-perf-20261011-r73/source/wasmtime-capi/lib:'+env.get('LD_LIBRARY_PATH','')
 def setup():os.sched_setaffinity(0,{2})
 te=open(D/'target.stderr','wb')
 target=subprocess.Popen(spec['cmd'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=te,env=env,preexec_fn=setup)
 tid=identity(target.pid);assert tid['cgroup']=='0::'+N.CG and tid['cpus']==[2]
 assert readyread(target,8,time.monotonic()+90)==b'READY1\0\0'
 before=jitcapture(target.pid,'before')
 cf=open(D/'collector.stdout','wb');ce=open(D/'collector.stderr','wb');res=D/'vtune'
 argv=[V,'-collect',spec['analysis'],'-knob','sampling-interval='+str(spec.get('interval',20))]
 if spec['analysis']=='hotspots':argv+=['-knob','sampling-mode=hw','-knob','enable-characterization-insights=false','-knob','enable-stack-collection=false']
 else:argv+=['-knob','pmu-collection-mode=detailed','-knob','dram-bandwidth-limits=false']
 argv+=['-target-pid',str(target.pid),'-start-paused','-no-follow-child','-data-limit','16']
 argv+=spec['passthru'];argv+=['-result-dir',str(res)]
 N.save(D/'launch.json',dict(spec,target=tid,argv=argv,rx=before,boot=N.BOOT))
 collector=subprocess.Popen(argv,stdout=cf,stderr=ce);cid=identity(collector.pid);assert cid['cgroup']=='0::'+N.CG
 deadline=time.monotonic()+45
 while 'Collection paused.' not in (D/'collector.stderr').read_text(errors='replace'):
  assert collector.poll() is None,('collector failed',(D/'collector.stderr').read_text())
  assert time.monotonic()<deadline;time.sleep(.1)
 def ctl(verb):
  j=subprocess.run([V,'-command',verb,'-r',str(res)],capture_output=True,timeout=20)
  (D/('control-'+verb+'.stdout')).write_bytes(j.stdout);(D/('control-'+verb+'.stderr')).write_bytes(j.stderr);assert j.returncode==0,(verb,j.stderr)
 if spec.get('configuration_only'):
  ctl('stop');rc=collector.wait(timeout=90);assert rc==0,rc
  stat_before=process_counters(target.pid);roi_start=clockpair();target.stdin.write(b'G');target.stdin.flush()
 else:
  ctl('status');ctl('resume')
  stat_before=process_counters(target.pid);roi_start=clockpair();target.stdin.write(b'G');target.stdin.flush()
 protocol=readyread(target,16,time.monotonic()+45);roi_end=clockpair();stat_after=process_counters(target.pid)
 elapsed=struct.unpack('<Q',protocol[:8])[0];assert protocol[8:]==b'DONE1\0\0\0'
 if not spec.get('configuration_only'):assert elapsed>=spec.get('minimum_elapsed_ns',500_000_000)
 after=jitcapture(target.pid,'after')
 assert [(x['lo'],x['hi'],x['sha']) for x in before]==[(x['lo'],x['hi'],x['sha']) for x in after]
 if not spec.get('configuration_only'):
  ctl('stop');rc=collector.wait(timeout=90);assert rc==0,rc
 target.stdin.write(b'A');target.stdin.flush();assert target.wait(timeout=10)==0
 tail=target.stdout.read();assert tail==b'\x00' or not tail,tail
 te.close();ce.close();cf.close()
 stderr_lines=(D/'target.stderr').read_text().splitlines()
 publication_rows=[]
 if spec['variant'] in ('published-function-type-producers','guarded-small-defined-call-ref-set','immutable-array-length-values','typed-array-load','typed-array-access','typed-array-access-inline'):
  assert stderr_lines and all(l.startswith('UWVM-GC-FUNC-PUB ') for l in stderr_lines),stderr_lines
  publication_rows=[list(map(int,l.split()[1:])) for l in stderr_lines]
  assert all(len(a)==3 and a[2]<4294967295 for a in publication_rows),publication_rows
 else: assert not stderr_lines,stderr_lines
 N.save(D/'cold-publication-audit-c20.json',{'rows':publication_rows,'variant':spec['variant'],'stderr_sha':N.sha(D/'target.stderr'),'worker_sha':N.sha(__file__),'scope':'only expected cold publication diagnostics accepted; all other target stderr rejected'})
 N.save(D/'capture.json',dict(spec,target=tid,collector=cid,collector_argv=argv,roi_start=roi_start,roi_end=roi_end,elapsed_ns=elapsed,rx=before,boot=N.BOOT,hardware_qualified=False,process_stat_before=stat_before,process_stat_after=stat_after,process_stat_delta={k:stat_after[k]-stat_before[k] for k in ('minflt','majflt','utime_ticks','stime_ticks')},process_stat_scope='whole target process counters bracketing ROI protocol; warmup and JIT excluded; no claim of pure GC allocator attribution'))
 print('CAPTURED',spec['label'],elapsed,flush=True)
finally:
 for p in [collector,target]:
  if p is not None and p.poll() is None:
   try:
    fd=os.pidfd_open(p.pid);signal.pidfd_send_signal(fd,signal.SIGTERM);p.wait(timeout=5);os.close(fd)
   except Exception:
    try:p.kill();p.wait(timeout=5)
    except Exception:pass
 print('ARCHIVE',flush_archive(),flush=True)
