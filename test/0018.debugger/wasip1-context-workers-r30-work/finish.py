from pathlib import Path
import sys,subprocess,json,hashlib,tarfile,shlex,os,resource
B=Path('/Users/liyinan/Documents/MacroModel/src');L=Path(__file__).parent
E=Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17');D=E/'rounds/wasip1-context-workers-20261007-r30'
ssh=['ssh','-i',str(Path.home()/'.ssh/id_ed25519'),'macromodel@100.123.133.75'];scp=['scp','-i',str(Path.home()/'.ssh/id_ed25519')]
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
def get(remote,local):subprocess.run([*scp,'macromodel@100.123.133.75:'+str(remote),str(local)],check=True)
def guard(phase):subprocess.run([*ssh,'python3','-u',str(E/('guard-joint-'+phase+'-r30.py')),'joint-'+phase+'-r30'],check=True)
guard('report');get(D/'world-install-final-qualified.json',L/'world-install-final-qualified.json')
Q=json.loads((L/'world-install-final-qualified.json').read_text());assert Q['passed'] and Q['native_program_runs']==48 and Q['counted_assertions']==14472 and not Q['complete_world_restore']
C=Path('/Users/liyinan/Documents/MacroModel/wasip1-r30-owned-cold-evidence');old=C.with_name('wasip1-r29-owned-cold-evidence')
assert C.is_dir() and old.is_dir() and not C.is_symlink() and not old.is_symlink()
assert sum(p.stat().st_size for p in C.iterdir())<1<<30 and sum(p.stat().st_size for p in old.iterdir())<1<<30
assert sum(p.stat().st_size for p in C.iterdir())+sum(p.stat().st_size for p in old.iterdir())<2<<30
archives={}
for repo in ('uwvm2','uwvm2-ros'):
 for platform in ('linux-integrated','windows','freebsd','macos'):
  qpath=L/(repo+'-'+platform+'-local-cold-qualified.json');R=json.loads(qpath.read_text());p=Path(R['archive'])
  assert R['passed'] and p.parent==C and p.is_file() and not p.is_symlink() and p.stat().st_mode&0o222==0 and sha(p)==R['archive_sha256']
  actual={}
  with tarfile.open(p,'r|gz') as t:
   for m in t:
    assert m.isfile() and m.name in R['payloads'] and m.name not in actual
    with t.extractfile(m) as inp:actual[m.name]=hashlib.file_digest(inp,'sha256').hexdigest()
  assert actual==R['payloads']
  archives[p.name]=dict(bytes=p.stat().st_size,sha256=sha(p),qualification_sha256=sha(qpath),all_payloads_read_back=True)
assert set(archives)==set(p.name for p in C.iterdir())
upper=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+(128<<20);assert upper<2<<30
cold=dict(passed=True,directory=str(C),archives=archives,all_payloads_read_back=True,limit_bytes=1<<30,bytes=sum(v['bytes'] for v in archives.values()),aggregate_limit_bytes=2<<30,aggregate_bytes=sum(p.stat().st_size for p in C.iterdir())+sum(p.stat().st_size for p in old.iterdir()),memory_upper_bytes=upper)
(L/'local-cold-final-qualified.json').write_text(json.dumps(cold,indent=2)+'\n');C.chmod(0o555)
paths=['src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_world_wasip1_workers.h','src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_world_wasip1.h','src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_world_preparation_api.h','src/uwvm2/runtime/lib/uwvm_runtime.h','test/0017.runtime/debug_wasip1_checkpoint_runtime.cc','test/0017.runtime/debug_wasip1_environment_group_runtime.cc']
M=json.loads((L/'inputs.json').read_text());scope={}
needles={paths[0]:['class private_wasip1_dispatch_workers final','workers_[0].held_module=first'],paths[1]:['workers.validate(maximum_workers)','original.wasip1_memory_resolver!=storage::resolve_current_wasip1_memory'],paths[2]:['request.maximum_private_wasip1_workers','out.verified_wasip1_dispatch_module_visits='],paths[3]:['maximum_private_wasip1_workers{2u}','verified_wasip1_dispatch_module_visits'],paths[4]:['custom memory resolver is refused without invocation','worker_quota.maximum_private_wasip1_workers=1u'],paths[5]:['actual native workers simultaneously select shared environment']}
for repo in ('uwvm2','uwvm2-ros'):
 for n in paths:
  p=B/repo/n;text=p.read_text();assert all(needle in text for needle in needles[n]);key=repo+'/'+n
  scope[key]=dict(qualified_sha256=M[key],current_sha256=sha(p),matches_qualified_cut=sha(p)==M[key],r30_changes_present=True)
(L/'workspace_scope.json').write_text(json.dumps(dict(passed=True,source_manifest_sha256=sha(L/'inputs.json'),paths=scope,source_cut_only=True,peer_changes_preserved=True),indent=2)+'\n')
guard('storage')
for name in ('input-freeze-qualified.json','final-storage-qualified.json','historical-r29-local-qualified.json','historical-r29-hot-retirement.json'):
 get(D/name,L/name)
for phase in ('inputs','report','storage','rehome-r29'):
 get(E/('guard-joint-'+phase+'-r30.json'),L/('guard-joint-'+phase+'-r30.json'))
S=json.loads((L/'final-storage-qualified.json').read_text());assert S['passed'] and S['directory_allocated_bytes']<6<<30
mac=max(c['aggregate_peak_upper_bytes'] for r in Q['repos'].values() for c in r['macos']['cases'])
rows=[]
for repo,data in Q['repos'].items():
 for osname,r in data.items():rows.append(f"| {repo} | {osname} | {len(r['cases'])} | {r['counted_assertions']} |")
differences=sum(not r['matches_qualified_cut'] for r in scope.values())
markdown=f'''# WASIp1 private dispatch workers — R30

Both repositories pass the four native OS matrix: **48 program runs and
14,472 counted assertions**, plus **8 Linux complete-instance/preload regressions**
and **2 Linux memory-binding unit runs / 20 assertions**.

| Repository | Native OS | WASIp1 runs | Counted assertions |
|---|---|---:|---:|
{chr(10).join(rows)}

## Implemented behavior

Private preparation creates two genuine `fast_io::native_thread` workers. Each
visits every import-visible module and uses the actual scoped environment and
memory binding. It checks explicit null masking, nested environment/module
switching and restoration. For shared environments with different module
memories, the workers hold both TLS scopes concurrently until all arrivals have
been inspected. The full cohort is released, scope restoration is checked and
both native workers join before private candidate destruction.

`maximum_private_wasip1_workers` defaults to 2. A smaller bound returns
`wasip1_preparation_declined` / `resource_limit` before native thread creation.
An unknown custom memory resolver returns `unsupported_resource`; a spy in the
real fixture confirms zero callback invocations. Failure results keep the
dispatch verification fields zero/false, including the later root-stage refusal.

Successful preparation reports `verified_wasip1_dispatch_workers`,
`verified_wasip1_dispatch_module_visits` and `wasip1_dispatch_tls_restored`.
The main success result in every run certifies two joined workers: **96 primary
verified workers / 160 primary module visits** across this matrix. Further
positive preparations inside the fixtures are not included in those totals.

Checkpoint/alias runs check one actual module; environment-group runs check
three actual modules, two genuine environments and one shared environment alias.
Both LLVM exception policies (`instruction`, `unwind`) run on each native OS.
Windows/FreeBSD use QEMU with KVM and read-only base disks; Linux and all cross
compilation run in the original 64 GiB cgroup, swap disabled. Native macOS is
arm64 under the 2 GiB aggregate controller/test bound and a verified no-fork
profile. Its largest recorded upper bound is {mac} bytes ({mac/(1<<20):.2f} MiB).
C++ thread and I/O operations use FastIO.

## Source and storage qualification

The immutable integration cut contains {len(M)} files. Its manifest SHA-256 is
`{sha(L/'inputs.json')}` and source archive SHA-256 is
`{sha(L/'inputs.tar.gz')}`. Source dependencies, objects, binaries, SDK inputs,
native logs and guard receipts are verified. Concurrent peers' root-worker and
per-call WASIp1 resolver changes are preserved. At delivery, {differences} of
the 12 relevant workspace source paths differ from this cut; all R30 additions
remain present. Later workspace edits are outside the measured qualification.

The Linux hot filesystem is bounded at 8 GiB, with 6 GiB admission and 7 GiB
stopping thresholds. Closing allocated usage is {S['directory_allocated_bytes']}
bytes ({S['directory_allocated_bytes']/(1<<30):.3f} GiB); volume free space is
{S['owned_volume_free_bytes']} bytes ({S['owned_volume_free_bytes']/(1<<30):.3f} GiB).
Existing Linux cold evidence retains its checked 2 GiB cap. A Windows VM
admission was refused before the VM or its test child started when hot allocated usage
reached 6,617,608,192 bytes. Two own R29 Linux product archives (272,601,225
bytes) were fully recovered in the remaining space of the prior local 1 GiB
region, then retired by a dedicated cleanup-only cgroup guard under the existing
7 GiB running stop. The 6 GiB test admission, 7 GiB stopping threshold, memory
limits and all other reserves remain unchanged. The prior six archives and
their original immutable qualification are preserved; the extension has separate
qualification and retirement receipts.

The ROS native macOS admission also refused to launch while free space was
below its unchanged 4 GiB reserve. Two already-passing, archived uwvm2 macOS
duplicates (562,957,904 bytes) were removed after complete archive readback.
Their source, test receipts and recoverable compiled products remain available.

All eight R30 compiled-product archives have whole-file and every-payload
readback qualification in `{C}`: {cold['bytes']} bytes / checked 1 GiB cap.
Together with the prior local cold region, {cold['aggregate_bytes']} bytes /
checked 2 GiB cap. Only this task's matching remote archive copies and raw
products are retired after authenticated cgroup verification and local recovery
qualification. Source, SDK evidence, logs and receipts remain available. Cold
caps are checked limits, not separate physical filesystem quotas.

## Remaining work

Complete joint world restore/publication and restored guest replay remain
unfinished. These are finite private preparation workers and do not issue a
restored execution credential. Actual old-world worker retirement, installation
of new execution workers/roots, and atomic publication of source, stores,
engines, generation and the private WASIp1 cache still need integration and
end-to-end restoration tests. This round does not change the portable WASIp1
wire format or claim new directed OS migrations. A Wasm checkpoint still
requires simultaneous WASIp1 capture at the same cooperative stop to include
FD/environment state; that reminder remains in the tested runtime logs.
'''
(L/'report-draft.md').write_text(markdown)
P=L/'delivery';P.mkdir()
files={'wasip1_private_dispatch_r30_test_report.md':L/'report-draft.md','wasip1_private_dispatch_r30_test_report.json':L/'world-install-final-qualified.json','wasip1_private_dispatch_r30_source_inputs.json':L/'inputs.json','wasip1_private_dispatch_r30_source_cut.json':L/'local-source-cut-qualified.json','wasip1_private_dispatch_r30_source_freeze.json':L/'input-freeze-qualified.json','wasip1_private_dispatch_r30_workspace_scope.json':L/'workspace_scope.json','wasip1_private_dispatch_r30_local_cold.json':L/'local-cold-final-qualified.json','wasip1_private_dispatch_r30_storage.json':L/'final-storage-qualified.json'}
for phase in ('inputs','report','storage','rehome-r29'):files['wasip1_private_dispatch_r30_guard_'+phase+'.json']=L/('guard-joint-'+phase+'-r30.json')
for repo in ('uwvm2','uwvm2-ros'):files['wasip1_private_dispatch_r30_'+repo.replace('-','_')+'_macos.json']=L/(repo+'-macos-native-evidence.json')
files['wasip1_private_dispatch_r30_recovered_history.json']=L/'historical-r29-local-qualified.json'
files['wasip1_private_dispatch_r30_history_retirement.json']=L/'historical-r29-hot-retirement.json'
files['wasip1_private_dispatch_r30_storage_refusal.json']=L/'storage-admission-refusal-r30.json'
files['wasip1_private_dispatch_r30_early_local_retirement.json']=L/'early-local-product-retirement.json'
manifest={}
for name,p in files.items():
 data=p.read_bytes();(P/name).write_bytes(data);manifest[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
manifest_name='wasip1_private_dispatch_r30_delivery_inputs.json';(P/manifest_name).write_text(json.dumps(manifest,indent=2)+'\n')
subprocess.run([*ssh,'mkdir','-m','700',str(D/'delivery')],check=True)
subprocess.run([*scp,*map(str,P.iterdir()),'macromodel@100.123.133.75:'+str(D/'delivery')+'/'],check=True)
guard('delivery')
get(D/'delivery-metadata.tar.gz',L/'delivery-metadata.tar.gz');get(D/'delivery-metadata-receipt.json',L/'delivery-metadata-receipt.json')
R=json.loads((L/'delivery-metadata-receipt.json').read_text());assert R['passed'] and sha(L/'delivery-metadata.tar.gz')==R['archive_sha256']
observed={}
with tarfile.open(L/'delivery-metadata.tar.gz','r|gz') as t:
 for m in t:
  assert m.isfile() and m.name in R['files'] and m.name not in observed
  data=t.extractfile(m).read();assert hashlib.sha256(data).hexdigest()==R['files'][m.name]['sha256'];observed[m.name]=data
assert set(observed)==set(R['files'])
for repo in ('uwvm2','uwvm2-ros'):
 for name,data in observed.items():
  p=B/repo/'test/0018.debugger'/name;assert not p.exists();p.write_bytes(data)
  assert sha(p)==R['files'][name]['sha256']
early=json.loads((L/'early-local-product-retirement.json').read_text())
assert early['passed'] and early['all_payloads_read_back'] and early['source_manifest_sha256']==sha(L/'inputs.json')
assert early['native_admission_bytes']==4<<30 and early['retired_bytes']==562957904
early_rows={r['path']:r for r in early['retired']};assert len(early_rows)==2
retired=[]
for repo in ('uwvm2','uwvm2-ros'):
 O=L/'macos'/repo;q=json.loads((O/'qualified.json').read_text());receipt=json.loads((O/'receipt.json').read_text());assert receipt['passed']
 recover=json.loads((L/(repo+'-macos-local-cold-qualified.json')).read_text());assert recover['passed'] and recover['all_payloads_read_back']
 for name,key in [('fixture.exe','binary_sha256'),('environment-group.exe','group_binary_sha256')]:
  p=O/name;assert q[key]==receipt[key]==recover['payloads'][name]
  if p.exists():
   assert not p.is_symlink() and sha(p)==q[key];retired.append(dict(path=str(p),bytes=p.stat().st_size,sha256=sha(p)));p.unlink()
  else:
   assert repo=='uwvm2' and str(p) in early_rows
   row=early_rows[str(p)];assert row['sha256']==q[key] and row['archive_member']==name
   assert early['archive']==recover['archive'] and early['archive_sha256']==recover['archive_sha256']
   retired.append(row)
assert sum(r['bytes'] for r in retired)==964915024
(L/'local-product-retirement.json').write_text(json.dumps(dict(passed=True,source_manifest_sha256=sha(L/'inputs.json'),retired=retired,local_cold_read_back=True),indent=2)+'\n')
(L/'local-delivery-qualified.json').write_text(json.dumps(dict(passed=True,repositories=['uwvm2','uwvm2-ros'],files=len(observed),delivery_archive_sha256=sha(L/'delivery-metadata.tar.gz'),native_runs=48,counted_assertions=14472,large_local_products_retired=sum(r['bytes'] for r in retired)),indent=2)+'\n')
print('R30 complete qualification and synchronized delivery',len(observed),14472,'native macOS peak',mac,flush=True)
