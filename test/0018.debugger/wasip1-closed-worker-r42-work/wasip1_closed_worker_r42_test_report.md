# WASIp1 checkpoint：发布前预创建 OS 线程（R42）

两仓库已同步实现真正保留的私有 OS 线程组：候选实例完成代码所有权准备后、旧实例开始退休前，分配全部固定 packet、ledger、FastIO native_thread，安装实际线程上的 root/debug/WASIp1 TLS，并整体停在私有关闭 gate 中。此前的短暂 rehearsal 线程仍按原语义全部 join，新的保留线程另行创建。

旧实例的执行排空与实际 OS/TLS join 完成后，管理器检查自己保留的同一线程组、真实 participant、native wrapper 和安装记录。放弃候选实例时，先取消 gate、实际 join 每个候选 OS 线程，核验根和 TLS 已清除，再重新打开原实例的普通入口。world 成员反向析构保证保留线程先退出，再释放其借用的 WASIp1 cache、帧、代码、GC 和 source。

新增 maximum_private_closed_workers（默认 256）和独立诊断字段；超出数量预算在旧实例退休前拒绝。诊断字段不授予运行权限。保留共享根在多个线程重复安装时，以原 rehearsal 的实际安装总量校验，避免只计算一次 retained roots。ROS 原模式及条件分支保留，C++ 新线程和输出使用 FastIO。

## 验证

- 最终 v2：24/24 Linux 原生用例，包括 Core 8、WASIp1 8、线程登记压力 4、活动 generation 2 嵌套帧 4。
- 其中 16 个用例真实保留、取消并 join 24 个新的候选 OS 线程，覆盖普通和间接调用、instruction 和 unwind；验证预算拒绝保留旧世界、whole-body lease、旧 TLS 未退出时保持关闭、旧 OS join 后重新核验、错误 token 拒绝、取消后真正 join 再打开旧入口。
- 24 项四目标 frontend：两个仓库的 runtime、Core 测试、WASIp1 测试分别编译 Linux、Windows、FreeBSD、macOS target。后三个 OS 本轮尚未原生运行。
- 原线程登记压力仍创建 2340 个真实 native worker、完成 1316 次 Wasm 返回 42；原 generation 2 同世界继续执行仍返回 1190。它们不证明 heap 回滚。
- 14 个 WAT 输入的 assemble/validator 共 28 项数据检查，不代表全部 Wasm 3.0 特性覆盖。
- v1 在原生测试开始前主动中断，修正 retained roots 计数；其进程全部物理退出、RAM 已清理、日志保留，未计入 v2 通过结果。

## 尚未完成

这个线程组仍属于私有候选世界。尚未联合发布新 source/initializer/GC/WASIp1/code，尚未给它真实执行 generation lease、迁入运行时 pause controller、在所有 root/ledger 就绪后打开入口，也尚未接通新世界 saved-frame 回放和恢复后的 proc_exit/终结清理。真实 OS create 失败的注入测试未覆盖。不能称为完整实例恢复或跨 OS 原生恢复通过。

Wasm checkpoint 必须同时 checkpoint WASIp1；跨 OS 需要重新配置 mount，状态包不保存文件内容。

## 资源与证据

全部本轮编译、validator 和原生测试位于原 64 GiB Linux cgroup；RAM 产品上限 768 MiB、aggregate RSS 上限 6 GiB、持久输出上限 16 MiB，原持久四 OS 套件磁盘保护不变。最终 guard 验证所有受管进程已物理退出、RAM 产品清理和父 memory.events 不变。归档包含精确 delta、原源码、完整命令/日志/散列并逐成员读回；同名 JSON 提供细节。
