# R58：当前完整源码、真实常量调试与 ROS .12 验证（2026-10-08）

本轮补齐 R57 之后整份当前源码的验证。普通版和 ROS 均捕获完整 src 与实际依赖头文件，重新编译 runtime、host、main 三个 TU 并链接；两仓库各有 118 个变化源文件和 10 个新增源文件进入捕获版本。捕获完成后的并发修改不计入本轮资格。

新增并同步 run_dap_direct_constants.py，验证编译器原始 DW_AT_const_value 从 CLI 到真实 Linux broker/DAP 的路径。没有修改其他 agent 的生产源码或后端文件。

## 实际常量调试

C/C++ × DWARF4/5 × O1 × instruction/unwind，两仓库各 8 个 CLI 用例、8 个 DAP 会话。Source variables、Watch、Hover、variables context 的值和类型一致：

| 变量 | 值 | C 类型 | C++ 类型 |
| --- | --- | --- | --- |
| leaf_negative | -31 | const int | const int |
| leaf_positive | 23 | const unsigned int | const unsigned int |
| leaf_boolean | true | const _Bool | const bool |

16 个 DAP 会话共 288 次 evaluate、96 个直接常量来源收据。每个收据核对编译器原始 DIE 的直接 DW_AT_const_value、无 DW_AT_location、具体父 scope、type 引用，以及真实 thread、stop、Code offset。不会用源码字面量或同名其他变量替代来源证明。

单步前后分别读取三项常量；值只读、不暴露 memoryReference；立即排队的旧 frame/scope/evaluate 引用均被拒绝，新的停止点重新取得来源。原始 guest OS wait、broker wait、DAP EOF 均为 0。原始编译器、validator、DWARF verifier/oracle 命令及日志哈希一起核对，Wasm/DWARF 未重写。实际 IDE UI 未计入验证。

## ROS 的真实依赖

当前 ROS 生产源码要求 23.1.1-uwvm-ros.12 和 LLVM_UWVM_X86_TAILCC_ALIGNED_FRAME_FIXED=1。SSH 旧 .11 provider 被校验拒绝后，未放宽版本条件，也未伪造宏；从全部 12,874 个清单条目一致的私有源树重新构建 .12 的 64 个 X86 静态库。

CMake 的实际 Release consumer contract 提供完整库顺序；ROS 使用新生成头文件、真实新库重新构建完整产品。metadata consumer 和 llvm-config 可执行文件未构建或执行。

当时本机清单与 7 个并发修改中的 MIPS/LoongArch 文件不一致。本轮只在私有测试树恢复 SHA 完全匹配清单的旧字节，保留本机 peer 修改，构建 X86 provider。因此这些待提交后端修改及其他 ISA 的最新完整 provider 不算通过。

## 完整回归和环境

- 两仓库各 493 项 DAP/常量来源单测、11 个 C++ 组件程序通过。
- 捕获数值的限定类型共 1284 个真实会话、46224 次 evaluate，涵盖四个 C-family frontends、Wasm32/64、DWARF4/5、O0/O1 和两种栈策略。
- 732 个 caller、aggregate、false bool、float、primitive、C/Rust/TinyGo/Zig 等已有真实 broker/DAP 回归会话通过。
- 持续覆盖原始 32 profiles，交替 instruction/unwind：uwvm2 622.204 秒 / 576 会话；uwvm2-ros 623.121 秒 / 576 会话。
- 当前源码的 portable copied-value/type DATA 组件在 16 架构 × 两仓库执行 32 个实际目标 ELF；machine、位数、端序、依赖/provider 哈希和输出对 pinned native x86_64 的一致性全部核对。架构包括 x86_64, aarch64, i686, riscv64, ppc64, ppc64le, ppc32, mips64, mips64el, mips32, mips32el, sparc64, loongarch64, s390x, armhf, armel。
- 所有实际编译、validator、DWARF、VM、DAP、组件和 QEMU 测试均在原 SSH linux 的 64 GiB / swap 0 cgroup 内执行；birth/PIDFD 原始监督与退役收据保留，OOM 未增长。

保留的失败收据包括 .11 版本拒绝、单测缺失 helper 导入、CLI Finder 元数据 fingerprint 格式不符，以及 provider 监督封装的目录错误和退出进程 metadata 竞态。修正测试封装后建立新的冻结 cut 重跑，没有把失败轮次算作通过。

## 仍未完成

本轮验证的是捕获时的完整源码和上述有限真实路径；不是全部 native 语言调试能力完成。uwvm-int full 的本轮实际执行、module BMI、所有语言/ABI和优化 location、标准 Go/AssemblyScript 全能力、float/block/aggregate 的直接 DW_AT_const_value、赋值/watchpoint/guest call 等仍未全部完成。

QEMU DATA 通过不能代表完整目标 VM/JIT/DAP 或各架构 native IDE 体验一致。ASM 仍严格限定 guest Wasm 所产生的上下文，禁止调试 VM 本身。88 项历史统计保持 14 implemented、39 partial、33 missing、1 separate level、1 prohibited；这些数字没有被本轮有限用例自动提升为全能力通过。

本机同步的新 driver 哈希已核对；捕获结束后源树变化数量见详细 JSON，后续变化不在本轮完整构建资格内。

## 证据和空间

详细收据：[debug_current_language_qualification_r58_20261008.json](debug_current_language_qualification_r58_20261008.json)。
SSH canonical：/home/macromodel/Documents/uwvm3-implementation/retained-current-language-r58-final/

完整证据归档 424,859,524 bytes，SHA256 4d8fe3e60907feaa6c0f872fe39bc5302b75429563bfe28ff337c16ce44c97b6；90,510 个成员逐项流式校验、原文件前后哈希不变、归档 fsync。符号链接另有清单。中间对象恢复归档另保留。

已归档校验后只清理本轮自有 2,180 个 .o/.pch，释放 758,884,072 bytes；再清理 2 个已经被完整归档覆盖的重复上传 tar，释放 343,224,320 bytes。VM、Wasm、静态库、目标 ELF、源码和协议日志保留；未删除其他 agent 文件。本轮目录去重后的实际分配约 1.972 GiB，SSH 可用约 28.53 GiB，均为收尾观察值。本机仅下载小型证据 JSON，未下载新 tar。
