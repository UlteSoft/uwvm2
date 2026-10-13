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
def worker_context_line(text,group):
 expected='wasi_workers=2 wasi_visits='+('6' if group else '2')+' wasi_tls_restored=1'
 assert expected in text,expected
assert frozen['passed'] and frozen['files']==len(M)==8771 and frozen['manifest_sha256']==sha(manifest) and frozen['source_archive_sha256']==sha(D/'inputs.tar.gz')
assert all(sha(D/'inputs'/p)==h for p,h in M.items())
text=(D/'products/linux-integrated/uwvm2/environment-group-instruction.log').read_text();match=re.search(r'^debug_wasip1_environment_group_runtime PASS checks=([0-9]+) policy=instruction$',text,re.M);assert match;group_checks=int(match[1]);assert group_checks>300
proof=dict(passed=False,round='R30',scope='Genuine FastIO native workers validate candidate WASIp1 per-module memory TLS, nested selection, simultaneous shared environments, restoration and join; custom resolver policy refuses before invocation; both repositories and four native OSes',source_manifest=evidence(manifest),source_archive=evidence(D/'inputs.tar.gz'),source_freeze=evidence(D/'input-freeze-qualified.json'),source_cut='Immutable integration cut with concurrent frame/root and retirement declarations preserved. Later unrelated workspace edits are not covered.',complete_world_restore=False,wasip1_portable_wire_changed=False,portable_migration_requalification='No new directed OS migration claimed. Prior R26 portable metadata migration remains separate evidence.',repos={},historical_negative_tests_preserved=True,group_checks_per_run=group_checks)
total=0;runs=0;regressions=0
for repo in ('uwvm2','uwvm2-ros'):
 platforms={}
 for osname in ('linux','windows','freebsd','macos'):
  nativeplatform='linux-integrated' if osname=='linux' else osname;O=D/'products'/nativeplatform/repo
  artifact=D/(repo+'-'+nativeplatform+'-product-retirement.json');A=read(artifact)
  assert A['passed'] and A['all_payloads_read_back'] 
  relocation=read(D/(repo+'-'+nativeplatform+'-local-cold-qualified.json'));retired=read(D/(repo+'-'+nativeplatform+'-local-cold-retirement.json'))
  assert relocation['passed'] and relocation['all_payloads_read_back'] and relocation['archive_sha256']==A['archive_sha256'] and relocation['payloads']==A['payloads'] and relocation['limit_bytes']==1<<30 and relocation['memory_upper_bytes']<2<<30 and relocation['local_cold_aggregate_limit_bytes']==2<<30 and relocation['local_cold_aggregate_bytes']<2<<30
  assert retired['passed'] and retired['archive_sha256']==A['archive_sha256'] and retired['qualification_sha256']==sha(D/(repo+'-'+nativeplatform+'-local-cold-qualified.json')) and not Path(A['archive']).exists()
  guard(E/('guard-joint-product-retire-r30-'+nativeplatform+'-'+repo+'.json'))
  archive_guard=E/('guard-joint-archive-r30-'+nativeplatform+'-'+repo+'.json');guard(archive_guard)
  guard_path=E/('guard-joint-'+('linux-integrated-r30-'+repo if osname=='linux' else 'cross-r30-'+osname+'-'+repo)+'.json');G=guard(guard_path)
  record=dict(passed=True,qualification_guard=evidence(guard_path),qualification_guard_sampled_rss_peak=G['owned_peak_aggregate_rss_upper_bytes'],archive_guard=evidence(archive_guard),artifact_retirement=evidence(artifact),compiled_product_archive=dict(path=relocation['archive'],sha256=A['archive_sha256'],recovery_qualification=evidence(D/(repo+'-'+nativeplatform+'-local-cold-qualified.json'))),cases=[])
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
     prefix='debug_wasip1_environment_group_runtime' if fixture=='environment-group' else 'debug_wasip1_checkpoint_runtime';checks=group_checks if fixture=='environment-group' else 212
     found=re.search('^'+prefix+r' PASS checks=([0-9]+) policy='+policy+r'$',text,re.M);assert found and int(found[1])==checks
     assert ('PRIVATE_WASIP1_WORLD status=0 environments=2 modules=3 memories=3 shared=1' in text if fixture=='environment-group' else 'JOINT_PREPARATION status=0 ' in text)
     worker_context_line(text,fixture=='environment-group')
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
     checks=group_checks if row['label'].startswith('environment-group-') else 212
     assert row['passed'] and row['exit']==0 and row['checks']==checks and row['pid_reaped'] and row['aggregate_peak_upper_bytes']<2<<30
     log=O/Path(row['log']).name;assert sha(log)==row['log_sha256'];worker_context_line(log.read_text(),row['label'].startswith('environment-group-'))
     record['cases'].append(dict(label=row['label'],checks=checks,log=evidence(log),aggregate_peak_upper_bytes=row['aggregate_peak_upper_bytes']))
   else:
    candidates=[]
    for p in D.glob('vm-'+osname+'-joint-preparation-*/receipt.json'):
     r=read(p)
     if r.get('passed') and r['input_sha256'].get(repo+'-fixture.exe')==q['binary_sha256'] and r['input_sha256'].get(repo+'-environment-group.exe')==q['group_binary_sha256']:candidates.append((p,r))
    assert len(candidates)==1,(repo,osname,len(candidates))
    p,execution=candidates[0];assert execution['qemu_exit']==0 and execution['base_before']==execution['base_after'] and execution['host_kvm_permissions_unchanged'] and execution['kvm']['enabled']
    assert execution['target_source_manifest_sha256']==sha(manifest) and len(execution['tests'])==6
    native_guard=E/('guard-joint-vm-r30-'+osname+'-'+repo+'.json');guard(native_guard)
    record['native_execution']=evidence(p);record['serial_log']=evidence(p.parent/'serial.log');record['native_execution_guard']=evidence(native_guard);record['qemu_guest_ram_bytes']=2<<30
    assert execution['serial_sha256']==sha(p.parent/'serial.log')
    serial=(p.parent/'serial.log').read_text(errors='replace');assert len(re.findall(r'wasi_workers=2\s+wasi_visits=2\s+wasi_tls_restored=1',serial))==4 and len(re.findall(r'wasi_workers=2\s+wasi_visits=6\s+wasi_tls_restored=1',serial))==2
    for case in execution['tests']:
     checks=group_checks if case['fixture']=='environment-group' else (207 if osname=='windows' else 212)
     assert execution['test_statuses'][case['label']] and execution['test_pass_checks'][case['label']]==case['checks']==checks and not case.get('minimum')
     record['cases'].append(dict(label=case['label'],checks=checks,status=0))
  assert len(record['cases'])==6
  record['counted_assertions']=sum(x['checks'] for x in record['cases']);total+=record['counted_assertions'];runs+=len(record['cases']);platforms[osname]=record
 proof['repos'][repo]=platforms
assert total==2*(3*(4*212+2*group_checks)+4*207+2*group_checks) and runs==48 and regressions==8
proof.update(primary_success_verified_native_workers=96,primary_success_verified_module_visits=160,counted_assertions=total,native_program_runs=runs,linux_complete_graph_regression_runs=regressions,policy=read(E/'storage-policy.json'))
proof['cold_storage']=evidence(E.parent/'wasip1-r27-owned-cold-evidence/qualification.json')
history=read(D/'historical-r29-hot-retirement.json');local_history=read(D/'historical-r29-local-qualified.json')
assert history['passed'] and history['bytes']==272601225 and local_history['passed'] and local_history['region_bytes']<1<<30 and local_history['aggregate_bytes']<2<<30
assert history['local_qualification_sha256']==sha(D/'historical-r29-local-qualified.json')
guard(E/'guard-joint-rehome-r29-r30.json')
proof['recovered_r29_linux_products']=dict(qualification=evidence(D/'historical-r29-local-qualified.json'),retirement=evidence(D/'historical-r29-hot-retirement.json'),cleanup_guard=evidence(E/'guard-joint-rehome-r29-r30.json'),test_admission_refusal=evidence(D/'storage-admission-refusal-r30.json'),windows_vm_started_before_cleanup=False)
proof['local_product_cold']=dict(directory='/Users/liyinan/Documents/MacroModel/wasip1-r30-owned-cold-evidence',checked_limit_bytes=1<<30,aggregate_with_historical_region_limit_bytes=2<<30,archives=8,all_archive_payloads_read_back=True,guarded_remote_copy_retirement=True)
proof['linux_fast_path_regression_runs']=2;proof['linux_fast_path_regression_assertions']=20
proof['remaining_requirements']=['Integrate actual old-world worker retirement/join with joint restoration','Enroll actual new startup execution workers and install their live roots during complete publication','Publish the prepared native WASIp1 cache with the complete Wasm world, source, engines and generation; activate per-call module memory under actual restored worker startup','Qualify actual complete-instance restore/replay; prepared_and_discarded never means restored']
proof['passed']=True
(D/'world-install-final-qualified.json').write_text(json.dumps(proof,indent=2)+'\n')
print('R30 private WASIp1 installation four-OS native qualification complete',runs,total,flush=True)
