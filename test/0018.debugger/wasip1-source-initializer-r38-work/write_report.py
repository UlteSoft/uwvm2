from pathlib import Path
import hashlib,json
L=Path(__file__).parent;B=L.parents[3]
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
m=json.loads((L/'syntax-overlay-v2.json').read_text());assert len(m)==36 and all(sha(B/k)==v for k,v in m.items())
q=json.loads((L/'analysis-compile-qualified-v2.json').read_text());g=json.loads((L/'guard-analysis-compile-v2.json').read_text())
n=json.loads((L/'ram-native-a2-qualified.json').read_text());ng=json.loads((L/'guard-ram-native-a2.json').read_text())
nr=json.loads((L/'ram-native-a2-results.json').read_text());storage=json.loads((L/'r38-storage-final.json').read_text())
assert q['passed'] and g['passed'] and len(q['rows'])==16 and len(q['wasm_data_fixture_checks'])==8
assert n['passed'] and n['native_runs']==24 and n['results_sha256']==sha(L/'ram-native-a2-results.json')
assert ng['passed'] and ng['parent_events_unchanged'] and ng['ram_disposable_products_retired']
assert all(x['pidfd_retired'] for x in g['processes']) and all(x['pidfd_retired'] for x in ng['processes'])
assert all(x['passed'] for x in nr['rows']) and len([x for x in nr['rows'] if x['actual_native_runtime_execution']])==24
assert all(x['source_manifest_sha256']==sha(L/'syntax-overlay-v2.json') for x in [q,n,nr])
names=['syntax-overlay-v2.json','syntax-overlay-v2.tar.gz','owned-source-changes.patch',
 'analysis-compile-qualified-v2.json','guard-analysis-compile-v2.json','wasm-fixture-validation-v2.json',
 'ram-native-a2-qualified.json','ram-native-a2-results.json','guard-ram-native-a2.json','r38-storage-final.json']
result=dict(round=38,source_manifest=m,proofs={x:sha(L/x) for x in names},compiler_frontend_checks=16,analysis_wasm_data_checks=8,
 linux_actual_native_runs=24,linux_native_regression_passed=True,all_four_os_native_validation=False,
 analysis_rows=q['rows'],analysis_guard=g,native_results=nr,native_guard=ng,storage=storage,
 private_source_initializer_bindings_implemented=True,post_actual_physical_join_source_binding_preflight_implemented=True,
 source_seal_or_initializer_serial_issued=False,world_publication=False,restored_guest_worker_startup=False,restored_guest_replay=False,
 asm_vm_or_host_context_granted=False,pending_native_cases=72,
 pending=['Windows/FreeBSD/macOS native regression.',
 'Coupled actual source/engine/GC/WASIp1/native owner publication.',
 'Real startup leases, roots/ledgers/participants, guest replay and terminal cleanup.'])
md="""# R38：候选 source 初始化对应表与 join 后复查

两个仓库已同步实现固定 main/preload 对应表，检查实际 parser 文件、不可移动 registry 成员、真实 dense 顺序、Wasm declaration 类型，以及完整且未发布的 GC staging。maximum_private_source_initializer_modules 默认 4096，在创建新 engine 前拒绝超额；记录内存由原 maximum_native_payload_bytes 计费。真实旧 generation drain 和 OS/TLS join 后，管理器再次检查候选表、当前 source/epoch/initializer serial 和真实 builtin WASIp1 loader 归属，才报告 prepared_world_ready_closed。完整 world 定义之后的私有 helper 解决头文件依赖；旧状态枚举编号保留。

| 最终检查 | 结果 |
|---|---:|
| 两仓库 Linux runtime/core/WASIp1/单模块/preload；Windows/FreeBSD/macOS runtime 前端 | 16 项通过 |
| 组装／validator 数据检查 | 8 项通过 |
| 两仓库 Linux 原生 LLVM full | 24 个用例通过 |
| 本轮 Windows/FreeBSD/macOS 原生 | 尚未执行 |

Linux 原生覆盖普通／间接调用的 core 与 WASIp1、完整单模块和 preload，分别运行 instruction/unwind。配额拒绝后原 pause/epoch 保持不变；正向验证对应表计数，以及真实物理 join 后的复查。此前私有 dispatch、间接目标、typed/resume 端点和 CFI 保留也在这些用例中实际运行。

原生构建产物只写入本轮拥有的 RAM tmpfs：768 MiB 文件额度、6 GiB RSS 和共享 cgroup 60 GiB 停止线均由 guard 监控。全部测试在原 SSH Linux 64 GiB cgroup 完成；进程均已回收，RAM 产物删除，日志／命令／hash 保留。原磁盘和 inode 门槛保持，持久四 OS suite 仍被拒绝。Mac 本机没有运行本轮测试，2 GiB 限制保留。

首轮核心四例通过后，WASIp1 因测试器 8 GiB RLIMIT_AS 不足而 SIGILL。GDB C++ 栈定位到原始 memory32 初始化的 PROT_NONE/NORESERVE mmap：8 GiB 保护区还需要对齐尾部及其他映射。测试器修正为 compiler AS 8 GiB、guest AS 64 GiB；物理 RSS、cgroup 和 RAM 文件限额不变。最终结果只采用第二轮完整 24 例，首轮失败和调用栈单独保留。

**整体 world 发布、恢复线程启动及 guest replay 仍未接通。** 对应表没有设置 source initialized、发布 initializer serial、发放新 epoch／执行权或 VM/host ASM 上下文。后续必须真实地共同发布 source、engine、GC、WASIp1 与原生归属，再用 closed startup lease 完成线程、根、ledger、pause participant enrollment。Wasm 与 WASIp1 须在同一停止点一起 checkpoint；外部文件内容与 I/O 不会回滚。
"""
for repo in ('uwvm2','uwvm2-ros'):
 d=B/repo/'test/0018.debugger'
 (d/'wasip1_source_initializer_r38_test_report.json').write_text(json.dumps(result,indent=2)+'\n')
 (d/'wasip1_source_initializer_r38_test_report.md').write_text(md)
print(json.dumps(dict(frontends=16,linux_native_passed=24,other_os_native_pending=72,world_publication=False)))
