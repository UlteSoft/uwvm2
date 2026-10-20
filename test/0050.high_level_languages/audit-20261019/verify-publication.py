"""Independent inert publication readback. Never execute source in evidence."""
from pathlib import Path
import datetime,hashlib,json,os,resource
V=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-full-requirements-audit-20261019-a1')
P=V/'public'
G='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
def h(b):return hashlib.sha256(b).hexdigest()
def bracket():
 c=Path('/sys/fs/cgroup'+G)
 assert Path('/proc/self/cgroup').read_text().strip()=='0::'+G
 assert(c.stat().st_dev,c.stat().st_ino)==(29,19075)
 assert(c/'memory.max').read_text().strip()=='68719476736'
 assert(c/'memory.swap.max').read_text().strip()=='0'
 assert Path('/proc/sys/kernel/random/boot_id').read_text().strip()=='51c27aa1-d709-4a06-ba16-2ac3003fbcbd'
 return {'memory_current':int((c/'memory.current').read_text()),'memory_events':(c/'memory.events').read_text()}
def main():
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,512<<20));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 before=bracket()
 mfraw=(P/'audit-20261019/publication-members.json').read_bytes();mf=json.loads(mfraw)
 verified=[]
 for rel,r in mf['members'].items():
  assert not Path(rel).is_absolute()and'..'not in Path(rel).parts
  b=(P/rel).read_bytes()
  assert len(b)==r['bytes']and h(b)==r['sha256'],rel
  verified.append({'path':rel,**r})
 A=P/'audit-20261019'
 contract=json.loads((A/'source-contract-result.json').read_bytes())
 original=json.loads((V/'source-contract-result.json').read_bytes())
 assert contract==original
 inp=(A/'input-manifest.json').read_bytes()
 manifest=json.loads(inp)
 assert h(inp)==contract['input_manifest_sha256']
 for r in manifest['members']:
  b=(V/'source'/r['path']).read_bytes()
  assert len(b)==r['bytes']and h(b)==r['sha256']
 assert h((A/'source-contract-audit.py').read_bytes())==manifest['runner_sha256']
 assert (A/'source-contract-audit.py').read_bytes()==(V/'source-contract-audit.py').read_bytes()
 authority=contract['existing_authoritative_checker']
 assert authority['status']=='SOURCE_CONTRACT_FAILED'
 assert authority['error']=="ValueError('optional client catalog does not bind the complete actual Host')"
 rows=contract['bindings']
 assert len(rows)==22 and sum(r['declarations']for r in rows)==891
 assert all(r['passed']and not(r['missing']or r['extra']or r['wrong_widths'])for r in rows)
 assert contract['python_dispatch']['passed']and contract['python_dispatch']['names']==47
 assert contract['C_dispatch']['status']=='PASS'and len(contract['C_dispatch']['tables'])==2
 host=(V/'source/src/uwvm2/uwvm/imported/ros/host.h').read_bytes()
 catalog=json.loads((V/'source/sdk/abi/service-client-v1.json').read_bytes())
 assert h(host)!=catalog['authority_source']['sha256']
 sync=json.loads((A/'source-sync-inventory.json').read_bytes())
 assert len(sync['common_source_rows'])==sync['common_source_count']==234
 actual_equal=0
 for r in sync['common_source_rows']:
  parts=r['repositories']
  eq=all(parts.values())and len({v['sha256']for v in parts.values()if v})==1
  assert bool(eq)==bool(r['all_present_identical'])
  actual_equal+=bool(eq)
 assert actual_equal==sync['common_source_identical_count']==219
 assert len(sync['common_source_differences'])==15
 assert all(r['all_three_byte_identical']for r in sync['selected_published_packets'])
 assert[sorted(r['member_counts'].values())for r in sync['selected_published_packets']]==[[9,9,9],[6,6,6],[7,7,7],[10,10,10]]
 data=json.loads((A/'requirements-audit.json').read_bytes())
 expected=['c','cpp','rust','go','tinygo','zig','java','csharp','fsharp','vb','python','javascript','assemblyscript','moonbit','kotlin']
 assert[r['id']for r in data['languages']]==expected
 assert all(not r['all_original_requirements_complete']and r['debug_option_default_on']for r in data['languages'])
 assert len(data['requirements'])==18 and not data['verdict']['original_complete_requirement_satisfied']
 assert data['verdict']['accepted_complete_language_bundles']==0
 assert data['verdict']['other_agents_contacted']is False
 assert data['current_source_contract']['result_sha256']==h((A/'source-contract-result.json').read_bytes())
 assert data['sync']['inventory_sha256']==h((A/'source-sync-inventory.json').read_bytes())
 assert data['queue_snapshot_sha256']==h((A/'live-queues.json').read_bytes())
 queues=json.loads((A/'live-queues.json').read_bytes())
 old=[r for r in queues['queues']if r['root'].endswith('-r197')][0]
 assert(old['actors'],old['closed'],old['passed_actors'],len(old['failed_actual']))==(86,86,80,6)
 tiny=[r for r in queues['queues']if r['root'].endswith('-r340')][0]
 assert(tiny['actors'],tiny['closed'],tiny['passed_actors'])==(5,5,5)
 assert all(r['actors']==0 for r in queues['queues']if r is not old and r is not tiny)
 for rel,r in mf['members'].items():
  b=(P/rel).read_bytes();assert len(b)==r['bytes']and h(b)==r['sha256']
 after=bracket()
 result={'status':'INDEPENDENT_INERT_AUDIT_PUBLICATION_READBACK_PASSED','captured_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Hashes every published report/data member, checks frozen source input bytes, independently reconciles 15-language/18-requirement verdict, 891 declaration entries, preserved current authoritative failure, sync counts and captured queue accounting. No compiler, target, debugger, profiler, emulator or archived program is executed.','publication_manifest_sha256':h(mfraw),'verified_members':verified,'frozen_source_members':len(manifest['members']),'language_count':15,'requirement_count':18,'authoritative_current_source_failure_retained':True,'base_declaration_entries':891,'common_source_identical':219,'common_source_total':234,'original_complete_language_bundles':0,'verifier_sha256':h(Path(__file__).read_bytes()),'before':before,'after':after,'memory_events_unchanged':before['memory_events']==after['memory_events'],'cgroup':G,'memory.max':64<<30,'swap_max':0}
 (V/'independent-readback.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(result))
if __name__=='__main__':main()
