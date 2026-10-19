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
  target=W/'qualified-v3'/k;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(data)
  qualified[k]=sha(target);qualified_before[k]=dict(sha256=sha(original))
  comparison[k]=dict(local_before=before[k]['sha256'],local_after=h,qualified_before=sha(original),qualified_after=sha(target),owned_hunks=len(hunks),unowned_local_changes_preserved=True)
 else:
  qualified[k]=h
  if k in before:qualified_before[k]=before[k]
extra=json.loads((W/'source-equivalence-v2.json').read_text())['added_unchanged_regression_inputs']
assert all(sha(P/k)==h for k,h in extra.items());qualified.update(extra)
unchanged={k:h for k,h in previous.items() if k not in qualified};assert all(sha(P/k)==h for k,h in unchanged.items())
manifest=dict(unchanged,**qualified)
eq=dict(previous_manifest_sha256=sha(R/'source-manifest-v2.json'),unchanged_inputs=unchanged,delta=qualified,before=qualified_before,
        local_runtime_projection=comparison,local_current_delta=local,all_local_unowned_cpp_hunks_preserved=True,
        added_unchanged_regression_inputs=extra,owned_delta_files=22)
(W/'source-manifest-v3.json').write_text(json.dumps(manifest,indent=2)+'\n');(W/'source-equivalence-v3.json').write_text(json.dumps(eq,indent=2)+'\n')
with tarfile.open(W/'delta-v3.tar.gz','w:gz') as out:
 for k in qualified:out.add(W/'qualified-v3'/k if k in comparison else P/k,arcname=k,recursive=False)
 for k in qualified_before:
  path=W/('frozen-'+k.split('/')[0]+'-before.cpp') if k in comparison else W/'before'/k
  out.add(path,arcname='before/'+k,recursive=False)
s=(W/'run_matrix-v2.py').read_text().replace('S=D/"source-v2"','S=D/"source-v3"')
for name in ('source-manifest','source-equivalence','delta','native-storage','results','qualified'):
 s=s.replace('D/"'+name+'-v2.', 'D/"'+name+'-v3.')
s=s.replace('uwvm-checkpoint-r43-v2-','uwvm-checkpoint-r43-v3-').replace('"-v2.log"','"-v3.log"')
(W/'run_matrix-v3.py').write_text(s);ast.parse(s)
g=(W/'guard_matrix-v2.py').read_text().replace('run_matrix-v2.py','run_matrix-v3.py').replace('native-storage-v2.json','native-storage-v3.json').replace('uwvm-checkpoint-r43-v2-','uwvm-checkpoint-r43-v3-').replace('guard-matrix-v2.json','guard-matrix-v3.json')
(W/'guard_matrix-v3.py').write_text(g);ast.parse(g)
print(dict(manifest=len(manifest),unchanged=len(unchanged),qualified=len(qualified),owned_delta=22,added_regression_inputs=len(extra)))
