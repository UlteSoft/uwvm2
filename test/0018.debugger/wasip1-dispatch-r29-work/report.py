from pathlib import Path
import json,hashlib,re,os
D=Path(__file__).parent;E=D.parent.parent
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text())
def evidence(p):p=Path(p);return dict(path=str(p),sha256=sha(p))
def guard(p):
 g=read(p);assert g['passed'] and g['actual_root_exit']==0 and all(x['pidfd_retired'] for x in g['retirement'])
 assert g['limits']['memory.max']=='68719476736' and g['limits']['memory.swap.max']=='0'
 return g
manifest=D/'inputs.json';M=read(manifest);frozen=read(D/'input-freeze-qualified.json')
assert frozen['passed'] and frozen['files']==len(M)==8767 and frozen['manifest_sha256']==sha(manifest) and frozen['source_archive_sha256']==sha(D/'inputs.tar.gz')
assert all(sha(D/'inputs'/p)==h for p,h in M.items())
text=(D/'products/linux-integrated/uwvm2/environment-group-instruction.log').read_text();match=re.search(r'^debug_wasip1_environment_group_runtime PASS checks=([0-9]+) policy=instruction$',text,re.M);assert match;group_checks=int(match[1]);assert group_checks>300
proof=dict(passed=False,round='R29',scope='Final private WASIp1 native dispatch contexts and external trace binding policy; genuine module/environment/memory sharing; both repositories, four native operating systems',source_manifest=evidence(manifest),source_archive=evidence(D/'inputs.tar.gz'),source_freeze=evidence(D/'input-freeze-qualified.json'),source_cut='Immutable integration cut with concurrent frame/root and retirement declarations preserved. Later unrelated workspace edits are not covered.',complete_world_restore=False,wasip1_portable_wire_changed=False,portable_migration_requalification='No new directed OS migration claimed. Prior R26 portable metadata migration remains separate evidence.',repos={},historical_negative_tests_preserved=True,group_checks_per_run=group_checks)
total=0;runs=0;regressions=0
for repo in ('uwvm2','uwvm2-ros'):
 platforms={}
 for osname in ('linux','windows','freebsd','macos'):
  nativeplatform='linux-integrated' if osname=='linux' else osname;O=D/'products'/nativeplatform/repo
  artifact=D/(repo+'-'+nativeplatform+'-product-retirement.json');A=read(artifact)
  assert A['passed'] and A['all_payloads_read_back'] and sha(A['archive'])==A['archive_sha256']
  archive_guard=E/('guard-joint-archive-r29-'+nativeplatform+'-'+repo+'.json');guard(archive_guard)
  guard_path=E/('guard-joint-'+('linux-integrated-r29-'+repo if osname=='linux' else 'cross-r29-'+osname+'-'+repo)+'.json');G=guard(guard_path)
  record=dict(passed=True,qualification_guard=evidence(guard_path),qualification_guard_sampled_rss_peak=G['owned_peak_aggregate_rss_upper_bytes'],archive_guard=evidence(archive_guard),artifact_retirement=evidence(artifact),compiled_product_archive=dict(path=A['archive'],sha256=A['archive_sha256']),cases=[])
  group=read(O/'environment-group-qualified.json');assert group['passed'] and group['source_manifest_sha256']==sha(manifest)
  assert all(sha(k)==h for k,h in group['dependencies'].items());record['group_qualification']=evidence(O/'environment-group-qualified.json')
  results=read(O/'results.json');assert all(r['passed'] and r['exit']==0 and sha(O/(r['name']+'.log'))==r['log_sha256'] for r in results)
  record['build_and_test_results']=evidence(O/'results.json')
  for label in ('runtime','host-api'):
   dep=O/(label+'-qualified.json');qdep=read(dep);assert all(sha(k)==h for k,h in qdep['dependencies'].items());assert A['payloads'][label+'.o']==qdep['object_sha256'];record[label+'_qualification']=evidence(dep)
  if osname=='linux':
   q=read(O/'fixture-qualified.json');assert q['source_manifest_sha256']==sha(manifest) and A['payloads']['fixture']==q['binary_sha256']
   assert A['payloads']['environment-group']==group['binary_sha256']
   assert all(sha(k)==h for k,h in q['dependencies'].items());record['fixture_qualification']=evidence(O/'fixture-qualified.json')
   for fixture in ('checkpoint','aliases','environment-group'):
    for policy in ('instruction','unwind'):
     log=O/(({'checkpoint':'native-','aliases':'aliases-','environment-group':'environment-group-'}[fixture])+policy+'.log');text=log.read_text()
     prefix='debug_wasip1_environment_group_runtime' if fixture=='environment-group' else 'debug_wasip1_checkpoint_runtime';checks=group_checks if fixture=='environment-group' else 210
     found=re.search('^'+prefix+r' PASS checks=([0-9]+) policy='+policy+r'$',text,re.M);assert found and int(found[1])==checks
     assert ('PRIVATE_WASIP1_WORLD status=0 environments=2 modules=3 memories=3 shared=1' in text if fixture=='environment-group' else 'JOINT_PREPARATION status=0 ' in text)
     record['cases'].append(dict(fixture=fixture,policy=policy,checks=checks,log=evidence(log)))
   unit=read(O/'wasip1-memory-binding-qualified.json');assert unit['passed'] and unit['checks']==10 and unit['source_manifest_sha256']==sha(manifest)
   assert A['payloads']['wasip1-memory-binding']==unit['binary_sha256'] and sha(O/'wasip1-memory-binding.log')==unit['log_sha256']
   assert all(sha(k)==h for k,h in unit['dependencies'].items());record['default_context_fast_path_regression']=evidence(O/'wasip1-memory-binding-qualified.json')
   record['complete_graph_regressions']=[]
   for fixture in ('debug_checkpoint_complete_instance','debug_checkpoint_complete_preload'):
    for policy in ('instruction','unwind'):
     log=O/(fixture+'-'+policy+'.log');text=log.read_text()
     assert 'positive_episodes=2' in text and 'actual_census=2' in text and 'whole_restore=0' in text and 'WORLD_PREPARATION status=0 ' in text and 'frames=2 roots=12' in text
     record['complete_graph_regressions'].append(dict(fixture=fixture,policy=policy,log=evidence(log)));regressions+=1
  else:
   q=read(O/'qualified.json');assert q['passed'] and q['source_manifest_sha256']==sha(manifest) and not q['actual_native_execution']
   assert A['payloads']['fixture.exe']==q['binary_sha256'] and A['payloads']['environment-group.exe']==q['group_binary_sha256']==group['binary_sha256']
   assert all(sha(k)==h for k,h in q['dependencies'].items());record['fixture_qualification']=evidence(O/'qualified.json')
   if osname=='macos':
    execution=read(O/'receipt.json');assert execution['passed'] and execution['binary_sha256']==q['binary_sha256'] and execution['group_binary_sha256']==q['group_binary_sha256'] and execution['source_manifest_sha256']==sha(manifest)
    assert execution['execution_arch']=='arm64 native' and execution['host_arch']=='arm64' and execution['owned_ramdisk_bytes']==0
    assert execution['execution_sha256']==sha(O/'native-execution.json') and execution['qualified_sha256']==sha(O/'qualified.json') and execution['group_qualified_sha256']==sha(O/'environment-group-qualified.json')
    assert execution['controller_sha256']==sha(O/'macos_native.py') and execution['monitor_sha256']==sha(O/'macos_monitor.py') and execution['nofork_probe_sha256']==sha(O/'nofork-probe.log')
    record['native_execution']=evidence(O/'receipt.json');record['native_execution_architecture']=execution['execution_arch'];record['native_memory_limit_bytes']=2<<30
    assert len(execution['rows'])==6
    for row in execution['rows']:
     checks=group_checks if row['label'].startswith('environment-group-') else 210
     assert row['passed'] and row['exit']==0 and row['checks']==checks and row['pid_reaped'] and row['aggregate_peak_upper_bytes']<2<<30
     log=O/Path(row['log']).name;assert sha(log)==row['log_sha256']
     record['cases'].append(dict(label=row['label'],checks=checks,log=evidence(log),aggregate_peak_upper_bytes=row['aggregate_peak_upper_bytes']))
   else:
    candidates=[]
    for p in D.glob('vm-'+osname+'-joint-preparation-*/receipt.json'):
     r=read(p)
     if r.get('passed') and r['input_sha256'].get(repo+'-fixture.exe')==q['binary_sha256'] and r['input_sha256'].get(repo+'-environment-group.exe')==q['group_binary_sha256']:candidates.append((p,r))
    assert len(candidates)==1,(repo,osname,len(candidates))
    p,execution=candidates[0];assert execution['qemu_exit']==0 and execution['base_before']==execution['base_after'] and execution['host_kvm_permissions_unchanged'] and execution['kvm']['enabled']
    assert execution['target_source_manifest_sha256']==sha(manifest) and len(execution['tests'])==6
    native_guard=E/('guard-joint-vm-r29-'+osname+'-'+repo+'.json');guard(native_guard)
    record['native_execution']=evidence(p);record['serial_log']=evidence(p.parent/'serial.log');record['native_execution_guard']=evidence(native_guard);record['qemu_guest_ram_bytes']=2<<30
    assert execution['serial_sha256']==sha(p.parent/'serial.log')
    for case in execution['tests']:
     checks=group_checks if case['fixture']=='environment-group' else (205 if osname=='windows' else 210)
     assert execution['test_statuses'][case['label']] and execution['test_pass_checks'][case['label']]==case['checks']==checks and not case.get('minimum')
     record['cases'].append(dict(label=case['label'],checks=checks,status=0))
  assert len(record['cases'])==6
  record['counted_assertions']=sum(x['checks'] for x in record['cases']);total+=record['counted_assertions'];runs+=len(record['cases']);platforms[osname]=record
 proof['repos'][repo]=platforms
assert total==2*(3*(4*210+2*group_checks)+4*205+2*group_checks) and runs==48 and regressions==8
proof.update(counted_assertions=total,native_program_runs=runs,linux_complete_graph_regression_runs=regressions,policy=read(E/'storage-policy.json'))
proof['cold_storage']=evidence(E.parent/'wasip1-r27-owned-cold-evidence/qualification.json')
proof['historical_local_cold']=evidence(D/'historical-products-local-qualified.json')
retirement=read(D/'historical-products-hot-retirement.json');assert retirement['passed'] and retirement['retired_bytes']==770193276
historical_guard=E/'guard-joint-historical-retire-r29.json';guard(historical_guard);proof['historical_products_hot_retirement']=evidence(D/'historical-products-hot-retirement.json');proof['historical_products_retirement_guard']=evidence(historical_guard)
correction=read(D/'source-fixture-observer-correction.json');assert correction['passed'] and correction['new_manifest_sha256']==sha(manifest) and len(correction['changes'])==2
proof['fixture_observer_correction']=evidence(D/'source-fixture-observer-correction.json')
proof['linux_fast_path_regression_runs']=2;proof['linux_fast_path_regression_assertions']=20
proof['remaining_requirements']=['Integrate actual old-world worker retirement/join with joint restoration','Install live GC roots and enroll actual new startup workers','Publish the prepared native WASIp1 cache with the complete Wasm world, source, engines and generation; activate per-call module memory under actual restored worker startup','Qualify actual complete-instance restore/replay; prepared_and_discarded never means restored']
proof['passed']=True
(D/'world-install-final-qualified.json').write_text(json.dumps(proof,indent=2)+'\n')
print('R29 private WASIp1 installation four-OS native qualification complete',runs,total,flush=True)
