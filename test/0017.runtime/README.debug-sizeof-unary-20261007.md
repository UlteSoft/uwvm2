# 一元操作数 sizeof：实现与验证（2026-10-07）

uwvm2 与 uwvm2-ros 的已有有限标量表达式语法现在支持不带括号的一元
`sizeof` 操作数。表达式操作数只接受已有语法；类型名仍使用 `sizeof(type)`。
直接对象选择器沿用已有声明大小回调，算术/逻辑一元操作数使用有界的类型推导。
缺少变量值不会妨碍已知声明类型的大小查询；类型不存在或不受支持仍返回不可用。

| 表达式 | 已支持的 Wasm32 ABI 结果 |
|---|---:|
| `sizeof value`，value 声明为 int | 4 |
| `sizeof -value`，value 声明为 int | 4 |
| `sizeof +short_value`，声明为 short | 4：一元加号执行类型提升 |
| `sizeof !flag` | C++：1；C/C23：4 |
| `sizeof -1 + 2` | 6：加法在 sizeof 操作数之外 |
| `sizeof -1 * 2` | 8：乘法在 sizeof 操作数之外 |
| `sizeof sizeof +value` | 4 |
| `sizeof p`，p 为已有声明指针 | 4 |
| `sizeof *p`，p 指向 int | 4：沿用已有声明选择器 |
| `sizeof array`，array 为 short[5] | 10：不做数组衰变 |

在实际有效的源码暂停中，使用真实 thread/stop/frame 标识：

```text
print-frame THREAD STOP FRAME sizeof value
print-frame THREAD STOP FRAME sizeof -value
print-frame THREAD STOP FRAME sizeof !flag
print-frame THREAD STOP FRAME sizeof -1 + 2
```

DAP 的 evaluate/watch/hover 路径保留同一语法与完整停止/帧标识约束。
这些示例说明已实现的接口；本轮没有链接最新完整产品或验证真实 `-Rdbg`
会话，不把下述组件、元数据和协议 DATA 测试算作真实运行时资格。

## 语义与边界

解析器仅消费紧跟 sizeof 的一个一元操作数，后续二元/条件运算保持原优先级。
非选择器操作数复用 expression_size 和已有类型推导，不执行一元算术、
逻辑取值或其内部算术。嵌套 sizeof 也仅推导类型。
例如 `sizeof -value / 0` 会拒绝外部除零，`sizeof(value / 0)` 仍能查询 int 大小。
外部普通变量操作数仍按原路径求值，测试验证它恰好读取一次。

直接对象、成员、数组和指针选择器保留已有 extent 回调，不把它误称为
所有情况下零回调。新增复合路径的类型查询不调用任何值或 extent 回调；
直接路径的测试区分声明大小查询与操作数值读取。
有限 DATA 提供者不授予真实帧、内存、指针或原生主机调试权限。
生产控制器的停止、线程、帧、epoch 校验与指针/位域策略未修改；
ASM 仍仅限 Wasm 生成上下文，ROS 的 full uwvm-int/full LLVM-JIT 模式范围未修改。

DAP 的选择器快捷路径曾把单独的 `sizeof` 当作变量名放行。
现在保留字根必须进入标量解析，缺少操作数及 `sizeof.field`、`*sizeof`、
`(sizeof)`、`sizeof::field` 均在 broker IO 前被拒绝。
`sizeof_value`、`sizeofValue` 等合法前缀名称仍保留普通变量路径。
所有调用、自增/自减、赋值、计算指针、注入及非选中非法分支继续被拒绝。

32 层递归、128 个 AST 节点、4096 次类型访问预算和 4096 字节标量输入上限不变；
DAP/console 请求仍限制为 256 字节。结果仍为 guest 宽度的通用无符号整数，
没有猜测 size_t 身份、operand DIE、可写 glvalue 或主机地址。
C++ 新测试的字符串构造与输出使用 fast_io concat_std/print；
既有数值解析仍使用 fast_io parse_by_scan。C 编译器见证没有 IO。

依据 [C++ 一元表达式语法](https://eel.is/c++draft/expr.unary) 与
[sizeof 规则](https://eel.is/c++draft/expr.sizeof)，并以独立 C17/C23/C++ 编译器
静态/运行见证验证当前固定大小子集。VLA、用户类型名、函数/不完整类型、
调用、自增、重载、完整引用/glvalue、宽/字符串字面量及完整原生语法仍未实现。
C23 自动语言版本选择、准确 size_t 身份、完整 ptype/CV/typedef/reference、
其他语言完整 evaluator、最新完整产品与其余架构的产品体验仍未完成。
Rust/标准 Go/TinyGo/Zig/AssemblyScript 的完整原生语义不因共享语法扩展获得资格。

## 测试结果

所有编译器、执行文件及功能性 Python 验证均在 SSH Linux 的同一个
64 GiB/swap-0 cgroup 内完成。boot `d9ee997c-9129-43ea-a0f3-c9785344bba8`，keeper PID/birth
`9769/16929`。使用原 birth/PIDFD 监督与回收策略；
本机仅做编辑、捕获、哈希/归档检查和文档更新，没有本机功能测试。

| 当前 A2 测试 | 通过数量 |
|---|---:|
| 受监督的 native/metadata/frontend/QEMU 作业 | 12 |
| 两仓库原生编译/执行/协议阶段 | 96 |
| LLVM 元数据与生产者编译阶段 | 48 |
| 生产控制器 uwvm-int/LLVM-JIT frontend 路径 | 4 |
| 新鲜 DWARF Wasm32 模块 | 40 |
| 新一元 sizeof 检查 | 1196992 |
| 真实声明元数据驱动的新 sizeof 验证 | 840 |
| 独立 Wasm 一元 sizeof 编译器静态见证 | 560 |
| 独立 C17/C23 原生静态见证 | 80 |
| C 原生运行检查 | 20 |
| 旧复合 sizeof 回归检查 | 50600 |
| 旧复合 sizeof 真实元数据回归 | 840 |
| 整数 rank 回归 | 855880 |
| primitive bridge 回归 | 482344 |
| exact narrow/C23 回归 | 140984 |
| 实际 parser/producer 检查 | 8880 |
| QEMU 组件逐字节匹配固定原生输出 | 78 |
| 精确保留旧头文件并复现/修复的拒绝 | 12 |
| DAP source-frame/WASIp1-state/commit-observation 测试 | 148 |
| DAP/controller 正/负语法用例 | 305 / 250 |
| 旧标量/IEEE property 比较 | 1360000 |
| 输入哈希检查 | 163368 |

每次新组件执行通过 149624 项检查。独立 C++ 见证覆盖 14 种整数/Boolean/
浮点原生类型的一元类型大小、整数提升和嵌套大小；guest32/64 与 C/C++/C23/
shared profile 检查声明不可用、零值/大小回调、外部求值、数组/成员/指针、
递归限制和完整非法语法。重复 200 轮有限模型回归不意味着真实语言运行时资格。

每仓库重新编译 20 个 C17/C23/C++20/Objective-C/Objective-C++ 模块，覆盖
DWARF4/5 与 O0/O2；每模块新增 14 个独立 sizeof 静态见证。
实际 LLVM 解析器对每模块的七种真实宽整数声明新增三个一元大小查询，
每仓库得到 420 个新元数据验证，并回归旧 compound sizeof/rank/primitive 行为。
本轮真实 metadata 为 Wasm32；guest64 是有限数值模型，不是新的 Wasm64 live 资格。

PPC64 大端、x86_64、aarch64 每架构每仓库运行 13 个当前组件，
验证目标 ELF machine/宽度/字节序、实际依赖及与原生 stdout 的逐字节一致。
这是组件资格，未宣称完整产品、实际硬件或所有架构等价。

## 保留尝试与资源

A1 的普通仓库完成 45 个编译/执行阶段，但随后因 DAP 放行单独 sizeof 而失败。
失败作业、日志、源码和原先四个 metadata/frontend PASS 均独立保留。
A2 修复 reserved-root 快捷路径并增加关键字边界用例，在新目录完整重跑两仓库。
当前统计没有继承 A1 通过数或把失败改成 PASS。

当前十二个成功作业累计工作 1715.08 秒，
加上本轮保留的早期尝试共 2083.68 秒。
这是作业工作时间之和，不是墙钟时间或性能资格；不包含最终 collector/管理操作。
OOM 计数始终未增加。DATA 峰值 own RSS 812310528 字节，
低于原每作业 1 GiB；frontend 峰值 2195849216 字节，
低于其独立 16 GiB 编译预算。每个角色的 64 MiB 自有输出、8 MiB 单文件、
1 MiB 日志及共享磁盘/内存 admission 预算不变；并行 QEMU 共用原 cross-role 输出上限。

仍使用原 hard 16 GiB ext4 arena，归档后实际占用
4131139584 字节（3.847 GiB）。
源码流式上传；大型证据仅留在 Linux，本机只新增小报告、补丁和证据 sidecar。
本轮没有删除他人数据、放宽 quota 或修改测试机公共工具链。

## 证据与剩余项

uwvm2 源码 cut：`sha256:998d5e560ca52f2942da5f20a10f61735ad958f78e41b58ef9303f07129b513c`。
uwvm2-ros 源码 cut：`sha256:cfc8362b69e2689e847117ee69a6e85abf6eb80296d0f824286a78f1e69dd0f6`。
当前十个实现/测试/adapter/registry 路径在两仓库逐字节一致，且等于各自测试 cut。
旧 inventory 字段与其他 feature 保持；sizeof_type 仍为 partial。

Linux 主归档：`/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/sizeof-unary-a2/sizeof-unary-evidence-a2.tar.xz`。
53016464 字节、7853 个 regular member，
SHA-256 `12ca2af67443efcb4ff3ba4817640998fbff9453020729cab8ec1f8a13ffbd14`。全部 member 已检查，归档已 fsync。
旧源码 cut 由保留的差异字节和当前相同 member 重建，原 Linux 目录也保留。
前一轮 scalar/adapter 的来源与原 SDK/QEMU provider 哈希另行核对；
外部 SDK 仍有独立归档来源，这不是自包含 SDK 或最新完整产品构建。
本机小摘要与主归档 member 一致；最终成对源码/文档/补丁在小型 sidecar 中
由两端逐 member 校验并 fsync。完整记录见 debug_sizeof_unary_qualification_20261007.json。
