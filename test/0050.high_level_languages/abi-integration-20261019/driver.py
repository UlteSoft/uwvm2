"""Bounded source-only ABI continuation; never compiles or runs target code."""
from pathlib import Path
import datetime,hashlib,importlib.util,io,json,os,resource,sys,traceback,unittest
O=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-abi-integration-20261019-a2')
G='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
def sha(b):return hashlib.sha256(b).hexdigest()
def guard():
 c=Path('/sys/fs/cgroup'+G)
 assert Path('/proc/self/cgroup').read_text().strip()=='0::'+G
 assert(c.stat().st_dev,c.stat().st_ino)==(29,19075)
 assert(c/'memory.max').read_text().strip()=='68719476736'and(c/'memory.swap.max').read_text().strip()=='0'
 assert Path('/proc/sys/kernel/random/boot_id').read_text().strip()=='51c27aa1-d709-4a06-ba16-2ac3003fbcbd'
 assert int((c/'memory.current').read_text())<62<<30,'shared cgroup current guard'
 return(c/'memory.events').read_text()
def load_module(name,path):
 spec=importlib.util.spec_from_file_location(name,path);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);return mod
def main():
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,512<<20));resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_CPU,(120,120))
 before=guard();input_bytes=(O/'inputs.json').read_bytes();members=json.loads(input_bytes)['members'];source=O/'source-original'
 for r in members:
  b=(source/r['path']).read_bytes();assert len(b)==r['bytes']and sha(b)==r['sha256']
 sys.path.insert(0,str(source/'sdk/tools'))
 import abi_contract as abi,check_bindings as cb,rebind_service_client_catalog as rebinder
 result={'scope':'Current source association and all-language raw declaration/generated-call checks only; no compiler, Guest/native ROS, debugger, profiler or emulator.','input_manifest_sha256':sha(input_bytes),'before':before}
 try:cb.check();raise RuntimeError('expected stale association unexpectedly passed')
 except ValueError as error:
  assert str(error)=='optional client catalog does not bind the complete actual Host'
  result['before_repair']={'status':'EXPECTED_SOURCE_ASSOCIATION_FAILURE_REPRODUCED','error':str(error)}
 original_legacy=(source/rebinder.LEGACY).read_bytes();original_catalog=(source/rebinder.CATALOG).read_bytes()
 (O/'catalog.before.json').write_bytes(original_catalog)
 scratch=O/'scratch';scratch.mkdir()
 candidate=rebinder.candidate(source,sha((source/rebinder.HOST).read_bytes()),sha(original_catalog),scratch)
 (O/'catalog.candidate.json').write_bytes(candidate)
 repaired=O/'source-repaired';repaired.mkdir()
 for r in members:
  p=repaired/r['path'];p.parent.mkdir(parents=True,exist_ok=True)
  p.write_bytes(candidate if r['path']==rebinder.CATALOG else(source/r['path']).read_bytes())
 cb.ROOT=abi.ROOT=rebinder.ROOT=repaired
 result['after_repair']=cb.check()
 (O/'bindings.json').write_text(json.dumps(result['after_repair'],indent=2)+'\n')
 # Each suite reads only the individually staged, hashed source files.
 preservation=load_module('actual_preservation_suite',source/'tests/ros_service_availability/test_source_abi_preservation.py')
 all_languages=load_module('actual_all_language_suite',source/'tests/ros_service_availability/test_all_language_bindings.py')
 s1=O/'preservation-scratch';s1.mkdir();s2=O/'bindings-scratch';s2.mkdir()
 suite=unittest.TestSuite([preservation.suite(abi,scratch=s1,guard=guard),
  all_languages.suite(cb,rebinder,inputs=[r['path']for r in members],scratch=s2,guard=guard)])
 class Recorded(unittest.TextTestResult):
  def __init__(self,*args,**kwargs):super().__init__(*args,**kwargs);self.subcases=[]
  def addSubTest(self,test,subtest,error):
   self.subcases.append({'group':test.id(),'case':str(subtest),'passed':error is None})
   super().addSubTest(test,subtest,error)
 log=io.StringIO();test=unittest.TextTestRunner(stream=log,verbosity=2,resultclass=Recorded).run(suite)
 (O/'regressions.log').write_text(log.getvalue())
 result['regressions']={'groups':test.testsRun,'passed':test.wasSuccessful(),'failures':test.failures,'errors':test.errors,'subcases':test.subcases}
 # JSON retains failure text instead of unserializable TestCase objects.
 for key in ['failures','errors']:result['regressions'][key]=[{'group':t.id(),'text':msg}for t,msg in result['regressions'][key]]
 assert (repaired/rebinder.LEGACY).read_bytes()==original_legacy
 assert (source/rebinder.CATALOG).read_bytes()==original_catalog
 for r in members:
  b=(source/r['path']).read_bytes();assert len(b)==r['bytes']and sha(b)==r['sha256']
 after=guard();result['after']=after;result['memory_events_unchanged']=after==before
 result['catalog_change']={'before_sha256':sha(original_catalog),'candidate_sha256':sha(candidate),'host_sha256':sha((source/rebinder.HOST).read_bytes()),'legacy_sha256':sha(original_legacy),'legacy_unchanged':True,'original_catalog_preserved':True}
 result['status']='SOURCE_ABI_INTEGRATION_AND_REGRESSIONS_PASSED'if test.wasSuccessful()and before==after else'FAILED_RETAINED'
 result['driver_sha256']=sha(Path(__file__).read_bytes());result['captured_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();result['cgroup']=G;result['memory.max']=64<<30;result['swap_max']=0
 (O/'result.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps({'status':result['status'],'source_binding_entries':result['after_repair']['backend_qualified_declarations_checked'],'python_calls':result['after_repair']['python_generated_calls'],'groups':test.testsRun,'subcases':len(test.subcases),'failures':result['regressions']['failures'],'errors':result['regressions']['errors'],'catalog':result['catalog_change'],'memory_events_unchanged':before==after,'result_sha256':sha((O/'result.json').read_bytes())}))
 if result['status']!='SOURCE_ABI_INTEGRATION_AND_REGRESSIONS_PASSED':raise SystemExit(1)
if __name__=='__main__':main()
