from pathlib import Path
import hashlib, json, shutil, os

L = Path(__file__).parent
B = L.parents[3]
assert B.name == 'src'
q = json.loads((L / 'final-native-matrix-qualified.json').read_text())
assert q['passed'] and q['native_runs'] == 48
assert len(q['groups']) == 8 and all(g['native_runs'] == 6 for g in q['groups'])
M = json.loads((L / 'repaired-inputs-v8.json').read_text())
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
q['qualification_scope'] = 'Immutable 8791-file V8 source closure; concurrent changes outside the 22 owned paths are not certified'
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
delivery_storage = json.loads((L / 'final-delivery-storage-status.json').read_text())
assert delivery_storage['native_qualification_preserved'] and delivery_storage['original_limits_unchanged']
assert delivery_storage['original_48_native_qualification_sha256'] == sha(L / 'final-native-matrix-qualified.json')
q['delivery_storage_status'] = delivery_storage
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
完整读回后合并归档。V4、V6、V7 的历史运行均不计入本轮最终 V8 的 48 次。
V8 manifest SHA256：`{q['source_manifest_sha256']}`，完整冻结并再次校验 **8791 文件**。
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

V7 Windows 真实执行中，双线程 Core 与旧退役用例都停在要求两个 TLS 析构同时进入
等待点的断言；单线程 WASIp1 用例的两种策略实际均通过，但不计入最终 V8。
Windows 线程分离通知可能在 loader lock 下串行执行，与
[微软 DllMain 文档](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain)
描述的通知串行化一致。V8 给两个 TLS 析构分别设置放行位：先等待真正进入的析构，
再仅放行其中一个，必须观察另一个进入并仍阻塞、首次析构完成，且生产管理接口
仍返回 pending_native_join、普通入口仍关闭；最后才放行第二个并要求两个真实 OS join。
保留原 20 秒等待，并增加部分 TLS 清理后不得开放入口的检查。
生产库源码未因这项测试修正而改变，两仓库四个 fixture 同步修正并独立冻结。
一次 Windows 关闭过程中，守卫读取 owned QEMU 的 procfs fd 时遇到 PermissionError。
现在此竞争分支必须在原 50 毫秒内得到真实 pidfd death 才结束统计；若进程仍活着则失败，
没有跳过活进程容量计数。该历史失败和完整 serial 输出均保留。

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

本任务 R34 旧产品元数据，以及 R28–R33 已完成运行的产品元数据，都先完整归档并
逐项读回 SHA、大小和 mode，再退役 raw 副本；R33 后续编译仍需读取的两份 runtime
编译记录保留原路径。恢复成员清单和独立 guard 留在原任务卷，历史证据可完整恢复。
仅对已有只读、mode 相同、SHA 相同的历史源码叶文件使用 hardlink，保留全部原路径
和 manifest，目录 mode 随即恢复；部分历史命名空间列为未处理，没有接管外部目录。
详细 hardlink 清单以 gzip 保留，解码后 SHA 与原 JSON 相同，原 JSON 重复副本随后退役。
最终 V8 两仓库原生产品容器如被合并，报告重新读回合并容器的全部成员，核验原
retirement receipt、仓库前缀、source manifest 和专用 cgroup guard。合并仅使用
4 GiB 维护上限，原生测试限制、6 GiB 准入线与 7 GiB 停止线均保持原值。
R29 的历史 macOS/Windows/FreeBSD 产物如被合并，最终汇总还会独立读回全部原生字节、文件大小
和 mode，并核对旧 receipt 与维护 guard；它们不计入本轮原生运行次数。
本机 R31/R32 的两个旧源码传输包转存到原 Linux 有界任务卷，原始压缩字节、
SHA、全部 tar 源成员与只读 mode 都核验后才退役本机副本。冷区内容和容量未改变，
本机保留恢复路径清单；最终汇总再次核验远端原始包和逐成员内容。
macOS 普通版链接曾因主盘低于原 32 GiB 余量而由 guard 终止，全部 owned PIDFD
退出后才整理。对唯一经核验的 8 GiB loop31 任务卷 trim 回收已删除块，前后
首次核验 158926 个现存文件的字节、大小、mode、owner 完全一致，归还主盘约 1.49 GiB；后续回收同样逐项核验。
已成功的 runtime/host API 对象单独完整读回缓存后退役 raw 副本；恢复时再次校验
对象、依赖、编译器和 Wasm 字节，再续接未完成链接。中断日志/guard 保留，未算作
原生通过；原 32 GiB 主盘余量、6/7 GiB 热区线与 cgroup 限制均未放宽。
本机又出现一次并发写盘触发 ENOSPC，ROS 的编译/链接已实际通过，但本机控制器
未能写入阶段清单，尚未运行的 native 用例没有算作通过。把本任务 R28–R33 的
90 个旧备份/较大元数据流式转存，逐项核验 SHA、大小、mode 后退役本机 raw 副本；
当前源码、Python 控制器和三个只读冷区均保留，旧目录保留远端逐文件恢复清单。
首个辅助传输因 8 MiB 元数据容量预估不足被拒绝，原副本未删；改用独立的
64 MiB 辅助传输上限，仍低于原 1 GiB 文件上限，native 元数据 8 MiB 上限不变。
复核本机控制器完整并恢复磁盘余量后，单独运行真正的 ROS 六项 native 测试。

交付时 Linux 主盘余量低于原 32 GiB 线，任务卷占用高于原 6 GiB 新测试准入线，
同时仍低于 7 GiB 停止线。可选 R29 FreeBSD 历史压缩再次被 guard 终止；全部
原始完整容器继续保留，失败压缩前缀在核验实际 PIDFD death 与原容器 SHA 后退役。
此前独立汇总通过的 48 次原生结果保持有效；后续 native suite 暂不准入，不扩大
阈值、不接管其他任务文件或进程。报告 JSON 中分别保留原生汇总快照与最新磁盘
准入状态，磁盘状态观察不是一次新的原生测试。

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
