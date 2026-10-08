# WASIp1 portable checkpoint 路径准入修复（R18，2026-10-06）

两个仓库的控制台和直接 request 校验原先只检查路径长度及 NUL，因此接受非法 UTF-8；DAP 和最终文件层已有拒绝规则。export 请求可能先生成 recording label 并捕获环境，之后才在文件层失败。

已在 wasip1_state 增加共享 valid_portable_path，并用于单环境、组 import/export request 和 portable 文件层。路径必须为非空 RFC 3629 UTF-8、无 NUL、最多 4096 字节。模块接口增加 UTF 模块依赖。命令拒绝发生在控制器分发之前，解析输出保持原值。argv/env 仍保持 WASI 原始字节规则，版本 1 文件格式不变，所有原生文件操作及字符串处理继续使用 FastIO。

修复前，两仓库真实命令解析器各 280 项基线检查通过，证明坏路径确实被准入；修复后同一夹具要求拒绝。覆盖四种操作、11 类非法 UTF-8、4096 字节截断边界、合法 Unicode、空/NUL/超长路径、原始 argv/env、实际 Unicode 单/组文件读写及拒绝时输出不变。

| 本轮实际测试 | uwvm2 | uwvm2-ros |
|---|---:|---:|
| Linux 新路径夹具 | 280 | 280 |
| Linux rebinding 回归 | 164 | 164 |
| Linux portable codec / 文件 / FIFO 回归 | 2127 | 2127 |
| Linux checkpoint 命令回归 | 17 | 17 |
| Linux DAP 协议回归 | 28 | 28 |
| macOS 本机新路径夹具 | 280 | 280 |
| Windows QEMU 新路径夹具 | 280 | 280 |
| FreeBSD QEMU 新路径夹具 | 280 | 280 |

Linux 测试、交叉编译与 QEMU 在恢复后的原 64 GiB、swap=0 cgroup 中；本任务使用同一串行租约和 8 GiB 文件系统，保留主机磁盘 32 GiB 空闲。仅 clean FreeBSD 镜像 materialization 阶段的单文件限制为 3 GiB（已知输出 2,671,443,968 字节），其余阶段仍为 1 GiB。guest overlay 限制和清理单独记录。没有清理或接管其他 agent 的目录、进程、VM。

macOS 两仓库编译与运行的保守内存上界分别为 1,362,903,040 / 1,358,610,432 字节，均低于 2 GiB；测试执行禁止 fork。

测试范围是本次变更的真实 C++ request/控制台解析器、codec、原生文件 helper 和 DAP mock 传输回归。没有把这些结果称为当前完整 uwvm CLI/LLVM JIT 重建、整个四层 DBG 或 C++ named module 构建通过。控制器在 wasip1_state::valid 后才进入 portable export 分支的顺序另经源码核对。

本轮修复后共 **6912 项检查通过**；另有 560 项修复前基线检查。Windows / FreeBSD 均有随机 nonce、精确 PASS、测试与 QEMU 退出 0、KVM enabled、原始 backing identity 不变的证据；两个 VM 的私有 overlay、ISO 和复制的 vars 已在正常退出后删除。下载失败/取消、夹具与打包驱动配置失败单独保留，未计入通过数。

最终保留一份 Windows SDK、一份 FreeBSD SDK 和已校验只读 FreeBSD base，登记到原 active-environment JSON 供后续复用。8 GiB volume 的 backing image 实际分配 **4,059,013,120 字节（约 3.78 GiB）**；宿主文件系统空闲 **124,253,106,176 字节（约 115.7 GiB）**。已清理本任务的不完整下载临时文件，未留下私有 VM overlay。所有最终 guard 的 memory.events 未变化，已绑定自有 PID 均确认退出。

Linux 证据归档 `91df2e8b3919df7d0f46b4faf1bb9f0eb38e133a811a578cbb77a43efff76c8e`，13,337,614 字节；其中 4,753 个 payload 均已在 Linux 和 macOS 逐项重读核验。原始结果和报告同步到两仓库，保留实际 SDK/编译器/依赖/二进制/日志摘要及失败尝试。完整 archive 仍保存在本机临时证据目录与受限 Linux volume，归档不复制 OS base 和 SDK 大块内容。
