from pathlib import Path
import sys,subprocess,json,hashlib,tarfile,shlex,resource
L=Path(__file__).parent;B=Path('/Users/liyinan/Documents/MacroModel/src')
E=Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17');D=E/'rounds/wasip1-joint-worker-contexts-20261007-r33';prior=E/'rounds/wasip1-joint-worker-contexts-20261007-r31'
ssh=['ssh','-i',str(Path.home()/'.ssh/id_ed25519'),'macromodel@100.123.133.75'];scp=['scp','-i',str(Path.home()/'.ssh/id_ed25519')]
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
def get(remote,local):subprocess.run([*scp,'macromodel@100.123.133.75:'+str(remote),str(local)],check=True)
def guard(phase):subprocess.run([*ssh,'python3','-u',str(E/('guard-joint-'+phase+'-r33.py')),'joint-'+phase+'-r33'],check=True)
get(D/'world-install-final-qualified.json',L/'world-install-final-qualified.json');Q=json.loads((L/'world-install-final-qualified.json').read_text())
assert Q['passed'] and Q['native_program_runs']==64 and Q['counted_assertions']==22328 and len(Q['foreign_observation_callback_regressions'])==24 and not Q['complete_world_restore']
C=Path('/Users/liyinan/Documents/MacroModel/wasip1-r33-owned-cold-evidence');assert C.is_dir() and not C.is_symlink()
import qualified_archive as qa
archives={}
for repo in ('uwvm2','uwvm2-ros'):
 for platform in ('linux-integrated','windows','freebsd','macos'):
  p=L/(repo+'-'+platform+'-local-cold-qualified.json');q=json.loads(p.read_text());a=Path(q['archive'])
  assert q['passed'] and a.parent==C and sha(a)==q['archive_sha256'] and a.stat().st_mode&0o222==0
  actual={}
  with qa.open_reader(a) as t:
   for m in t:
    assert m.isfile() and m.name in q['payloads'] and m.name not in actual
    with t.extractfile(m) as f:actual[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
  assert actual==q['payloads'];archives[a.name]=dict(bytes=a.stat().st_size,sha256=sha(a),qualification_sha256=sha(p),all_payloads_read_back=True)
assert set(archives)=={p.name for p in C.iterdir()}
regions=list(C.parent.glob('wasip1-r*-owned-cold-evidence'));aggregate=sum(p.stat().st_size for c in regions for p in c.iterdir() if p.is_file())
upper=qa.local_memory_upper();assert upper<2<<30 and aggregate<2<<30 and sum(v['bytes'] for v in archives.values())<256<<20
cold=dict(passed=True,directory=str(C),archives=archives,all_payloads_read_back=True,limit_bytes=256<<20,bytes=sum(v['bytes'] for v in archives.values()),aggregate_limit_bytes=2<<30,aggregate_bytes=aggregate,memory_upper_bytes=upper,decoder=qa.decoder_identity())
(L/'local-cold-final-qualified.json').write_text(json.dumps(cold,indent=2)+'\n');C.chmod(0o555)
M=json.loads((L/'inputs.json').read_text());paths=['src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_world_worker_wasip1.h','src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_world_root_workers.h','src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_observation_calls.h','src/uwvm2/runtime/lib/uwvm_runtime_generated_wasm_bridge.h','src/uwvm2/runtime/lib/uwvm_runtime.h','src/uwvm2/runtime/lib/uwvm_runtime.default.cpp','src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_debug_host_bridge.h','src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_emit.h','test/0017.runtime/debug_wasip1_checkpoint_runtime.cc','test/0017.runtime/debug_wasip1_environment_group_runtime.cc','test/0017.runtime/debug_checkpoint_observation_foreign_runtime.cc','test/0017.runtime/fixtures/debug_wasip1_worker_environment_main.wat','test/0017.runtime/fixtures/debug_wasip1_worker_environment_provider.wat']
scope={}
for repo in ('uwvm2','uwvm2-ros'):
 for n in paths:
  p=B/repo/n;k=repo+'/'+n;assert k in M
  scope[k]=dict(qualified_sha256=M[k],current_sha256=sha(p),matches_qualified_cut=sha(p)==M[k])
(L/'workspace_scope.json').write_text(json.dumps(dict(passed=True,source_manifest_sha256=sha(L/'inputs.json'),paths=scope,source_cut_only=True,peer_changes_preserved=True),indent=2)+'\n')
for n in ('first-failed-products-retirement.json','chain-probe-retirement.json'):get(prior/n,L/n)
get(E/'guard-joint-linux-integrated-r31-uwvm2.json',L/'first-failed-guard.json')
failed=json.loads((L/'first-failed-guard.json').read_text());assert not failed['passed'] and failed['actual_root_exit']==1
negative=dict(passed=True,first_source_manifest_sha256='d81d0f6d08c1fc699031e859ce4135fc8e207094ecfb2acbc40e98161bfd5a9c',actual_failed_test_guard_sha256=sha(L/'first-failed-guard.json'),first_failed_products_recovery=json.loads((L/'first-failed-products-retirement.json').read_text()),first_probe_recovery=json.loads((L/'chain-probe-retirement.json').read_text()),first_failure_preserved=True,diagnostic='Genuine observation caller/leaf shadow recording had two frames, but the generic foreign-host wrapper made capture incomplete_logical_frames. Repair classifies the actual retained import-cache leaf before deciding whether a host island is needed.',repair_manifest_sha256=sha(L/'inputs.json'))
second=E/'rounds/wasip1-joint-worker-contexts-20261007-r32'
for n in ('first-failed-products-retirement.json','foreign-probes-retirement.json','retained-products-retirement.json','foreign-probe3.json'):
 get(second/n,L/('second-'+n))
get(E/'guard-joint-linux-integrated-r32-uwvm2.json',L/'second-failed-guard.json')
second_guard=json.loads((L/'second-failed-guard.json').read_text());assert not second_guard['passed'] and second_guard['actual_root_exit']==1
positive=json.loads((L/'second-foreign-probe3.json').read_text());assert positive['passed'] and positive['diagnostic_only'] and len(positive['rows'])==12
negative['second_attempt']=dict(source_manifest_sha256='ea1734aea48d131bc0edaef08c0f2fcdcf952de686e35232154554fb29f2309b',actual_failed_guard_sha256=sha(L/'second-failed-guard.json'),failure_preserved=True,fixture_setup_corrected='Original owned CLI helper needs retain_immutable_image=true and defers active segments to startup. Embedding callback fixtures now perform both before any guest entry.',successful_diagnostic_after_correction=positive,failed_products_recovery=json.loads((L/'second-first-failed-products-retirement.json').read_text()),probe_recovery=json.loads((L/'second-foreign-probes-retirement.json').read_text()),final_retirement=json.loads((L/'second-retained-products-retirement.json').read_text()))
(L/'negative-baseline.json').write_text(json.dumps(negative,indent=2)+'\n')
guard('storage')
for n in ('input-freeze-qualified.json','final-storage-qualified.json'):get(D/n,L/n)
for phase in ('inputs','report','storage'):get(E/('guard-joint-'+phase+'-r33.json'),L/('guard-joint-'+phase+'-r33.json'))
S=json.loads((L/'final-storage-qualified.json').read_text());assert S['passed'] and S['directory_allocated_bytes']<6<<30
get(D/'historical-source-duplicate-retirement.json',L/'historical-source-duplicate-retirement.json')
get(E/'guard-joint-source-duplicate-retire-r33.json',L/'guard-joint-source-duplicate-retire-r33.json')
relocated=json.loads((L/'historical-source-duplicate-retirement.json').read_text());source_guard=json.loads((L/'guard-joint-source-duplicate-retire-r33.json').read_text())
assert relocated['passed'] and source_guard['passed'] and source_guard['actual_root_exit']==0
for row in relocated['rows']:
 a=Path(row['local']);assert a.is_file() and a.stat().st_mode&0o222==0 and sha(a)==row['sha256'] and a.stat().st_size==row['bytes']
(L/'historical-source-relocation.json').write_text(json.dumps(dict(passed=True,recovery=json.loads((L/'historical-source-duplicate-qualified.json').read_text()),retirement=relocated,guard_sha256=sha(L/'guard-joint-source-duplicate-retire-r33.json'),existing_local_archives_only=True),indent=2)+'\n')

mac=max(c['aggregate_peak_upper_bytes'] for r in Q['repos'].values() for c in r['macos']['cases']);rows=[]
for repo,platforms in Q['repos'].items():
 for osname,p in platforms.items():rows.append(f"| {repo} | {osname} | {len(p['cases'])} | {p['counted_assertions']} |")
report=f'''# WASIp1 joint worker contexts — R33

两个仓库的四 OS 原生矩阵通过：**64 次运行、22,328 个计数断言**。
另有 **8 次 Linux 完整实例/预加载回归、24 次真实宿主回调拒绝回归、
2 次内存绑定单元运行/20 个断言**。

| 仓库 | 原生 OS | WASIp1 运行 | 计数断言 |
|---|---|---:|---:|
{chr(10).join(rows)}

## 实现与修复

同一真实 `fast_io::native_thread` 私有准备工作线程同时持有 GC 根、调试
activation/shadow TLS，以及最终私有世界的 WASIp1 环境和新模块内存绑定。
所有帧的 owner/dense/cache/environment/memory 关系在 OS 线程启动前校验。
工作线程保持叶帧上下文，逐一嵌套检查调用者帧、显式空内存遮蔽和 LIFO 恢复；
协调器确认整个队列持有上下文后释放。线程先恢复 WASIp1 TLS，再退出根和调试
作用域并 join，之后才允许销毁候选世界。原生储存计入既有准备预算；额度不足
在任何 OS 工作线程启动前拒绝。未包含 WASIp1 的请求不会伪造成功计数。

新增跨模块 fixture 实际执行 main → provider，两模块拥有不同 WASIp1 环境
和内存；捕获真实两帧和 GC 引用。每轮主成功计数为一个共同工作线程、两次帧
上下文访问。整个矩阵主成功结果共验证 64 个共同工作线程、80 次帧上下文访问。
之前独立检查完整调度缓存的两工作线程仍保留，共 128 个/256 次模块访问。
这些是有限私有准备的诊断数据，不是执行恢复凭据。

新用例暴露了 observation 模式的桥接错误：typed shadow 已完整记录两帧，
但通用桥接包装把真实 defined-Wasm 导入误标为 foreign-host island，导致
`incomplete_logical_frames`。两个仓库的编译器/运行时同步加入 observation
桥接，通过实际保留的 source、publication、profile、cache 和 ABI 成员关系
识别真实 Wasm 叶目标，再交给原有调度器绑定其 WASIp1 上下文。真实宿主目标
继续进入 foreign scope；六种普通/尾调用形式的真实 native callback reentry
在两种栈策略下全部拒绝 thread capture，原程序仍正确返回且只发生一次调用。
捕获完整性检查和禁止 ASM 越出 guest 上下文的边界保留。
回调测试还同步修正了旧嵌入启动方式：显式保留不可变 Wasm 输入镜像，
并在 guest 进入前执行 CLI 延迟的 active element 初始化；没有放宽运行时拒绝规则。

## 验证与资源

Linux、所有交叉编译及 Windows/FreeBSD QEMU 原生执行均在原 SSH Linux
64 GiB cgroup，swap=0。QEMU 使用只读原始基盘及本任务临时 overlay，真实退出
后验证基盘未变并清理。macOS 在本机 arm64 运行原始未剥离二进制，禁止 fork；
暂存二进制使用本任务新建且逐个退役的 1 GiB RAM 磁盘，整个容量计入 2 GiB
内存上限。最大记录上界 {mac} 字节。这个 RAM 暂存配置保留 2 GiB 物理磁盘
余量、原生元数据小于 8 MiB；不沿用需要写入大型实体二进制的旧磁盘配置。
每次卸载前已有完整读回的可恢复编译产物。解码阶段在 RAM 磁盘退役后运行。

冻结源码 {len(M)} 文件，manifest `{sha(L/'inputs.json')}`。
同时发生的其他 agent 修改按路径 SHA 对照记录；此报告只认证该冻结集。
第一失败集、两次诊断及原始失败 guard 均保留；失败产物经完整读回后可恢复
归档，不会把归档成功说成测试通过。C++ I/O、解析及原生线程仍使用 FastIO。

Linux 本任务热区为独立 8 GiB 文件系统，测试准入 6 GiB、停止 7 GiB；
最终分配 {S['directory_allocated_bytes']} 字节，卷剩余 {S['owned_volume_free_bytes']}
字节。8 个本轮编译归档完整读回保存在 `{C}`，共 {cold['bytes']} 字节，
检查上限 256 MiB；所有本地该任务冷区合计 {aggregate} 字节/2 GiB。
使用 512 MiB 窗口上限的流式 Zstd；仅退役本任务经认证的重复产物。
R31/R32 原始源码归档已逐项完整读回，原样只读副本保留在已有本机工作目录；
仅释放 Linux 上相同的重复归档，没有新增本机归档副本或改变冷区上限。
最初等待器的 1,200 秒窗口先于长矩阵结束而超时，原生测试并未失败；
只把等待窗口对齐已有 guard 的 3,600 秒期限，资源限制未改。

## 仍未完成

**完整 Wasm 世界的原子恢复/发布和恢复后的 guest replay 尚未完成。**
还需要旧世界真实执行工作线程退役、join，新执行工作线程和活根安装，以及
source/store/engine/generation/WASIp1 cache 的共同发布与端到端执行验证。
`prepared_and_discarded` 不表示已恢复。本轮不改变 WASIp1 可移植格式，也不
声称新增 OS 迁移方向或回滚外部文件内容、网络、输出。保存 Wasm 检查点时，
仍明确提醒必须在同一个合作停止点同时保存 WASIp1 才能包含 FD/环境状态。
'''
(L/'report-draft.md').write_text(report)
P=L/'delivery';P.mkdir();prefix='wasip1_joint_worker_contexts_r33_'
files={prefix+'test_report.md':L/'report-draft.md',prefix+'test_report.json':L/'world-install-final-qualified.json',prefix+'source_inputs.json':L/'inputs.json',prefix+'source_cut.json':L/'local-source-cut-qualified.json',prefix+'source_freeze.json':L/'input-freeze-qualified.json',prefix+'workspace_scope.json':L/'workspace_scope.json',prefix+'local_cold.json':L/'local-cold-final-qualified.json',prefix+'storage.json':L/'final-storage-qualified.json',prefix+'negative_baseline.json':L/'negative-baseline.json'}
for phase in ('inputs','report','storage'):files[prefix+'guard_'+phase+'.json']=L/('guard-joint-'+phase+'-r33.json')
files[prefix+'historical_source_relocation.json']=L/'historical-source-relocation.json'
files[prefix+'orchestration_wait_repair.json']=L/'pipeline-first-wait-timeout.json'
for repo in ('uwvm2','uwvm2-ros'):
 files[prefix+repo.replace('-','_')+'_macos.json']=L/(repo+'-macos-native-evidence.json');files[prefix+repo.replace('-','_')+'_ramdisk.json']=L/(repo+'-ramdisk-retirement.json')
manifest={}
for n,p in files.items():data=p.read_bytes();(P/n).write_bytes(data);manifest[n]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
(P/'wasip1_private_dispatch_r33_delivery_inputs.json').write_text(json.dumps(manifest,indent=2)+'\n')
subprocess.run([*ssh,'mkdir -m 700 '+shlex.quote(str(D/'delivery'))],check=True)
subprocess.run([*scp,*map(str,P.iterdir()),'macromodel@100.123.133.75:'+str(D/'delivery')+'/'],check=True);guard('delivery')
get(D/'delivery-metadata.tar.gz',L/'delivery-metadata.tar.gz');get(D/'delivery-metadata-receipt.json',L/'delivery-metadata-receipt.json')
q=json.loads((L/'delivery-metadata-receipt.json').read_text());assert q['passed'] and sha(L/'delivery-metadata.tar.gz')==q['archive_sha256'];observed={}
with tarfile.open(L/'delivery-metadata.tar.gz','r|gz') as t:
 for m in t:
  assert m.isfile() and m.name in q['files'] and m.name not in observed
  with t.extractfile(m) as f:data=f.read();assert len(data)==q['files'][m.name]['bytes'] and hashlib.sha256(data).hexdigest()==q['files'][m.name]['sha256']
  observed[m.name]=data
assert set(observed)==set(q['files'])
for repo in ('uwvm2','uwvm2-ros'):
 for n,data in observed.items():(B/repo/'test/0018.debugger'/n).write_bytes(data)
print('R33 qualified metadata delivered identically to both repositories',len(observed),flush=True)
