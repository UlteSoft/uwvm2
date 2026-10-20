from pathlib import Path
import datetime,hashlib,json,os,resource,signal,subprocess,sys,time,traceback,ctypes
V=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1')
O=V/'root-language-moonbit-image-20261019-a9';S=O/'source-r3';T=O/'moon-home-r2'
G='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
def sha(b):return hashlib.sha256(b).hexdigest()
def pin(p):
 p=Path(p).resolve();h=hashlib.sha256()
 with p.open('rb') as f:
  st=os.fstat(f.fileno())
  while b:=f.read(65536):h.update(b)
  assert os.fstat(f.fileno())==st
 return {'path':str(p),'bytes':st.st_size,'sha256':h.hexdigest(),'device':st.st_dev,'inode':st.st_ino}
def save(p,x):p.write_text(json.dumps(x,indent=2)+'\n')
def guard():
 c=Path('/sys/fs/cgroup'+G)
 assert Path('/proc/self/cgroup').read_text().strip()=='0::'+G
 assert (c.stat().st_dev,c.stat().st_ino)==(29,19075)
 assert (c/'memory.max').read_text().strip()=='68719476736' and (c/'memory.swap.max').read_text().strip()=='0'
 assert Path('/proc/sys/kernel/random/boot_id').read_text().strip()=='51c27aa1-d709-4a06-ba16-2ac3003fbcbd'
 st=dict(x.split()for x in(c/'memory.stat').read_text().splitlines());cur=int((c/'memory.current').read_text())
 rec=max(0,min(cur,int(st['inactive_file']))-int(st['file_dirty'])-int(st['file_writeback'])-int(st['unevictable']))
 assert cur<=64<<30 and cur-rec<56<<30,'functional capacity'
 ev=dict(x.split()for x in(c/'memory.events').read_text().splitlines());assert all(ev[k]=='0'for k in ('oom','oom_kill','oom_group_kill'))
 return {'memory_current':cur,'recoverable_file_estimate':rec,'functional_estimate':cur-rec,'events':ev}
def proc(pid):
 p=Path('/proc')/str(pid);f=(p/'stat').read_text();f=f[f.rfind(')')+2:].split()
 rss=next((int(l.split()[1])*1024 for l in(p/'status').read_text().splitlines()if l.startswith('VmRSS:')),0)
 return {'pid':pid,'pgrp':int(f[2]),'ppid':int(f[1]),'start':int(f[19]),'rss':rss,'cgroup':(p/'cgroup').read_text().strip(),'state':f[0]}
def group(pgid):
 rows=[]
 for pid in(Path('/sys/fs/cgroup'+G)/'cgroup.procs').read_text().split():
  try:r=proc(int(pid))
  except (FileNotFoundError,ProcessLookupError):continue
  if r['pgrp']==pgid:assert r['cgroup']=='0::'+G;rows.append(r)
 return rows
result={}
def run(label,argv,cwd,env=None,expected=0,deadline=180,rss=1024<<20,stdin=None):
 st=guard();assert st['functional_estimate']<54<<30,'functional admission54'
 d=O/'matrix-r3'/label;d.mkdir(parents=True)
 row={'label':label,'argv':list(map(str,argv)),'cwd':str(cwd),'expected_exit':expected,'before':st,'passed':False,'sampled_peak_rss':0,'sampled_processes':[],'cleanup_signals':[],'log_directory':str(d)}
 result['commands'].append(row);save(O/'matrix-result-r3.json',result);known={};fds={};p=None;err=None;start=time.monotonic()
 def prep():
  os.sched_setaffinity(0,{18});resource.setrlimit(resource.RLIMIT_AS,(128<<30,128<<30));resource.setrlimit(resource.RLIMIT_CPU,(180,180))
 env=env or {'HOME':os.environ.get('HOME','/root'),'MOON_HOME':str(T),'PATH':str(T/'bin')+':/usr/bin:/bin','TZ':'UTC','LANG':'C.UTF-8','TMPDIR':str(O/'tmp')}
 with (d/'stdout.log').open('xb')as out,(d/'stderr.log').open('xb')as er:
  try:
   p=subprocess.Popen(row['argv'],cwd=cwd,env=env,stdin=subprocess.DEVNULL if stdin is None else subprocess.PIPE,stdout=out,stderr=er,start_new_session=True,preexec_fn=prep)
   row['pid']=p.pid;leader=proc(p.pid);known[p.pid]=leader;fds[p.pid]=os.pidfd_open(p.pid)
   if stdin is not None:p.stdin.write(stdin);p.stdin.close()
   while p.poll()is None:
    state=guard();row['sampled_raw_peak']=max(row.get('sampled_raw_peak',0),state['memory_current'])
    row['sampled_functional_peak']=max(row.get('sampled_functional_peak',0),state['functional_estimate']);rows=group(p.pid)
    for r in rows:
     if r['pid'] in known:assert r['start']==known[r['pid']]['start']
     else:
      known[r['pid']]=r
      try:fds[r['pid']]=os.pidfd_open(r['pid'])
      except ProcessLookupError:pass
    total=sum(r['rss']for r in rows);row['sampled_peak_rss']=max(row['sampled_peak_rss'],total)
    assert total<=rss,'owned aggregate RSS guard'
    assert time.monotonic()-start<deadline,'deadline'
    assert out.tell()+er.tell()<4<<20,'output bound'
    time.sleep(.02)
   row['exit']=p.wait()
   for _ in range(30):
    for r in group(p.pid):
     if r['state']=='Z':
      try:os.waitpid(r['pid'],os.WNOHANG)
      except ChildProcessError:pass
    if not group(p.pid):break
    time.sleep(.02)
   assert not group(p.pid),'owned group left descendants'
  except BaseException as e:
   err=repr(e)
   if p is not None:
    for sig in (signal.SIGTERM,signal.SIGKILL):
     for r in group(p.pid):
      if r['pid'] not in known:
       known[r['pid']]=r
       try:fds[r['pid']]=os.pidfd_open(r['pid'])
       except ProcessLookupError:continue
      assert r['start']==known[r['pid']]['start']
      try:signal.pidfd_send_signal(fds[r['pid']],sig);row['cleanup_signals'].append({'pid':r['pid'],'signal':int(sig),'start':r['start']})
      except ProcessLookupError:pass
     try:p.wait(timeout=3)
     except subprocess.TimeoutExpired:pass
    row['exit']=p.returncode
  finally:
   for fd in fds.values():os.close(fd)
 row['error']=err;row['sampled_processes']=list(known.values());row['watchdog_seconds']=time.monotonic()-start
 row['remaining_owned_group']=[] if p is None else group(p.pid)
 for name in ('stdout','stderr'):
  b=(d/(name+'.log')).read_bytes();row[name]={'path':str(d/(name+'.log')),'bytes':len(b),'sha256':sha(b)}
 row['passed']=err is None and row['exit']==expected and not row['remaining_owned_group']
 save(O/'matrix-result-r3.json',result);print(json.dumps({'label':label,'passed':row['passed'],'error':err,'peak_rss':row['sampled_peak_rss']}),flush=True)
 assert row['passed'],label
 return row,(d/'stdout.log').read_bytes(),(d/'stderr.log').read_bytes()
def layout(checker,raw,wat,target,pure):
 r=checker.Reader(raw);assert bytes(r.take(8))==b'\0asm\1\0\0\0'
 mem=[];segments=[];exports={};seen=set()
 while r.position<len(r.data):
  sec=r.byte();b=checker.Reader(r.take(r.uint()))
  if sec in (5,7,11):assert sec not in seen;seen.add(sec)
  if sec==5:
   for _ in range(b.count()):
    assert b.uint()==1;mem.append([b.uint(),b.uint()])
   b.end()
  elif sec==7:
   for _ in range(b.count()):
    n=b.name();k=b.byte();i=b.uint();assert n not in exports;exports[n]=[k,i]
   b.end()
  elif sec==11:
   for _ in range(b.count()):
    mode=b.uint();assert mode in (0,1,2);row={'mode':mode}
    if mode!=1:
     memory=0 if mode==0 else b.uint();assert memory==0 and b.byte()==0x41
     row['offset']=b.sint(32)&0xffffffff;assert b.byte()==0x0b
    row['bytes']=len(b.take(b.uint()));segments.append(row)
   b.end()
 assert mem==[[256,1024]] and exports.get('memory')==[2,0]
 assert set(exports)==({'memory','_start'} if pure else {'memory','_start','abi_coverage'})
 active=0
 for row in segments:
  if row['mode']==1:continue
  lo=row['offset'];hi=lo+row['bytes'];assert hi<=256*65536
  assert all(hi<=a or lo>=b for a,b in ((0,16),(32,289),(65536,8454160)))
  active=max(active,hi)
 anchor=None
 if target=='wasm':
  lines=wat.splitlines();pos=[i for i,l in enumerate(lines)if l.startswith('  (func $tlsf/initialize ')]
  assert len(pos)<=1,'unique named pinned allocator anchor'
  if not pos:assert '$tlsf/'not in wat,'allocation-free producer only when no allocator instructions retained'
  else:
   st=pos[0];en=next((i for i in range(st+1,len(lines))if lines[i].startswith('  (func ')),len(lines))
   first=next(l.strip()for l in lines[st+1:en]if not l.strip().startswith('(local '))
   m=re.fullmatch(r'i32.const ([0-9]+)',first);assert m
   anchor=int(m[1]);assert anchor>=8519680 and anchor>=active
 else:assert '$tlsf/'not in wat and re.search(r'^\s+memory\.(init|grow)\b',wat,re.M)is None
 return {'memory':mem,'exports':exports,'segments':segments,'active_end':active,'pinned_allocator_anchor':anchor,'scope':'Actual section extents and named pinned-producer allocator anchor; not formal general allocator proof'}
def main():
 global result
 import shutil,importlib.util,re
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,128<<30));resource.setrlimit(resource.RLIMIT_CORE,(0,0));ctypes.CDLL(None).prctl(36,1,0,0,0)
 parent=json.loads((O/'result-r4.json').read_bytes());assert parent['status']=='ACTUAL_MOONBIT_IMAGE_MODELS_PASSED'
 result={'status':'RUNNING','before':guard(),'commands':[],'artifacts':[],'all15_complete':False,'new_native_ros_delivery':False,'new_debugger_credit':False,'new_performance_credit':False}
 try:
  source=json.loads((O/'source-inputs-r3.json').read_bytes())
  for row in source['members']:assert pin(S/row['path'])['sha256']==row['sha256']
  P=O/'project-r1';assert P.is_dir()
  for row in source['members']:
   if row['path'].startswith('sdk/examples/moonbit/'):assert pin(P/Path(row['path']).relative_to('sdk/examples/moonbit'))['sha256']==row['sha256']
   elif row['path'].startswith('sdk/moonbit/'):assert pin(P/'ros-sdk'/Path(row['path']).relative_to('sdk/moonbit'))['sha256']==row['sha256']
  spec=importlib.util.spec_from_file_location('checker',S/'sdk/tools/check_wasm_imports.py');checker=importlib.util.module_from_spec(spec);spec.loader.exec_module(checker)
  wt=V/'root-language-tinygo-complete-examples-20261019-a7/validation-tools/wasm-tools'
  result['tools']=[pin(T/'bin/moon'),pin(T/'bin/moonc'),pin(wt)]
  apps=sorted(p.name for p in P.iterdir()if p.is_dir()and(p/'main.mbt').exists());assert len(apps)==15
  result['application_names']=apps
  for target in ('wasm','wasm-gc'):
   for profile in ('debug','release-names'):
    label=target+'-'+profile;out=O/('build-'+label)
    flags=['--debug']if profile=='debug'else['--release','--no-strip']
    env={'HOME':os.environ.get('HOME','/root'),'MOON_HOME':str(T),'PATH':str(T/'bin')+':/usr/bin:/bin','LANG':'C.UTF-8','TMPDIR':str(O/'tmp'),'XDG_CACHE_HOME':str(O/'home/cache')}
    if label=='wasm-debug':
     previous=json.loads((O/'matrix-result-r1.json').read_bytes())['commands'][0];assert previous['passed'] and previous['exit']==0;result.setdefault('reused_build_commands',[]).append(previous)
    else:run('build-'+label,[T/'bin/moon','-C',P,'build','--frozen','--target',target,*flags,'-j','1','--target-dir',out],P,env,deadline=180)
    built=out/target/('debug'if profile=='debug'else'release')/'build'/'uwvm/ros_examples'
    for app in apps:
     wasm=built/app/(app+'.wasm');assert wasm.exists(),wasm
     entry=pin(wasm);entry.update({'target':target,'profile':profile,'app':app})
     run('validate-'+label+'-'+app,[wt,'validate',wasm],P,rss=256<<20)
     _,wat,err=run('print-'+label+'-'+app,[wt,'print',wasm],P,rss=256<<20);assert not err
     pure=app=='07_image_codec';backend=app[-1]
     if pure:
      imports=checker.inspect(wasm.read_bytes());assert imports==[{'module':'wasi_snapshot_preview1','name':'fd_write','kind':0,'params':['i32']*4,'results':['i32']}],imports
     else:
      report=checker.check(wasm.read_bytes(),backend,('wasi_snapshot_preview1',),True);imports=report['imports']
      wasi=[i for i in imports if i['module']=='wasi_snapshot_preview1'];assert len(wasi)==1 and wasi[0]['name']=='fd_write'and wasi[0]['params']==['i32']*4 and wasi[0]['results']==['i32']
     entry['imports']=imports;entry['layout']=layout(checker,wasm.read_bytes(),wat.decode(),target,pure);result['artifacts'].append(entry);save(O/'matrix-result-r3.json',result)
  runtimes=json.loads((V/'root-language-go-sdk-20261019-a3/runtime-restoration.json').read_bytes())['runtimes']
  result['runtimes']=runtimes
  features=['-Rint','-WFE-gc','-WFE-exceptions','-WFE-tail-call','-WFE-function-references','-WFE-reference-types','-WFE-simd','-WFE-relaxed-simd','-WFE-extended-const','-WFE-multi-memory','-WFE-memory64','-WFE-table64','--wasip1-global-noinherit-system-environment']
  for runtime in runtimes:
   r=Path(runtime['path']);assert pin(r)['sha256']==runtime['sha256']
   for a in result['artifacts']:
    expected=re.findall(r'@stdio.line_ascii\("([^"]+)"\)',(P/a['app']/'main.mbt').read_text());assert len(expected)==1
    _,out,err=run('ordinary-'+runtime['mode']+'-'+a['target']+'-'+a['profile']+'-'+a['app'],[r,*features,'--run',a['path']],P,rss=96<<20,deadline=60)
    assert out==(expected[0]+'\n').encode() and not err,(out,err)
  result['status']='ACTUAL_MOONBIT60_PRODUCTS120_DEFAULT_BASELINE_EXECUTIONS_PASSED'
 except BaseException as e:result['status']='FAILED_RETAINED';result['primary']=repr(e);result['traceback']=traceback.format_exc()
 result['after']=guard();save(O/'matrix-result-r3.json',result);print(result['status'],flush=True)
if __name__=='__main__':
 import re
 main()
