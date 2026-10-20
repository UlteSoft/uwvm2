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
  last=os.fstat(f.fileno());assert (last.st_dev,last.st_ino,last.st_size,last.st_mtime_ns,last.st_ctime_ns,last.st_mode,last.st_nlink)==(st.st_dev,st.st_ino,st.st_size,st.st_mtime_ns,st.st_ctime_ns,st.st_mode,st.st_nlink)
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
 d=O/'probe-r1'/label;d.mkdir(parents=True)
 row={'label':label,'argv':list(map(str,argv)),'cwd':str(cwd),'expected_exit':expected,'before':st,'passed':False,'sampled_peak_rss':0,'sampled_processes':[],'cleanup_signals':[],'log_directory':str(d)}
 result['commands'].append(row);save(O/'probe-result-r1.json',result);known={};fds={};p=None;err=None;start=time.monotonic()
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
 save(O/'probe-result-r1.json.json',result);print(json.dumps({'label':label,'passed':row['passed'],'error':err,'peak_rss':row['sampled_peak_rss']}),flush=True)
 assert row['passed'],label
 return row,(d/'stdout.log').read_bytes(),(d/'stderr.log').read_bytes()
def main():
 global result
 import shlex,struct,importlib.util
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,128<<30));resource.setrlimit(resource.RLIMIT_CORE,(0,0));ctypes.CDLL(None).prctl(36,1,0,0,0)
 result={'status':'RUNNING','before':guard(),'commands':[],'mapped_products':[],'new_native_ros_delivery':False,'new_performance_credit':False}
 try:
  P=O/'project-r1';D=O/'mapped-products-r1';D.mkdir()
  lines=(O/'extended-r4/moon-dry-run/stdout.log').read_text().splitlines()
  for target in ('wasm',):
   line=next(l for l in lines if l.startswith('moonc link-core ')and'/07_image_codec/07_image_codec.core 'in l)
   argv=shlex.split(line)
   argv=[v.replace('$MOON_HOME',str(T)).replace(str(O/'source-map-plan-r1'),str(O/'build-wasm-debug'))for v in argv];argv[0]=str(T/'bin/moonc')
   out=D/'image-codec.wasm';argv[argv.index('-o')+1]=str(out);argv+=['-source-map','-source-map-url','image-codec.wasm.map']
   inputs=[pin(v)for v in argv if v.endswith('.core')];assert len(inputs)==6
   result['link_core_inputs']=inputs
   run('source-map-link',argv,P,rss=512<<20)
   assert all(pin(r['path'])==r for r in inputs)
   mapping=Path(str(out)+'.map');assert mapping.is_file()
   value=json.loads(mapping.read_bytes());assert value['version']==3 and len(value['sources'])>0
   result['mapped_products'].append({'wasm':pin(out),'map':pin(mapping),'sources':value['sources'],'sourceRoot':value.get('sourceRoot')})
   wt=V/'root-language-tinygo-complete-examples-20261019-a7/validation-tools/wasm-tools'
   run('mapped-validation',[wt,'validate',out],P,rss=128<<20)
  c=Path('/usr/lib/llvm-21/bin/clang');result['native_compiler']=pin(c)
  cpp=O/'native-source-r1/moonbit_image_contract.cc';result['native_source']=pin(cpp)
  native=O/'native-image-contract'
  run('native-build',[c,'--driver-mode=g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-I',S,cpp,'-o',native],S,rss=512<<20)
  result['native_product']=pin(native)
  _,out,err=run('native-contract',[native],S,rss=128<<20)
  frame=b'camera/optical';data=bytes.fromhex('ff000000ff00aabb0000ffffffffccdd')
  wire=struct.pack('<III',0,1,2)+struct.pack('<I',len(frame))+frame+struct.pack('<II',2,2)+struct.pack('<I',4)+b'rgb8'+b'\0'+struct.pack('<II',8,len(data))+data
  assert out==wire.hex().encode()+b'\nPASS_NATIVE_IMAGE_CONTRACT checks=81\n'and not err,(out,err)
  result['independent_vector_hex']=wire.hex();result['status']='ACTUAL_MAPPED_MOONBIT_PRODUCER_AND_NATIVE_HOST_IMAGE_CONTRACT_PASSED'
 except BaseException as e:result['status']='FAILED_RETAINED';result['primary']=repr(e);result['traceback']=traceback.format_exc()
 result['after']=guard();save(O/'probe-result-r1.json',result);print(result['status'],flush=True)
if __name__=='__main__':main()
