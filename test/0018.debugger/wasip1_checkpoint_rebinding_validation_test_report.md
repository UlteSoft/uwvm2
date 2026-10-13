# WASIp1 checkpoint：重绑命令校验修复（R16）

2026-10-06，uwvm2 与 uwvm2-ros 同步修复。ROS 的运行模式配置未改动。

## 问题与结果

底层 portable 重绑解码器原本会拒绝错误列表，但上层 WASIp1 请求和 CLI 解析器没有使用同样的校验：单环境导入接受任意非 NUL 文本，组导入只检查字符集合。重复资源编号、数值溢出、缺少逗号和第 65 项因此可能先通过命令解析，控制器读取 checkpoint 文件后才处理错误。文件本身不存在时，还会先得到文件操作错误，掩盖脚本错误。

现在单环境、组导入和 detached 重绑解码共用 FastIO parse_by_scan 数字扫描器。RESOURCE 必须在 0..65535，TARGET_FD 必须在 0..INT32_MAX，最多 64 项；资源编号按数值去重，因此 `2=7,0002=8` 也拒绝。不同资源写同一个 TARGET_FD 在语法上仍允许，实际恢复继续执行已有的独立资源所有者校验。空列表继续表示自动选择；导出拒绝无关的重绑参数。

CLI 和结构化 request 校验在读取文件前拒绝错误列表。控制器也把单环境检查提前，并先检查所有组 selector，再打开组文件。失败不发布已扫描的前缀，也不覆盖调用者的 request 或重绑输出。

```text
# 合法；文件 /tmp/wasi.uwp 仍须存在，目标 FD 仍须实际持有所需能力：
set wasip1 import 0 2f746d702f776173692e757770 2=7,3=8

# 以下现在直接在命令解析阶段拒绝：
set wasip1 import 0 2f746d702f776173692e757770 2=7,2=8
set wasip1 import 0 2f746d702f776173692e757770 2=2147483648
set wasip1 import-group 2f746d702f776173692e757770 0:2=73=8
```

每个仓库修改五处生产/文档文件，并新增一个回归测试：

- wasip1_state.h：共享扫描与单环境/组请求准入。
- wasip1_portable_checkpoint.h：底层解码使用同一扫描器，仍在完整成功后交换候选输出。
- wasip1_portable_checkpoint.cppm：增加对 wasip1_state 模块的依赖。
- controller_wasip1_portable.h：把重绑校验提前到文件读取前。
- wasip1_checkpoint.md：记录范围、去重、空列表和实际恢复能力边界。
- test/0013.debugger/wasip1_rebinding_validation.cc：真实 CLI 解析、请求校验、底层解码、边界及原生 FastIO 文件测试。

没有改变 wire version=1、恢复权限、recording label 规则或保存的文件内容范围。C++ 字符串、打印、文件操作与数字解析使用 FastIO。

## 已执行验证

| 检查 | uwvm2 | uwvm2-ros | 范围 |
|---|---:|---:|---|
| Linux 旧实现对照 | 164 | 164 | 实际旧生产头文件；确认错误命令仍会被接受 |
| Linux 新重绑测试 | 164 | 164 | CLI、request、底层解码、原生单/组文件 |
| Windows QEMU/KVM 新重绑测试 | 164 | 164 | guest 内真实 Windows 程序与 FastIO 文件 |
| FreeBSD QEMU/KVM 新重绑测试 | 164 | 164 | guest 内真实 FreeBSD 程序与 FastIO 文件 |
| macOS 本机新重绑测试 | 164 | 164 | 原生程序与 FastIO Unicode 文件名 |
| Linux 已有 portable 格式回归 | 2126 | 2126 | 签名、UTF-8、FD 布局、稀疏 reserved、扫描预算、失败输出等 |
| Linux 已有 checkpoint 命令回归 | 17 | 17 | 原有命令与 if-stop 行为 |
| Linux 完整 controller.h 编译 | 通过 | 通过 | 实际控制器及 portable 控制分支，使用真实 LLVM 头文件 |

修复后共 12 个原生测试进程、5598 项检查全部通过；旧实现对照另计两个进程、328 项。控制器编译两次另计，不加入检查数量。新测试覆盖 19 类错误列表、数值上界、零填充重复资源、允许重复目标 FD、64/65 项、4096/4097 字节、失败输出不变，以及单/组 version-1 文件的真实独占保存和 Unicode 文件名读回。

Linux 编译、程序测试与两个 QEMU guest 均由 guard 纳入原来的 64 GiB cgroup，swap=0，所有自有进程完成后通过 pidfd 确認退出。其他 agent 的进程只观察，不纳入本任务所有权。macOS 编译及测试的保守内存上界分别为 uwvm2 1380122624 字节、ROS 1374814208 字节，均低于 2 GiB。

Windows 最终 nonce 为 `53a2d5b811fa39dfc79f118932b75460`，FreeBSD 为 `1703ddeb475f48694ad0a6713489dcc0`；两个 guest 内两仓库都输出 164 checks / unsupported=0 / status=0，最终 QEMU 正常退出，base 盘元数据未改变。写入使用本任务私有 overlay。

初次测试夹具的 FastIO 显式 view 和指针打印用法已修正；完整控制器编译的 backend 宏和 LLVM include 配置补齐后通过。第一次 Windows 自动输入未进入 Run 命令框，没有执行测试；仅停止并退出那次自有 VM，增加启动等待、显式选择输入框后重试通过。失败或取消的 guard 历史不计成功检查。

## 证据与限制

实际编译输入和 SDK/链接文件 SHA、argv、日志、before/post 冻结源码、原生测试二进制、Mac 源码/日志和 VM nonce receipt 已归档。逐项读回校验 14978 个 payload，归档 52873778 字节：

```text
/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-rebinding-validation-20261006-r16/source-and-test-evidence.tar.gz
SHA-256: 5df4345cea5ed20297f44de3560bd2d3b2fd9a6142f62ab6f2ac876715e3f1e0
```

完整原始 test-results.json 与编译依赖字典在归档中；本报告旁的 results JSON 保留摘要、实际测试 receipt 与原始文件哈希。两个生产源码快照各 6239 个文件；此次交付的 12 个源码/文档/测试文件哈希与执行输入一致。并发 agent 的其他源码按测试时快照冻结，未修改或回退。

磁盘不足时，仅在核对原始 R6 归档及每个文件 SHA 后，删除了本任务已完成的 6313 个旧 macOS 临时源码副本；完整内容仍可从原归档恢复。归档分配仅使用新的私有文件，没有改变文件系统全局保留策略。

本轮验证输入校验、CLI 解析、文件格式与实际 native 文件 I/O，不重新声称四个 OS 的运行中完整 VM 恢复、所有 Wasm 特性、全部热更新路径或全量 C++ Modules 构建已验证。元数据与编号仍然只是数据；真正恢复仍需目标 source/provider、当前停点和 capability 证明。

**Wasm 与 WASIp1 checkpoint 必须在同一个真实 cooperative stop 同时采集；整实例联合原子恢复仍需单独实现和验证。**
