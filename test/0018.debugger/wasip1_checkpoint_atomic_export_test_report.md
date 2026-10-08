# WASIp1 portable checkpoint 原子导出及模块构建（R24）

日期：2026-10-07。`uwvm2` 与 `uwvm2-ros` 同步修复；没有调整 ROS 的执行模式。

## 已实现

旧导出直接写公开目标路径。其他读取者可以在写完或检查关闭之前看到部分 checkpoint，失败清理还需处理目标路径被替换的情形。现在 single/group 共用 FastIO `write_exclusive_file`：先钉住父目录，在随机私有子目录独占创建 payload，写完整并检查关闭后，通过 FastIO `native_linkat` 独占发布最终名字。已有文件或目录、以及竞争写入者都不能被覆盖；清理只触及本次私有临时目录，永不删除最终路径。关闭/写入失败不发布；成功发布后的清理错误不会撤回成功。POSIX 父目录被 rename/rebind 时，发布和清理跟随原目录句柄；Windows 实测确认私有子目录句柄会阻止 NT 父目录重命名，收到明确 STATUS_ACCESS_DENIED 后继续导出，校验原父目录、完整 wire 与清理均正确。

目标文件系统必须支持原生硬链接。[Microsoft 官方文件系统对照](https://learn.microsoft.com/en-us/windows/win32/fileio/filesystem-functionality-comparison)列明 NTFS 支持，FAT32/exFAT 不支持；不支持时返回原生错误，没有非原子退路。私有目录需要可信父目录：POSIX 使用 0700，Windows 继承父 ACL。无法清理时可能留下私有目录，最终名字仍可重试。本轮保证原子可见性，未加入 fsync 或断电持久化保证。格式不变，WASI checkpoint 不携带宿主文件内容；跨 OS 导入仍需重配挂载。

另外修复 `wasip1_state.cppm` 的 FastIO scan manipulator 可见性：全局模块片段包含 FastIO 声明，现有 hex_get 权限解析代码在 named-module 构建中可编译。checkpoint 模块使用已导出的 u8native_file 目录类型。未修改 FastIO 供应商源码。

**Wasm checkpoint 必须同时捕获 WASIp1 checkpoint，在同一个协作停止点完成。** Wasm 内存快照不包含 FD、argv、环境变量、挂载等 WASIp1 状态。本轮没有实现原子联合恢复。

## 测试结果

修复前两个仓库共 1764 项观察复现公开部分文件及旧清理行为；基线通过表示成功复现旧行为，不计入修复后的通过数。

| 平台 | 新原子导出检查（两仓库） | 额外回归 | 执行环境 |
|---|---:|---|---|
| Linux | 1712 | codec 4254、路径 560、close 130、cleanup 1990、DAP 56、module consumer 20 | 已恢复的 64 GiB swap=0 cgroup |
| Windows 11 | 1712 | — | 实际 KVM Windows 客机，原生 NT FastIO 后端 |
| FreeBSD 15.1 | 1712 | — | 实际 KVM FreeBSD 客机，原生 POSIX FastIO 后端 |
| macOS | 1712 | — | 本机，聚合 RSS 限制 2 GiB |

共 **13858 次检查通过**；重复仓库、平台和场景均计入，是断言/测试观察数，不代表独立功能数。Windows nonce `498ae4cd857b62e4e3a2d92f73db6d5e`，FreeBSD nonce `d5e5d3331242ea2eb8d1263b4e1d7cca`。两个客机每仓库均准确报告 856 PASS、退出 0；KVM 实际启用，QEMU 退出 0，基础盘 stat 前后相同。

新夹具使用真正文件操作，覆盖实际先写 7 字节后的可见性、检查关闭前的可见性、原始写入/关闭错误、清理错误、发布错误及不支持硬链接错误、竞争目标、预先存在文件/目录、payload 打开失败、mkdir 碰撞、父目录替换/Windows 原生拒绝、253 字符最终文件名（Windows 使用显式扩展路径）、正常单个/整组 wire 往返、非法 UTF-8/图的 IO 前拒绝。错误在测试翻译单元注入；产品没有测试钩子。并非真的制造磁盘满或硬件 I/O 故障。

两个仓库的 named-module 依赖图完成 BMI/object 构建、链接和真实 consumer 运行，每个 10 项。模块实际依赖 SHA 在最终快照再次核对；其后只有文档/夹具/其他非模块依赖的集成更新，因此没有复用失效 BMI。真实 LLVM 头文件下的完整 controller header 两仓库语法编译通过，这两次没有计为运行测试。未重建运行完整 CLI/LLVM JIT，未在活跃 Wasm 上验证 WASIp1 状态恢复；四 OS 结论限定为 native 文件 helper。

## 修正过的测试与失败记录

保留且排除所有未通过的编译/测试尝试（9 次 guard 尝试，3 次客机失败记录）：初始 probe constructor 选择/默认权限问题；FastIO native OS 路径不能接收 string_view 的编译错误，改为 owned text；旧 cleanup 夹具假设 `.kept` 存在导致 ENOENT；模块 hex_get 声明不可见；module consumer 使用供应商未导出的 at_fdcwd，改用公开目录 handle；Windows 夹具将 NT 不允许的父目录重命名误当成功场景，以及普通 DOS 长路径在绝对读回时报 NAME_TOO_LONG；按实际原生规则修正预期和路径形式，详见独立 diagnostic record 和 serial log。它们不计为通过，修正后按最终源码重新编译运行。

## 资源与证据

Linux 重启后复用其他 agent 已恢复的原 64 GiB anchor，按既有 UUID 重新挂载本任务 8 GiB ext4 卷，核对 boot ID、PID birth、guard SHA 和 SDK 清单。没有创建第二个大 cgroup，没有格式化卷，没有终止其他 agent 的进程。检测到其他 agent 的 35 个 controller/runtime/DAP 等文件更新，复制到新的集成快照后重新检查完整 controller header 和 DAP；没有编辑或回滚这些文件。具体旧/新 SHA 在 integration-input-refresh.json。冻结后继续发生的非本轮源码修改（记录时 6 个文件）见 live-source-drift-after-freeze.json，未覆盖在本轮集成/DAP 结论中；本轮负责的 14 个文件全部保持已测 SHA。

本任务 flock 串行编译/VM；每个客机 2 GiB、2 vCPU。macOS 聚合编译 RSS 保守上界 1088012288 字节；本机测试禁止 fork。所有 guard history 的 memory.events 前后相同，绑定进程均经 pidfd 退休。私有 VM overlay、ISO、Windows vars 已在退出及证据固定后删除。归档前本卷占用 5086072832 字节、可用 3398443008 字节；最终占用见 delivery receipt。

冻结前后源码、FastIO 模块输入、二进制、BMI/object、编译命令及依赖 SHA、四 OS 日志、失败记录、VM nonce、provider 清单和 guard 位于 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/rounds/wasip1-atomic-export-20261007-r24/evidence.tar.gz`。归档逐项读回；本机再次流式读回并校验完整 SHA，不留大归档副本。验证后仅退休本轮自己的本机备份和上传 wrapper。
