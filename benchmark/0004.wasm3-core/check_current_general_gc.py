#!/usr/bin/env python3
"""Pure synthetic parser/admission controls. No native/VM/performance execution."""
import argparse, ast, copy, hashlib, json, pathlib, types
P=pathlib.Path
path=P(__file__).with_name('run_current_general_gc.py'); source=path.read_text(); ast.parse(source)
m=types.ModuleType('uwvm_general_gc_pure_controls'); m.__file__=str(path)
exec(compile(source,str(path),'exec'),m.__dict__)
class Log:
 def __init__(self,text): self.text=text
 def read_text(self,**options): return self.text
owner='[llvm-jit-full] owning-source=yes pending-plan=native object-cache=disabled body-fallback=no\n'
times='Total WASM execution time: 0.5s.\nTotal process time: 0.6s.\n'
def fixture(family,phase,n):
 k={'mutable-struct':1,'reference-cycle':2,'numeric-array':1,'reference-array':3}[family]
 return dict(schema='uwvm-general-gc-fixture-v1',family=family,phase=phase,iterations=n,root_ring=1024,compact_numeric_eligible=False,
  wasm_sha256='1'*64,expected=dict(guest_planned_allocations=k*(n if phase=='allocate' else 1024),final_root_group_count=1024,
   final_reachable_objects=k*1024,return_checksum_u32=1,collection_count=None,reclaimed_count=None))
def test(phase,metrics,good,code=0,tail=owner+times):
 definition=fixture('mutable-struct',phase,2000000); plan={'fixtures':{'cell':{'definition':definition}}}
 result=m.semantic(plan,{'fixture':'cell','profile':m.PROFILE},Log(metrics+tail),code)
 assert result['passed']==good,result
 assert result['collector_qualified']==(good and phase=='allocate')
 assert result['mutation_lookup_qualified']==(good and phase=='mutate')
alloc='[gc-managed] allocations=2000000 attempts=488 collections=488 reclaimed=1997823 roots_requested=1 disabled=0 reason=0\n'
mutate='[gc-managed] allocations=1024 attempts=0 collections=0 reclaimed=0 roots_requested=1 disabled=0 reason=2\n'
test('allocate',alloc,True); test('mutate',mutate,True); negatives=0
for text in [alloc.replace('allocations=2000000','allocations=1999999'),alloc.replace('collections=488','collections=0'),
 alloc.replace('reclaimed=1997823','reclaimed=0'),alloc.replace('roots_requested=1','roots_requested=0'),
 alloc.replace('disabled=0','disabled=1'),alloc.replace('reason=0','reason=2'),'',alloc+alloc]:
 test('allocate',text,False); negatives+=1
for text in [mutate.replace('attempts=0','attempts=1'),mutate.replace('collections=0','collections=1'),
 mutate.replace('reclaimed=0','reclaimed=1'),mutate.replace('roots_requested=1','roots_requested=0'),
 mutate.replace('disabled=0','disabled=1'),mutate.replace('reason=2','reason=0'),mutate.replace('allocations=1024','allocations=1025')]:
 test('mutate',text,False); negatives+=1
for phase,text in [('allocate',alloc),('mutate',mutate)]:
 test(phase,text,False,code=1); test(phase,text,False,tail=owner); test(phase,text,False,tail=times)
 test(phase,text,False,tail=owner+times+times); negatives+=4
for family in m.FAMILIES:
 for phase in m.PHASES:
  definition=fixture(family,phase,2000000); m.fixture_contract(definition)
  for key,value in [('iterations',4000000),('root_ring',2048),('compact_numeric_eligible',True)]:
   bad=copy.deepcopy(definition); bad[key]=value
   try: m.fixture_contract(bad)
   except RuntimeError: negatives+=1
   else: raise AssertionError('Bad fixture admission accepted')
  for key,value in [('guest_planned_allocations',1),('collection_count',488),('final_root_group_count',2048)]:
   bad=copy.deepcopy(definition); bad['expected'][key]=value
   try: m.fixture_contract(bad)
   except RuntimeError: negatives+=1
   else: raise AssertionError('Bad planned/actual count admission accepted')
# Actual immutable protocol code remains exactly unchanged; these are file
# integrity checks, not a native ownership/PMU pass on this machine.
for name,expected in [('run_current_host_pcore_hw_counting.py',m.HOST_SHA),
 ('run_current_pcore_hw_counting.py',m.HW_SHA),('run_current_pcore_diagnostic.py',m.BASE_SHA)]:
 assert hashlib.sha256(path.with_name(name).read_bytes()).hexdigest()==expected
# Token controls are synthetic, while the optional actual response check
# verifies the original bytes supplied by the keeper. Neither compiles code.
m.response_tokens_contract(['-O3','-DNDEBUG','-Xclang','-fno-pch-timestamp'])
response_negatives=0
for tokens in [['-D'],['-D','NDEBUG'],['-DUWVM_EXPERIMENTAL_COMPACT_NUMERIC=0'],['-DNDEBUG=0'],
 ['-UFOO'],['-B/other/compiler'],['@nested.rsp'],['/DUWVM_EXPERIMENTAL_COMPACT_NUMERIC=1'],
 ['/UFOO'],['-Xpreprocessor'],['-Xclang'],['-Xclang','-fexceptions']]:
 try: m.response_tokens_contract(tokens)
 except RuntimeError: response_negatives+=1
 else: raise AssertionError('Response override accepted: '+str(tokens))
for data in (b'',b'-DNDEBUG\n',b'x'*m.RESPONSE_BYTES):
 try: m.response_contract(data)
 except RuntimeError: response_negatives+=1
 else: raise AssertionError('Unpinned response bytes accepted')
parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('--actual-response',type=P)
args=parser.parse_args(); actual_response_verified=False
if args.actual_response is not None:
 m.response_contract(args.actual_response.read_bytes()); actual_response_verified=True
assert m.short_identity_observation(True,0,34_000_000,[]) is True
short_identity_negatives=0
for values in [(False,0,34_000_000,[]),(True,None,34_000_000,[]),(True,1,34_000_000,[]),
 (True,0,None,[]),(True,0,0,[]),(True,0,100_000_000,[]),(True,0,200_000_000,[]),
 (True,0,34_000_000,['Observed executable/argv mismatch']),
 (True,0,34_000_000,['Observed cgroup/roster/identity violation'])]:
 assert m.short_identity_observation(*values) is False
 short_identity_negatives+=1
assert m.plain_role({'profile':'wasmtime49/copying'})=='reference'
assert m.plain_role({'profile':m.PROFILE})=='guest'
assert m.plain_role({'profile':'wasmtime49/other'})=='guest'
parent=123; pid=456; cg='0::/synthetic-owned-scope\n'
actual=dict(pid=pid,birth=789,ppid=parent,pgid=pid,uid=[1000]*4,cgroup=cg,cpus='0',
            argv=['/engine','fixture'],executable='/engine')
entry=dict(role='guest',process=types.SimpleNamespace(pid=pid),birth=789,pidfd=4,cpu='0',
           exec_argv=[b'/engine',b'fixture'],expected_exe=P('/engine'),actual_exec=actual,reaped=False)
row=dict(pid=pid,birth=789,ppid=parent,pgid=pid,uid=[1000]*4,cgroup=cg,cpus='0',argv=[],rss=0,state='R')
def stat_text(row,nonzero=None):
 fields=['0']*49; fields[0]=row['state']; fields[1]=str(row['ppid']); fields[2]=str(row['pgid']);fields[19]=str(row['birth'])
 if nonzero is not None: fields[nonzero]='1'
 return str(row['pid'])+' (synthetic) '+' '.join(fields)+'\n'
stat=m.actual_mm_stat(stat_text(row))
assert m.clear_mm_retirement_candidate(entry,row,stat,cg,parent)
terminal_negative_controls=0
for key,value in [('pid',457),('birth',790),('ppid',124),('pgid',457),('uid',[0]*4),
 ('cgroup','0::/elsewhere\n'),('cpus','16'),('argv',[b'/changed']),('rss',1)]:
 changed=copy.deepcopy(row);changed[key]=value
 assert not m.clear_mm_retirement_candidate(entry,changed,stat,cg,parent);terminal_negative_controls+=1
for index in (20,21,23,24,25,42,43,44,45,46,47,48):
 assert not m.clear_mm_retirement_candidate(entry,row,m.actual_mm_stat(stat_text(row,index)),cg,parent);terminal_negative_controls+=1
for key,value in [('role','perf'),('pidfd',None),('pidfd',-1),('actual_exec',None),('cpu','16')]:
 changed=copy.deepcopy(entry);changed[key]=value
 assert not m.clear_mm_retirement_candidate(changed,row,stat,cg,parent);terminal_negative_controls+=1
for key,value in [('pid',457),('birth',790),('ppid',124),('pgid',457),('uid',[0]*4),
 ('cgroup','0::/elsewhere\n'),('cpus','16'),('argv',['/changed']),('executable','/changed')]:
 changed=copy.deepcopy(entry);changed['actual_exec'][key]=value
 assert not m.clear_mm_retirement_candidate(changed,row,stat,cg,parent);terminal_negative_controls+=1
for key,value in [('pid',457),('birth',790),('ppid',124),('pgid',457)]:
 changed=copy.deepcopy(stat);changed[key]=value
 assert not m.clear_mm_retirement_candidate(entry,row,changed,cg,parent);terminal_negative_controls+=1
for text in ('',str(pid)+' (truncated) R 1 2\n'):
 try: m.actual_mm_stat(text)
 except RuntimeError: terminal_negative_controls+=1
 else: raise AssertionError('Incomplete stat accepted')
# Pure in-process fake observations exercise the adapter's terminal control
# flow. No processes/threads/PIDFDs/proc files are created or read by this test;
# these are never native ownership, cgroup or hardware qualifications.
def terminal_flow(case):
 saved={key:getattr(m,key) for key in ('P','os','time','select','save')}; e=copy.deepcopy(entry); r=copy.deepcopy(row)
 records=[]; clock=[0]; phase=[0]; last=[r]; seen=[]
 class FakeStat:
  def __truediv__(self,value):return self
  def read_text(self):
   if phase[0]>1 and case in ('absent','absent-not-ready'):raise FileNotFoundError(2,'synthetic missing','/proc/456/stat')
   return stat_text(last[0],20 if phase[0]>1 and case=='mm-returned' else None)
 def now():clock[0]+=100;return clock[0]
 def ident(observed_pid):
  assert observed_pid==pid;phase[0]+=1
  current=copy.deepcopy(r)
  if phase[0]>1:
   if case in ('absent','absent-not-ready'):raise FileNotFoundError(2,'synthetic missing','/proc/456/stat')
   if case in ('Z','Z-not-ready','fd-changed'):current['state']='Z'
   if case=='argv-changed':current['argv']=[b'/changed']
   if case=='birth-changed':current['birth']+=1
   if case=='fd-changed':e['pidfd']=5
  last[0]=current;return current
 def json_ident(current):
  result=dict(current);result['argv']=[value.decode() for value in result['argv']];return result
 class FakeGuard:
  def __init__(self,b,admission):self.b=b;self.cg=cg;self.owned=[e]
  def check(self):
   try:current=self.b.ident(pid)
   except FileNotFoundError:seen.append('actual missing');return
   seen.append(current)
   if current['state']!='Z':raise RuntimeError('Owned command changed')
 try:
  m.P=lambda value:FakeStat();m.os=types.SimpleNamespace(getpid=lambda:parent,fsdecode=lambda v:v.decode() if isinstance(v,bytes) else v)
  m.time=types.SimpleNamespace(perf_counter_ns=now,sleep=lambda seconds:clock.__setitem__(0,clock[0]+int(seconds*1e9)))
  m.select=types.SimpleNamespace(select=lambda reads,*args:([reads[0]] if phase[0]>1 and case not in ('timeout','Z-not-ready','absent-not-ready') else [],[],[]))
  m.save=lambda path,value:records.append(copy.deepcopy(value))
  base=types.SimpleNamespace(ident=ident,json_ident=json_ident)
  guard=m.observed_guard(types.SimpleNamespace(HostGuard=FakeGuard),base,{},P('/synthetic'))
  try:guard.check()
  except RuntimeError as error:return False,str(error),records,seen
  return True,None,records,seen
 finally:
  for key,value in saved.items():setattr(m,key,value)
for case in ('Z','absent'):
 passed,error,records,seen=terminal_flow(case);assert passed,(case,error)
 audit=next(value['observation'] for value in records if 'observation' in value)
 assert audit['terminal_confirmed'] and audit['finished_ns']<=audit['deadline_ns']
 assert seen[-1]=='actual missing' if case=='absent' else seen[-1]['state']=='Z'
terminal_flow_negative_controls=0
for case in ('timeout','Z-not-ready','absent-not-ready','argv-changed','birth-changed','mm-returned','fd-changed'):
 passed,error,records,seen=terminal_flow(case);assert not passed,(case,error)
 assert not any(value.get('observation',{}).get('terminal_confirmed') for value in records)
 terminal_flow_negative_controls+=1
print(json.dumps(dict(source_only=True,AST_passed=True,native_vm_or_perf_execution=False,parser_positive_controls=2,
 fixture_positive_controls=8,negative_controls=negatives,response_negative_controls=response_negatives,
 short_identity_negative_controls=short_identity_negatives,actual_response_verified=actual_response_verified,
 terminal_candidate_negative_controls=terminal_negative_controls,terminal_flow_negative_controls=terminal_flow_negative_controls,
 immutable_protocol_hashes_match=True,runner_sha256=m.digest(path)),sort_keys=True))
