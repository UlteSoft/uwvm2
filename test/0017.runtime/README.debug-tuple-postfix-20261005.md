本轮继续检查 DBG 的 language、Wasm、ASM、WASIp1 四组，并在 uwvm2 和 uwvm2-ros 同步补齐具体缺口。所有编译、QEMU、实际编译器产物验证、运行和单元测试均通过 SSH Linux 原 cgroup 的自有 PIDFD 保护器执行。完整原生语言调试体验仍未完成。

Rust 现在支持 `pair.0`、`pair.1` 和 tuple-struct 数字成员。别名只在原始 Rust CU 中匹配实际 `__0`/`__1`，字段偏移来自 DWARF；重复、歧义、未知位和不存在成员不会猜值。Zig 新增有限 `p.*`、成员链和 `p.* + 1`，沿用已有常规 guest pointer 的位宽、完整范围和复制权限检查。`.?`、optional/null、隐式 Zig pointer member 等仍缺失。

DAP watch/hover 接受相邻 `.*`。另修复了 evaluate 未检查回复 `source-stop` 的问题：查询前后停点相同也不能发布旧/无身份回复。缺失、重复、过期、空内容、越界大小和控制字节回复均失败并退休帧标签；可选 origin receipt 也须唯一并匹配 stop/thread。新断言能在旧适配器上复现失败。对象、值和常量 origin 仍是复制的显示文本，不转化成宿主/VM 地址或读取权限。新增 C++ 读取、构造文本、打印和扫描使用 fast_io。

| 本轮实际范围 | 两仓库结果 | 能证明的范围 |
| --- | --- | --- |
| Rust/Zig 新选择器 | 16 Linux QEMU profiles × 2，32 PASS | 实际目标 ELF、大小端/位宽及 owned DATA；不是语言停点 |
| Rust 实际 DWARF | 4/5 × O0/O1 × 2，8 产物 PASS | 真 rustc 编译、validator、类型/偏移解析和 owned-byte 查询 |
| Zig 实际 DWARF | Debug/ReleaseSafe × 2，4 产物 PASS | 真 Zig 0.17 编译、validator、指针类型/位置和 owned-byte 查询 |
| x86_64 相关组件 | 7 cases × 2，14 PASS | 实际 QEMU/native 输出一致；包括 Wasm/WASIp1 状态与变更 DATA |
| 关闭异常及 UBSan | 2 cases × 2 profiles × 2，8 运行 PASS | 选择器/数值表达式，16 个 build/run 命令通过 |
| 最新 DAP | 每仓库 204 PASS | 协议 DATA；不是真实产品 broker/native 平台资格 |
| PPC64/ARM 内核夹具 | ppc64 BE、armel、armhf × 2，6 PASS | 真信号/寄存器/指令及恢复；不是 Wasm owner 发出的停点 |

16 profiles 为 x86_64、aarch64、i686、riscv64、ppc64、ppc64le、ppc32、mips64、mips64el、mips32、mips32el、sparc64、loongarch64、s390x、armhf、armel。PPC64 ELFv1 修正的是夹具的描述符调用；ARM soft-float 修正的是夹具错误要求浮点参数在 VFP d0 中，仍验证 GPR、真实两条指令、无资格 FP 字节屏蔽、过期拒绝、取消和代码恢复。生产 ASM 的 Wasm owner、代码范围、停点/激活检查没有放宽。

x86_64 QEMU TF 单步仍失败。本轮诊断实际看到 `si_code=1`、异常向量 `-1`、TF 在保存 flags 中、si_addr 等于 PC；其分类与 [QEMU 11.1.1 的 i386 CPU loop](https://raw.githubusercontent.com/qemu/qemu/v11.1.1/linux-user/i386/cpu_loop.c) 一致。没有把 `TRAP_BRKPT` 当作成功的 `TRAP_TRACE`，也没有允许调试 VM。该诊断的负结果保留，不算正向 ASM 资格。

普通版尝试从本轮冻结源码重新编译 main/runtime/host API。首次实际编译触发原共享压力阈值中止；随后更保守的 6 GiB 启动余量等待 120 秒未满足，没有启动编译器。原 1 GiB 压力中止、64 GiB/swap0/cpuset/PIDFD/disk guards 不变。ROS 核验到的实际 SDK 头是 `.ros.10`，被当前 `.ros.11` 版本门拒绝；没有伪造 SDK 版本或复用旧 runtime 作为当前产品。本轮两仓库完整最新 `-Rdbg` 尚未取得资格。

语言清单仍覆盖 C、C++、Objective-C、Rust、Go/gc、TinyGo、Zig、AssemblyScript 共 88 个有限类别，现为 14 implemented、35 partial、37 missing、1 separate_level、1 prohibited_by_scope。Rust tuple 和 Zig pointer 类改为 partial；保留其他 agent 的历史资格。C/C++/Objective-C/TinyGo、标准 Go 和 AssemblyScript 的旧产物/运行记录不自动取得本轮资格。条件断点、watchpoint、反向执行、value history、完整语言调用/赋值、动态类型、容器格式化、Go goroutine、split DWARF 等仍缺失或不完整。详见 [完整能力清单](language_debug_capabilities_20261004.json)，这些类别数量不是原生支持百分比。

Wasm/WASIp1 本轮新增验证是组件范围，未重新取得全部 Wasm 3.0 中间状态、脚本/替换的完整产品资格。检查点仍不能声称所有实例与外部文件偏移、管道、socket、外部 I/O 的完整回滚。ASM 跨调用、返回/调用者展开、完整 guest native stack 仍有边界。

证据封存了 93 个有 receipt 的保护器尝试，其中 83 个根任务正常结束；收集进程成功与每个 case 通过分别检查，不能用根退出 0 覆盖编译/断言失败。所有失败和共享压力中止保留。原始输入/日志共 838 个文件，本机证据包 `/tmp/uwvm2-debug-complete-20261005/final-r2/evidence.tar.gz`，SHA-256 `d15ded7833da60b31615bf5ba5b7d485eeb80ee285efbe57cbb7b55287063e46`。所有本任务已完成根/后代 PIDFD 退休；OOM=60、kill=12、group=0 未增加，没有向其他任务发送信号。

本轮受影响文件在两仓库一致。冻结后并发修改的 native_provenance.h、uwvm_runtime_wasip1_portable_environment.h 和 wasip1_checkpoint.md 另记为 drift，保留其内容，不声称本轮覆盖它们。精确源码 ID、文件哈希、失败范围和证据位置见 [资格记录](debug_tuple_postfix_qualification_20261005.json)。
