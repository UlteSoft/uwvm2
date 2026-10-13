# R36：新 world 的私有 call_indirect 绑定

两个仓库已同步实现新 source 的 canonical 类型表、call_indirect 目标缓存和私有表视图。表导入逐跳验证实际新记录的成员资格；defined 目标来自新引擎，import 目标借用新的私有 import cache。调用者类型投影使用未缓存的完整 Core 3 匹配，保留子类型和不匹配 sentinel；GC、extern、exn 表不暴露可调用视图，null 保留空目标。所有分配和检查在私有视图写入前完成，当前 runtime、TLS 类型缓存及全局表刷新 hook 不改变。

maximum_private_indirect_bindings 合计限制类型记录、caller 表视图和每个 caller 的 function-table 槽位，按 caller 计算别名扩张。超限返回 indirect_binding_preparation_declined。添加的 C++ 回归覆盖拒绝后同一旧 pause/epoch，以及准备候选的目标分类计数。

| 本轮验证 | 结果 | 范围 |
|---|---:|---|
| C++ 编译器前端 | 12 项通过 | 两仓库 Linux runtime/core/WASI；Windows、FreeBSD、macOS runtime |
| Wasm 输入组装及 validator | 8 项通过 | 两仓库的核心和 WASIp1 间接调用新输入 |
| 新源码原生运行 | 0 次 | 新 C++ 行为断言尚未运行，不能按通过计数 |

全部检查在原 SSH Linux 64 GiB cgroup 中完成，Linux LLVM-only + delayed template parsing；另外三个 OS 使用对应 SDK 进行交叉前端检查。这不替代四 OS 原生 full-mode 执行，也不证明新缓存运行时正确。没有在本机 macOS 运行本轮编译或 guest，不占用额外的本机 2 GiB 测试预算。分析文件受独立 16 MiB 上限约束，仅新增少量源码、日志与 Wasm 数据，没有编译对象、可执行文件或 VM 镜像。

原生测试启动门槛保持原值：测试目录低于 6 GiB、宿主盘保留至少 32 GiB。本轮最终磁盘检查仍拒绝启动；未删除其他 agent 的数据或进程。R34 原生结果属于 R34 冻结源码，R35 前端结果属于 R35，均未当作 R36 原生通过。

新增的 debug_checkpoint_indirect_retirement_cohort.wat 含 GC 表、空 typed table 和三个普通 function 槽（null、callback、setup）。debug_wasip1_indirect_prepared_retirement.wat 含三个槽（null、WASIp1 args_sizes_get、定义函数）。后续原生运行使用对应 prepared_retirement fixture，并追加第四个参数 indirect；已有三参数输入仍支持。跨模块 alias、继承类型、不匹配 sentinel 和恶意地址拒绝的原生行为验证仍待完成。

**整体 world 发布、恢复 guest worker 启动和 guest replay 仍未实现。** 本轮私有 compiled_module_record 只准备类型/表缓存，其 ready 标志保持默认关闭，不构成执行权限。下一步必须把真实代码/source/GC/WASI/worker 生命周期接入闭合发布事务。

Wasm checkpoint 发现 WASIp1 时必须在同一个停止点一起 checkpoint WASIp1；文件内容和外部 I/O 不会回滚。源码 hash、实际命令、日志 hash、PIDFD 回收和磁盘状态见同名 JSON。首次控制器使用错误 validator 路径的拒绝记录单独保留，没有算作通过。
