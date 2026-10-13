# Go / TinyGo 数组 len、cap 调试补齐 — R34，2026-10-07

两个仓库同步补齐了有限 Go/TinyGo 固定数组和数组指针的 `len` / `cap`，包含实际 TinyGo 命名数组、嵌套数组、nil 数组指针、零长度数组和零大小元素数组。查询使用已有 stop/frame/root 资格和经过检查的 DWARF 类型图；C++ owned 字符串继续使用 fast_io。DW_LANG_Go 的省略 lower-bound 现在按 DWARF5 的零默认值解析。

[Go 规范](https://go.dev/ref/spec#Length_and_capacity)规定数组和数组指针的长度/容量为数组长度；参数中没有相关函数调用或 channel receive 时，参数不求值。因此有限语法中的 `len(*nilArray)` 和 `len(nilBox.Array)` 可以返回类型中的常量，不应先解引用 nil 指针。这里在动态 string/slice 路径之前检查数组类型。组件通过全部值字节未知、reader 拒绝所有读取的测试，确认这些常量查询不调用 evaluator reader；完整控制器仍需真实 stop/frame/root，并可能捕获 root 的 guest bytes，不能把组件结果扩展为整条控制器零读内存。

| 原始 TinyGo fixture 表达式 | 原始编译器 oracle / 实际 debugger |
|---|---:|
| len(box.Array)、cap(box.Array)、len(box.Named) | 4 |
| len(box.Matrix)、cap(box.Matrix) | 2 |
| len(box.Matrix[0])、cap(box.Matrix[1]) | 3 |
| len(nilArray)、cap(nilArray)、len(*nilArray) | 4 |
| len(nilBox.Array)、cap(nilBox.NilPointer) | 4 |
| len(box.Zero)、len(*box.ZeroPointer) | 0 |
| len(box.ZeroSized)，元素为 struct{} | 6 |
| len(box.Array)-1 | 3 |
| len(box.Array)+cap(box.Matrix) | 6 |

启动普通仓库的 full LLVM 产品：

```sh
uwvm -Rdbg -Rcc jit -Rcm full -Rct 0 -Rllvm-call-stack instruction -Rllvm-cache-path disable --run tinygo-O0.wasm
```

ROS 产品省去 `-Rcc jit -Rcm full`。在 `arrayProbe` 的真实源码断点取得当前 participant 和 stop-id 后，替换下列占位符：

```text
print <thread> <stop-id> len(nilArray)
print-frame <thread> <stop-id> 0 cap(box.Matrix[1])
ptype <thread> <stop-id> len(box.ZeroSized)
```

前两个分别得到 4 和 3；类型为 `int kind=scalar byte-size=4`。真实 producer 是原始 Wasm32 TinyGo 0.42.0 / Go 1.27.1，使用 canonical O0 build、linker `-extldflags=-O0`，并通过所有 Wasm features 的 validator 和完整 DWARF verifier。没有重写 Wasm、DWARF、源码路径或参数的有效范围。组件另外检查 guest width 4/8；这不声称已有真实 TinyGo Wasm64 产品验证。

| 同一最终源码 / 产品的检查 | uwvm2 | uwvm2-ros |
|---|---:|---:|
| 新鲜完整产品 TU + link | 3 + 1 | 3 + 1 |
| 原始 TinyGo O0 validator / 完整 DWARF verify | PASS | PASS |
| 新数组查询，两栈策略，会话 / 比较 | 2 / 60 | 2 / 60 |
| 同一 fixture 的旧 R33 产品拒绝，会话 / 表达式 | 2 / 60 | 2 / 60 |
| 原 string/slice 查询回归，会话 / 比较 | 2 / 18 | 2 / 18 |
| C 普通同名 len/cap 变量回归，会话 / 比较 | 2 / 16 | 2 / 16 |
| C17/C23/C++/Objective-C/Objective-C++，guest32/64，DWARF4/5，两栈策略，会话 / 比较 | 40 / 720 | 40 / 720 |
| 10 分钟重复真实会话 | 88 | 89 |
| 持续 compiler-oracle 比较 | 2640 | 2670 |
| 持续秒数 | 603.832 | 603.182 |
| 新数组 / 原 descriptor UBSan 断言 | 450 / 145 | 450 / 145 |
| 原 DAP 单元组、正 / 负语法、实际 C++ value rows | 4、595 / 330、595 | 4、595 / 330、595 |
| QEMU 架构配置 / 对应目标 ELF | 16 / 32 | 16 / 32 |

新的短会话合计 92 个、1628 次比较；持续会话另外合计 177 个、5310 次比较。每个数组会话检查 30 个有限表达式，以及显式 frame、同停点重复值、ptype、拒绝不支持表达式、伪造/过期 stop-id、guest 自检结果、exit 0 和 managed shutdown。重复比较次数不是独立功能数量。原 R33 string/slice report 的历史 soak 数字保留其原源码范围，本表全部新产品/soak 属于 R34 同一最终指纹。

QEMU 的 16 个 Linux 配置为 x86_64、aarch64、i686、riscv64、ppc64、ppc64le、ppc32、mips64、mips64el、mips32、mips32el、sparc64、loongarch64、s390x、armhf、armel。双仓库共 32 个 repository/profile job，执行 64 个真实目标 ELF，每 profile 的数组/descriptor 两组件分别通过 450 / 145 断言，合计 19040 断言。每个 ELF 的 machine、位数、端序及 GNU_STACK flags 均核对；大端、32 位宿主和 guest width 4/8 的组件语义结果一致。MIPS 组件沿用隔离 DATA fixture 的 execstack 编译选项，未扩大 VM/ASM 访问权限。这是 DATA 组件资格；完整跨架构 VM/JIT、语言运行时、单步/寄存器/ASM 调试体验仍需分别验证。

所有编译、功能 Python 和目标程序在 SSH Linux 原 64 GiB / swap=0 cgroup、原 CPU 集合中，通过 birth/PIDFD supervisor 执行。full-build/native/producer 角色保留 16 GiB owned RSS / 2 GiB output 上限；CLI/QEMU 独立角色保留 1 GiB RSS / 64 MiB output、8 MiB 单文件和 1 MiB root log 上限。原 6 GiB component admission、9288400896-byte arena floor、25 GiB host floor 和 OOM guards 保留。50 个 guard job 通过；8 个初次 MIPS 编译失败的 receipts/logs 保留，其原因是恢复 Debian cross SDK 时遗漏公共 linux-libc-dev 依赖。独立 a2 provider 加入 exact official package 后全部复测通过。另两条 x86 label 在启动测试前拒绝，修正 label 后通过。所有已启动 owned roots 已回收、descendant PIDFD 已退休，memory.events 全部保持零。

恢复的 GCC/libc/kernel headers、GNU linker 和 QEMU 只存在隔离 provider 目录，未安装到系统；官方 Ubuntu/Debian 包的 URI/SHA、提取清单、实际 compile dependencies 和执行文件均固定，before/after 验证。完整构建使用单独固定的历史 LLVM23 ROS.11 developer provider；最新 ROS 完整 LLVM provider parity 尚未验证。

生产查询只接受有效 Go/TinyGo 来源、单维零下界/已知 count、连续 row-major、已知 element/array extent，检查 count 的 signed guest-int 范围、乘法溢出和 extent 一致性；指针还检查 width、byte count、reference/address-class。未知边界、非连续/损坏 extent、foreign C/Rust lookalikes 和越界 matrix index 被拒绝。TinyGo 的嵌套 array DIE 已验证；单个 DIE 扁平记录多维仍拒绝。普通动态 string/slice 和非 builtin 指针读取资格没有改变。

map/channel、builtin shadowing、完整 Go 表达式/优先级/untyped constants、所有 producer/别名变体、gc Go 真实语言运行时、goroutines、Rust `.len()`、完整新 Go DAP broker/IDE 会话仍未完成。TinyGo O1 的 upstream runtime inline ranges 仍需严格 DWARF 验证，未放宽 verifier。其他语言的全部 native 体验也没有因本次组件测试取得资格。ASM 始终仅限 Wasm 产生的上下文，禁止调试 VM/私有 helper；本轮没有改变 ASM、checkpoint 或 WASIp1。

R34 frozen source 指纹分别是 `sha256:387b7b36f5ca23e5b2eb5267dc6a76cda025049e253d44eb7b3009e075f17d87` 和 `sha256:a50450635e5ba15c570af32c2f222dacc3e28766de836f2b484c63cab6767805`。本机两个 header、fixture、component、CLI/soak/QEMU 公共驱动共 7 个路径仍与测试 cut/独立 helper pin 一致。每仓库另外 8 个并发源码差异保留，未冒充整个最新工作区已测试。

机器记录：[debug_go_arrays_qualification_20261007.json](debug_go_arrays_qualification_20261007.json)。公共入口为 `debug_source_go_arrays.cc`、`run_debug_source_tinygo_arrays_cli.py`、`run_debug_tinygo_arrays_soak.py` 和 `run_debug_go_arrays_qemu.py`；必须由 Linux cgroup supervisor 驱动。

Linux 完整证据归档：/home/macromodel/Documents/uwvm3-implementation/retained-go-arrays-r34-final/go-arrays-r34-evidence.tar.xz，97578804 bytes，SHA-256 ce4cfca6c51e6135140782e5e532f770af0158d3efb7578bc87e40aa6802d844。全部 9163 个成员已逐一验证、输入 before/after 未变并 fsync；包含 frozen source、两份完整 VM、6 个对象、SDK provenance、成功/失败日志和 guards。SDK/QEMU 大目录按独立精确 pin 外部保留。本机仅保留小型说明和 sidecars。验证后仅退休 6 个重复 raw 对象副本，释放 353879400 bytes；最终两份 VM 和原始成功/失败日志仍保留，历史 R33 两份归档未改写。R33 旧产品在拒绝对照和 PIDFD 退休后由已验证的旧归档保留，可恢复；其原 report 的“二进制保留可执行”描述是 R33 完成时的历史状态。
