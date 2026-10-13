# R35：私有新世界 dispatch 绑定及编译记录修复

uwvm2 与 uwvm2-ros 同步了 7 个源码、测试和文档路径。候选世界现在准备新源码的 defined/import 缓存，以及 table/funcref 查找所需的排序函数指针范围；逐项检查新引擎、typed entry、sealed resume plan、generation、module 和 profile。import 从新世界解析并保留 caller 的值命名空间，仅允许已认证的 builtin WASIp1 原生元数据。缓存配额和 native payload 在分配前检查。失败发生在旧 worker retirement 启动之前。

修复 ROS full-only 缓存没有主仓库浮点策略字段的兼容问题。WASI 签名读取使用返回成功/失败的元数据接口，避免普通运行时 helper 在准备失败时直接终止进程。新增断言覆盖绑定配额不足时原 guest 继续暂停、epoch 不变，以及成功准备时定义、导入和指针范围计数；这些新增 C++ 运行断言本轮尚未原生执行。

| 验证 | 本轮结果 | 范围 |
|---|---|---|
| 最终源码语法检查 | 12 项通过 | 两仓库 × 四目标的 runtime，共 8 项；两仓库 Linux core/WASIp1 测试文件，共 4 项 |
| 缓存编译命令回归 | 12 项通过 | 复现旧 bug，缓存/新步骤返回自身记录，参数或日志改变拒绝，历史控制器保持原样 |
| 历史编译记录更正 | 2 条已补充 | 主仓库 macOS runtime/host-api；ROS 两条记录独立核对正确；两份归档全部 payload 再次读回 |
| R35 原生执行 | 0 次 | 尚未运行新源码的 Linux、Windows、FreeBSD 或 macOS 原生回归 |

所有最终前端检查在原 SSH Linux 64 GiB cgroup 内完成，使用 LLVM-only 分支及 delayed template parsing。Windows、FreeBSD 和 macOS 项是使用对应 SDK 的交叉目标语法检查，不是目标 OS 原生执行，也不替代 full interpreter/JIT 回归。本机 macOS 前端触及内存停止线后已终止；没有计为通过。

原生回归仍不满足原来的磁盘启动条件：测试目录需低于 6 GiB，主盘保留空间需至少 32 GiB。限制保持不变；未清理其他 agent 的文件或进程。本轮新增分析文件由 16 MiB 上限约束，不生成编译对象、可执行文件或 VM 镜像。最终源码和文档的 14 项输入与冻结 manifest 一致；候选缓存只保存私有数据。

原 R34 的 48 次原生执行仍对应 R34 冻结源码。缓存复用时，旧 cross.py 把结果列表最后一条（wasm 验证）命令写入主仓库 macOS 对象 qualification。R35 已修复返回记录的逻辑，并用原编译日志、实际对象 hash、依赖 hash 和归档 payload 补充更正记录；原 qualification、归档和原生测试矩阵均未覆盖。

**整体 world 发布、恢复 guest worker 启动和 guest replay 仍未实现。** 下一步需完成这些事务，并在磁盘条件恢复后执行四 OS 的 full-mode 原生回归、跨模块 defined import/alias/table/funcref 及错误元数据拒绝用例。

Wasm 与 WASIp1 必须在同一次 cooperative stop 中一起 checkpoint；可见 WASIp1 时设置 `include_wasip1 = true`。Strict managed 模式仍拒绝外部资源；显式允许保留外部绑定不代表能够回滚外部 I/O 或文件内容。

详细命令、source hash、pidfd retirement、内存峰值、磁盘状态和更正记录见同名 JSON；执行日志和输入归档保存在 `wasip1-function-bindings-r35-work` 以及远端对应 R35 目录。
