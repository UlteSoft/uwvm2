from pathlib import Path
import json,hashlib,shutil,tarfile,os,resource,datetime
O=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-tinygo-sdk-resources-20261019-a6')
def sha(p):
 h=hashlib.sha256()
 with p.open('rb')as f:
  while b:=f.read(65536):h.update(b)
 return h.hexdigest()
def main():
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,512<<20))
 assert json.loads((O/'independent-readback-r2.json').read_bytes())['status']=='INDEPENDENT_INERT_TINYGO_RESOURCE_MATRIX_READBACK_PASSED'
 assert json.loads((O/'independent-selected-inputs-r1.json').read_bytes())['status'].endswith('_PASSED')
 perf=json.loads((O/'perf-calibration-r3-result.json').read_bytes())
 assert perf['status']=='FAILED_RETAINED'and not perf['commands']and not perf.get('runs')
 coordination={'status':'NO_NEW_PERFORMANCE_CREDIT_LOCK_WAIT_ENDED_ON_STRICT_CAPACITY_GUARD','target_or_collector_started':False,'calibration_result_sha256':sha(O/'perf-calibration-r3-result.json'),'primary':perf['primary'],'locks_overridden':False,'guards_relaxed':False,'performance_qualified':False,'all15_complete':False}
 (O/'performance-coordination.json').write_text(json.dumps(coordination,indent=2)+'\n')
 P=O/'publication';assert not P.exists();P.mkdir()
 chosen=['source-r1','source-r2','source-r3','probe-source-r2','source-inventory-current','source-current-guides','build-runs-r2','build-runs-r3','build-runs-r4','build-runs-r5','native-build-runs-r1','native-products-r1','products-r3','products-r5','startup-runs','startup-runs-r2','native-smoke-r1','native-full-r1','ordinary-runs','ordinary-runs-r2','probe-build-runs-r1','probe-build-runs-r2','probe-products-r1','probe-products-r2','rollback-r1','rollback-r2','debug-r1','debug-r2','codec-ordinary-runs','dwarf-runs-r1','source-gate-runs-r1','source-gate-runs-r2','tool-runs','reviewed-precedent','perf-source-r1']
 sources=[p for p in O.iterdir()if p.is_file()and p.suffix in ('.py','.json','.log','.md')]
 for name in chosen:
  path=O/name
  if path.exists():sources.extend(p for p in path.rglob('*')if p.is_file())
 mapping=[]
 for path in sorted(set(sources)):
  rel=path.relative_to(O);new=Path(str(rel)+'.source')if path.suffix=='.cc'else rel
  assert not path.is_symlink()and not(new.is_absolute()or'..'in new.parts)
  dst=P/new;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,dst)
  assert sha(path)==sha(dst)
  if new!=rel:mapping.append({'original_relative':str(rel),'published_relative':str(new),'bytes':path.stat().st_size,'sha256':sha(path)})
 (P/'source-transport-map.json').write_text(json.dumps({'kind':'INERT_CPP_SOURCE_ARCHIVE_NO_XMAKE_TEST_TARGETS','members':mapping},indent=2)+'\n')
 assert not list(P.rglob('*.cc'))
 rows=[{'path':str(p.relative_to(P)),'bytes':p.stat().st_size,'sha256':sha(p)}for p in sorted(P.rglob('*'))if p.is_file()]
 manifest={'format':'uwvm-tinygo-sdk-resource-publication-v1','files':rows,'all15_complete':False,'performance_qualified':False,'reader_result_sha256':sha(O/'independent-readback-r2.json'),'large_shared_inputs':'Pinned retained Linux locations, independently rehashed; not duplicated into this packet'}
 (P/'publication-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 archive=O/'publication-data.tar.gz';assert not archive.exists()
 with tarfile.open(archive,'w:gz',compresslevel=6)as t:
  for p in sorted(P.rglob('*')):
   if p.is_file():t.add(p,arcname=str(p.relative_to(P)),recursive=False)
 receipt={'status':'INERT_TINYGO_PUBLICATION_ARCHIVE_CREATED','files':len(rows)+1,'manifest_sha256':sha(P/'publication-manifest.json'),'archive_sha256':sha(archive),'archive_bytes':archive.stat().st_size,'public_data_bytes':sum(r['bytes']for r in rows)+(P/'publication-manifest.json').stat().st_size,'cpp_payload_mappings':mapping}
 (O/'publication-transport.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt))
if __name__=='__main__':main()
