# DWARF 数值分类与 Zig 8 位转换（2026-10-06）

uwvm2 和 uwvm2-ros 同步保留声明和值的整数/布尔类型分类。
此前只保存位宽、符号和浮点标记，合法 i8/u8 转换被一并拒绝；
较宽存储的 DWARF bool 还可能被误当 unsigned 整数。

## 已实现

- `integer` 新增 `value_category`，区分已声明整数、布尔和旧 API 的未分类值。
- 当前帧的 owned guest-object 与 copied-local 路径通过 `from_dwarf_numeric`
  打包值与类型元数据。5 处实际 controller 调用点均同步修改。
- 允许已分类 i8/u8 的同类型和安全扩宽；按整个声明类型的值域证明，
  不根据当前值猜测 narrowing 或 signed→unsigned。
- 已拥有的数字 cast/@as 结果保存分类，可继续嵌套转换。
- 声明与读取值的分类必须一致；即使位宽、符号、float 标记一样，
  integer→bool 或 classified→unknown 漂移也返回 unavailable，不发布结果。
- 所有已分类 bool 存储位宽都拒绝数值 @as，且声明预检发生在读取值之前。
- 共享 C 风格算术中的已分类 bool 提升到 signed 32-bit int；
  非零 copied bits 规范化为 1，独立于生产者的存储宽度。
- DWARF 打包拒绝非 8/16/32/64 位存储，以及 f32/f64 分类与存储宽度不匹配。

| 表达式与声明 | 结果 |
| --- | --- |
| `@as(i16, byte_value)`，byte_value 声明 u8、值 255 | i16 255 |
| `@as(i64, signed_byte)`，signed_byte 声明 i8、值 -128 | i64 -128 |
| `@as(i16, @as(i8, -128))` | i16 -128 |
| `@as(i64, @as(u8, 255))` | i64 255 |
| `@as(i8, byte_value)`，byte_value 声明 u8 | unsupported，零值读取 |
| `@as(u64, signed_byte)`，signed_byte 声明 i8 | unsupported，零值读取 |
| `@as(u64, boolean_value)`，声明 bool，任一支持存储宽度 | unsupported，零值读取 |
| `+boolean_value`，已分类 bool 非零值 | signed 32-bit 1 |
| `boolean_value + 1`，已分类 bool 非零值 | signed 32-bit 2 |
| 读取值分类与声明不一致 | unavailable，无结果发布 |

布尔提升与值规范化参考 [C11 草案 N1570，6.3.1.1/6.3.1.2](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)。

控制台继续使用既有已鉴权接口，例如 `print THREAD STOP @as(i16, byte_value)`。
fixture 中的 thread/stop/frame 数字只是语法 DATA，不能产生真实停止令牌。
已鉴权停止事务、guest 范围和读取权限没有因类型分类而扩张。
ROS full-mode 范围与 ASM 只能访问 Wasm 生成上下文的限制保持原有逻辑。

参照 [Zig 的完整源类型值域扩宽规则](https://ziglang.org/documentation/0.15.2/#Type-Coercion-Integer-and-Float-Widening)。
这里仍是有限数值子集；共享 `?:` 是 C 风格调试语法，不是 Zig if，
也没有补全 Zig CTFE、u1/i128、f16/f128、完整 Unicode 或原生各语言表达式语义。

## 验证

所有编译、可执行文件运行、Python DAP 检查与 QEMU 均在 SSH linux
原 64 GiB / swap=0 cgroup 中完成。本机只编辑、阅读、比对和保存证据。

最终冻结源码对应两仓库各 22 个原生阶段：10 次编译、
9 个当前组件运行、3 个旧 header 预期失败对照。
新组件每次 247,159 条检查：Wasm32/64 语境、全部 256 个 byte bit patterns、
DWARF signed/signed-char/unsigned/unsigned-char、10 个整数目的类型，
根、选中与未选中分支、嵌套转换、分类/符号/缺失/非法 metadata 漂移；
还覆盖所有支持 bool 存储宽度的规范化提升和 DWARF kind-width 配对拒绝。
旧 header 对照复现：typed u8 widening 被拒绝、嵌套 u8 常量 widening 被拒绝、
宽存储 bool 被误当整数。它们使用与旧 controller 打包结果相同的有损 tuple；
没有声称执行旧完整 VM。

原有 Zig negative 58,095、preflight 9,652、typeproof 2,960 条组件检查保持通过；
scalar、conditional、character、legacy Zig 和 DAP bridge 保持通过。
字符 C++/Python DAP 差分合计 23,604 条。
实际 DAP 求值入口接受 188 个语法用例并拒绝 60 个负例；
新增 10 个嵌套 narrow @as 用例，DAP 源码没有修改。

公共 registry 在 16 个 Linux 架构、两个仓库执行
`debug_source_zig_category`、`debug_source_zig_negative`、
`debug_source_dap_expression`，32 个组合、96 行均通过。
新组件累计 7,909,088 条检查，加上 negative 1,859,040 条，
合计 9,768,128 条；另有 6,016 条跨架构 DAP/C++ 结果行。
所有 QEMU 输出与固定 x86_64 原生对应结果逐字节一致。

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

冻结 source_id：

- uwvm2: `sha256:57562c15001aaaf5b05794aa47391ac5c33f05d91033ee3977a53ad5688b43b5`
- uwvm2-ros: `sha256:0c5a566e3dc803f0705e024e224be18157242a203984d1e89ff427fe91042f87`

核验完整生产 source cuts、fixture、实际依赖、编译器/链接器/QEMU、
已安装链接提供者、ELF machine/位数/大小端、原始日志和 cgroup 回执。
34 个最终成功监控阶段累计 1725.308 秒，
最高自有进程树 RSS 749092864 字节。
另保留首版源码的 2 个成功原生回归记录；最终结果使用 a2 冻结源码。
另有 1 个共享内存余量不足的中断监控记录，
保留原始脚本/partial 输出和回执，没有算作通过。
在原限制下，用同一 a2 冻结源码、仅更换私有输出目录的 a3 runner 重跑。
最终逐目标记录选择的是成功回执，没有把失败状态改写为通过。

所有终止的自有 PIDFD 已退休并回收，memory.events 前后相同。
6 GiB DATA admission、1 GiB own RSS、64 MiB 总输出与单文件/日志预算、
1 GiB 实际写入文件系统保留、CPU/boot/init/血缘检查均保留。
没有降低 full VM/SDK/JIT/producer 的既有资源门槛。

跨架构按 12/12/8 个组合分批。前两批已结束的 72 个自有 ELF，
在压缩归档及本机持久化副本逐成员校验后退休原副本，保留可恢复字节。
其他任务的文件、进程、SDK、源代码和当前 native 二进制没有清理。
本轮此前观测到 SSH 共享持久化磁盘无剩余空间；归档与最终配对核验的持久化位置
如实记录在 JSON 的 `evidence_backup`。原始 ELF 可从完整归档恢复。

## 尚未验证或未实现

- 未重建/运行完整当前 VM 或 compiler producer，没有真实 stopped-frame
  生命周期或 live native LLVM-JIT 事务的新证据。
- controller 调用点按冻结源码审阅；有限组件调用同一打包函数，
  不能当作上述真实 controller/VM 生命周期验证。
- C++ module interface 更新了直接依赖，但未新做 modules build。
- 旧未分类 8 位 callback 仍拒绝数值 @as；宽旧 callback 保留既有兼容语义。
- 完整 native 语言体验、Zig CTFE、语言专属 bool/条件结果类型等仍未完成。
- 热更新、检查点、Wasm 全特性、ASM 和 WASIp1 本轮没有新增资格声明。

原始冻结源码/脚本/运行结果位于 SSH linux
`/run/user/1000/uwvm2-zig-category-20261006-a2`。
完整归档、最终报告和配对核验以已验证实际副本为准。

## 实际持久化结果

完整归档已在本机持久化，逐成员验证 13,547 个条目，包括初始/最终两轮配对冻结源码、原始脚本/日志/回执，以及已退休的 72 个 ELF 的恢复字节。
归档大小 27,887,928 字节，SHA-256：`901c96cb551900011e3a5a089e5503843d9560b3c3a0af2309cc7f54b952d99b`。
本机归档：`/Users/liyinan/.codex/artifacts/uwvm2-zig-category-evidence-20261006-a1/zig-category-evidence-a1.tar.xz`。
SSH linux 运行期归档：`/run/user/1000/uwvm2-zig-category-20261006-a2/zig-category-evidence-a1.tar.xz`；SSH 持久化盘本次剩余空间实测 0 字节，未保存新的完整持久化副本。
最终报告、资格 JSON 与配对核验另存为 sidecar；完整归档哈希不再改写。
