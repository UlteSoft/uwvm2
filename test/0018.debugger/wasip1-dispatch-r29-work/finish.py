from pathlib import Path
import subprocess,sys,json,hashlib,tarfile,time
L=Path(__file__).parent;B=L.parents[3]
E=Path("/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17");D=Path("/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/rounds/wasip1-dispatch-20261007-r29")
ssh=['ssh','-i',str(Path.home()/'.ssh/id_ed25519'),'macromodel@100.123.133.75']
scp=['scp','-i',str(Path.home()/'.ssh/id_ed25519')]
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
rows=[]
def run(label,args):
 print('R29 FINISH',label,flush=True);start=time.monotonic();p=subprocess.run(args)
 rows.append(dict(label=label,argv=args,exit=p.returncode,seconds=time.monotonic()-start))
 (L/'finish-progress.json').write_text(json.dumps(rows,indent=2)+'\n');assert p.returncode==0,label
phases=('report','storage')
if sys.argv[1:]==['--resume-after-report']:
 previous=json.loads((L/'initial-finish-progress.json').read_text())
 assert previous[0]['label']=='report' and previous[0]['exit']==0 and previous[-1]['label']=='storage' and previous[-1]['exit']!=0
 rows.extend(previous);phases=('storage',)
else:assert not sys.argv[1:]
for phase in phases:run(phase,[*ssh,'python3','-u',str(E/('guard-joint-'+phase+'-r29.py')),'joint-'+phase+'-r29'])
remote={
 D/'world-install-final-qualified.json':'world-install-final-qualified.json',
 D/'final-storage-qualified.json':'final-storage-qualified.json',
 D/'input-freeze-qualified.json':'input-freeze-qualified.json',
 D/'historical-products-hot-retirement.json':'historical-products-hot-retirement.json',
 D/'source-fixture-observer-correction.json':'source-fixture-observer-correction.json',
 E/'guard-joint-report-r29.json':'guard-joint-report-r29.json',
 E/'guard-joint-storage-r29.json':'guard-joint-storage-r29.json',
 E/'guard-joint-historical-retire-r29.json':'guard-joint-historical-retire-r29.json',
 E/'storage-policy.json':'storage-policy.json',
 E.parent/'wasip1-r27-owned-cold-evidence/qualification.json':'cold-storage-qualification.json',
 E.parent/'wasip1-active-environment.json':'active-environment-final-r29.json'
}
for repo in ('uwvm2','uwvm2-ros'):remote[D/(repo+'-macos-product-retirement.json')]=repo+'-macos-product-retirement.json'
for path,name in remote.items():run('fetch-'+name,[*scp,'macromodel@100.123.133.75:'+str(path),str(L/name)])
q=json.loads((L/'world-install-final-qualified.json').read_text());assert q['passed'] and q['counted_assertions']==14392
for phase in ('report','storage','historical-retire'):
 g=json.loads((L/('guard-joint-'+phase+'-r29.json')).read_text());assert g['passed'] and g['actual_root_exit']==0 and all(r['pidfd_retired'] for r in g['retirement'])
assert sha(L/'inputs.json')==q['source_manifest']['sha256']
run('prepare-local-delivery',[sys.executable,'-u',str(L/'prepare_delivery.py')])
run('stage-delivery',[*scp,'-r',str(L/'delivery'),'macromodel@100.123.133.75:'+str(D)+'/'])
run('qualify-delivery',[*ssh,'python3','-u',str(E/'guard-joint-delivery-r29.py'),'joint-delivery-r29'])
for path in (D/'delivery-metadata-receipt.json',D/'delivery-metadata.tar.gz',E/'guard-joint-delivery-r29.json'):
 run('fetch-'+path.name,[*scp,'macromodel@100.123.133.75:'+str(path),str(L/path.name)])
receipt=json.loads((L/'delivery-metadata-receipt.json').read_text());g=json.loads((L/'guard-joint-delivery-r29.json').read_text())
assert receipt['passed'] and g['passed'] and g['actual_root_exit']==0 and all(x['pidfd_retired'] for x in g['retirement'])
assert sha(L/'delivery-metadata.tar.gz')==receipt['archive_sha256']
observed={}
with tarfile.open(L/'delivery-metadata.tar.gz','r|gz') as t:
 for m in t:
  assert m.isfile() and Path(m.name).name==m.name and m.name in receipt['files'] and m.name not in observed
  with t.extractfile(m) as f:h=hashlib.file_digest(f,'sha256').hexdigest()
  assert h==receipt['files'][m.name]['sha256'] and m.size==receipt['files'][m.name]['bytes']
  assert sha(L/'delivery'/m.name)==h
  for repo in ('uwvm2','uwvm2-ros'):assert sha(B/repo/'test/0018.debugger'/m.name)==h
  observed[m.name]=h
assert set(observed)==set(receipt['files'])
for repo in ('uwvm2','uwvm2-ros'):
 (B/repo/'test/0018.debugger/wasip1_private_dispatch_r29_delivery_receipt.json').write_bytes((L/'delivery-metadata-receipt.json').read_bytes())
 (B/repo/'test/0018.debugger/wasip1_private_dispatch_r29_delivery_guard.json').write_bytes((L/'guard-joint-delivery-r29.json').read_bytes())
local=dict(passed=True,archive_sha256=receipt['archive_sha256'],all_payloads_read_back=True,files=len(observed),both_repositories_match=True,native_program_runs=48,counted_assertions=14392)
(L/'local-delivery-qualified.json').write_text(json.dumps(local,indent=2)+'\n')
print('R29 FINAL QUALIFIED',json.dumps(local),flush=True)

