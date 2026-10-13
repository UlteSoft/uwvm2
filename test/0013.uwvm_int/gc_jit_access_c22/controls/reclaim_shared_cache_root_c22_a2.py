from pathlib import Path
import os,sys,json,time,resource
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
assert os.geteuid()==0 and Path('/proc/sys/kernel/random/boot_id').read_text().strip()==N.BOOT
assert N.unique_groups()==[str(N.C)] and (N.C/'memory.max').read_text().strip()==str(64<<30) and (N.C/'memory.swap.max').read_text().strip()=='0'
(N.C/'cgroup.procs').write_text(str(os.getpid()));os.sched_setaffinity(0,{20});resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_FSIZE,(128<<20,128<<20))
p=R/'source/typed-store-c21-cache-reclaim-proposal-a1.json';a=json.loads((R/'source/typed-store-c21-cache-reclaim-authority-a1.json').read_text());d=json.loads(p.read_text());assert a['user_approved'] and not a['executed'] and a['proposal_sha']==N.sha(p)
assert d['path']==str(N.C/'memory.reclaim') and d['payload']=='12884901888 swappiness=0\n'
f=json.loads((R/'source/c21-authorized-shared-cache-reclaim-used-by-c22-a1.json').read_text());c=json.loads((R/'source/c22-reclaim-before-open-failure-clarification-a1.json').read_text());assert f['error']['errno']==13 and not c['kernel_request_issued']
intent=R/'source/c22-authorized-root-cache-reclaim-a1-intent.json';out=R/'source/c22-authorized-root-cache-reclaim-a1.json';assert not intent.exists() and not out.exists()
def audit():
 m=int((N.C/'memory.current').read_text());h=N.host_available();g=N.unique_groups();assert g==[str(N.C)] and m<48<<30 and h>8<<30
 return dict(boot=N.BOOT,cgroup=N.CG,memory=m,host_available=h,unique64=g,memory_events=(N.C/'memory.events').read_text(),memory_stat=(N.C/'memory.stat').read_text(),scope='Recovery control only; native test/build admission40/32GiB unchanged.')
pre=audit();N.save(intent,dict(pre=pre,proposal_sha=N.sha(p),root_single_request=True,prior_attempt_failed_before_open=True));start=time.monotonic();error=None
try:
 with open(d['path'],'w') as stream:stream.write(d['payload']);stream.flush()
except OSError as e:error=dict(errno=e.errno,text=str(e))
N.save(out,dict(pre=pre,post=audit(),seconds=time.monotonic()-start,requested_bytes=12884901888,error=error,kernel_request_attempted=True,one_shot_consumed=True,retried_after_partial=False))
for file in [intent,out,N.BASE/'recovery'/intent.relative_to(R),N.BASE/'recovery'/out.relative_to(R)]:os.chown(file,1000,1000)
print('ROOT_RECLAIM_RESULT',out.read_text(),flush=True)
