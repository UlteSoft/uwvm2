from pathlib import Path
import hashlib,json,tarfile,ast
P=Path('/Users/liyinan/Documents/MacroModel/src');W=P/'uwvm2/test/0018.debugger/wasip1-root-identity-r43-work'
R=W.parent/'wasip1-closed-worker-r42-work';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
previous=json.loads((R/'source-manifest-v2.json').read_text());local=json.loads((W/'local-delta.json').read_text());before=json.loads((W/'before.json').read_text())
hunks=json.loads((W/'cpp-hunks.json').read_text());qualified={};qualified_before={};comparison={}
for k,h in local.items():
 assert sha(P/k)==h
 if k.endswith('uwvm_runtime.default.cpp'):
  repo=k.split('/')[0];original=W/('frozen-'+repo+'-before.cpp');data=original.read_text()
  for row in hunks:
   assert data.count(row['before'])==1,(k,row['before'][:100]);data=data.replace(row['before'],row['after'],1)
  target=W/'qualified-v2'/k;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(data)
  qualified[k]=sha(target);qualified_before[k]=dict(sha256=sha(original))
  comparison[k]=dict(local_before=before[k]['sha256'],local_after=h,qualified_before=sha(original),qualified_after=sha(target),owned_hunks=len(hunks),unowned_local_changes_preserved=True)
 else:
  qualified[k]=h
  if k in before:qualified_before[k]=before[k]
extra={}
for repo in ('uwvm2','uwvm2-ros'):
 for rel in ('test/0017.runtime/debug_checkpoint_actual_observation_runtime.cc','test/0017.runtime/debug_native_code_runtime.cc',
             'test/0017.runtime/fixtures/checkpoint_actual_continuation.wat','test/0017.runtime/debug_activation_runtime.wat'):
  k=repo+'/'+rel;assert k not in local and k not in previous;extra[k]=sha(P/k);qualified[k]=extra[k]
unchanged={k:h for k,h in previous.items() if k not in qualified};assert all(sha(P/k)==h for k,h in unchanged.items())
manifest=dict(unchanged,**qualified)
eq=dict(previous_manifest_sha256=sha(R/'source-manifest-v2.json'),unchanged_inputs=unchanged,delta=qualified,before=qualified_before,
        local_runtime_projection=comparison,local_current_delta=local,all_local_unowned_cpp_hunks_preserved=True,
        added_unchanged_regression_inputs=extra,owned_delta_files=22)
(W/'source-manifest-v2.json').write_text(json.dumps(manifest,indent=2)+'\n');(W/'source-equivalence-v2.json').write_text(json.dumps(eq,indent=2)+'\n')
with tarfile.open(W/'delta-v2.tar.gz','w:gz') as out:
 for k in qualified:out.add(W/'qualified-v2'/k if k in comparison else P/k,arcname=k,recursive=False)
 for k in qualified_before:
  path=W/('frozen-'+k.split('/')[0]+'-before.cpp') if k in comparison else W/'before'/k
  out.add(path,arcname='before/'+k,recursive=False)
s=(W/'run_matrix-v1.py').read_text()
s=s.replace('S=D/"source-v1"','S=D/"source-v2"')
for name in ('source-manifest','source-equivalence','delta','native-storage','results','qualified'):
 s=s.replace('D/"'+name+'-v1.', 'D/"'+name+'-v2.')
s=s.replace('uwvm-checkpoint-r43-v1-','uwvm-checkpoint-r43-v2-').replace('"-v1.log"','"-v2.log"')
s=s.replace('len(manifest)==72 and len(eq["unchanged_inputs"])==50 and len(eq["delta"])==22','len(manifest)==80 and len(eq["unchanged_inputs"])==50 and len(eq["delta"])==30')
s=s.replace('"debug_checkpoint_worker_registry_reuse")','"debug_checkpoint_worker_registry_reuse","checkpoint_actual_continuation","debug_activation_runtime")')
s=s.replace('  if not wat.exists():wat=parent/', '  if name=="debug_activation_runtime":wat=root/"test/0017.runtime/debug_activation_runtime.wat"\n  if not wat.exists():wat=parent/')
needle='("private-generation2","debug_checkpoint_private_code_handoff_generation_runtime.cc",(("normal","debug_checkpoint_retirement_cohort"),)))'
assert s.count(needle)==1
s=s.replace(needle,'("private-generation2","debug_checkpoint_private_code_handoff_generation_runtime.cc",(("normal","debug_checkpoint_retirement_cohort"),)),\n        ("observation","debug_checkpoint_actual_observation_runtime.cc",(("normal","checkpoint_actual_continuation"),)),\n        ("native-code","debug_native_code_runtime.cc",(("normal","debug_activation_runtime"),)))')
s=s.replace('sum(r["actual_native_runtime_execution"] and r["passed"] for r in rows)==28','sum(r["actual_native_runtime_execution"] and r["passed"] for r in rows)==36')
s=s.replace('fresh_native_cases=28','fresh_native_cases=36,no_profile_native_code_cases=4,observation_only_checkpoint_cases=4')
s=s.replace(' if native and stage.startswith(("core-","wasip1-")):', ''' if native and stage.startswith("observation-"):assert "checkpoint_actual_observation: PASS" in evidence
 if native and stage.startswith("native-code-"):assert "debug_native_code_runtime: PASS" in evidence
 if native and stage.startswith("active-generation2-"):assert "ROOT_IDENTITY private_generation2=1 live_seal=0 PASS" in evidence
 if native and stage.startswith(("core-","wasip1-")):''')
(W/'run_matrix-v2.py').write_text(s);ast.parse(s)
g=(W/'guard_matrix-v1.py').read_text().replace('run_matrix-v1.py','run_matrix-v2.py').replace('native-storage-v1.json','native-storage-v2.json').replace('uwvm-checkpoint-r43-v1-','uwvm-checkpoint-r43-v2-').replace('guard-matrix-v1.json','guard-matrix-v2.json')
(W/'guard_matrix-v2.py').write_text(g);ast.parse(g)
print(dict(manifest=len(manifest),unchanged=len(unchanged),qualified=len(qualified),owned_delta=22,added_regression_inputs=len(extra)))
