import os,pathlib,json,time,subprocess,resource,fcntl,hashlib,shutil,signal
ROOT=pathlib.Path("/tmp/uwvm-gc-perf-20261011-r74")
BASE=pathlib.Path("/home/macromodel/Documents/uwvm-gc-perf-20261011-r74")
CG="/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope"
BOOT="e000195b-afa2-4a53-93ba-1fb8e28eac03"
C=pathlib.Path("/sys/fs/cgroup"+CG)
PMU_NAMES={"perf","vtune","amplxe-runss","amplxe-cl","sep","emon","sampling"}
def sha(p):
 h=hashlib.sha256()
 with open(p,"rb") as f:
  for b in iter(lambda:f.read(1<<20),b""):h.update(b)
 return h.hexdigest()
def persist(p):
 p=pathlib.Path(p)
 if p.is_file():
  d=BASE/"recovery"/p.relative_to(ROOT);d.parent.mkdir(parents=True,exist_ok=True)
  q=d.with_name(d.name+".partial");shutil.copy2(p,q);os.replace(q,d)
def latest(p,j):
 p=pathlib.Path(p);p.parent.mkdir(parents=True,exist_ok=True)
 t=p.with_name(p.name+".partial");t.write_text(json.dumps(j,indent=2)+"\n");os.replace(t,p);persist(p)
def save(p,j):
 p=pathlib.Path(p);assert not p.exists(),p;latest(p,j)
def perf_rate():
 return int(pathlib.Path("/proc/sys/kernel/perf_event_max_sample_rate").read_text())
def host_available():
 return int(next(x.split()[1] for x in pathlib.Path("/proc/meminfo").read_text().splitlines() if x.startswith("MemAvailable:")))*1024
def unique_groups():
 return [str(p.parent) for p in pathlib.Path("/sys/fs/cgroup").rglob("memory.max") if p.read_text().strip()==str(64<<30)]
def check(build=False):
 assert pathlib.Path("/proc/sys/kernel/random/boot_id").read_text().strip()==BOOT
 assert pathlib.Path("/proc/self/cgroup").read_text().strip()=="0::"+CG
 assert (C/"memory.max").read_text().strip()==str(64<<30)
 assert (C/"memory.swap.max").read_text().strip()=="0"
 g=unique_groups();assert g==[str(C)],("unique-64g",g)
 m=int((C/"memory.current").read_text());assert m<(32 if build else 40)<<30,("memory-admission",m)
 a=host_available();assert a>8<<30,("host-mem-available",a)
 assert shutil.disk_usage(ROOT).free>2<<30 and shutil.disk_usage(BASE).free>25<<30
 return {"boot":BOOT,"cgroup":CG,"memory":m,"host_available":a,"unique64":g,"affinity":sorted(os.sched_getaffinity(0)),"memory_events":(C/"memory.events").read_text()}
def processes(pgid=None):
 owned=[];foreign=[]
 for p in pathlib.Path("/proc").iterdir():
  if not p.name.isdigit():continue
  try:
   s=(p/"stat").read_text();rest=s[s.rfind(")")+2:].split();group=int(rest[2])
   comm=(p/"comm").read_text().strip()
   if pgid is not None and group==pgid:
    owned.append({"pid":int(p.name),"comm":comm,"rss":int((p/"statm").read_text().split()[1])*os.sysconf("SC_PAGE_SIZE")})
   elif comm in PMU_NAMES:foreign.append({"pid":int(p.name),"comm":comm,"pgid":group})
  except (OSError,ValueError,IndexError):continue
 return owned,foreign
class lease:
 def __enter__(self):
  self.f=open(BASE/"control/native-task.lock","a+");fcntl.flock(self.f,fcntl.LOCK_EX|fcntl.LOCK_NB);check();return self
 def __exit__(self,*a):fcntl.flock(self.f,fcntl.LOCK_UN);self.f.close()
def signal_own(p,fd,sig):
 try:signal.pidfd_send_signal(fd,sig)
 except ProcessLookupError:pass
 try:os.killpg(p.pid,sig)
 except ProcessLookupError:pass
def run(label,argv,cpu=2,timeout=120,build=False,env=None,cwd=None,pmu=False):
 pre=check(build);E=ROOT/"evidence";E.mkdir(exist_ok=True);receipt=E/(label+".json")
 assert not receipt.exists();out=E/(label+".stdout");err=E/(label+".stderr")
 assert not out.exists() and not err.exists(),label
 pmulock=None
 if pmu:
  _,foreign=processes();assert not foreign,("foreign-PMU-collector",foreign)
  pmulock=open("/tmp/uwvm-shared-pmu-"+BOOT+".lock","a+");fcntl.flock(pmulock,fcntl.LOCK_EX|fcntl.LOCK_NB)
 def setup():
  os.sched_setaffinity(0,{cpu});resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_FSIZE,(128<<20,128<<20))
  j={"pid":os.getpid(),"pgid":os.getpgrp(),"affinity":sorted(os.sched_getaffinity(0)),"cgroup":pathlib.Path("/proc/self/cgroup").read_text().strip()}
  (E/(label+".preexec.json")).write_text(json.dumps(j))
  assert j["affinity"]==[cpu] and j["cgroup"]=="0::"+CG
 start=time.monotonic();initial_rate=perf_rate();reason=None;error=None;recent=[];lastslow=0;peakrss=0;peakcg=0;minavail=host_available()
 with open(out,"wb") as fo,open(err,"wb") as fe:
  p=subprocess.Popen([str(a) for a in argv],stdout=fo,stderr=fe,start_new_session=True,preexec_fn=setup,env=env,cwd=cwd)
  fd=os.pidfd_open(p.pid)
  save(E/(label+".started.json"),{"argv":list(map(str,argv)),"pid":p.pid,"pgid":p.pid,"time":time.time(),"pre":pre,"pmu":pmu})
  persist(E/(label+".preexec.json"))
  try:
   while p.poll() is None:
    now=time.monotonic()
    try:
     m=int((C/"memory.current").read_text());a=host_available();peakcg=max(peakcg,m);minavail=min(minavail,a)
     if now-start>timeout:reason="deadline"
     elif m>48<<30:reason="cgroup-memory"
     elif a<8<<30:reason="host-memory"
     elif pmu and perf_rate()<initial_rate:reason="kernel-perf-throttled"
     elif out.stat().st_size+err.stat().st_size>32<<20:reason="logs"
     elif shutil.disk_usage(ROOT).free<2<<30 or shutil.disk_usage(BASE).free<25<<30:reason="disk"
     if now-lastslow>=1:
      g=unique_groups()
      if g!=[str(C)]:reason="multiple-64g-cgroups"
      owned,foreign=processes(p.pid);rss=sum(x["rss"] for x in owned);peakrss=max(peakrss,rss)
      if rss>16<<30:reason="aggregate-rss"
      if pmu and foreign:reason="foreign-PMU-collector"
      s={"time":time.time(),"elapsed":now-start,"cgroup_memory":m,"host_available":a,"unique64":g,"aggregate_rss":rss,"processes":owned,"foreign_collectors":foreign,"guard":reason}
      recent=(recent+[s])[-64:];latest(E/(label+".heartbeat.json"),{"samples":recent})
      for f in (out,err):persist(f)
      lastslow=now
     if reason:signal_own(p,fd,signal.SIGTERM);break
    except (OSError,ValueError) as e:reason="guard-observation-failed";error=str(e);signal_own(p,fd,signal.SIGTERM);break
    time.sleep(.1)
   try:rc=p.wait(timeout=5)
   except subprocess.TimeoutExpired:signal_own(p,fd,signal.SIGKILL);rc=p.wait()
  except BaseException:
   signal_own(p,fd,signal.SIGKILL);p.wait();raise
  finally:
   os.close(fd)
   if pmulock is not None:fcntl.flock(pmulock,fcntl.LOCK_UN);pmulock.close()
 j={"label":label,"argv":list(map(str,argv)),"cpu":cpu,"pid":p.pid,"pgid":p.pid,"returncode":rc,"seconds":time.monotonic()-start,"guard":reason,"guard_error":error,"pre":pre,"post_memory":int((C/"memory.current").read_text()),"peak_group_memory":peakcg,"peak_aggregate_rss":peakrss,"min_host_available":minavail,"stdout_sha":sha(out),"stderr_sha":sha(err),"pmu":pmu,"initial_perf_max_sample_rate":initial_rate,"final_perf_max_sample_rate":perf_rate()}
 save(receipt,j)
 for f in (out,err,E/(label+".preexec.json")):persist(f)
 assert rc==0 and reason is None,j
 return j
