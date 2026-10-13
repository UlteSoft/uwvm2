from pathlib import Path
import hashlib,json
D=Path(__file__).parent;E=D.parent.parent;OLD=E/'rounds/wasip1-prepared-retirement-20261008-r34'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
m=json.loads((D/'syntax-overlay-v4.json').read_text());old=json.loads((D/'syntax-overlay-v3.json').read_text())
q=json.loads((D/'analysis-compile-qualified-v4.json').read_text());g=json.loads((D/'guard-analysis-compile-v4.json').read_text())
q3=json.loads((D/'analysis-compile-qualified-v3b.json').read_text());g3=json.loads((D/'guard-analysis-compile-v3b.json').read_text())
assert q['passed'] and g['passed'] and q3['passed'] and g3['passed']
assert g['parent_events_unchanged'] and g3['parent_events_unchanged'] and all(x['pidfd_retired'] for x in [*g['processes'],*g3['processes']])
assert q['source_manifest_sha256']==sha(D/'syntax-overlay-v4.json') and q3['source_manifest_sha256']==sha(D/'syntax-overlay-v3.json')
assert all(sha(D/'syntax-overlay-v4'/k)==v for k,v in m.items()) and all(sha(D/'syntax-overlay-v3'/k)==v for k,v in old.items())
changed=sorted(k for k in m if m[k]!=old[k]);assert changed==q['changed_paths']
assert len(changed)==4 and all(k.endswith('debug_checkpoint_complete_preload_runtime.cc') or k.endswith('wasip1_checkpoint.md') for k in changed)
parents={}
for r in q3['rows']:
 P=OLD/('products-v8/linux-integrated' if r['target_os']=='linux' else 'products-v8/'+r['target_os'])/r['repo']
 proof=P/'runtime-qualified.json';assert sha(proof)==r['parent_compile_proof_sha256']
 p=json.loads(proof.read_text());assert all(sha(k)==v for k,v in p['dependencies'].items())
 assert sha(r['argv'][0])==r['compiler_sha256']==p['compiler_sha256']
 parents[str(proof)]=sha(proof)
nr=json.loads((D/'ram-native-v3-results.json').read_text());ng=json.loads((D/'guard-ram-native-v3.json').read_text())
assert ng['error'] is None and ng['exit']==1 and ng['ram_disposable_products_retired'] and ng['parent_events_unchanged']
assert all(x['pidfd_retired'] for x in ng['processes']) and all(sha(k)==v for k,v in nr['dependencies'].items()) and all(sha(k)==v for k,v in nr['fixtures'].items())
native=[r for r in nr['rows'] if r['actual_native_runtime_execution']]
assert len(native)==11 and sum(r['passed'] for r in native)==10 and native[-1]['stage']=='preload-normal-instruction' and native[-1]['exit']==-4
log=D/'uwvm2-preload-normal-instruction-ram-native-v3.log'
assert 'status=6 resource=9 engine=5' in log.read_text()
qualified=dict(passed=True,source_equivalence_verified=True,changed_paths=changed,production_runtime_inputs_unchanged=True,
 reused_native_passes=10,source_manifest_sha256=sha(D/'syntax-overlay-v4.json'),previous_source_manifest_sha256=sha(D/'syntax-overlay-v3.json'),
 frontend_qualification_sha256=sha(D/'analysis-compile-qualified-v4.json'),previous_frontend_qualification_sha256=sha(D/'analysis-compile-qualified-v3b.json'),
 previous_native_results_sha256=sha(D/'ram-native-v3-results.json'),previous_native_guard_sha256=sha(D/'guard-ram-native-v3.json'),
 previous_failure_only_preload_quota=True,parent_compile_proofs=parents,all_previous_native_dependencies_and_fixtures_rechecked=True,
 controller_sha256=sha(__file__),cgroup=Path('/proc/self/cgroup').read_text())
assert qualified['cgroup']==g['cgroup']
(D/'v4-source-equivalence-qualified.json').write_text(json.dumps(qualified,indent=2)+'\n')
print('source/code equivalence and 10 prior native passes audited',flush=True)
