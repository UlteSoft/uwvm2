# Zig @as 类型证据与无额外读取修复（2026-10-06）

本轮在 uwvm2 和 uwvm2-ros 同步修复有限语言表达式求值器的三个缺口。
评估值与推导类型现在使用同一套 Zig 转换检查；没有扩大表达式语法或 VM/ASM 调试权限。

## 可复现的问题与修复

以下示例使用调试器现有的 C 风格 `?:` 语法；它不是 Zig 的 `if` 表达式。
假设 `value` 的已声明类型是 i32：

| 调试器表达式 | 修复前问题 | 修复后 |
| --- | --- | --- |
| `1 ? 7 : @as(i16, value)` | 未执行分支绕过安全转换检查，误接受目标类型 | unsupported；类型检查不读取变量值 |
| `1 ? 7 : @as(u8, 256)` | 未执行分支漏检字面量范围 | arithmetic；仅根据自身持有的字面量 DATA 判断 |
| `@as(i64, value + 2)` | 已知不支持复合操作数，仍先读变量再拒绝 | unsupported；在值回调前拒绝不支持的语法 |

修复集中在 `source_scalar_expression.h`。
提取 `zig_coercion_constant`、`zig_coercion_source_supported` 和 `check_zig_coercion`，
让 `infer_type` 与实际求值共享现有的符号、位宽和常量范围规则。
类型回调的值位被清零；只有直接字面量自身的数据可以作为常量范围证据。
支持的未执行分支（包括变量值不可用但声明类型可用的分支）保持零次值读取；
`1 || @as(i64, missing)` 和 `0 && @as(i64, missing)` 的逻辑短路保持不变。

原始头文件 SHA-256:
`d58991fb7caf1b5e8351dfff3dae06206fbebe5252a99503943ee8f880deba8f`。
修复后:
`4c231e94412fdafa8de8614f0f9b169ebb1976bdb7c657cbd05e46c6c4d8e7da`。
两个仓库均以旧头文件分别复现以上三类失败，然后使用新头文件通过回归。

Zig 的安全 widening 规则以源类型所有值都能表示为条件；
编译期数字转换另有可表示性要求。规则依据官方
[整数与浮点 widening](https://ziglang.org/documentation/master/#Type-Coercion-Integer-and-Float-Widening)、
[编译期数字转换](https://ziglang.org/documentation/master/#Type-Coercion-Compile-Time-Known-Numbers)
和 [@as](https://ziglang.org/documentation/master/#as) 文档。
本轮没有运行 Zig 编译器，也没有将 C 风格条件表达式当作原生 Zig 编译期分支。

## 验证结果

所有编译、原生执行、Python DAP 回归与 QEMU 执行均在 SSH linux 原有 64 GiB cgroup 内完成。
本机仅用于读写源码、比对、保存证据。

- 每个仓库的新 `debug_source_zig_typeproof.cc` 有 2,960 条断言，
  覆盖 12 个转换目标、整数/浮点、符号/位宽、Wasm32/64 类型上下文、
  三种条件分支位置、嵌套转换、边界常量、拒绝类别和值读取次数。
  类型回调故意携带伪造值位，验证类型检查不会借用这些值。
- 两个仓库各通过 16 个原生阶段：7 次编译、6 个当前 fixture 执行、3 个旧实现预期失败复现。
  现有 Zig、整数/浮点、条件表达式、字符字面量和 DAP bridge 回归全部通过。
- 字符字面量 C++/DAP 差分回归各 11,802 条（594 正例、1,208 反例和 10,000 算术性质），合计 23,604 条。
- 使用实际公共 `run_debug_linux_qemu_components.py` 测试入口，
  每架构/仓库运行新类型证据与既有 Zig 转换两个组件。
  32 个架构/仓库组合、64 个组件结果全部通过；
  新 fixture 在矩阵中累计 94,720 条断言。
  每个 QEMU 输出与固定 x86_64 原生对应组件逐字节一致。
  这里的“一致”只指这两个有限 DATA 组件，不代表完整调试体验或性能一致。

| Linux 架构 | uwvm2 | uwvm2-ros |
| --- | --- | --- |
| x86_64 | PASS | PASS |
| aarch64 | PASS | PASS |
| i686 | PASS | PASS |
| riscv64 | PASS | PASS |
| ppc64 | PASS | PASS |
| ppc64le | PASS | PASS |
| ppc32 | PASS | PASS |
| mips64 | PASS | PASS |
| mips64el | PASS | PASS |
| mips32 | PASS | PASS |
| mips32el | PASS | PASS |
| sparc64 | PASS | PASS |
| loongarch64 | PASS | PASS |
| s390x | PASS | PASS |
| armhf | PASS | PASS |
| armel | PASS | PASS |

实际 ELF machine、32/64 位和大小端均核验；保存编译依赖、编译器、链接器、
QEMU、已安装链接提供者、脚本、输入、ELF、输出日志与 SHA-256。
链接提供者在每个组合运行前后均验证。
完整冻结生产源码和本轮选定 fixture 使用以下 source_id：

- uwvm2: `sha256:b4c1df1ab92b4c4c23744989a180fed0b8cdddce39fa374ccc13438326b533db`
- uwvm2-ros: `sha256:7c59d8eb2978645ec65cd06d62e7ea485e6e77551bacdab859d7a562bde35a57`

34 个成功监管阶段累计 1045.764 秒（含编译与监管，不是性能基准）；
最高自有进程树 RSS 723496960 字节。
每阶段 memory.events 前后相同，原有历史 OOM 计数没有增长。
所有已跟踪自有 PIDFD 均证明退休、根进程已回收，其 PID 不在结束时的 cgroup 名单内。
该名单属于共享 cgroup，仍包含 init 和其他任务；这些任务没有被触碰。

本轮只使用专用于已拥有的有限 DATA 测试的监管入口：
原有 memory.max=64 GiB、swap.max=0、固定 CPU、身份/血缘/PIDFD/回收检查不变；
自有聚合 RSS 限 1 GiB、单输出文件 8 MiB、日志 1 MiB、输出总量 64 MiB，
实际写入文件系统保留 1 GiB / 4096 inode，使用独立 TMPDIR。
这一入口不授权构建或运行 VM/JIT/SDK/语言生产者。
完整产品的既有资源门槛未降低。

## 本轮边界与未完成事项

仍是有限、只读数字表达式 DATA：
typed 8-bit carrier 无法区分 bool 与窄整数，因此继续保守拒绝；
复合 CTFE、负号常量 CTFE、用 C cast 证明 Zig 转换、Unicode 和完整 Zig 语言求值尚不支持。
typed 整数与浮点之间转换的原有限制也没有扩大。

本轮没有新资格证明实际 Zig 生产者 DWARF、运行中 VM 停止/帧/内存、完整八种语言 native 体验、
Wasm3 全特性或 ASM 调试。ASM 的 guest 上下文边界没有修改。
当前资源余量不满足完整产品既有磁盘保留门槛，因此没有用这次 DATA 通过结果替代完整产品验证。
ROS 模式限制未修改，未触及其他 agent 的代码或资源。

原始证据位于
`/run/user/1000/uwvm2-zig-typeproof-20261006-a1`。
包括两仓库完整冻结生产源码、旧头文件、实际测试脚本、原生/QEMU ELF、
依赖/输入清单、日志、监管回执、差异和本 JSON 统计。
压缩归档及持久备份状态另见 `zig-typeproof-evidence-archive-a1.json`；
最终保存后的准确路径在配对资格 JSON 的 `evidence_backup` 字段。

复跑单组件时使用本轮新增公共入口的 case:
`--cases debug_source_zig_typeproof,debug_source_zig_coercion`，
并继续使用对应 cgroup 监管，不直接在登录 shell 启动测试。

最终持久化完整归档：
`/home/macromodel/Documents/uwvm3-implementation/uwvm2-zig-typeproof-evidence-20261006-a1/zig-typeproof-evidence-a1.tar.xz`

归档 13358508 字节、6825 个已验证成员，SHA-256:
`a9b180f7e8d61410bdfbf5c0bd7ac82381a6bed477cef811dc7c3899237479c2`。
本机归档目录/请求文件创建曾返回 ENOSPC；原始归档已完整保存并验证到上述 Linux 持久目录。
最终报告、资格 JSON 和行政复制/重复清单退役记录作为归档后补充件保存在归档旁；压缩归档保持原哈希。
