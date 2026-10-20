
"""Independent inert readback for the official-Go SDK continuation.
Run only on the admitted SSH Linux host; never starts another program.
"""
from pathlib import Path
import argparse,datetime,hashlib,json,os,re,resource
G='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
DEFAULT=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-go-sdk-20261019-a3')
def check(value,message):
 if not value:raise ValueError(message)
def identity(path):
 h=hashlib.sha256();n=0
 with path.open('rb')as f:
  before=os.fstat(f.fileno())
  while True:
   b=f.read(1<<20)
   if not b:break
   n+=len(b);h.update(b)
  after=os.fstat(f.fileno())
 check((before.st_dev,before.st_ino,before.st_size,before.st_mtime_ns,before.st_ctime_ns)==(after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns,after.st_ctime_ns),'changed while reading '+str(path))
 check(n==before.st_size,'short read')
 return {'bytes':n,'sha256':h.hexdigest()}
def load(path):return json.loads(path.read_bytes())
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=DEFAULT);args=p.parse_args();o=args.root
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,512<<20));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 cg=Path('/sys/fs/cgroup'+G)
 check(Path('/proc/self/cgroup').read_text().strip()=='0::'+G,'original group')
 check((cg.stat().st_dev,cg.stat().st_ino)==(29,19075),'group identity')
 check((cg/'memory.max').read_text().strip()=='68719476736'and(cg/'memory.swap.max').read_text().strip()=='0','original limits')
 check(Path('/proc/sys/kernel/random/boot_id').read_text().strip()=='51c27aa1-d709-4a06-ba16-2ac3003fbcbd','boot identity')
 before=(cg/'memory.events').read_text();checks=[]
 statuses={'producer-final-result.json':'ACTUAL_GO_SDK_FINAL_FOUR_PROFILE_TWELVE_WASI_BUILDS_PASSED','consumer-final-result.json':'ACTUAL_GO_SDK_FINAL_24_UWVM_WORKFLOWS_AND_BINARY_VALIDATION_PASSED','argument-result.json':'ACTUAL_GO_RESOURCES_16_ARGUMENT_REFUSALS_PASSED','policy-result.json':'GO_RESOURCE_POLICIES_TAG_REFUSAL_AND_ALL15_SOURCE_GATE_PASSED','native-build-result-r6.json':'ACTUAL_GO_NATIVE_ROS2_WITNESS_COMPILE_LINK_PASSED','native-result.json':'ACTUAL_GO_FOUR_NATIVE_ROS2_TELEMETRY_DELIVERIES_PASSED'}
 logs={'producer-final-result.json':'runs-final','consumer-final-result.json':'consumer-final-runs','argument-result.json':'argument-runs','policy-result.json':'policy-runs','native-build-result-r6.json':'native-build-runs-r6','native-result.json':'native-cases'}
 results={}
 drivers={'producer-final-result.json':'producer-final-driver.py','consumer-final-result.json':'consumer-final-driver.py','argument-result.json':'argument-driver.py','policy-result.json':'policy-driver.py','native-build-result-r6.json':'native-build-driver-r6.py','native-result.json':'native-driver.py'}
 for name,status in statuses.items():
  d=load(o/name);results[name]=d;check(d['status']==status,name);check(not d.get('primary')and not d.get('finalization_errors'),name+' failures')
  check(d['before']['memory_events']==d['after']['memory_events']==before,name+' memory events')
  check(identity(o/drivers[name])['sha256']==d['driver_sha256'],name+' executed driver')
  for row in d['commands']:
   check(row['passed']and row['exit']==row.get('expected_exit',0)and row['complete_output'],'actual exit/output '+row['label'])
   for stream in['stdout','stderr']:
    pin=identity(o/logs[name]/row['label']/(stream+'.log'));check(pin=={'bytes':row[stream+'_bytes'],'sha256':row[stream+'_sha256']},'complete log hash')
  checks.append({'result':name,'identity':identity(o/name),'commands':len(d['commands']),'passed':True})
 producer=results['producer-final-result.json'];consumer=results['consumer-final-result.json'];native=results['native-result.json'];policy=results['policy-result.json']
 artifacts=producer['artifacts'];check(len(artifacts)==12,'12 final artifacts')
 check({(a['backend'],a['profile'],Path(a['path']).name)for a in artifacts}=={(b,m,n+'.wasm')for b in['ros1','ros2']for m in['release','debug']for n in['resources','sdk-test','telemetry']},'complete four-profile product set')
 for a in artifacts:
  check(identity(Path(a['path']))=={'bytes':a['bytes'],'sha256':a['sha256']},'actual Wasm bytes')
  r=load(o/'inspection-final'/(a['backend']+'-'+a['profile']+'-'+Path(a['path']).stem+'.json'))
  check(r['status']=='BINARY_IMPORT_CONTRACT_PASS'and r['sha256']==a['sha256'],'matching import inspector')
  check(all(i['module']in['wasi_snapshot_preview1','uwvm_'+a['backend']+'_v1']for i in r['imports']),'no opposite ROS backend')
  if Path(a['path']).name=='sdk-test.wasm':check(r['required_complete_inventory']and r['ROS_functions_checked']==(38 if a['backend']=='ros1'else 43),'full binary raw inventory')
 for r in consumer['runtimes']:check(identity(Path(r['path']))=={'bytes':r['bytes'],'sha256':r['sha256']},'actual Runtime')
 workflows=consumer['target_workflows'];check(len(workflows)==24,'24 final workflows')
 test_count=0
 for w in workflows:
  b=(o/'consumer-final-runs'/w['label']/'stdout.log').read_bytes();check(w['passed'],'workflow pass')
  if w['sdk_test_groups']:
   found=re.findall(rb'^--- PASS: (Test\w+) \(',b,re.M);check(found==[n.encode()for n in consumer['test_groups']]and b.endswith(b'PASS\n')and b'--- FAIL:'not in b,'11 complete test groups')
   test_count+=len(found)
  else:check(b==(b'PASS_GO_ROS_RESOURCES_DEFAULT_DENIED\n'if'RESOURCES'in w['kind']else b'PASS_ROS_DEFAULT_DENIED\n'),'exact denial marker')
 check(test_count==88,'88 assertion-group executions')
 check(len(results['argument-result.json']['commands'])==16,'16 actual argument refusals')
 for row in results['argument-result.json']['commands']:check(row['exit']==2,'application argument error2')
 check(len(policy['policy_cases'])==10,'10 selected lesson policies')
 for case in policy['policy_cases']:
  out=o/'prepared-policies'/('ros'+str(case['backend'])+'-'+case['lesson']);plan=load(out/'policy.json')
  check(identity(out/'policy.json')['sha256']==case['policy_sha256']and identity(out/'gateway.conf')['sha256']==case['config_sha256'],'policy output identity')
  check([(r['alias'],r['operation'])for r in plan['grants']]==[tuple(x)for x in case['exact_aliases']],'exact selected grants')
  check('ungranted-telemetry'not in plan['Gateway_config_text'],'forbidden alias excluded')
 abi=load(o/'source-integration-result.json');check(abi['status']=='SOURCE_CONTRACT_PASS'and abi['backend_qualified_declarations_checked']==891 and len(abi['bindings'])==22,'all15 source gate')
 check(len(native['cases'])==4 and len(native['commands'])==12,'four native cases /12 actors')
 check(not Path(native['scratch']).exists(),'own short socket directory retired')
 check({(c['runtime_mode'],c['producer_profile'])for c in native['cases']}=={(r,p)for r in['release','debug']for p in['release','debug']},'native four-profile set')
 for row in native['commands']:
  own=row['ownership'];check(own['closed']and not own['signals']and not own['retirement_errors']and all(not m['pidfd_retained']for m in own['members']),'native actor clean retirement')
 for index,case in enumerate(native['cases']):
  check(case['native_positive_delivery']and case['quiet_min_seconds']==2 and case['native_publisher_count_at_exit']==0 and case['worker_exit']==case['Gateway_exit']==case['peer_exit']==0,'native exact outcome')
  witness=(o/'native-cases'/('c'+str(index)+'-peer')/'stdout.log').read_text().splitlines();check(len(witness)==4,'four exact native phases');gids=[]
  for phase,line in zip(['READY','MATCHED','DELIVERED','PASS'],witness):
   match=re.fullmatch(phase+'_NATIVE_TELEMETRY namespace='+re.escape(case['namespace'])+r' gateway=roswasm_language_gateway gid=([0-9a-f]{48}) warmup=([01]) delivered=([01])',line);check(match is not None,'native phase syntax');gids.append(match[1]);check(int(match[3])==(1 if phase in['DELIVERED','PASS']else 0),'native delivery count')
  check(gids[0]=='0'*48 and gids[1]!='0'*48 and len(set(gids[1:]))==1,'actual unique writer')
  b=(o/'native-cases'/('c'+str(index)+'-go')/'stdout.log').read_bytes();check(b==b'READY_ROS_PUBLISH\nPUBLISHED_ROS_VALUE\nPASS_ROS_PUBLISH\n','complete Guest native protocol')
  audit=[json.loads(l)for l in(o/'native-cases'/('c'+str(index)+'-go')/'stderr.log').read_bytes().splitlines()];denied=[r for r in audit if r.get('event')=='unresolved-denial'];check(len(denied)==1 and denied[0]['error']==13,'positive ungranted denial13')
 for path,pin in native['preserved_inputs'].items():check(identity(Path(path))==pin,'native selected code unchanged')
 built=results['native-build-result-r6.json']
 check(identity(Path(built['product']['path']))=={k:built['product'][k]for k in['bytes','sha256']},'independent native witness bytes')
 for path,pin in built['actual_MD_inputs'].items():check(identity(Path(path))==pin,'actual compiler dependency')
 published=load(o/'source-published-manifest-v3.json')
 for r in published['members']:check(identity(o/'source-published-v3'/r['path'])=={k:r[k]for k in['bytes','sha256']},'published source identity')
 for r in load(o/'source-final-formatted.json')['members']:check(identity(o/'source-published-v3'/r['path'])=={k:r[k]for k in['bytes','sha256']},'exact final compiler input')
 receipt=load(o/'source-published-v3/sdk/go/generation.numeric.json')
 for r in receipt['products']:
  if r['path'].startswith('go/'):check(identity(o/'source-published-v3/sdk'/r['path'])=={k:r[k]for k in['bytes','sha256']},'unchanged generated raw source receipt')
 sections=load(o/'custom-section-inventory.json');check(len(sections['products'])==12,'12 section inventories')
 for row in sections['products']:
  check(identity(Path(row['path']))=={k:row[k]for k in['bytes','sha256']},'section inventory product bytes')
  check(row['custom_sections']==['go:buildid','producers','name']and not row['debug_info_present'],'actual no DWARF sections')
 check(before==(cg/'memory.events').read_text(),'reader memory event drift')
 outcome={'status':'INDEPENDENT_INERT_GO_SDK_AND_NATIVE_READBACK_PASSED','scope':'Independent inert hashes, complete raw logs, matrix/marker/native witness/process-retirement checks. No compiler, Runtime, native, debugger, profiler or QEMU launched by this reader. Does not rerun producers or prove complete language/ROS parity.','checked_results':checks,'final_wasm_products':12,'ordinary_workflows':24,'sdk_assertion_group_executions':88,'argument_refusals':16,'policy_profiles':10,'source_binding_entries':891,'native_ROS2_deliveries':4,'native_actors':12,'source_files':len(published['members']),'source_manifest':identity(o/'source-published-manifest-v3.json'),'section_inventory':identity(o/'custom-section-inventory.json'),'before_memory_events':before,'after_memory_events':(cg/'memory.events').read_text(),'reader_sha256':identity(Path(__file__))['sha256'],'captured_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'all15_complete':False,'source_debug_complete':False,'new_performance_credit':False}
 target=o/'independent-readback-final.json';check(not target.exists(),'fresh readback required');target.write_text(json.dumps(outcome,indent=2)+'\n');print(outcome['status'])
if __name__=='__main__':main()
