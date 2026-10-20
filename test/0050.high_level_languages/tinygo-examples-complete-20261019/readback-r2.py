"""Inert independent readback of source-bound TinyGo example evidence.
No compiler, module, guest, profiler, debugger or archived program is executed.
"""
import argparse,collections,hashlib,json,os,pathlib,re,resource,time
G='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
def need(v,m):
 if not v:raise ValueError(m)
def identity(p):
 h=hashlib.sha256();n=0
 with p.open('rb')as f:
  while True:
   b=f.read(65536)
   if not b:break
   h.update(b);n+=len(b)
 return {'bytes':n,'sha256':h.hexdigest()}
def checked(p,row):
 actual=identity(p);need(actual=={k:row[k]for k in ['bytes','sha256']},'identity '+str(p));return actual
def imports(b):
 need(b[:8]==b'\0asm\1\0\0\0','Wasm header')
 class Cursor:
  def __init__(self,data):self.b=data;self.i=0
  def byte(self):
   need(self.i<len(self.b),'truncated binary');v=self.b[self.i];self.i+=1;return v
  def uint(self):
   n=0
   for shift in range(0,35,7):
    v=self.byte();n|=(v&127)<<shift
    if not v&128:return n
   raise ValueError('overlong u32')
  def take(self,n):
   need(n<=len(self.b)-self.i,'section bounds');v=self.b[self.i:self.i+n];self.i+=n;return v
  def name(self):return self.take(self.uint()).decode('utf-8')
 values={127:'i32',126:'i64',125:'f32',124:'f64',123:'v128',112:'funcref',111:'externref'}
 cur=Cursor(b[8:]);types=[];rows=[]
 while cur.i<len(cur.b):
  sid=cur.byte();s=Cursor(cur.take(cur.uint()))
  if sid==1:
   for _ in range(s.uint()):
    need(s.byte()==96,'expected function type')
    ps=[values[s.byte()]for _ in range(s.uint())];rs=[values[s.byte()]for _ in range(s.uint())];types.append((ps,rs))
  elif sid==2:
   for _ in range(s.uint()):
    module=s.name();name=s.name();kind=s.byte();need(kind==0,'unexpected nonfunction import')
    index=s.uint();need(index<len(types),'function type index')
    ps,rs=types[index];rows.append({'module':module,'name':name,'kind':kind,'params':ps,'results':rs})
  if sid in [1,2]:need(s.i==len(s.b),'consumed typed section')
 return rows
def readjson(p):return json.loads(p.read_bytes())
def logs(row,default):
 d=pathlib.Path(row.get('log_directory',str(default/row['label'])));answer={}
 need(row['complete_output']and row['passed']and row['error']is None,'closed passed command '+row['label'])
 for stream in ['stdout','stderr']:
  p=d/(stream+'.log');actual=identity(p);need(actual=={'bytes':row[stream+'_bytes'],'sha256':row[stream+'_sha256']},'complete log '+str(p));answer[stream]=p.read_bytes()
 for p in row['processes']:need(p['cgroup']=='0::'+G and p['pgrp']==row['pid'],'child original cgroup')
 return answer
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--stage',type=pathlib.Path,required=True);ap.add_argument('--source-stage',type=pathlib.Path);ap.add_argument('--producer',default='producer-result-r1.json');ap.add_argument('--ordinary',default='ordinary-result-r3.json');ap.add_argument('--debug',default='debug-r1-result.json');ap.add_argument('--allow-partial',action='store_true');ap.add_argument('--output',type=pathlib.Path,required=True);a=ap.parse_args()
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,512<<20));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 c=pathlib.Path('/sys/fs/cgroup'+G)
 need(pathlib.Path('/proc/self/cgroup').read_text().strip()=='0::'+G,'original cgroup')
 need((c.stat().st_dev,c.stat().st_ino)==(29,19075),'cgroup inode')
 need((c/'memory.max').read_text().strip()=='68719476736'and(c/'memory.swap.max').read_text().strip()=='0','original limit')
 before=(c/'memory.events').read_text();S=a.stage;SS=a.source_stage or S;src=SS/'source-r1';m=readjson(SS/'source-inputs-r1.json')
 result={'status':'RUNNING','scope':'Independent inert source, binary import and closed-log evidence readback. No new target execution, native delivery, full SDK, performance or architecture credit.','memory_events_before':before,'all15_complete':False,'new_performance_credit':False,'inputs':{},'source_files':len(m['members'])}
 for r in m['members']:checked(src/r['path'],r)
 p=readjson(S/a.producer);result['inputs'][a.producer]=identity(S/a.producer)
 full=p['status']=='ACTUAL_TINYGO135_SELF_CONTAINED_EXAMPLE_PRODUCERS_AND9_BACKEND_REFUSALS_PASSED'
 need(full or a.allow_partial and p['status']=='FAILED_RETAINED','producer status')
 products=p['artifacts'];need(len(products)==(135 if full else 121),'actual producer cardinality')
 need(p['source_manifest_sha256']==identity(SS/'source-inputs-r1.json')['sha256'],'compiler source manifest identity')
 for path,pin in p['tools'].items():checked(pathlib.Path(path),pin)
 schema=readjson(src/'sdk/abi/abi-v1.json')['modules'];keys=set();pure={'scheduling','sdk_codec_debug','codec_benchmark'}
 for r in products:
  key=tuple(r[k]for k in ['backend','profile','kind']);need(key not in keys,'unique product');keys.add(key)
  f=pathlib.Path(r['path']);checked(f,r);need(f.stat().st_size<16<<20,'bounded product')
  rows=imports(f.read_bytes());report=readjson(pathlib.Path(r['import_report']));need(rows==report['imports'],'actual independent import parser '+str(key))
  ros=[x for x in rows if x['module']!='wasi_snapshot_preview1']
  if r['kind']in pure:need(not ros and report['status']=='PURE_WASI_IMPORTS_NO_ROS_CREDIT','pure import classification')
  else:
   module='uwvm_'+r['backend']+'_v1';wanted={x['name']:([v['type']for v in x['params']],x['results'])for x in schema[module]['functions']}
   need(ros and report['sha256']==r['sha256'],'ROS report artifact identity')
   for x in ros:need(x['module']==module and(x['params'],x['results'])==wanted[x['name']],'actual ROS scalar contract')
   need(len({x['name']for x in ros})==len(ros),'unique ROS imports')
   if r['kind']=='coverage':need(len(ros)==(38 if r['backend']=='ros1'else 43)and report['required_complete_inventory'],'complete coverage imports')
 rows=[]
 for r in p.get('reused_commands',[]):
  if 'command'in r:
   row=dict(r['command']);row['log_directory']=r['log_directory']
  else:
   row=dict(r);row['log_directory']=str(SS/'build-runs-r1'/row['label'])
  rows.append(row)
 rows+=p['commands'];qualified=[]
 for r in rows:
  if not r['passed']:continue
  logs(r,(S/'producer-runs-r1'if full else SS/'build-runs-r2'));qualified.append(r)
 if full:
  need(len(qualified)==417 and len(p['refusals'])==9,'417 actual compile/validate/import/refusal/selection commands')
  need(all(x['passed']for x in p['refusals']),'actual refusals')
 result.update(producer_full=full,products=len(products),qualified_compiler_commands=len(qualified))
 ordinary_path=S/a.ordinary
 if ordinary_path.exists():
  o=readjson(ordinary_path);result['inputs'][a.ordinary]=identity(ordinary_path)
  partial_o=o['status']=='STOPPED_CAPACITY_WITH_COMPLETED_OBSERVATIONS';need(o['status']in ['ACTUAL_AVAILABLE121_TINYGO224_GUEST_RUNS102_ARGS18_PREFLIGHT_REFUSALS_PASSED','ACTUAL_TINYGO250_GUEST_EXECUTIONS120_ARGS20_MISSING_GATEWAY_PREFLIGHTS_PASSED']or a.allow_partial and partial_o,'ordinary status')
  cmds={r['label']:r for r in o['commands']};need(len(cmds)==len(o['commands']),'unique ordinary observations')
  for runtime in o['runtimes']:checked(pathlib.Path(runtime['path']),runtime)
  full_o=len(o['commands'])==390;need(len(o['commands'])==(34 if partial_o else 390 if full_o else 344),'ordinary command count');ordinary_logs=S/a.ordinary.replace('ordinary-partial-result-','ordinary-partial-runs-').replace('ordinary-result-','ordinary-runs-').removesuffix('.json')
  counts=collections.Counter()
  for w in o['workflows']:
   r=cmds[w['label']];v=logs(r,ordinary_logs);out=v['stdout'];err=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',v['stderr'])
   need(r['exit']==w['actual_exit']==w['expected_exit'],'actual workflow exit');counts[w['scope']]+=1
   if w['scope']=='missing-gateway':
    need(r['exit']==126 and not out and err==('uwvm: [error] ROS policy: gateway preflight failed for '+w['backend']+'/global/\n').encode()and not w['Guest_executed'],'pre-Guest startup refusal')
   elif w['kind']=='native_resources':need(r['exit']==0 and out==b'PASS_TINYGO_ROS_RESOURCES_DEFAULT_DENIED\n'and not err,'default denial positive')
   elif w['kind']=='scheduling':need(r['exit']==0 and out+err==b'owned deferred result 42\none local tick\n','exact local scheduling')
   elif w['kind']=='sdk_codec_debug':need(r['exit']==0 and not out and not err,'pure SDK codec')
   elif w['kind']=='codec_benchmark':need(r['exit']==0 and out==b'READY_TINYGO_SDK_CODEC\nDONE_TINYGO_SDK_CODEC 17 1267701127\n'and not err and r['owned_input_hex']=='4744','complete codec functional work')
   else:
    need(r['exit']==-4 and b'catch unreachable'in err and b'Call stack:'in err,'observable Guest trap')
    need((b'open error 13\n'if w['kind']=='raw'else b'roswasm: capability call failed (inspect Code and HasRequired)')in out+err,'exact checked error')
  invalid=[r for r in o['commands']if '-invalid-'in r['label']]
  for r in invalid:
   v=logs(r,ordinary_logs);need(r['exit']==2 and not v['stdout']and not v['stderr'],'argument refusal')
  need(counts=={'disabled':24 if partial_o else 250 if full_o else 224,'missing-gateway':2 if partial_o else 20 if full_o else 18},'separate Guest and preflight cardinalities')
  need(len(invalid)==(8 if partial_o else 120 if full_o else 102),'argument cardinality');need(len(o['commands'])==len(o['workflows'])+len(invalid),'every observed command classified')
  result.update(ordinary_full=full_o,ordinary_commands=len(cmds),ordinary_counts=dict(counts),argument_refusals=len(invalid),ordinary_stopped_for_capacity=partial_o,remaining_planned_commands=390-len(cmds))
 else:need(a.allow_partial,'ordinary required');result['ordinary_full']=False
 dpath=S/a.debug
 if dpath.exists():
  d=readjson(dpath);result['inputs'][a.debug]=identity(dpath);need(d['status']=='ACTUAL_TINYGO_FIVE_OPT_THREE_REPOSITORY_SOURCE_DEBUG_CAPABILITIES_RECORDED','debug status')
  need(len(d['sessions'])==15 and len(d['commands'])==15,'debug cardinality')
  for path,pin in d['preserved_inputs'].items():checked(pathlib.Path(path),pin)
  for row in d['commands']:
   root=S/a.debug.removesuffix('-result.json')/row['label'];raw=(root/'stdout.log').read_bytes()
   for stream in ['stdout','stderr']:need(identity(root/(stream+'.log'))=={'bytes':row[stream+'_bytes'],'sha256':row[stream+'_sha256']},'debug raw log')
   need(row['passed']and row['exit']==0 and row['complete_output']and row['ownership']['closed']and not row['ownership']['signals'],'normal debug actor closure')
   session=next(s for s in d['sessions']if row['label']==s['runtime']+'-opt'+s['profile']+'-'+s['backend']);need(session['normal_exit'],'normal Guest completion')
   cursor=0
   for action in session['actions']:
    reply=action['reply'].encode();position=raw.find(reply,cursor);need(position>=0,'recorded ordered debugger reply');cursor=position+len(reply)
   if session['profile']=='0':need(session['finite_full_value_workflow']and session['value']==13,'opt0 original source-value baseline')
  profiles={};need(len({(s['runtime'],s['profile'])for s in d['sessions']})==15,'unique repository/profile debug observations')
  for session in d['sessions']:
   profiles.setdefault(session['runtime'],set()).add(session['profile']);need(session['normal_exit'],'normal Guest outcome')
   need(any('guest exited: 0'in x['reply']or'Program exited normally.'in x['reply']for x in session['actions']),'actual Guest exit observed')
   reads=[x for x in session['actions']if x['command']is not None and x['command'].startswith('print ')]
   if session['value_available']:
    need(session['value']==13 and len(reads)>=2 and reads[0]==reads[1],'same-stop repeated complete source value')
    need(re.search(r'(?:, value=| = [iu]32=)13(?: |\r|\n)',reads[0]['reply'])is not None,'actual source value13')
   elif session['source_breakpoint']:
    need('error: source value unavailable'in session['value_unavailable_reason'],'explicit optimized value unavailability')
   else:need('error:'in session['source_breakpoint_refusal'],'explicit source breakpoint refusal')
  need(all(v=={'0','1','2','s','z'}for v in profiles.values())and len(profiles)==3,'complete5x3 DBG matrix')
  result.update(debug_sessions=15,debug_full_value_workflows=sum(s['finite_full_value_workflow']for s in d['sessions']),debug_all_profiles_full_value_workflow=d['all_profiles_full_value_workflow'])
 else:need(a.allow_partial,'debug required');result['debug_sessions']=0
 kpath=S/'kernel-result-r1.json';k=readjson(kpath);need(k['status']=='ACTUAL_SAME_SOURCE_NATIVE3_UWVM_LLVM20_WASMTIME5_CODEC_FUNCTIONAL_CASES_PASSED','kernel functional cohort');need(len(k['commands'])==30 and len(k['cases'])==28,'complete native/LLVM/reference case count')
 result['inputs']['kernel-result-r1.json']=identity(kpath)
 for path,pin in k['pins'].items():checked(pathlib.Path(path),pin)
 need(k['native_product']['source_root']==str(src),'native and TinyGo use identical source root')
 build=next(x for x in k['commands']if x['label']=='build-native-same-source');need(build['argv'][-1]=='./codec_benchmark'and build['cwd']==str(src/'sdk/examples/tinygo'),'actual native source compile')
 all_commands={x['label']:x for x in k['commands']};need(len(all_commands)==30,'unique kernel commands')
 for row in k['commands']:logs(row,S/'kernel-runs-r1');need(row['exit']==0,'closed functional command')
 def affine_seed(count):
  modulus=(1<<32)-1;mul=1664525;add=1013904223;accmul=1;accadd=0
  while count:
   if count&1:accmul=mul*accmul&modulus;accadd=(mul*accadd+add)&modulus
   add=(mul*add+add)&modulus;mul=mul*mul&modulus;count>>=1
  return(accmul*0x12345678+accadd)&modulus
 seen=set();engine_counts=collections.Counter();native_counts=set();llvm=set();wasmtime=set();seeds={}
 for case in k['cases']:
  need(case['label']not in seen,'unique functional case');seen.add(case['label']);row=all_commands[case['label']];out=logs(row,S/'kernel-runs-r1');n=case['iterations'];seed=affine_seed(n);seeds[n]=seed
  expected=('READY_TINYGO_SDK_CODEC\nDONE_TINYGO_SDK_CODEC '+str(n)+' '+str(seed)+'\n').encode()
  need(out['stdout']==expected and not out['stderr']and row['owned_input_hex']=='4744'and case['expected_final_seed']==seed,'independent affine seed/count and complete output')
  need(case['performance_qualification']is False and case['native_ROS_delivery']is False,'functional qualification boundary');engine_counts[case['engine']]+=1
  if case['engine']=='native-Go':native_counts.add(n);need(row['argv'][0]==k['native_product']['path'],'actual native product')
  elif case['engine']=='UWVM-LLVM':
   meta=case['metadata'];llvm.add((meta['repository'],meta['language_debug_selection'],case['Wasm_product']['profile']));need(n==65537 and'-Rllvm-cache-path'in row['argv']and row['argv'][row['argv'].index('-Rllvm-cache-path')+1]=='disable','explicit uncached LLVM functional run')
  elif case['engine']=='Wasmtime':
   wasmtime.add(case['Wasm_product']['profile']);need(n==65537 and'cache=n,parallel-compilation=n'in row['argv'],'reference cache/parallel settings')
  else:raise ValueError('unexpected functional engine')
 need(native_counts=={1,17,65537}and engine_counts=={'native-Go':3,'UWVM-LLVM':20,'Wasmtime':5},'engine cardinalities')
 expected_llvm={(repo,selection,profile)for repo,selection in [('uwvm2','all'),('uwvm2-stable','all'),('uwvm2-ros','all'),('uwvm2-ros','none')]for profile in ['0','1','2','s','z']}
 need(llvm==expected_llvm and wasmtime=={'0','1','2','s','z'},'all/none and five-optimization functional grid')
 result['kernel_functional']={'cases':28,'closed_commands':30,'engines':dict(engine_counts),'affine_seed_check':seeds,'native_binary':k['native_product'],'Wasmtime_version':k['Wasmtime_version'],'performance_credit':False,'native_ROS_delivery_credit':False,'unqualified_other_engines':k['unqualified_reference_engines']}
 association=readjson(SS/'source-current-association-r1.json');need(association['producer_source_code_changed']is False and len(association['members'])==65,'active producer source association');diff={x['path']for x in association['members']if not x['matches_frozen_producer_input']};need(diff==set(association['documentation_updates']),'only declared documentation changed');result['active_source_association']=identity(SS/'source-current-association-r1.json')
 result['memory_events_after']=(c/'memory.events').read_text();need('oom_kill 0\n'in result['memory_events_after']and'oom 0\n'in result['memory_events_after'],'no OOM observed')
 result['events_unchanged_during_reader']=before==result['memory_events_after']
 complete=full and result['ordinary_full']and result['debug_sessions']==15
 need(complete or a.allow_partial,'full matrix required')
 result['status']='INDEPENDENT_INERT_TINYGO_COMPLETE_EXAMPLE_MATRIX_READBACK_PASSED'if complete else 'INDEPENDENT_INERT_TINYGO_PARTIAL_EXAMPLE_READBACK_PASSED'
 result['resource_profiles']={'producer':p.get('resource_profile'), 'ordinary_fresh':o.get('fresh_resource_profile'), 'debug':d.get('resource_profile')if dpath.exists()else None};result['producer_backend_profiles']={b:{profile:sum(x['backend']==b and x['profile']==profile for x in products)for profile in ['0','1','2','s','z']}for b in ['ros1','ros2']}
 result['reader_identity']=identity(pathlib.Path(__file__));a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
