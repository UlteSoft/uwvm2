#!/usr/bin/env python3
"""Bound one SSH/Linux build or execution cohort with retained process ownership.

No cgroup/affinity/mount/trust configuration is changed. Whole-cgroup memory
includes page cache/tmpfs; aggregate RSS is independently bounded. Every workload remains within the human-authorized test scope.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import resource
import shutil
import subprocess
import sys
import time


def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()


def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--report',type=Path,required=True);ap.add_argument('--cwd',type=Path,required=True)
 ap.add_argument('--timeout',type=int,default=600);ap.add_argument('--rss-mib',type=int,default=8192)
 ap.add_argument('--disk-root',type=Path,default=Path('/home/macromodel/Documents/uwvm-test-261018'))
 ap.add_argument('--working-set-limit-mib',type=int,help='Optional cache-aware limit; requires the actual 64GiB hard cap and at least 8GiB working-set reserve')
 ap.add_argument('--cg-limit-mib',type=int,default=24576);ap.add_argument('command',nargs=argparse.REMAINDER)
 a=ap.parse_args()
 if a.command[:1]==['--']:a.command=a.command[1:]
 if not a.command:ap.error('missing command')
 if os.uname().sysname!='Linux':raise RuntimeError('candidate workloads require authorized SSH Linux')
 resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 address_space_limit=list(resource.getrlimit(resource.RLIMIT_AS))
 if address_space_limit[0]!=resource.RLIM_INFINITY and address_space_limit[0]<16*1024**3:raise RuntimeError('VM address-space soft limit must be unlimited or at least 16GiB; RSS is bounded independently')
 root=Path(__file__).resolve().parents[2]
 loader=importlib.util.spec_from_file_location('wsea_cgroup_probe',root/'tests/ros_cgroup/probe.py');probe=importlib.util.module_from_spec(loader);loader.loader.exec_module(probe)
 sys.path.insert(0,str(root/'test/0050.high_level_languages'))
 from owned_process import remember_process,wait_process,terminate_group,owned_process_record
 cg=Path('/sys/fs/cgroup/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope')
 pins={'expected_leaf':str(cg),'expected_device':29,'expected_inode':19075,'expected_boot':'51c27aa1-d709-4a06-ba16-2ac3003fbcbd'}
 before=probe.probe(**pins)
 if before['status']!='PASS' or not os.sched_getaffinity(0) or not set(os.sched_getaffinity(0))<={16,17}:raise RuntimeError('original cgroup and CPU16,17 required')
 ancestor_identities=[{'path':row['path'],'device':Path(row['path']).stat().st_dev,'inode':Path(row['path']).stat().st_ino} for row in before['cgroup_ancestors']]
 if not 128<a.cg_limit_mib<=65536 or not 128<a.rss_mib<=8192:raise RuntimeError('resource limit outside authorized maximum')
 if a.working_set_limit_mib is not None:
  if a.cg_limit_mib!=65536 or not 128<a.working_set_limit_mib<=a.cg_limit_mib-8192 or before['cgroup_ancestors'][0]['memory.max']!=str(64*1024**3) or before['cgroup_ancestors'][0]['memory.swap.max']!='0':raise RuntimeError('cache-aware guard requires the unchanged 64GiB hard cap, no swap and 8GiB working-set reserve')
 a.report.parent.mkdir(mode=0o700,parents=True,exist_ok=True)
 log=a.report.with_suffix('.log')
 r={'status':'FAIL','argv':a.command,'cwd':str(a.cwd),'start_utc_seconds':time.time(),'cgroup_before':before,'ancestor_identities_before':ancestor_identities,'baseline_cgroup_memory_current':int((cg/'memory.current').read_text()),
    'limits':{'address_space_rlimit':address_space_limit,'aggregate_rss_bytes':a.rss_mib*1048576,'whole_cgroup_bytes':a.cg_limit_mib*1048576,'reserve_bytes':128*1048576,'disk_floor_bytes':3*1024**3},
    'peak_aggregate_rss_bytes':0,'peak_cgroup_memory_current':0,'min_disk_free_bytes':None,'driver_sha256':sha(__file__),
    'disk_root':str(a.disk_root.resolve(strict=True)),'rootfs_free_before':shutil.disk_usage('/').free,
    'ownership_source_sha256':sha(root/'test/0050.high_level_languages/owned_process.py'),'processes':{}}
 if a.working_set_limit_mib is not None:
  r['limits']['working_set_bytes']=a.working_set_limit_mib*1048576
  r['limits']['working_set_measurement']='memory.current minus memory.stat inactive_file; hard cgroup limit unchanged'
  r['peak_working_set_bytes']=0
 child=None
 def guard():
  disk=shutil.disk_usage(a.disk_root).free
  r['min_disk_free_bytes']=disk if r['min_disk_free_bytes'] is None else min(r['min_disk_free_bytes'],disk)
  if disk<3*1024**3:raise RuntimeError('actual workload filesystem below 3GiB floor')
  current=int((cg/'memory.current').read_text());r['peak_cgroup_memory_current']=max(r['peak_cgroup_memory_current'],current)
  if a.working_set_limit_mib is None:
   if current>(a.cg_limit_mib-128)*1048576:raise RuntimeError('whole-cgroup memory ceiling approached')
  else:
   stats=dict(line.split() for line in (cg/'memory.stat').read_text().splitlines())
   inactive=max(0,min(current,int(stats['inactive_file'])))
   working=current-inactive
   r['peak_working_set_bytes']=max(r['peak_working_set_bytes'],working)
   if working>(a.working_set_limit_mib-128)*1048576 or current>a.cg_limit_mib*1048576:raise RuntimeError('whole-cgroup memory ceiling approached')
  if log.exists() and log.stat().st_size>16*1048576:raise RuntimeError('workload output exceeds 16MiB')
  aggregate=0
  if child is not None:
   for member in owned_process_record(child)['members']:
    ident=member['identity'];pid=ident['pid'];directory=Path('/proc')/str(pid)
    try:
     stat=(directory/'stat').read_text().rpartition(')')[2].split()
     if int(stat[19])!=ident['start']:continue
     fields=dict(line.split(':',1) for line in (directory/'status').read_text().splitlines() if ':' in line)
     rss=int(fields.get('VmRSS','0 kB').split()[0])*1024;aggregate+=rss
     affinity=sorted(os.sched_getaffinity(pid));membership=(directory/'cgroup').read_text().splitlines()
     if not set(affinity)<={16,17} or membership!=before['membership']:raise RuntimeError('owned child left admitted cgroup/affinity')
     key=str(pid)+':'+str(ident['start']);row=r['processes'].setdefault(key,{'identity':ident,'affinity':affinity,'membership':membership,'max_rss_bytes':0,'cmdline':(directory/'cmdline').read_bytes().replace(b'\0',b' ').decode(errors='replace')})
     row['max_rss_bytes']=max(row['max_rss_bytes'],rss)
    except (FileNotFoundError,ProcessLookupError):continue
  r['peak_aggregate_rss_bytes']=max(r['peak_aggregate_rss_bytes'],aggregate)
  if aggregate>(a.rss_mib-128)*1048576:raise RuntimeError('aggregate owned RSS ceiling approached')
 try:
  guard()
  with log.open('wb') as out:
   child=subprocess.Popen(a.command,cwd=a.cwd,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
   remember_process(child)
   code=wait_process(child,a.timeout,observe=guard)
   r['returncode']=code;r['status']='PASS' if code==0 else 'FAIL'
 except Exception as e:r['error']=str(e)
 finally:
  if child is not None:
   try:terminate_group(child,term_timeout=2,kill_timeout=3)
   except Exception as e:r['cleanup_error']=str(e);r['status']='FAIL'
   r['owned_processes']=owned_process_record(child)
   r['owned_chain_closed']=bool(r['owned_processes'] and r['owned_processes']['closed'])
   if not r['owned_chain_closed']:r['status']='FAIL'
  else:r['owned_chain_closed']=True
  r['rootfs_free_after']=shutil.disk_usage('/').free
  after=probe.probe(**pins);r['cgroup_after']=after
  if after['status']!='PASS' or after['cgroup_incarnation']!=before['cgroup_incarnation']:r['status']='FAIL'
  after_ancestors=after.get('cgroup_ancestors',[])
  r['ancestor_identities_after']=[{'path':row['path'],'device':Path(row['path']).stat().st_dev,'inode':Path(row['path']).stat().st_ino} for row in after_ancestors]
  if len(before['cgroup_ancestors'])!=len(after_ancestors) or r['ancestor_identities_after']!=ancestor_identities:r['status']='FAIL'
  for old,new in zip(before['cgroup_ancestors'],after_ancestors):
   if old['path']!=new['path'] or old.get('memory.max')!=new.get('memory.max') or old.get('memory.swap.max')!=new.get('memory.swap.max') or old.get('controllerless_hierarchy_root')!=new.get('controllerless_hierarchy_root'):r['status']='FAIL'
   if old.get('memory.events') is not None:
    oldevents=dict(line.split() for line in old['memory.events'].splitlines());newevents=dict(line.split() for line in new['memory.events'].splitlines())
    if any(oldevents.get(k)!=newevents.get(k) for k in ('oom','oom_kill')):r['status']='FAIL'
  r['end_utc_seconds']=time.time();r['log_sha256']=sha(log) if log.exists() else None
  a.report.write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps({k:r.get(k) for k in ('status','returncode','error','cleanup_error','owned_chain_closed','peak_aggregate_rss_bytes','peak_cgroup_memory_current','min_disk_free_bytes')}))
 return 0 if r['status']=='PASS' else 1

if __name__=='__main__':sys.exit(main())
