from pathlib import Path
import json,hashlib,time,shutil
L=Path(__file__).parent;B=L.parents[3]
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
read=lambda p:json.loads(Path(p).read_text())
q=read(L/'world-install-final-qualified.json');assert q['passed'] and q['native_program_runs']==48 and q['counted_assertions']==14392
S=read(L/'final-storage-qualified.json');assert S['passed']
M=read(L/'inputs.json');before=read(L/'before.json')
assert sha(L/'inputs.json')==q['source_manifest']['sha256']
rows=[]
for key in before:
 p=B/key;h=sha(p);rows.append(dict(repo=key.split('/',1)[0],path=key.split('/',1)[1],frozen_sha256=M[key],current_sha256=h,matches=h==M[key]))
scope=dict(passed=True,timestamp_ns=time.time_ns(),scope='Hash comparison only; latest concurrent workspace is not the qualified immutable source cut',compiled_own_paths=rows,compiled_own_path_matches=sum(r['matches'] for r in rows),compiled_own_path_count=len(rows),source_manifest_sha256=sha(L/'inputs.json'),concurrent_changes_preserved=True)
(L/'workspace_scope.json').write_text(json.dumps(scope,indent=2)+'\n')
mac=dict(passed=True,round='R29',scope='Actual local arm64 macOS native executions under 2 GiB aggregate conservative bound',limit_bytes=2<<30,owned_ramdisk_bytes=0,repos={r:read(L/'macos'/r/'receipt.json') for r in ('uwvm2','uwvm2-ros')})
retired=[]
for repo in ('uwvm2','uwvm2-ros'):
 receipt=read(L/(repo+'-macos-product-retirement.json'))
 assert receipt['passed'] and receipt['all_payloads_read_back']
 for name in ('fixture.exe','environment-group.exe'):
  p=L/'macos'/repo/name;expected=receipt['payloads'][name]
  assert p.is_file() and not p.is_symlink() and sha(p)==expected
  retired.append(dict(repo=repo,path=str(p),bytes=p.stat().st_size,sha256=expected,qualified_recovery_archive=receipt['archive'],archive_sha256=receipt['archive_sha256'],all_payloads_read_back=True))
# Report/remote guards were already read back before retiring these four
# transport duplicates. Original source and native test logs stay available.
for row in retired:Path(row['path']).unlink()
transport=dict(passed=True,round='R29',retired_bytes=sum(r['bytes'] for r in retired),rows=retired,foreign_file_cleanup_performed=False,native_qualification_sha256=sha(L/'world-install-final-qualified.json'))
(L/'local-transport-retirement.json').write_text(json.dumps(transport,indent=2)+'\n')
table=['| 仓库 | Linux | Windows | FreeBSD | macOS |','|---|---:|---:|---:|---:|']
for repo in ('uwvm2','uwvm2-ros'):
 table.append('| '+repo+' | '+' | '.join(str(q['repos'][repo][osname]['counted_assertions'])+' / 6 次通过' for osname in ('linux','windows','freebsd','macos'))+' |')
macpeak=max(row['aggregate_peak_upper_bytes'] for r in mac['repos'].values() for row in r['rows'])
matrix='\n'.join(table)+f"\n\n合计 48 次原生执行、{q['counted_assertions']} 项累计断言，均为真实原生执行。另有 8 次 Linux 完整实例/preload 对象图回归、2 次默认上下文快速路径单元回归（20 项断言）。本机 macOS 保守聚合内存最大上界 {macpeak:,} bytes（{macpeak/(1<<20):.2f} MiB），低于 2 GiB。"
scope_text=f"本轮相关 16 条仓库路径中，交付时有 {scope['compiled_own_path_matches']} 条仍与冻结切片相同。不同项记录在 workspace_scope.json 中，保留当前并发工作区内容，不用已测试旧文件覆盖它们。四 OS 测试对冻结切片负责；这一比较不会把最新完整工作区重新资格化。"
storage=f"收尾检查：活动目录按唯一 inode 分配计 {S['directory_allocated_bytes']/(1<<30):.3f} GiB，活动卷剩余 {S['owned_volume_free_bytes']/(1<<30):.3f} GiB；Linux 历史冷区 {S['cold_evidence_bytes']/(1<<30):.3f} GiB / 2 GiB。macOS 四份运输可执行重复文件共 {transport['retired_bytes']/(1<<20):.2f} MiB，在远端产品归档全量读取与最终资格通过后删除；源码、原生日志和可恢复归档保留。"
report=(L/'report-draft.md').read_text().replace('RESULT_MATRIX_PENDING',matrix).replace('SOURCE_SCOPE_PENDING',scope_text).replace('STORAGE_PENDING',storage)
assert '_PENDING' not in report
historical=dict(passed=True,local_cold=read(L/'historical-products-local-qualified.json'),transfer=read(L/'historical-products-transfer.json'),remote_hot_retirement=read(L/'historical-products-hot-retirement.json'),retirement_guard=read(L/'guard-joint-historical-retire-r29.json'),cold_local_recovery_required=True)
prefix='wasip1_private_dispatch_r29_'
docs={
 prefix+'test_report.md':report.encode(),
 prefix+'test_report.json':(L/'world-install-final-qualified.json').read_bytes(),
 prefix+'source_manifest.json':(L/'inputs.json').read_bytes(),
 prefix+'source_freeze.json':(L/'input-freeze-qualified.json').read_bytes(),
 prefix+'workspace_scope.json':(L/'workspace_scope.json').read_bytes(),
 prefix+'qualification_guard.json':(L/'guard-joint-report-r29.json').read_bytes(),
 prefix+'post_reboot_state.json':(L/'final-storage-qualified.json').read_bytes(),
 prefix+'post_reboot_guard.json':(L/'guard-joint-storage-r29.json').read_bytes(),
 prefix+'storage_policy.json':(L/'storage-policy.json').read_bytes(),
 prefix+'cold_storage_state.json':(L/'cold-storage-qualification.json').read_bytes(),
 prefix+'active_environment.json':(L/'active-environment-final-r29.json').read_bytes(),
 prefix+'native_macos_evidence.json':(json.dumps(mac,indent=2)+'\n').encode(),
 prefix+'local_transport_retirement.json':(L/'local-transport-retirement.json').read_bytes(),
 prefix+'historical_storage.json':(json.dumps(historical,indent=2)+'\n').encode(),
 prefix+'fixture_observer_correction.json':(L/'source-fixture-observer-correction.json').read_bytes()
}
P=L/'delivery';assert not P.exists();P.mkdir()
for name,data in docs.items():
 (P/name).write_bytes(data)
 for repo in ('uwvm2','uwvm2-ros'):(B/repo/'test/0018.debugger'/name).write_bytes(data)
index={n:dict(sha256=hashlib.sha256(data).hexdigest(),bytes=len(data)) for n,data in docs.items()}
name=prefix+'delivery_inputs.json';data=(json.dumps(index,indent=2)+'\n').encode();(P/name).write_bytes(data)
for repo in ('uwvm2','uwvm2-ros'):(B/repo/'test/0018.debugger'/name).write_bytes(data)
print('R29 local delivery prepared',len(docs),sum(map(len,docs.values())),'scope',scope['compiled_own_path_matches'],len(rows),'mac peak',macpeak,flush=True)

