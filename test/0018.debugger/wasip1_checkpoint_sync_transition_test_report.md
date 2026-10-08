# WASIp1 同步标志转换修复与测试（R10）

已同步修复 uwvm2 / uwvm2-ros 的原始 fd_fdstat_set_flags：已有 SYNC 文件或目录请求降为 DSYNC 时，不能保留完整 SYNC 标志却返回成功。新代码在两个复合掩码分支中先清除 O_SYNC，再选择 O_DSYNC；原有读回核验可以发现内核拒绝转换，并回滚同次请求中的 APPEND/NONBLOCK。

修改位于 [共享 setter](../../src/uwvm2/imported/wasi/wasip1/func/fd_fdstat_set_flags.h)，文件和目录分支各两处。wasm64 调用共享 setter，模块接口也包含该头文件。此次增量每个仓库为 12 行；开工前已有的 Windows 分支修复保留。新增 [原始 ABI 测试](../0013.debugger/wasip1_sync_transition.cc)，同步补充 checkpoint 文档。ROS 的运行 mode 配置没有改动。

修复前，在 Linux 原始代码上，两个仓库各复现 128 次错误成功返回。例如，以 SYNC 打开的文件请求 DSYNC|APPEND：返回成功，但 native status 仍有完整 SYNC，APPEND 还可能已经改变。基线两仓库共 32,484 项检查，复现 256 次；基线使用本轮修复前冻结的头文件，未借用其他版本的测试结果。

| 实际运行 OS | uwvm2 检查数 | uwvm2-ros 检查数 | 最终结果 |
|---|---:|---:|---|
| Linux | 16,370 | 16,370 | 通过 |
| macOS | 11,118 | 11,118 | 通过 |
| FreeBSD 15.1 VM | 11,346 | 11,346 | 通过 |
| Windows VM | 2,924 | 2,924 | 通过 |
| 合计 | 41,758 | 41,758 | **83,516 项检查通过** |

测试使用 FastIO 创建原生文件和目录，通过原始 path_open、fd_fdstat_set_flags 和 fd_fdstat_get 的 wasm32 / wasm64 包装执行。覆盖 owned / borrowed 文件和目录、共享资源别名、权限拒绝、同步模式与 APPEND/NONBLOCK 组合、成功后的全部 native status bits、失败后的回滚、游标和文件内容保留。每仓库 POSIX 832 个组合，Windows 800 个组合。成功必须匹配独立计算的原生标志；ENOTSUP 必须保留原状态。

Windows 每仓库有 640 个组合在原始 path_open 阶段明确返回 ENOTSUP；macOS / FreeBSD 各有 256 个组合因不支持 RSYNC 打开而被拒绝。这些是验证了明确拒绝的组合，不能解读为这些 OS 已实现相应同步语义。Linux SYNC→DSYNC 返回 ENOTSUP，并保留原同步模式和别名状态；平台支持的状态转换和幂等请求仍可成功。

Linux 编译、交叉编译和两台 VM 均由认证的 guard 放入原来的 64 GiB、swap=0 cgroup。最终 guard 的所有自有进程均已通过 pidfd 确认退场，memory.events 未变化；本轮最大 parent memory.current 为 56,115,691,520 B。外部参与者的进程仅观察，不接管或发送信号。一次 FreeBSD 入场时共享 cgroup 的 swap.max 被外部改为 max，guard 在创建测试子进程前拒绝入场；检查 swap.current=0 后恢复原 swap=0，再完成测试。

macOS 使用本机原生编译器，编译与运行保持 2 GiB 上界；两个仓库的最大保守内存上界分别为 1,662,124,032 B / 1,668,087,808 B。执行测试禁止 fork。Windows / FreeBSD 使用各自实际 SDK，进入原生 guest 执行；串口 nonce、二进制摘要、退出码、KVM 启用及只读 base image 的前后状态均已核验。

失败尝试单独保留，不计入最终通过数：首个基线夹具误为目录申请了写访问；macOS 暂存和编译遇到 ENOSPC；首个 FreeBSD driver 用基本 grep 解释扩展正则，在第一个二进制通过后提前退出。修正 driver 后，两个相同且已核验的 FreeBSD 二进制重新完整通过。

本轮验证范围是修改涉及的原始 WASIp1 ABI 接口。完整 JIT/debugger 交互及跨 OS checkpoint 导入矩阵未在本轮重新运行。冻结源输入和原生编译依赖均已核验；交付时两个仓库的 6 个修改路径与测试源码一致，当前工作区参与本轮测试的源码依赖也与冻结输入一致。其他 agent 在 12 个 runtime / asm 调试相关路径的并发更新位于本轮 ABI 测试依赖闭包外，已保留。

[完整原始结果](wasip1_checkpoint_sync_transition_test_results.json)包含编译参数、编译器与依赖摘要、guard、guest nonce、通过数及失败记录。完整源输入与证据归档共有 12,696 个 payload，全部逐项验 SHA，包括 tar hardlink。

归档：test-inputs-evidence-1791233070644217140.tar.gz  
SHA-256：58a699b66ba73cfb554edbffcad117435958eae50d486aaa133b8031a35fffc7  
本机持久目录：/Users/liyinan/Documents/MacroModel/builds/wasip1-sync-transition-20261006-r10  
Linux 持久备份：/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-sync-transition-20261006-r10-private-vm/evidence/source-evidence.tar.gz

Windows 本轮私有磁盘在认证 QEMU 退场后无损 gzip，解压 SHA 核验后回收 2 GiB 预分配空间；基础 VM 和其他参与者资产保留。macOS 完成的源码暂存副本已在逐项校验持久归档后回收，可以从归档的 post/ 前缀还原。旧 R8 初始 macOS 暂存也经过既有持久归档逐项校验后回收。

**Wasm checkpoint 仍需与 WASIp1 checkpoint 在同一暂停点一起保存、一起协调恢复；文件内容依然由外部 mnt 提供。**
