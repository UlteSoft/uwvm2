"""Independent inert readback of source ABI continuation, not target execution."""
from pathlib import Path
import datetime,hashlib,json,os,resource
O=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-abi-integration-20261019-a2');P=O/'public'
G='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
def h(b):return hashlib.sha256(b).hexdigest()
def bracket():
 c=Path('/sys/fs/cgroup'+G)
 assert Path('/proc/self/cgroup').read_text().strip()=='0::'+G
 assert(c.stat().st_dev,c.stat().st_ino)==(29,19075)
 assert(c/'memory.max').read_text().strip()=='68719476736'and(c/'memory.swap.max').read_text().strip()=='0'
 assert Path('/proc/sys/kernel/random/boot_id').read_text().strip()=='51c27aa1-d709-4a06-ba16-2ac3003fbcbd'
 return(c/'memory.events').read_text()
def main():
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(256<<20,256<<20));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 before=bracket();raw=(P/'publication.json').read_bytes();manifest=json.loads(raw);verified=[]
 for rel,r in manifest['members'].items():
  assert not Path(rel).is_absolute()and'..'not in Path(rel).parts
  b=(P/rel).read_bytes();assert len(b)==r['bytes']and h(b)==r['sha256']
  verified.append({'path':rel,**r})
 inputs=json.loads((P/'inputs.json').read_bytes())['members']
 for r in inputs:
  b=(O/'source-original'/r['path']).read_bytes();assert len(b)==r['bytes']and h(b)==r['sha256']
 continuation=json.loads((P/'continuation.json').read_bytes())
 for r in continuation['current_source_members']:
  b=(O/'source-repaired'/r['path']).read_bytes();assert len(b)==r['bytes']and h(b)==r['sha256']
 result=json.loads((P/'result.json').read_bytes());bindings=json.loads((P/'bindings.json').read_bytes());cli=json.loads((P/'cli-result.json').read_bytes())
 assert result['status']=='SOURCE_ABI_INTEGRATION_AND_REGRESSIONS_PASSED'
 assert result['before_repair']['status']=='EXPECTED_SOURCE_ASSOCIATION_FAILURE_REPRODUCED'
 assert result['before_repair']['error']=='optional client catalog does not bind the complete actual Host'
 assert result['regressions']['groups']==18 and not result['regressions']['errors']and not result['regressions']['failures']
 assert len(result['regressions']['subcases'])==20 and all(r['passed']for r in result['regressions']['subcases'])
 assert result['after_repair']==bindings and bindings['status']=='SOURCE_CONTRACT_PASS'
 assert len(bindings['bindings'])==22 and sum(r['functions']for r in bindings['bindings'])==bindings['backend_qualified_declarations_checked']==891
 expected=['c','cpp','rust','go','tinygo','zig','java','csharp','fsharp','vb','python','javascript','assemblyscript','moonbit','kotlin']
 assert bindings['languages']==expected
 assert[r['generated_call_expressions']for r in bindings['python_generated_calls']]==[38,43]
 assert h((P/'driver.py').read_bytes())==result['driver_sha256']
 assert h((P/'cli-driver.py').read_bytes())==cli['driver_sha256']
 assert cli['status']=='ACTUAL_SOURCE_TOOL_CLI_CHECKS_PASSED'and cli['C_dispatch_peer_case_count']==170
 assert[r['returncode']for r in cli['commands']]==[0,0,0,0,1,1,1]
 for r in cli['commands']:
  assert r['passed']and r['returncode']==r['expected_returncode']
  for stream in ['stdout','stderr']:
   b=(P/'cli'/(r['label']+'.'+stream)).read_bytes()
   assert len(b)==r[stream+'_bytes']and h(b)==r[stream+'_sha256']
 markers={'legacy-normal-check':b'PASS source ABI contract','all-language-normal-check':b'891','candidate-normal':b'SOURCE_ASSOCIATION_CANDIDATE_VALIDATED','candidate-overwrite-refused':b'FileExistsError','candidate-wrong-host-refused':b'reviewed Host identity differs','candidate-wrong-catalog-refused':b'expected catalog identity differs'}
 for label,needle in markers.items():assert needle in(P/'cli'/(label+'.stdout')).read_bytes()+(P/'cli'/(label+'.stderr')).read_bytes()
 peer=json.loads((P/'cli/C-dispatch-peer.json').read_bytes());assert len(peer['cases'])==170
 assert all(r['accepted']==(r['expected']=='accept')for r in peer['cases'])
 old=json.loads((P/'catalog.before.json').read_bytes());new=json.loads((P/'catalog.candidate.json').read_bytes())
 assert new['prior_authority_source']==old['authority_source']
 restore=dict(new);restore['authority_source']=old['authority_source'];restore['prior_authority_source']=old['prior_authority_source']
 assert restore==old
 actual=h((O/'source-repaired/src/uwvm2/uwvm/imported/ros/host.h').read_bytes())
 assert actual==new['authority_source']['sha256']
 legacy1=(O/'source-original/sdk/abi/abi-v1.json').read_bytes();legacy2=(O/'source-repaired/sdk/abi/abi-v1.json').read_bytes()
 assert legacy1==legacy2 and h(legacy1)==continuation['original_abi_sha256']
 assert h((P/'catalog.candidate.json').read_bytes())==continuation['new_client_catalog_sha256']
 assert h((P/'result.json').read_bytes())==continuation['source_result_sha256']
 assert h((P/'cli-result.json').read_bytes())==continuation['cli_result_sha256']
 assert continuation['all15_complete']is False and continuation['other_agents_contacted']is False
 after=bracket();assert before==after
 receipt={'status':'INDEPENDENT_INERT_SOURCE_ABI_CONTINUATION_READBACK_PASSED','scope':'Readback of report/data, exact original/repaired source bytes, normal/negative CLI outputs, source regression accounting and two-field-only catalog association change. Does not execute evidence code or qualify Guest/native/DBG/performance/platform/security.','captured_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'publication_sha256':h(raw),'verified_members':verified,'source_input_members':len(inputs),'repaired_source_members':len(continuation['current_source_members']),'declarations':891,'python_call_shapes':81,'regression_groups':18,'recorded_backend_path_subcases':20,'C_dispatch_models':170,'source_tool_cli_processes':7,'verifier_sha256':h(Path(__file__).read_bytes()),'original_complete_bundle':False,'cgroup':G,'memory.max':64<<30,'swap_max':0,'before':before,'after':after,'memory_events_unchanged':True}
 (O/'independent-readback.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(json.dumps({k:v for k,v in receipt.items()if k!='verified_members'}))
if __name__=='__main__':main()
