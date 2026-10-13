# ASCII 字符转义：实现与验证（2026-10-07）

uwvm2 与 uwvm2-ros 的有限普通字符字面量现在接受 ASCII 范围内的八进制和
十六进制转义，并补齐标准简单转义。值范围仍为 0–127；没有增加 Unicode、
宽字符、UTF 前缀、多字符或目标 signed/unsigned-char 编码解释能力。

```text
print-frame THREAD STOP FRAME '\x41'
print-frame THREAD STOP FRAME '\101' + value
print-frame THREAD STOP FRAME sizeof '\x41'
print-frame THREAD STOP FRAME sizeof('\x28' + '\x29')
```

实际调用需要有效的 thread/stop/frame。以上是已实现接口的示例，本轮没有
链接最新完整产品或验证真实 `-Rdbg` 会话；组件及协议 DATA 不算作 live 资格。

| 输入 | 当前有限 guest profile 的结果 |
|---|---|
| `'\x41'` / `'\101'` | 值 65；C/C23 为 int，C++ 为 char |
| `'\x7F'` | 值 127；十六进制数字大小写均接受 |
| `'\0'` / `'\000'` | 值 0；不是字符串结束符 |
| `'\a'` / `'\b'` / `'\f'` / `'\v'` | 值 7 / 8 / 12 / 11 |
| `'\"'` / `'\?'` | 值 34 / 63 |
| `+'\x41'` | 值 65，经整数提升为 int |
| `sizeof '\x41'` | C++ 为 1，C/C23 为 4 |
| `sizeof('\x28' + '\x29')` | 4，转义中的括号值不参与语法定界 |
| `'\x80'`、`'\200'`、`'\x100'` | 超出已有 ASCII 子集，拒绝 |
| `'\0000'`、`'\1234'` | 八进制最多三个数字，后续字符造成多字符字面量，拒绝 |
| `'\x'`、`'\X41'`、`'\x41g'` | 缺少数字／不受支持形式，拒绝 |

依据 [C++ 字符字面量规则](https://eel.is/c++draft/lex.ccon)、
[C23 6.4.4.4](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf) 与
[GDB 字面量接口说明](https://sourceware.org/gdb/current/onlinedocs/gdb.html/C-Constants.html)，
十六进制转义消费连续的全部十六进制数字，八进制消费最多三个八进制数字。
关闭引号必须紧随一个完整支持的字符值。可以有有界的前导零，数值溢出不截断到 ASCII。

C++ scalar primary 与 sizeof 定界扫描共用同一个完整字符 token helper。
试探失败不消费 token，后续完整 operand 解析负责拒绝非法语法。
数字分隔符仍沿用原数值解析路径；没有把它们识别为变量或命令。
C++ 数值转义使用 fast_io parse_by_scan 的 hex_get/oct_get，输出与字符串构造
使用 fast_io print/concat_std 和有界 string_view。C 独立见证没有 IO。
DAP 镜像同一 token 规则，保留原 printable ASCII、256 字节、32 层、128 节点
请求限制；broker IO 前拒绝非法输入，watch/hover 保留原表达式和暂停身份。

声明、帧、停止 epoch、指针、内存、位域与 native/ASM 权限策略没有修改。
ASM 仍仅限 Wasm 生成上下文；ROS full uwvm-int/full LLVM-JIT 的模式范围没有修改。
共享 C 风格语法的扩展不构成 Rust/Go/TinyGo/Zig/AssemblyScript 的原生字符语义资格。
所有值仅是有限 DATA；不授予真实停止或主机 VM 调试权限。

## 测试结果

全部编译器、执行文件和功能性 Python 验证均在 SSH Linux 的原 64 GiB/swap-0
cgroup 内，由原 birth/PIDFD supervisor 限额并回收。本机仅编辑、哈希、
归档/摘要复制和文档更新，没有本机功能测试。

| 当前 A2 结果 | 通过数 |
|---|---:|
| 受监督 native/metadata/frontend/QEMU 作业 | 14 |
| 两仓库原生编译、执行、协议阶段 | 114 |
| 新字符组件重复回归检查 | 5216800 |
| 独立 C++ literal 编译器见证写法 | 283 |
| 真实 Wasm32 声明元数据参与的字符表达式验证 | 840 |
| 新鲜 DWARF Wasm32 模块 | 40 |
| Wasm 编译器字符值／类型静态断言 | 22640 |
| C17/C23 原生字符值／类型静态断言 | 2264 |
| C17/C23 固定常量运行见证的源码检查 | 1132 |
| 实际 LLVM parser/producer 检查 | 9720 |
| QEMU 当前组件匹配固定原生输出 | 90 |
| DAP source-frame/WASIp1-state/commit-observation 方法 | 148 |
| 额外 native 字符／sizeof 完整语法探针 | 176 |
| 其中拒绝非法 sizeof 的探针 | 40 |
| 额外 DAP 超过 256 字节的请求拒绝 | 6 |
| DAP/controller 主正／负语法用例 | 595 / 330 |
| 保留旧头文件后复现并修复的字符拒绝 | 12 |
| 旧一元 sizeof 回归检查 | 1196992 |
| 旧复合 sizeof 回归检查 | 50600 |
| 旧字符组件回归检查 | 118328 |
| 旧标量/IEEE property 比较 | 1360000 |

新夹具每次执行通过 652100 项检查：283 个独立编译的 literal 写法、
16 轮、guest32/64 和 shared/C/C++/C23 四种 finite profile，检查值、宽度、
原生 copied type、提升、条件类型、sizeof、零值/类型 resolver 调用与非法死分支。
编译器见证使用源码 literal 建立参考值，没有用被测 parser 构造 oracle。

每仓库 20 个 C17/C23/C++20/Objective-C/Objective-C++ 新 Wasm 模块覆盖 DWARF4/5
和 O0/O2。每个模块新增 566 个独立值/类型静态断言。实际 LLVM index 对七种
真实宽整数声明分别验证三个字符加法的 sizeof 类型推导，得到每仓库 420 项验证。
真实生产者为 Wasm32；guest64 的重复模型不是新 Wasm64 live 资格。

PPC64 大端、x86_64、aarch64 每架构每仓库运行 15 个当前组件，核对 ELF
machine/字节序/宽度、实际头文件及链接 provider，并与固定 native stdout 逐字节比较。
旧字符 golden corpus 中 56 条原拒绝记录因现在已支持相应 ASCII 写法而调整，
其余非法记录保留；旧 verifier 的 10000 项随机算术比较也在 native/QEMU 运行。
另外两个受监督作业执行 176 个 native 原始探针，包括 40 个非法 sizeof 形式；
DAP 接受准确的 256 字节字符 token，并拒绝 257/512/4096 字节请求。
C 原生见证使用固定常量，部分运行条件可被编译器优化；静态断言独立验证其类型/值。
新 DAP 用例验证正常 watch/hover 路由和 broker IO 前拒绝。协议 DATA 没有 live 权限。

## 保留尝试与资源

A1 首个新 C++ 夹具编译因 lambda 初始化缺一个闭合括号失败，未执行它。
另外四个 metadata/frontend 作业独立通过，均保留原日志、源码、哈希和退出记录。
A2 修正夹具后使用新目录完整重跑全部当前作业，最终通过数没有继承 A1。
没有覆盖失败日志，也没有把失败改成 PASS。

boot `d9ee997c-9129-43ea-a0f3-c9785344bba8`，keeper PID/birth `9769/16929`。
当前成功作业累计工作 1953.01 秒；含本轮早期尝试
为 2137.03 秒。工作时间之和不等于墙钟/性能资格，
不包含最终 collector 和行政操作。OOM 计数未增加。
DATA own RSS 峰值 835751936 字节，原上限 1 GiB；
frontend own RSS 峰值 2197041152 字节，原上限 16 GiB。
每角色原 64 MiB 自有输出、8 MiB 单文件、1 MiB 日志及磁盘/内存 admission 上限不变。
三组 QEMU 共用原 cross-role 输出限额。本轮不安装 SDK、不放宽 quota、不清理他人数据。

仍使用原 hard 16 GiB ext4 arena，主归档后 image 实际占用
4607635456 字节（4.291 GiB）。
大型证据仅留 Linux，本机只保留小型摘要/补丁/最终 sidecar。

## 证据和未完成范围

两仓库当前 14 个实现/测试/adapter/registry 路径逐字节一致并等于已测试 cut：
uwvm2 `sha256:a0c0ac826b8dd5066328c01beb33ee1abd4846c6d5da61552dafd608d4286361`；uwvm2-ros `sha256:efa5f64b9752660d8a9a917ad80e15279f066865ec85e9a7ff25a63b73657f98`。
旧统计字段与其他 feature 保留，char_literals 仍标记 partial。

Linux 主归档 `/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/character-escape-a2/character-escape-evidence-a2.tar.xz`，51916884 字节，7732 个 regular member；
SHA-256 `91007668f57d4f865c9fd1d3c23d0052d7560fb569271cd33dab3eb05a6f1279`，所有 member 已验证并 fsync。
早期 source cut 可由保留差异和相同当前 member 重建；原 Linux 目录也保留。
前轮 scalar/adapter 与 provider 来源有独立哈希记录。
本机小摘要对照主归档 member；最终成对源码/文档小 sidecar 在两端逐 member 验证和 fsync。
完整统计见 debug_character_escape_qualification_20261007.json。

Unicode、UTF/宽前缀、多字符、非 ASCII target 编码、unsigned-char/CU 编码、
完整 Rust/Go/TinyGo/Zig/AssemblyScript evaluator、准确 size_t、完整 ptype/CV/
typedef/reference/glvalue、最新完整产品 live -Rdbg 及其余架构产品体验仍未完成。
