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
 d=O/'runs-r4'/label;d.mkdir(parents=True)
 row={'label':label,'argv':list(map(str,argv)),'cwd':str(cwd),'expected_exit':expected,'before':st,'passed':False,'sampled_peak_rss':0,'sampled_processes':[],'cleanup_signals':[],'log_directory':str(d)}
 result['commands'].append(row);save(O/'result-r4.json',result);known={};fds={};p=None;err=None;start=time.monotonic()
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
 save(O/'result-r4.json',result);print(json.dumps({'label':label,'passed':row['passed'],'error':err,'peak_rss':row['sampled_peak_rss']}),flush=True)
 assert row['passed'],label
 return row,(d/'stdout.log').read_bytes(),(d/'stderr.log').read_bytes()
def main():
 global result
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,128<<30));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 assert ctypes.CDLL(None).prctl(36,1,0,0,0)==0
 before=guard();assert before['functional_estimate']<54<<30
 result={'status':'RUNNING','before':before,'commands':[],'all15_complete':False,'new_native_ros_delivery':False,'new_debugger_credit':False,'new_performance_credit':False}
 try:
  for name in ('tmp','home'):(O/name).mkdir(exist_ok=True)
  node=Path('/home/macromodel/.nvm/versions/node/v22.22.3/bin/node').resolve();wt=V/'root-language-tinygo-complete-examples-20261019-a7/validation-tools/wasm-tools'
  result['tools']=[pin(p)for p in [T/'bin/moon',T/'bin/moonc',node,wt]]
  run('node-version',[node,'--version'],O,rss=256<<20)
  run('models-default',[sys.executable,'-I','-B',S/'sdk/tools/run_moonbit_models.py','--moon',T/'bin/moon','--moon-home',T,'--node',node,'--wasm-tools',wt,'--scratch',O/'models-default-r4'],O,deadline=240)
  report=json.loads((O/'models-default-r4/report.json').read_bytes());assert report['passed'] and report['model_tests_per_target']=={'wasm':88,'wasm-gc':88}
  run('models-owned',[sys.executable,'-I','-B',S/'sdk/tools/run_moonbit_models.py','--moon',T/'bin/moon','--moon-home',T,'--node',node,'--wasm-tools',wt,'--scratch',O/'models-owned-r4','--include-owned-service'],O,deadline=240)
  report=json.loads((O/'models-owned-r4/report.json').read_bytes());assert report['passed'] and report['model_tests_per_target']=={'wasm':92,'wasm-gc':92}
  result['status']='ACTUAL_MOONBIT_IMAGE_MODELS_PASSED'
 except BaseException as e:result['status']='FAILED_RETAINED';result['primary']=repr(e);result['traceback']=traceback.format_exc()
 result['after']=guard();save(O/'result-r4.json',result);print(result['status'],flush=True)
if __name__=='__main__':main()
