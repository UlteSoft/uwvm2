
"""Independent inert verification of the Zig native-resource continuation.
This reader starts no compiler, Runtime, Gateway, native peer or profiler.
"""
from pathlib import Path
import argparse,datetime,hashlib,json,os,re,resource
GROUP='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
def need(value,message):
 if not value:raise ValueError(message)
def ident(path):
 h=hashlib.sha256();n=0
 with path.open('rb')as f:
  first=os.fstat(f.fileno())
  while True:
   b=f.read(1<<20)
   if not b:break
   n+=len(b);h.update(b)
  last=os.fstat(f.fileno())
 need((first.st_dev,first.st_ino,first.st_size,first.st_mtime_ns,first.st_ctime_ns)==(last.st_dev,last.st_ino,last.st_size,last.st_mtime_ns,last.st_ctime_ns),'changed during read '+str(path))
 need(n==first.st_size,'short read');return{'bytes':n,'sha256':h.hexdigest()}
def read(path):return json.loads(path.read_bytes())
def import_modules(path):
 b=path.read_bytes();need(b[:8]==b'\0asm\1\0\0\0','Core module')
 def uint(pos,end):
  value=0;shift=0
  for _ in range(5):
   need(pos<end,'bounded ULEB');octet=b[pos];pos+=1;value|=(octet&127)<<shift
   if octet<128:return value,pos
   shift+=7
  raise ValueError('overlong ULEB')
 def name(pos,end):
  n,pos=uint(pos,end);need(n<=end-pos,'bounded name');return b[pos:pos+n].decode('utf8'),pos+n
 pos=8;modules=[];seen=False
 while pos<len(b):
  kind=b[pos];size,start=uint(pos+1,len(b));end=start+size;need(end<=len(b),'section bounds')
  if kind==2:
   need(not seen,'one Import section');seen=True;count,p=uint(start,end)
   for _ in range(count):
    module,p=name(p,end);field,p=name(p,end);need(p<end and b[p]==0,'focused pure modules have function imports only');_,p=uint(p+1,end);modules.append(module)
   need(p==end,'complete Import section')
  pos=end
 need(pos==len(b),'complete module');return modules
def check_binary_ros(path,schema,backend):
 b=path.read_bytes()
 def uint(pos,end):
  value=0;shift=0
  for _ in range(5):
   need(pos<end,'integer bound');octet=b[pos];pos+=1;value|=(octet&127)<<shift
   if octet<128:return value,pos
   shift+=7
  raise ValueError('integer width')
 def string(pos,end):
  n,pos=uint(pos,end);need(n<=end-pos,'name extent');return b[pos:pos+n].decode(),pos+n
 sections={};pos=8
 while pos<len(b):
  kind=b[pos];size,start=uint(pos+1,len(b));end=start+size;need(end<=len(b),'section extent')
  if kind in[1,2]:need(kind not in sections,'duplicate type/import');sections[kind]=(start,end)
  pos=end
 start,end=sections[1];count,p=uint(start,end);types=[];width={127:'i32',126:'i64',125:'f32',124:'f64',123:'v128'}
 for _ in range(count):
  need(p<end and b[p]==96,'plain producer function type');p+=1;vectors=[]
  for _ in range(2):
   n,p=uint(p,end);need(n<=end-p,'type vector');values=[]
   for _ in range(n):need(b[p]in width,'known producer value type');values.append(width[b[p]]);p+=1
   vectors.append(values)
  types.append(vectors)
 need(p==end,'complete Type section')
 start,end=sections[2];count,p=uint(start,end);actual=[]
 expected={f['name']:([r['type']for r in f['params']],f['results'])for f in schema['modules']['uwvm_'+backend+'_v1']['functions']}
 for _ in range(count):
  module,p=string(p,end);field,p=string(p,end);need(p<end and b[p]==0,'function imports only');index,p=uint(p+1,end);need(index<len(types),'valid type index')
  if module=='wasi_snapshot_preview1':continue
  need(module=='uwvm_'+backend+'_v1'and field in expected,'exact admitted backend/name')
  need(types[index]==list(expected[field]),'independent actual binary signature');actual.append(field)
 need(p==end and actual and len(set(actual))==len(actual),'complete unique reachable ROS imports')
 return actual
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--run-name',default='matrix-final-r1');args=p.parse_args();o=args.root
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,512<<20));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 cg=Path('/sys/fs/cgroup'+GROUP);need(Path('/proc/self/cgroup').read_text().strip()=='0::'+GROUP,'original Linux cgroup')
 need((cg.stat().st_dev,cg.stat().st_ino)==(29,19075),'cgroup inode')
 need((cg/'memory.max').read_text().strip()=='68719476736'and(cg/'memory.swap.max').read_text().strip()=='0','original limits')
 need(Path('/proc/sys/kernel/random/boot_id').read_text().strip()=='51c27aa1-d709-4a06-ba16-2ac3003fbcbd','original boot')
 events=(cg/'memory.events').read_text();results={};checked=[]
 sources={
  'native-build-result-r1.json':('ACTUAL_ZIG_NATIVE_RESOURCE_WITNESS_COMPILE_LINK_PASSED','native-build-runs-r1','native-build-r1.py'),
  'producer-result-r4.json':('ACTUAL_ZIG_32_WASI_PRODUCERS_VALIDATED','build-runs-r4','producer-r4.py'),
  'ordinary-result.json':('ACTUAL_ZIG_112_ORDINARY_WORKFLOWS_AND48_ARGUMENT_REFUSALS_PASSED','ordinary-runs','ordinary-r1.py'),
  args.run_name+'-result.json':('ACTUAL_ZIG_NATIVE_RESOURCES_AND_SELECTED_WRONG_VALUES_PASSED',args.run_name,'native-matrix-r3.py')}
 for name,(status,logs,driver)in sources.items():
  d=read(o/name);need(d['status']==status and not d.get('primary')and not d.get('finalization_errors'),name+' final status')
  need(d['before']['memory_events']==d['after']['memory_events']==events,name+' memory events')
  need(ident(o/driver)['sha256']==d['driver_sha256'],name+' executed driver')
  for r in d['commands']:
   expected=r.get('expected_exit',0)
   need(r['passed']and r['complete_output']and(r['exit']!=0 if expected=='clock-refusal'else r['exit']==expected),'actual expected result '+r['label'])
   for stream in['stdout','stderr']:
    need(ident(o/logs/r['label']/(stream+'.log'))=={'bytes':r[stream+'_bytes'],'sha256':r[stream+'_sha256']},'complete raw log')
  results[name]=d;checked.append({'result':name,'identity':ident(o/name),'commands':len(d['commands'])})
 producer=results['producer-result-r4.json'];ordinary=results['ordinary-result.json'];matrix=results[args.run_name+'-result.json'];build=results['native-build-result-r1.json'];base=o.parent/'root-language-go-sdk-20261019-a3'
 need(len(producer['artifacts'])==32,'32 source-built products')
 need({(a['backend'],a['profile'],a['kind'])for a in producer['artifacts']}=={(b,m,k)for b in['ros1','ros2']for m in['Debug','ReleaseSafe','ReleaseFast','ReleaseSmall']for k in['resources','checks','unit','telemetry']},'complete build matrix')
 for a in producer['artifacts']:
  need(ident(Path(a['path']))=={k:a[k]for k in['bytes','sha256']},'actual new Wasm')
  if a['kind']=='unit':
   need(set(import_modules(Path(a['path'])))<={'wasi_snapshot_preview1'},'pure unit has no ROS/native import');continue
  actual=check_binary_ros(Path(a['path']),read(o/'source-r3/sdk/abi/abi-v1.json'),a['backend'])
  r=read(Path(a['import_report']));need(r['status']=='BINARY_IMPORT_CONTRACT_PASS'and r['sha256']==a['sha256'],'actual binary import inspector')
  need(all(i['module']in['wasi_snapshot_preview1','uwvm_'+a['backend']+'_v1']for i in r['imports']),'selected backend only')
 for path,pin in producer['tools'].items():need(ident(Path(path))==pin,'producer tool changed')
 for row in read(o/'source-formatted-r4.json')['members']:
  need(ident(o/'source-r3'/row['path'])=={k:row[k]for k in['bytes','sha256']},'native scenes use exact compiler input bytes')
 need(len(ordinary['commands'])==160 and len(ordinary['target_workflows'])==112,'112 ordinary plus48 argument refusals')
 for row in ordinary['target_workflows']:
  text=(o/'ordinary-runs'/row['label']/'stdout.log').read_bytes();err=(o/'ordinary-runs'/row['label']/'stderr.log').read_bytes()
  if row['kind']=='resources':need(text==b'PASS_ZIG_ROS_RESOURCES_DEFAULT_DENIED\n','actual exact ROS resource default denial')
  elif row['kind']=='telemetry':need(text==b'PASS_ROS_DEFAULT_DENIED\n','actual original publisher default denial')
  elif row['kind']=='checks':need(len(text.splitlines())==9 and all(x.startswith(b'PASS_ZIG_SDK_')for x in text.splitlines()),'nine actual SDK check groups')
  else:need(row['kind']=='unit'and b'All 1 tests passed'in text+err,'actual pure SDK length test')
 for row in ordinary['commands']:
  if row['label'].startswith('arguments-'):need(row['exit']==2 and(o/'ordinary-runs'/row['label']/'stdout.log').read_bytes()==b'','exact argument refusal')
 cases=matrix['cases'];positive=[c for c in cases if c['control']=='positive'];negative=[c for c in cases if c['control']=='wrong-value']
 modes=['subscribe','trigger','set-bool','server','parameter']
 need(len(cases)==44 and len(positive)==40 and len(negative)==4 and len(matrix['commands'])==176,'native matrix cardinality')
 need({(c['runtime_mode'],c['producer_profile'],c['lesson'])for c in positive}=={(r,p,m)for r in['release','debug']for p in['Debug','ReleaseSafe','ReleaseFast','ReleaseSmall']for m in modes},'complete positive matrix')
 need({c['lesson']for c in negative}==set(modes)-{'server'},'four actual wrong values')
 need(not Path(matrix['scratch']).exists(),'private short socket directory retired')
 expected_roles={'subscribe':[('sub','topic.subscribe')],'trigger':[('call','service.call')],'set-bool':[('call','service.call')],'server':[('serve','service.advertise'),('respond','service.respond')],'parameter':[('query','param.get')]}
 expected_values={'subscribe':'bounded native Zig telemetry','trigger':'native-zig-ready','set-bool':'native-zig-enabled','server':'bounded zig wasm service','parameter':'bounded native Zig parameter'}
 wrong={'subscribe':'unexpected native Zig telemetry','trigger':'unexpected-native-zig-ready','set-bool':'native-zig-enabled','parameter':'unexpected native Zig parameter'}
 by_label={r['label']:r for r in matrix['commands']}
 for r in matrix['commands']:
  own=r['ownership'];need(own['closed']and not own['signals']and not own['retirement_errors']and all(not m['pidfd_retained']for m in own['members']),'exact actor clean retirement')
 for index,c in enumerate(cases):
  pos=c['control']=='positive';mode=c['lesson'];folder=o/args.run_name;worker=by_label['c'+str(index)+'-zig'];peer=by_label['c'+str(index)+'-peer'];gateway=by_label['c'+str(index)+'-gateway']
  need(worker['exit']==(0 if pos else 1)and peer['exit']==gateway['exit']==0,'actual native actor status')
  need(c['quiet_min_seconds']==2 and c['native_gateway_node_count_at_exit']==0,'selected native retirement witness')
  plan=read(folder/('case'+str(index))/'policy/policy.json')
  need(ident(folder/('case'+str(index))/'policy/policy.json')['sha256']==c['policy_json_sha256'],'policy output')
  need([(g['alias'],g['operation'])for g in plan['grants']]==expected_roles[mode],'exact granted roles')
  need('worker-pid '+str(worker['pid'])+'\n'in plan['Gateway_config_text']and'ungranted-telemetry'not in plan['Gateway_config_text'],'actual held PID / forbidden alias')
  config=folder/('case'+str(index))/'policy/gateway.conf'
  need(config.read_bytes()==plan['Gateway_config_text'].encode(),'exact generated native configuration')
  runtime=next(r for r in matrix['runtimes']if r['mode']==c['runtime_mode'])
  artifact=next(a for a in producer['artifacts']if a['backend']=='ros2'and a['profile']==c['producer_profile']and a['kind']=='resources')
  business=read(folder/('case'+str(index))/'worker-argv.json')
  expected=[runtime['path'],'--wasip1-global-grant','monotonic-clock','--wasip1-global-grant','realtime-clock',*plan['Worker_arguments'],'--wasm-set-main-module-name','zig_resources','--run',artifact['path'],mode]
  need(business==expected and worker['target_argv']==expected and worker['live_executable_after_ready']==runtime['path'],'exact observed Runtime / grants / target argv')
  need(gateway['argv'][-1]==str(config)and gateway['argv'][-2]==matrix['native_origin']['binary']['path'],'actual Gateway selected native configuration')
  for g in plan['grants']:
   if 'timeout-ms'in g['limits']:need(g['limits']['timeout-ms']=='3000','service/parameter deadline not relaxed')
  need('timeout-ms 15000\n'in plan['Gateway_config_text'],'explicit separate Gateway frame deadline')
  need(ident(folder/('case'+str(index))/'worker-argv.json')['sha256']==worker['target_argv_sha256'],'deferred actual exec argv')
  text=(folder/worker['label']/'stdout.log').read_text().splitlines();ready={'subscribe':'READY_ZIG_ROS_SUBSCRIPTION','trigger':'READY_ZIG_ROS_SERVICE_CLIENT','set-bool':'READY_ZIG_ROS_SERVICE_CLIENT','server':'READY_ZIG_ROS_SERVICE_SERVER','parameter':'READY_ZIG_ROS_PARAMETER'}[mode]
  idle='IDLE_ZIG_ROS_SUBSCRIPTION'if mode=='subscribe'else'IDLE_ZIG_ROS_SERVICE_SERVER';main=[line for line in text if line!=idle]
  delivered={'subscribe':'DELIVERED_ZIG_ROS_SUBSCRIPTION','trigger':'DELIVERED_ZIG_ROS_SERVICE_RESPONSE','set-bool':'DELIVERED_ZIG_ROS_SERVICE_RESPONSE','server':'REPLIED_ZIG_ROS_SERVICE','parameter':'DELIVERED_ZIG_ROS_PARAMETER'}[mode]
  need(main==([ready,delivered,'PASS_ZIG_ROS_RESOURCES']if pos else[ready]),'complete Guest protocol')
  errs=(folder/worker['label']/'stderr.log').read_text().splitlines();audit=[json.loads(s)for s in errs if s.startswith('{')];other=[s for s in errs if not s.startswith('{')]
  denials=[r for r in audit if r.get('event')=='unresolved-denial']
  need(len(denials)==1 and denials[0]['error']==13 and all(r.get('error')in(0,11,13)for r in audit),'ungranted alias and actual Host outcomes')
  need(other==([]if pos else['primary 22 cleanup-status 0']),'preserved exact semantic rejection / cleanup')
  protocol=[]
  for line in(folder/peer['label']/'stdout.log').read_text().splitlines():
   phase,rest=line.split('_NATIVE_ZIG_RESOURCE ',1);fields=dict(piece.split('=',1)for piece in rest.split(' '));protocol.append({'phase':phase,**fields})
   need(fields['namespace']==c['namespace']and fields['mode']==mode and fields['control']==c['control']and fields['gateway']=='zig_resource_gateway','actual native case identity')
   value=(expected_values if pos else wrong)[mode];need(fields['value-hex']==value.encode().hex(),'complete actual native value')
   success='false'if mode=='set-bool'and not pos else'true'if mode in['trigger','set-bool','server']else'not-applicable';need(fields['value-success']==success,'actual native Bool')
  phases=['READY','QOS','MATCHED','SENT','ACK','PASS']if mode=='subscribe'else['READY','MATCHED','SENT','DELIVERED','ACK','PASS']if mode=='server'else['READY','REQUEST','MATCHED','DELIVERED','ACK','PASS']
  need([p['phase']for p in protocol]==phases and protocol==c['native_protocol'],'complete independent native protocol')
  last=protocol[-1];need(last['count']=='1','one native message/request')
  if mode=='subscribe':
   need(re.fullmatch('[0-9a-f]{48}',last['identity'])is not None and last['identity']!='0'*48,'nonzero native subscriber GID')
   need(last['qos-reliability']=='1'and last['qos-durability']=='2','actual reliable/volatile graph QoS')
   if last['history-depth-observation']=='unavailable':need(last['qos-history']=='3'and last['qos-depth']=='0','record unavailable graph history/depth')
   else:need(last['qos-history']=='1'and last['qos-depth']=='4','exact observed history/depth')
  elif mode=='server':
   need(int(last['sequence'])>0 and last['identity-kind']=='client-request-sequence','actual client future association')
   credits=peer['credits'];need([r['byte']for r in credits]==['S','A'],'explicit send then completion acknowledgement')
   first_poll=next(r for r in worker['credits']if r['byte']=='P');need(credits[0]['elapsed']<=first_poll['elapsed'],'Guest poll admitted alongside native send')
  else:need(re.fullmatch('[0-9a-f]{32}',last['identity'])is not None and last['identity']!='0'*32 and int(last['sequence'])>0,'native request GUID / sequence')
  need((folder/peer['label']/'stderr.log').read_bytes()==b'','native witness stderr')
 for path,pin in matrix['preserved_inputs'].items():need(ident(Path(path))==pin,'selected source/product/mapping unchanged')
 for path,pin in build['actual_MD_inputs'].items():need(ident(Path(path))==pin,'actual native compiler dependency unchanged')
 need(ident(Path(build['product']['path']))=={k:build['product'][k]for k in['bytes','sha256']},'actual compiled native witness')
 for row in read(o/'source-inputs-r3.json')['members']:need(ident(o/'source-r3'/row['path'])=={k:row[k]for k in['bytes','sha256']},'final frozen source')
 inventory=read(o/'custom-section-inventory.json')
 need(len(inventory['products'])==32 and all(r['debug_info_present']and '.debug_line'in r['custom_sections']for r in inventory['products']),'all32 actual retained DWARF sections; no usability credit from metadata alone')
 debug=read(o/'debug-codec-final-r1-result.json')
 need(debug['status']=='ACTUAL_ZIG_MINIMAL_CODEC_SOURCE_DEBUG_SESSIONS_PASSED'and len(debug['sessions'])==3,'three actual pure codec DBG sessions')
 need(ident(o/'debug-r5.py')['sha256']==debug['driver_sha256'],'actual codec DBG controller')
 need(debug['before']['memory_events']==debug['after']['memory_events']==events,'DBG memory events')
 need({s['runtime']for s in debug['sessions']}=={'uwvm2-ros','uwvm2','uwvm2-stable'},'three selected repositories')
 for s in debug['sessions']:
  need(s['source_breakpoint']and s['copied_scalar']==13 and s['source_step']and s['stale_read_refused'],'finite actual codec source DBG outcomes')
  acts=s['actions'];need(len(acts)>=13,'complete source control transcript')
  need(any(a['command']is not None and a['command'].startswith('breaksource ')and 'breakpoint 1 source='in a['reply']for a in acts),'actual source binding')
  need(any(a['command']is not None and a['command'].startswith('step source ')and 'stopped: selected participant step'in a['reply']for a in acts),'actual source step')
  need(any(a['command']is not None and a['command'].startswith('print ')and 'no longer current'in a['reply']for a in acts),'actual stale source refusal')
 for r in debug['commands']:
  own=r['ownership'];need(r['passed']and r['exit']==0 and r['complete_output']and own['closed']and not own['signals']and not own['retirement_errors'],'DBG actor natural retirement')
  for stream in['stdout','stderr']:need(ident(o/'debug-codec-final-r1'/r['label']/(stream+'.log'))=={'bytes':r[stream+'_bytes'],'sha256':r[stream+'_sha256']},'complete actual DBG output')
 for path,pin in debug['preserved_inputs'].items():need(ident(Path(path))==pin,'DBG selected source/product unchanged')
 pure=read(o/'codec-ordinary-result.json');need(pure['status']=='ACTUAL_ZIG_CODEC_8_ORDINARY_ALL_AND_NONE_ADAPTER_RUNS_PASSED'and len(pure['commands'])==8,'actual legal pure Zig Wasm on all/none products')
 need(ident(o/'codec-ordinary.py')['sha256']==pure['driver_sha256'],'actual codec ordinary controller')
 need(pure['before']['memory_events']==pure['after']['memory_events']==events,'codec ordinary memory events')
 for r in pure['commands']:
  need(r['passed']and r['exit']==0 and r['complete_output'],'actual pure ordinary success')
  for stream in['stdout','stderr']:need(ident(o/'codec-ordinary-runs'/r['label']/(stream+'.log'))=={'bytes':r[stream+'_bytes'],'sha256':r[stream+'_sha256']},'complete actual ordinary codec output')
 miniproducer=read(o/'codec-probe-producer-result.json');need(miniproducer['status']=='ACTUAL_ZIG_MINIMAL_CODEC_PROBES_BUILT_VALIDATED'and len(miniproducer['artifacts'])==2,'two minimal actual producer probes')
 need(ident(o/'codec-probe-producer.py')['sha256']==miniproducer['driver_sha256'],'actual minimal producer')
 need(miniproducer['before']['memory_events']==miniproducer['after']['memory_events']==events,'minimal producer memory events')
 for a in miniproducer['artifacts']:
  need(ident(Path(a['path']))=={k:a[k]for k in['bytes','sha256']},'actual codec probe Wasm');need(import_modules(Path(a['path']))==[],'pure codec probe has zero imports')
 source=read(o/'codec-probe-source-input.json');need(ident(Path(source['path']))=={k:source[k]for k in['bytes','sha256']},'original codec probe source')
 gate=read(o/'source-gate-result.json');need(gate['status']=='CURRENT_ALL15_SOURCE_DECLARATIONS_891_PASSED','current15 source gate')
 need(ident(o/'source-gate.py')['sha256']==gate['driver_sha256'],'actual source gate driver')
 need(gate['before']['memory_events']==gate['after']['memory_events']==events,'source gate memory events')
 declarations=read(o/'all15-source-gate.json');need(declarations['status']=='SOURCE_CONTRACT_PASS'and declarations['backend_qualified_declarations_checked']==891 and len(declarations['bindings'])==22 and len(declarations['languages'])==15,'actual complete source inventory')
 for row in read(o/'source-inventory-inputs.json')['members']:need(ident(o/'source-inventory-current'/row['path'])=={k:row[k]for k in['bytes','sha256']},'current frozen raw inventory unchanged')
 need(events==(cg/'memory.events').read_text(),'reader cgroup events')
 outcome={'status':'INDEPENDENT_INERT_ZIG_RESOURCE_NATIVE_MATRIX_READBACK_PASSED','scope':'Inert exact source/tool/product/log/policy/phase/value/count/retirement checks only; starts no test actors. Native graph QoS history/depth unobservable is recorded, not asserted observed. Finite pure codec source DBG is verified separately; no full language/SDK, native resource source DBG, performance or architecture credit.','checked_results':checked,'all15_source_declarations':891,'all15_source_binding_rows':22,'new_Wasm_products':32,'additional_pure_codec_probes':2,'actual_pure_codec_source_debug_sessions':3,'legal_pure_codec_all_none_ordinary_runs':8,'native_positive_cases':40,'actual_wrong_value_controls':4,'native_actors':132,'policy_processes':44,'ordinary_workflows':112,'SDK_groups':144,'pure_length_tests':16,'resource_clock_profiles':64,'ordinary_argument_refusals':48,'events_before':events,'events_after':(cg/'memory.events').read_text(),'reader_sha256':ident(Path(__file__))['sha256'],'captured_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'all15_complete':False,'Zig_native_resource_source_debug_complete':False,'new_performance_credit':False}
 target=o/'independent-readback.json';need(not target.exists(),'fresh readback');target.write_text(json.dumps(outcome,indent=2)+'\n');print(outcome['status'])
if __name__=='__main__':main()
