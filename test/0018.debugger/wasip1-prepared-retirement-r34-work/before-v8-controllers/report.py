from pathlib import Path
import hashlib, json, shutil, os

L = Path(__file__).parent
B = L.parents[3]
assert B.name == 'src'
q = json.loads((L / 'final-native-matrix-qualified.json').read_text())
assert q['passed'] and q['native_runs'] == 48
assert len(q['groups']) == 8 and all(g['native_runs'] == 6 for g in q['groups'])
M = json.loads((L / 'repaired-inputs-v7.json').read_text())
before = json.loads((L / 'before-edit-sha256.json').read_text())
paths = list(before)
for repo in ('uwvm2', 'uwvm2-ros'):
    paths += [repo + '/test/0017.runtime/' + n + '_runtime.cc'
        for n in ('debug_checkpoint_prepared_retirement', 'debug_wasip1_prepared_retirement')]
for repo in ('uwvm2', 'uwvm2-ros'):
    paths += [repo + '/test/0017.runtime/debug_checkpoint_native_cohort_retirement_runtime.cc',
        repo + '/test/0017.runtime/fixtures/debug_checkpoint_retirement_cohort.wat']
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
owned = [dict(path=str(B / n), tested_sha256=M[n], current_sha256=sha(B / n)) for n in paths]
assert len(owned) == 22 and all(r['tested_sha256'] == r['current_sha256'] for r in owned)
drift = []
for n, h in M.items():
    p = B / n
    if not p.is_file():
        drift.append(dict(path=str(p), tested_sha256=h, current_sha256=None))
    else:
        actual = sha(p)
        if actual != h:
            drift.append(dict(path=str(p), tested_sha256=h, current_sha256=actual))
q['delivery_owned_source_audit'] = owned
q['concurrent_other_source_drift'] = drift
q['concurrent_edits_preserved'] = True
q['qualification_scope'] = 'Immutable 8791-file V7 source closure; concurrent changes outside the 22 owned paths are not certified'
mac = [json.loads((L / ('macos-evidence-' + r) / 'receipt.json').read_text())
    for r in ('uwvm2', 'uwvm2-ros')]
mac_peak = max(row['aggregate_peak_upper_bytes'] for r in mac for row in r['rows'])
assert mac_peak < 2 << 30
cold_roots = [B.parent / ('wasip1-' + r + '-owned-cold-evidence') for r in ('r29', 'r30', 'r33')]
cold = []
for root in cold_roots:
    assert root.is_dir() and root.stat().st_mode & 0o222 == 0
    files = list(root.rglob('*'))
    assert all(not p.is_symlink() and (not p.is_file() or p.stat().st_mode & 0o222 == 0) for p in files)
    cold.append(dict(path=str(root), bytes=sum(p.stat().st_size for p in files if p.is_file()), readonly=True))
assert sum(c['bytes'] for c in cold) < 2 << 30
q['local_unchanged_cold_regions'] = cold
q['native_macos_peak_upper_bytes'] = mac_peak
q['new_local_cold_capacity_bytes'] = 0
tables = '\n'.join('| ' + g['repo'] + ' | ' + g['os'] + ' | 6 | ' + str(g['counted_wasip1_assertions']) + ' |'
    for g in q['groups'])
storage = q['final_storage']
text = f'''# Prepared world retention through actual native retirement — R34

两个仓库、四个 OS 的最终不可变输入矩阵通过：**48 次真实原生运行**。
其中 WASIp1 用例共 **{q['counted_wasip1_assertions']} 个实际计数断言**。
Core 与旧 native cohort 回归的 `actual_workers=2` 是工作线程见证，未算作断言。

| 仓库 | 原生 OS | 运行 | WASIp1 计数断言 |
|---|---|---:|---:|
{tables}

## 实现

两个仓库同步加入冷端 host API：
`llvm_jit_checkpoint_prepare_retire_instance_host_api(ticket, captures, request, deadline)`
以及 `llvm_jit_checkpoint_discard_prepared_instance_host_api(operation, deadline)`。
同一经过完整认证的暂停点先准备候选 world 的资源、engine、GC 根、调试上下文、
WASIp1 环境与新模块内存，以及真正启动后又 join 的私有准备线程。
所有准备成功后才激活原实例实际登记的可恢复 native guest 队列退役。
任一准备拒绝都不激活旧线程退役；暂时准备过的候选在锁外销毁，诊断不会仍称 retained。

候选、原 source/host、封闭 execution generation 与实际 cohort owner 跨 pending 返回共同保留。
`pending_execution` 和 `pending_native_join` 分别代表执行 lease 与真实 OS 退出/join。
后者包括 C++ TLS 析构，body-done、复制的状态或回调完成不能替代 OS death。
两阶段都完成只得到 `prepared_world_ready_closed`，普通执行入口仍关闭。
显式丢弃候选须再次认证维护、排空和物理 join，随后才重新开放同一个原实例；
原 Wasm epoch、source、输出哨兵不变，候选/提供者在 registry/admission 锁外析构。
只丢掉公开 operation owner 不会放开入口。

可见 WASIp1 必须设 `request.include_wasip1=true`，包括默认启用但 Core 模块没有
调用它的情形。遗漏返回 `preparation_declined` 与
`preparation.wasip1_checkpoint_required=true`。严格模式拒绝继承 stdio 等外部资源；
显式 `require_managed_wasip1_resources=false` 允许保留绑定，但不回滚外部 I/O。
**保存 Wasm checkpoint 时必须在同一个合作停止点同时保存 WASIp1。**
deadline 拒绝过期输入并限制排空/join 等待；有预算上限的同步准备不可由该 deadline 抢占。
进程内 canonical owner 不是可移植 checkpoint 文件凭据。ASM 调试范围仍限于生成的 Wasm 上下文。
ROS 保留其原有 bridge-scope getter 和仅 full 模式边界。

## 验证与修复

每组实际运行三个用例、两种策略 instruction/unwind：新增 Core prepared-retirement、
原 native-cohort 退役回归、新增 WASIp1 prepared-retirement。
新增用例刻意分别阻塞旧函数体和真实 TLS 析构，验证候选跨 pending 保留、
提前 discard 不绕过 join、join 后入口仍关闭、显式 discard 才开放原实例。
另测不完整/重复/别名 owner、未登记 native worker、过期 deadline、准备预算不足、
GC 忙、漏选 WASIp1、严格外部资源拒绝及旧 reset/stop/source 入口排他。
两套 Wasm 输入均经真实 wasm-tools 全特性 validator。
所有目标都重新编译 runtime 与 host API，按依赖 SHA 绑定真实完整 LLVM JIT。
C++ I/O、解析、原生线程使用 FastIO。

此前暴露的 private capture 访问权限和 complete-census 组合选择分支错误已同步修复。
Core-only 用例显式关闭默认 WASIp1 环境，未放松可见 WASIp1 必须共同捕获的规则。
准备失败的 `data_error` 现在完整转交，暂时保留但未激活的候选改回 discarded 诊断。
原始编译失败、第二/第三版失败、诊断日志与 guard 保留；可恢复原始负例产物经过
完整读回后合并归档。V4 的六次历史 Linux 成功不计入本轮最终 48 次。
V7 manifest SHA256：`{q['source_manifest_sha256']}`，完整冻结并再次校验 **8791 文件**。
交付时本轮 **22 个实现/文档/fixture 路径**均等于测试输入；另外记录
{len(drift)} 个并发修改路径，其当前内容不以此次冻结输入结果认证，也未覆盖它们。

专用 Core 输入保留原有 GC/循环引用、EH、SIMD、memory64/table64、typed table initializer、
extern 包装与 live/dropped segment，改用 32 个静态 nop 的有界循环替代 4096 个展开点。
原来的两个有限 guest 在 Windows 中可能在另一个线程到达之前离开，首次出现
完整暂停队列失败；另一轮在等待旧 guest 返回时失败。真实失败均保留。
新测试仍要求完整真实队列且保留原 20 秒等待，另补打印缺失 ticket、实际 roster
及 body/TLS waiter 的失败诊断，不以降低断言或跳过路径取得通过。
Windows guest 从 2 GiB 调到 4 GiB，仍位于原 8 GiB 本任务和 64 GiB 共享限制内。
一次诊断脚本错误地在展开的 PowerShell case 中使用 continue，提前结束输出；
修正后执行所有隔离 case，记录各项真实失败，并排除该次未完成运行。
V6 继承文件按 SHA 冻结但部分仍有 Unix 写位；V7 逐项确认字节未变后移除本任务
共享源 inode 的写位，最终 8791 个文件和目录均只读。失败 bootstrap 亦保留。
本轮新增两个专用 WAT 和两仓库旧 cohort fixture 的失败诊断，交付审核共 22 路径。

## OS 与资源

Linux 原生测试、全部交叉编译、Windows/FreeBSD QEMU 均在原 SSH Linux
64 GiB cgroup，swap=0、原 anchor/boot/CPU 集合未变。仅绑定并退役本任务
PID/birth/UID/pidfd；foreign 参与者未接管或终止。
VM 使用原始只读 base 和本任务临时 overlay，真实 native PASS、serial nonce、
guest 输入哈希与基盘前后身份都核验；没有以交叉链接代替原生执行。
macOS 在本机 arm64 运行未经剥离的同一目标二进制；继承 no-fork sandbox 的
独立负控制确实拒绝 fork。整个本任务 1 GiB RAM 磁盘容量、控制器、128 MiB
余量与真实程序峰值共同计入 2 GiB，上界最大 **{mac_peak} 字节**。
测试前和每项原生运行前保留 128 MiB 本机物理磁盘余量，物理原生元数据小于 8 MiB。
此配置只减少实体磁盘的暂存需求，未增加用户指定的内存上限。
每个 RAM 设备在编译产物完整可恢复后只卸载本任务新建且校验身份的设备。

第一次 Windows VM 准入因热区超过 6 GiB 拒绝；改为归档并完整读回可执行文件后
退役原始磁盘副本。下一次 Windows 真实运行又因 overlay 与 ISO 同时占用触发
7 GiB 停止阈值，guard 终止其已认证的 owned 进程；该运行未通过且不算成功。
仅在 PIDFD death、基盘未变、原可执行文件恢复缓存完整读回后清理失败的本任务
临时 ISO/overlay/vars，失败诊断仍保留。清理阶段只允许现有占用下降，不用于运行测试。
最终通过的 VM 从认证缓存建立封闭只读 memfd ISO，无需磁盘 ISO 副本；
guest 仍核对每个原始二进制。守卫把完整 memfd 大小计入原 8 GiB 本任务
VM RSS 上界，瞬时暂存总量另限 2 GiB，原目录准入 6 GiB/停止 7 GiB 不变。
源码未因资源调整而改变，失败记录均保留。

本轮 8 组编译产物及资格/原生记录，以 Zstd 归档完整读回，并在最终汇总时再次
逐成员读回。所有新归档留在原 Linux 8 GiB 有界任务卷，没有新增本机冷区。
最终分配 **{storage['allocated_bytes']} 字节**，卷可用 **{storage['volume_available_bytes']} 字节**。
本机三个只读历史冷区总量 **{sum(c['bytes'] for c in cold)} 字节 / 2 GiB**，未动原内容。
只合并本任务两仓库字节相同的历史 JSON hardlink、删除已核验 SSH Linux 上
有相同原始字节副本的本机 source archive；保留逐项 SHA 与恢复位置清单。
报告 JSON 内列出每组 source、native/guard、恢复归档与成员哈希。

保存旧 v2/v3 源码 manifest、逐项核验仍存 v6 中相同 SHA 的全部源成员，并完整归档/
读回所有不相同的源成员之后，退役两个本任务历史目录命名空间。每个旧文件均可
按原 manifest 从 v6 相同字节或 <version>/<member> 恢复；没有删除唯一源码内容。
这样释放 inode，保持原 8192 个准入余量而非放宽该限制。

## 剩余边界

**新 world 的原子发布、新执行 worker 的恢复帧/活根安装与恢复后的 guest 执行仍未实现。**
本轮完成候选与旧线程真正退出之间的闭环，以及明确的 closed-ready/discard 路径；
`prepared_and_retained`、`prepared_world_ready_closed` 不代表已经恢复运行。
没有增加新的 portable OS 迁移方向，也不声称回滚文件内容、网络或外部输出。
下一阶段须让 source/store/engine/generation/WASIp1 cache 共同发布并进行端到端执行验证。
'''
stem = 'wasip1_prepared_retirement_r34_test_report'
for repo in ('uwvm2', 'uwvm2-ros'):
    target = B / repo / 'test/0018.debugger'
    (target / (stem + '.json')).write_text(json.dumps(q, indent=2) + '\n')
    (target / (stem + '.md')).write_text(text)
assert sha(B / 'uwvm2/test/0018.debugger' / (stem + '.md')) == sha(B / 'uwvm2-ros/test/0018.debugger' / (stem + '.md'))
assert sha(B / 'uwvm2/test/0018.debugger' / (stem + '.json')) == sha(B / 'uwvm2-ros/test/0018.debugger' / (stem + '.json'))
print(json.dumps(dict(passed=True, native_runs=48, owned_paths=22, concurrent_drift=len(drift), report=str(B / 'uwvm2/test/0018.debugger' / (stem + '.md')))))
