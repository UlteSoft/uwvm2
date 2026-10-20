
"""Independent inert verification of the Go native-resource continuation.
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
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--run-name',default='matrix-final-r1');args=p.parse_args();o=args.root
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,512<<20));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 cg=Path('/sys/fs/cgroup'+GROUP);need(Path('/proc/self/cgroup').read_text().strip()=='0::'+GROUP,'original Linux cgroup')
 need((cg.stat().st_dev,cg.stat().st_ino)==(29,19075),'cgroup inode')
 need((cg/'memory.max').read_text().strip()=='68719476736'and(cg/'memory.swap.max').read_text().strip()=='0','original limits')
 need(Path('/proc/sys/kernel/random/boot_id').read_text().strip()=='51c27aa1-d709-4a06-ba16-2ac3003fbcbd','original boot')
 events=(cg/'memory.events').read_text();results={};checked=[]
 sources={
  'build-result-r6.json':('ACTUAL_GO_NATIVE_RESOURCE_WITNESS_COMPILE_LINK_PASSED','build-runs-r6','build-driver-r6.py'),
  'go-producer-result.json':('ACTUAL_FIXED_MARKER_GO_RESOURCE_FOUR_BUILDS_AND_VALIDATION_PASSED','go-build-runs','go-producer-driver.py'),
  'ordinary-result.json':('ACTUAL_GO_RESOURCE_32_CLOCK_PROFILES_AND16_ARGUMENT_REFUSALS_PASSED','ordinary-runs','ordinary-driver.py'),
  args.run_name+'-result.json':('ACTUAL_GO_NATIVE_RESOURCES_AND_SELECTED_WRONG_VALUES_PASSED',args.run_name,'matrix-driver-r4.py')}
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
 producer=results['go-producer-result.json'];ordinary=results['ordinary-result.json'];matrix=results[args.run_name+'-result.json'];build=results['build-result-r6.json'];base=o.parent/'root-language-go-sdk-20261019-a3'
 need(len(producer['artifacts'])==4,'four source-built products')
 need({(a['backend'],a['profile'])for a in producer['artifacts']}=={(b,m)for b in['ros1','ros2']for m in['release','debug']},'complete build matrix')
 for a in producer['artifacts']:
  need(ident(Path(a['path']))=={k:a[k]for k in['bytes','sha256']},'actual new Wasm')
  r=read(Path(a['import_report']));need(r['status']=='BINARY_IMPORT_CONTRACT_PASS'and r['sha256']==a['sha256'],'actual binary import inspector')
  need(all(i['module']in['wasi_snapshot_preview1','uwvm_'+a['backend']+'_v1']for i in r['imports']),'selected backend only')
 for path,pin in producer['tools'].items():need(ident(Path(path))==pin,'producer tool changed')
 for row in read(o/'source-formatted-r5.json')['members']:
  if row['path'].endswith('.go')or row['path'].endswith('/go.mod'):
   need(ident(o/'source-r6'/row['path'])=={k:row[k]for k in['bytes','sha256']},'native cohort uses exact new compiler inputs')
 need(len(ordinary['commands'])==48 and len(ordinary['target_workflows'])==32,'32 clock profiles +16 argument rejections')
 kinds={}
 for row in ordinary['target_workflows']:
  text=(o/'ordinary-runs'/row['label']/'stdout.log').read_bytes();err=(o/'ordinary-runs'/row['label']/'stderr.log').read_bytes()
  if row['kind']=='ACTUAL_APPLICATION_ROS_DEFAULT_DENIAL':
   need(row['exit']==0 and text==b'PASS_GO_ROS_RESOURCES_DEFAULT_DENIED\n','actual zero-ROS-grant app')
  else:
   need(row['kind']=='WASI_CLOCK_STARTUP_REFUSAL_NOT_ROS_CREDIT'and row['exit']!=0 and b'clock_time_get'in(text+err)and b'PASS_GO_ROS_RESOURCES'not in text,'actual clock permission refusal')
  kinds[row['kind']]=kinds.get(row['kind'],0)+1
 need(sorted(kinds.values())==[16,16],'separate startup and ROS denial scopes')
 for row in ordinary['commands']:
  if row['label'].startswith('arguments-'):need(row['exit']==2 and(o/'ordinary-runs'/row['label']/'stdout.log').read_bytes()==b'','exact argument error')
 cases=matrix['cases'];positive=[c for c in cases if c['control']=='positive'];negative=[c for c in cases if c['control']=='wrong-value']
 modes=['subscribe','trigger','set-bool','server','parameter']
 need(len(cases)==24 and len(positive)==20 and len(negative)==4 and len(matrix['commands'])==96,'native matrix cardinality')
 need({(c['runtime_mode'],c['producer_profile'],c['lesson'])for c in positive}=={(r,p,m)for r in['release','debug']for p in['release','debug']for m in modes},'complete positive matrix')
 need({c['lesson']for c in negative}==set(modes)-{'server'},'four actual wrong values')
 need(not Path(matrix['scratch']).exists(),'private short socket directory retired')
 expected_roles={'subscribe':[('sub','topic.subscribe')],'trigger':[('call','service.call')],'set-bool':[('call','service.call')],'server':[('serve','service.advertise'),('respond','service.respond')],'parameter':[('query','param.get')]}
 expected_values={'subscribe':'bounded native Go telemetry','trigger':'native-go-ready','set-bool':'native-go-enabled','server':'bounded go wasm service','parameter':'bounded native Go parameter'}
 wrong={'subscribe':'unexpected native Go telemetry','trigger':'unexpected-native-go-ready','set-bool':'native-go-enabled','parameter':'unexpected native Go parameter'}
 by_label={r['label']:r for r in matrix['commands']}
 for r in matrix['commands']:
  own=r['ownership'];need(own['closed']and not own['signals']and not own['retirement_errors']and all(not m['pidfd_retained']for m in own['members']),'exact actor clean retirement')
 for index,c in enumerate(cases):
  pos=c['control']=='positive';mode=c['lesson'];folder=o/args.run_name;worker=by_label['c'+str(index)+'-go'];peer=by_label['c'+str(index)+'-peer'];gateway=by_label['c'+str(index)+'-gateway']
  need(worker['exit']==(0 if pos else 1)and peer['exit']==gateway['exit']==0,'actual native actor status')
  need(c['quiet_min_seconds']==2 and c['native_gateway_node_count_at_exit']==0,'selected native retirement witness')
  plan=read(folder/('case'+str(index))/'policy/policy.json')
  need(ident(folder/('case'+str(index))/'policy/policy.json')['sha256']==c['policy_json_sha256'],'policy output')
  need([(g['alias'],g['operation'])for g in plan['grants']]==expected_roles[mode],'exact granted roles')
  need('worker-pid '+str(worker['pid'])+'\n'in plan['Gateway_config_text']and'ungranted-telemetry'not in plan['Gateway_config_text'],'actual held PID / forbidden alias')
  config=folder/('case'+str(index))/'policy/gateway.conf'
  need(config.read_bytes()==plan['Gateway_config_text'].encode(),'exact generated native configuration')
  runtime=next(r for r in matrix['runtimes']if r['mode']==c['runtime_mode'])
  artifact=next(a for a in producer['artifacts']if a['backend']=='ros2'and a['profile']==c['producer_profile'])
  business=read(folder/('case'+str(index))/'worker-argv.json')
  expected=[runtime['path'],'--wasip1-global-grant','monotonic-clock','--wasip1-global-grant','realtime-clock',*plan['Worker_arguments'],'--wasm-set-main-module-name','go_resources','--run',artifact['path'],mode]
  need(business==expected and worker['target_argv']==expected and worker['live_executable_after_ready']==runtime['path'],'exact observed Runtime / grants / target argv')
  need(gateway['argv'][-1]==str(config)and gateway['argv'][-2]==matrix['native_origin']['binary']['path'],'actual Gateway selected native configuration')
  for g in plan['grants']:
   if 'timeout-ms'in g['limits']:need(g['limits']['timeout-ms']=='3000','service/parameter deadline not relaxed')
  need('timeout-ms 15000\n'in plan['Gateway_config_text'],'explicit separate Gateway frame deadline')
  need(ident(folder/('case'+str(index))/'worker-argv.json')['sha256']==worker['target_argv_sha256'],'deferred actual exec argv')
  text=(folder/worker['label']/'stdout.log').read_text().splitlines();ready={'subscribe':'READY_GO_ROS_SUBSCRIPTION','trigger':'READY_GO_ROS_SERVICE_CLIENT','set-bool':'READY_GO_ROS_SERVICE_CLIENT','server':'READY_GO_ROS_SERVICE_SERVER','parameter':'READY_GO_ROS_PARAMETER'}[mode]
  idle='IDLE_GO_ROS_SUBSCRIPTION'if mode=='subscribe'else'IDLE_GO_ROS_SERVICE_SERVER';main=[line for line in text if line!=idle]
  delivered={'subscribe':'DELIVERED_GO_ROS_SUBSCRIPTION','trigger':'DELIVERED_GO_ROS_SERVICE_RESPONSE','set-bool':'DELIVERED_GO_ROS_SERVICE_RESPONSE','server':'REPLIED_GO_ROS_SERVICE','parameter':'DELIVERED_GO_ROS_PARAMETER'}[mode]
  need(main==([ready,delivered,'PASS_GO_ROS_RESOURCES']if pos else[ready]),'complete Guest protocol')
  errs=(folder/worker['label']/'stderr.log').read_text().splitlines();audit=[json.loads(s)for s in errs if s.startswith('{')];other=[s for s in errs if not s.startswith('{')]
  denials=[r for r in audit if r.get('event')=='unresolved-denial']
  need(len(denials)==1 and denials[0]['error']==13 and all(r.get('error')in(0,11,13)for r in audit),'ungranted alias and actual Host outcomes')
  need(other==([]if pos else['primary 22 cleanup-status 0']),'preserved exact semantic rejection / cleanup')
  protocol=[]
  for line in(folder/peer['label']/'stdout.log').read_text().splitlines():
   phase,rest=line.split('_NATIVE_GO_RESOURCE ',1);fields=dict(piece.split('=',1)for piece in rest.split(' '));protocol.append({'phase':phase,**fields})
   need(fields['namespace']==c['namespace']and fields['mode']==mode and fields['control']==c['control']and fields['gateway']=='go_resource_gateway','actual native case identity')
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
 for row in read(o/'source-inputs-r6.json')['members']:need(ident(o/'source-r6'/row['path'])=={k:row[k]for k in['bytes','sha256']},'final frozen source')
 inventory=read(o/'custom-section-inventory.json')
 need(len(inventory['products'])==4 and all(r['custom_sections']==['go:buildid','producers','name']and not r['debug_info_present']for r in inventory['products']),'actual Go source DBG metadata remains absent')
 need(events==(cg/'memory.events').read_text(),'reader cgroup events')
 outcome={'status':'INDEPENDENT_INERT_GO_RESOURCE_NATIVE_MATRIX_READBACK_PASSED','scope':'Inert exact source/tool/product/log/policy/phase/value/count/retirement checks only; starts no test actors. Native graph QoS history/depth unobservable is recorded, not asserted observed. No complete language/SDK, source DBG, performance or architecture credit.','checked_results':checked,'new_Wasm_products':4,'native_positive_cases':20,'actual_wrong_value_controls':4,'native_actors':72,'policy_processes':24,'ordinary_clock_profiles':32,'actual_ROS_default_denials':16,'WASI_startup_clock_refusals':16,'ordinary_argument_refusals':16,'events_before':events,'events_after':(cg/'memory.events').read_text(),'reader_sha256':ident(Path(__file__))['sha256'],'captured_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'all15_complete':False,'Go_source_debug_complete':False,'new_performance_credit':False}
 target=o/'independent-readback.json';need(not target.exists(),'fresh readback');target.write_text(json.dumps(outcome,indent=2)+'\n');print(outcome['status'])
if __name__=='__main__':main()
