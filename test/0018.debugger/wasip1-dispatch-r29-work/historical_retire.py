from pathlib import Path
import json,hashlib,os,time
D=Path(__file__).parent;E=D.parent.parent;R=E/'rounds/wasip1-cross-jit-20261007-r26'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
M=json.loads((D/'historical-products-transfer.json').read_text());L=json.loads((D/'historical-products-local-qualified.json').read_text())
assert L['passed'] and L['all_payloads_read_back'] and L['limit_bytes']==1<<30 and L['bytes']==M['bytes']<1<<30
assert L['initial_configuration_sha256']==sha(D/'historical-products-transfer.json') and L['memory_peak_upper_bytes']<L['macos_limit_bytes']==2<<30
assert set(M['archives'])==set(L['archives']) and len(M['archives'])==6
cg=Path('/sys/fs/cgroup/system.slice/docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope')
assert Path('/proc/self/cgroup').read_text().strip()=='0::/'+str(cg).removeprefix('/sys/fs/cgroup/')
rows=[];selected=set()
for name,row in M['archives'].items():
 p=Path(row['path']);assert p.parent==R and p.name==name and not p.is_symlink() and p.is_file()
 assert row['sha256']==L['archives'][name]['sha256']==sha(p) and row['bytes']==L['archives'][name]['bytes']==p.stat().st_size and L['archives'][name]['all_payloads_read_back']
 assert sha(row['prior_retirement'])==row['prior_retirement_sha256']
 selected.add((p.stat().st_dev,p.stat().st_ino))
 rows.append(dict(original_path=str(p),local_recovery_path=str(Path(L['local_directory'])/name),archive_sha256=row['sha256'],bytes=row['bytes'],prior_retirement=row['prior_retirement'],all_local_payloads_read_back=True))
# Observe other cgroup participants without adopting/signalling any of them.
for pid in map(int,(cg/'cgroup.procs').read_text().split()):
 try:
  for fd in (Path('/proc')/str(pid)/'fd').iterdir():
   try:s=fd.stat()
   except FileNotFoundError:continue
   assert (s.st_dev,s.st_ino) not in selected,('archive is currently open; leave intact',pid,fd)
 except (ProcessLookupError,FileNotFoundError):continue
for row in rows:
 p=Path(row['original_path']);assert sha(p)==row['archive_sha256'];p.unlink()
proof=dict(passed=True,timestamp_ns=time.time_ns(),scope='Only six owned R26 compressed product archives, fully recovered and read back on native macOS; source, SDK archives, logs and historical receipts untouched',retired=rows,retired_bytes=sum(r['bytes'] for r in rows),local_qualification_sha256=sha(D/'historical-products-local-qualified.json'),cgroup=Path('/proc/self/cgroup').read_text(),foreign_processes_signalled=False)
(D/'historical-products-hot-retirement.json').write_text(json.dumps(proof,indent=2)+'\n')
A=E.parent/'wasip1-active-environment.json';state=json.loads(A.read_text())
state.update(latest_round=str(D),latest_status='R29 private WASIp1 dispatch context and external trace qualification in progress; 64 GiB cgroup and existing hot/cold limits unchanged.')
state['qualified_products_r26']['six_original_product_archives_local_cold']=dict(proof=str(D/'historical-products-hot-retirement.json'),proof_sha256=sha(D/'historical-products-hot-retirement.json'),local_directory=L['local_directory'],checked_limit_bytes=1<<30)
temporary=A.with_name(A.name+'.r29-cold.tmp');assert not temporary.exists();temporary.write_text(json.dumps(state,indent=2)+'\n');temporary.replace(A)
print('Own historical compressed products moved to fully verified local recovery',proof['retired_bytes'],flush=True)
