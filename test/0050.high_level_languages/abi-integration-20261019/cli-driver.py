"""Actual source-tool CLI regression only, in the original Linux cgroup."""
from pathlib import Path
import datetime,hashlib,json,os,resource,subprocess,sys,time
O=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-abi-integration-20261019-a2');R=O/'source-repaired'
G='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
def h(b):return hashlib.sha256(b).hexdigest()
def guard():
 c=Path('/sys/fs/cgroup'+G)
 assert Path('/proc/self/cgroup').read_text().strip()=='0::'+G
 assert(c.stat().st_dev,c.stat().st_ino)==(29,19075)
 assert(c/'memory.max').read_text().strip()=='68719476736'and(c/'memory.swap.max').read_text().strip()=='0'
 assert int((c/'memory.current').read_text())<62<<30
 return(c/'memory.events').read_text()
def main():
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,512<<20));resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_CPU,(120,120))
 before=guard();records=[];out=O/'cli';out.mkdir()
 expected=json.loads((O/'result.json').read_bytes())['catalog_change']
 tool=O/'source-original/sdk/tools/rebind_service_client_catalog.py'
 def run(label,args,expected_code=0,contains=None):
  guard();argv=[sys.executable,'-S','-B',*map(str,args)];t=time.monotonic()
  p=subprocess.run(argv,cwd=R,env={'PATH':'/usr/bin:/bin','PYTHONDONTWRITEBYTECODE':'1','TMPDIR':str(out)},capture_output=True,timeout=30)
  row={'label':label,'argv':argv,'cwd':str(R),'returncode':p.returncode,'expected_returncode':expected_code,'elapsed_seconds':time.monotonic()-t,'stdout_bytes':len(p.stdout),'stderr_bytes':len(p.stderr),'stdout_sha256':h(p.stdout),'stderr_sha256':h(p.stderr),'passed':p.returncode==expected_code and(contains is None or contains.encode()in p.stdout+p.stderr)}
  (out/(label+'.stdout')).write_bytes(p.stdout);(out/(label+'.stderr')).write_bytes(p.stderr)
  records.append(row);(O/'cli-progress.json').write_text(json.dumps(records,indent=2)+'\n')
  assert row['passed'],row
  guard()
 run('legacy-normal-check',['sdk/tools/abi_contract.py'],contains='PASS source ABI contract')
 run('all-language-normal-check',['sdk/tools/check_bindings.py','--report',out/'bindings-cli.json'],contains='891')
 run('C-dispatch-peer',['sdk/tests/c_dispatch_peer_models.py','--root',R,'--report',out/'C-dispatch-peer.json'])
 common=[tool,'--reviewed-host-sha256',expected['host_sha256'],'--expected-catalog-sha256',expected['before_sha256'],'--scratch',out,'--out',out/'candidate.json']
 run('candidate-normal',common,contains='SOURCE_ASSOCIATION_CANDIDATE_VALIDATED')
 assert h((out/'candidate.json').read_bytes())==expected['candidate_sha256']
 run('candidate-overwrite-refused',common,1,contains='FileExistsError')
 assert h((out/'candidate.json').read_bytes())==expected['candidate_sha256']
 wrong_host=list(common);wrong_host[2]='0'*64;wrong_host[-1]=out/'wrong-host.json'
 run('candidate-wrong-host-refused',wrong_host,1,contains='reviewed Host identity differs')
 wrong_catalog=list(common);wrong_catalog[4]='0'*64;wrong_catalog[-1]=out/'wrong-catalog.json'
 run('candidate-wrong-catalog-refused',wrong_catalog,1,contains='expected catalog identity differs')
 assert not(out/'wrong-host.json').exists()and not(out/'wrong-catalog.json').exists()
 after=guard();peer=json.loads((out/'C-dispatch-peer.json').read_bytes())
 result={'status':'ACTUAL_SOURCE_TOOL_CLI_CHECKS_PASSED','scope':'Actual source-checker/generator CLI processes and source mutation models only; no compiled language, Wasm/ROS, DBG, VTune or QEMU credit.','commands':records,'C_dispatch_peer_case_count':len(peer['cases']),'C_dispatch_peer_status':peer['status'],'before':before,'after':after,'memory_events_unchanged':before==after,'driver_sha256':h(Path(__file__).read_bytes()),'cgroup':G,'memory.max':64<<30,'swap_max':0,'captured_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
 assert before==after
 (O/'cli-result.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps({k:v for k,v in result.items()if k!='commands'}))
if __name__=='__main__':main()
