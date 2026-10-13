#!/usr/bin/env python3
"""Small EH/memory64 diagnostic using the frozen long-run ownership guard.

Never ranks engines. Temperature is observation only; frequency and all hard
cgroup/CPU/PIDFD/deadline constraints remain visible and enforced.
"""
import argparse,decimal,hashlib,json,pathlib,re,resource,signal,types
P=pathlib.Path
BASE_SHA='0ab44317fea5c7cddcf236d79be232e24237386d17795684fa2f0fa51c487f89'
FIXTURES={
 'plain_normal':{'family':'eh','iterations':200000000,'checksum':1231817216,'catches':0,'sha256':'03c56518a1d2ed9d0149791e3a241ddff773395e51bd48aeb67a9160d5bdba01'},
 'eh_normal':{'family':'eh','iterations':200000000,'checksum':1231817216,'catches':0,'sha256':'f2b9df696835bdd0f759885b77862e4a3de56b506a96f3edc41e54b20a63be8a'},
 'eh_throws':{'family':'eh','iterations':8000000,'checksum':2464256,'catches':500000,'sha256':'560243a56c6b29dbe0337548fa96626db2c168e56ea69d88208cffa4dfc48560'},
 'memory64-random-store-5000000':{'family':'memory64','iterations':5000000,'sha256':'5ed7090a0cbafe866be3eae0bbfc76ceeed6b1cb607dc0bc6392f4a829269fd9'},
 'memory64-random-store-50000000':{'family':'memory64','iterations':50000000,'sha256':'322641f90c7b80225e942031988918da63e925e725a72dd2dd1f6c4b28cb36cc'},
}
def semantic_receipt(item,log,exit_code):
 definition=FIXTURES[item['fixture']]
 text=re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]','',log.read_text(errors='replace'))
 receipt={**definition,'exit_zero':exit_code==0,'passed':False,'failures':[],'formal_acceptance':False,
  'checksum_proof':'Exit zero from the pinned self-checking _start; checksum/catches are fixture properties, not printed runtime counters.'}
 if item['profile'].startswith('wasmtime49/'):
  receipt['passed']=exit_code==0;return receipt
 for label in ('gc-managed','gc-sealed-experimental','gc-page-experimental'):
  matches=re.findall(r'^\['+re.escape(label)+r'\] ([^\r\n]+)',text,re.M)
  if len(matches)==1:receipt[label]={k:int(v) for k,v in re.findall(r'([a-z_]+)=([0-9]+)',matches[0])}
 # EH and memory fixtures do not need a GC allocation receipt to pass.
 owners=re.findall(r'\[llvm-jit-full\] owning-source=yes pending-plan=([^ ]+) object-cache=disabled body-fallback=no',text)
 if len(owners)!=1 or owners[0] not in ('native','r2-phase'):
  receipt['failures'].append('Missing native-owned/cache-disabled/no-body-fallback receipt')
 else:
  receipt['observed_pending_plan']=owners[0]
  if item['profile'].endswith('/native-unwind') and owners[0]!='native':
   receipt['failures'].append('Explicit native-unwind unexpectedly used a pending plan')
 for field,label in (('guest_execution_ns','Total WASM execution time'),('whole_process_reported_ns','Total process time')):
  values=re.findall(re.escape(label)+r': ([0-9]+(?:\.[0-9]+)?)s\.',text)
  if len(values)==1:receipt[field]=int(decimal.Decimal(values[0])*decimal.Decimal(1000000000))
  else:receipt['failures'].append('Missing/unexpected '+label)
 receipt['passed']=exit_code==0 and not receipt['failures'];return receipt

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--plan',type=P,required=True);p.add_argument('--out',type=P,required=True)
 p.add_argument('--family',choices=('eh','memory64'),required=True)
 p.add_argument('--pairs',type=int,choices=(1,2),default=1);p.add_argument('--execute',action='store_true');a=p.parse_args()
 # Import only the fixed test runner's functions; its guarded main does not run.
 base_path=P(__file__).with_name('run_current_pcore_diagnostic.py')
 base_bytes=base_path.read_bytes()
 if hashlib.sha256(base_bytes).hexdigest()!=BASE_SHA:raise RuntimeError('Changed guard dependency; review and pin before running')
 base=types.ModuleType('uwvm_fixed_development_guard');base.__file__=str(base_path)
 exec(compile(base_bytes,str(base_path),'exec'),base.__dict__)
 base.require(base.digest(base_path)==BASE_SHA,'Guard dependency changed during loading')
 base.semantic_receipt=semantic_receipt
 plan_sha=base.digest(a.plan);runner_sha=base.digest(__file__);plan=json.loads(a.plan.read_text())
 base.require(base.digest(a.plan)==plan_sha,'Plan changed while reading')
 base.require(plan['schema']=='uwvm-current-pcore-gc-eh-plan-v1','Wrong fixed-plan schema')
 names=[name for name,definition in FIXTURES.items() if definition['family']==a.family]
 for name in names:
  actual=plan['fixtures'][name]
  base.require(all(actual[k]==v for k,v in FIXTURES[name].items()),'Fixture binding mismatch: '+name)
 base.require(not a.out.exists(),'New evidence path required');a.out.mkdir(parents=True)
 base.save(a.out/'plan.json',plan);(a.out/'runner.py').write_bytes(P(__file__).read_bytes());(a.out/'guard.py').write_bytes(base_path.read_bytes())
 base.require(a.execute,'Explicit --execute required')
 def cancel(signum,frame):raise KeyboardInterrupt('Controlled cancellation '+str(signum))
 signal.signal(signal.SIGTERM,cancel);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 rows=[];groups=[];complete=False;closure_before_ok=False;closure_after_ok=False;error=None
 try:
  base.closure(plan,a.out,'before');closure_before_ok=True
  # Guard checks the exact limits and identity before any guest is created.
  guard=base.Guard();guard.check()
  selected=[r for r in plan['commands'] if r['fixture'] in names]
  profiles=list(dict.fromkeys(r['profile'] for r in selected))
  products={product+'/'+trace+'/'+dispatch for product in ('ordinary','ros') for trace in ('instruction','unwind') for dispatch in (('auto','native-unwind') if a.family=='eh' else ('auto',))}
  expected=products|({'wasmtime49/native'})
  base.require(set(profiles)==expected,'Expected both products/traces, native EH controls and reference')
  base.require(len(selected)==len(names)*len(profiles),'Ambiguous/missing diagnostic commands')
  for pair in range(a.pairs):
   order=profiles[:] if pair%2==0 else profiles[::-1];pairrows=[]
   for profile in order:
    for name in (names if pair%2==0 else names[::-1]):
     item=next(r for r in selected if r['profile']==profile and r['fixture']==name)
     row=base.sample(item,str(pair)+'-'+name+'-'+profile.replace('/','-'),pair,a.out,guard)
     rows.append(row);pairrows.append(row)
     base.require(not row['hard_failures'],'Hard safety/ownership/deadline failure; stopped')
     base.require(row['semantic_receipt']['passed'],'Guest/diagnostic receipt failure; stopped')
   freqs=[r['median_run_frequency_khz'] for r in pairrows];quality=[]
   if None in freqs or min(freqs)<=0 or max(freqs)/min(freqs)-1>.1:quality.append('Pair frequency spread exceeds10% or missing')
   groups.append({'pair':pair,'profile_order':order,'quality_failures':quality,'formal_acceptance':False})
  base.closure(plan,a.out,'after');base.require(base.digest(a.plan)==plan_sha,'Plan changed during execution')
  base.require(base.digest(base_path)==BASE_SHA and base.digest(__file__)==runner_sha,'Runner/guard changed during execution')
  closure_after_ok=True;complete=True
 except BaseException as failure:error=type(failure).__name__+': '+str(failure);raise
 finally:
  base.save(a.out/'summary.json',{'schema':'uwvm-aux-development-diagnostic-v1','family':a.family,'runner_sha256':runner_sha,'guard_sha256':BASE_SHA,
   'plan_sha256':plan_sha,'source_ids':{k:v['source_id'] for k,v in plan['products'].items()},'rows':len(rows),'requested_pairs':a.pairs,'groups':groups,
   'complete':complete,'closure_before_ok':closure_before_ok,'closure_after_ok':closure_after_ok,'execution_error':error,
   'completed_guest_count':sum(r['exit']==0 for r in rows),'semantic_pass_count':sum(r['semantic_receipt']['passed'] for r in rows),
   'hard_failure_rows':sum(bool(r['hard_failures']) for r in rows),'quality_failure_rows':sum(bool(r['inconclusive_reasons']) for r in rows),
   'temperature_rejection_enabled':False,'diagnostic_disk_floor_bytes':1<<30,'formal_acceptance':False,
   'limitations':['Development attribution only; no engine ranking.','Short EH or memory controls may have insufficient active-frequency samples.','Retain host SMT observation and align product frequency by verbose guest markers.','Fixture catches/checksums are not dynamic throw/catch counters; GC logs do not cover every root/frame bookkeeping operation.']})
if __name__=='__main__':main()
