# R66：Rust 复合表达式推断与 Linux 编译兼容修复

日期：2026-10-08。两个仓库同步修复有限 Rust 标量表达式中的嵌套无后缀字面量推断。类型约束穿过算术、位运算、比较、取负和 !，连接显式后缀或真实停帧变量的声明类型；cast 的输入、输出分别推断，移位量保留独立整数类型。无约束的整数、浮点表达式分别回退到 i32、f64。规则依据 [Rust Literal Expressions](https://doc.rust-lang.org/reference/expressions/literal-expr.html) 和 [Rust Operator Expressions](https://doc.rust-lang.org/reference/expressions/operator-expr.html)。

~~~text
(1 + 2) + 3u8                    -> 6
(1 << 7) + 0i8                   -> -128
!(1 + 1) + 0u8                   -> 253
leaf_fraction + (1.0 + 2.0)       -> 4.25（原始停帧值 1.25f32）
(256 + 1) as u8                  -> 1
(1usize + 2) + 0u32              -> 拒绝：usize 与 u32 是不同类型
false && ((256 - 1) == 0u8)       -> 拒绝：死分支检查字面量范围
false && (((1 / 0) + 0u8) == 0u8) -> false：死分支不执行运算
~~~

类型约束和范围检查不读取 guest 值，沿用深度 32、节点 128、类型访问预算 4096。没有新增调用、写入或宿主 VM 调试权限。无后缀浮点字面量直接按目标 f32/f64 精度解析，边界测试验证单次舍入。isize/usize 的类型标志覆盖字面量、cast、声明元数据与运算一致性。C++ IO 使用 fast_io，含字符串、输出和 parse_by_scan。

完整 Linux 构建还发现 ARM CFI 的 ELF 常量名受 elf.h 同名宏影响。修复保留宿主宏，通过安全常量名访问 ABI 值；两个完整产品已重新构建。本轮未运行完整 ARM VM/JIT 的 CFI 调试场景。

所有编译器、VM、组件、单测和 QEMU 测试在 SSH Linux 既有 64GiB cgroup 内、普通用户身份执行，swap 禁用，保留 25GiB 宿主和原临时文件系统余量。完整 LLVM-JIT full 产品各新编译三个 TU 并链接；普通版用既有 LLVM23 provider，ROS 用已验证的 canonical LLVM12 X86 provider，ROS mode 范围保持 uwvm-int full 和 LLVM-JIT full。

- 每仓库 7 个 C++ 组件、3,455 项 Rust 数值检查，原有 499 个 DAP 单测通过。
- 每仓库 11,097 个生产解析器对照：3,000 个种子字面量、6,096 个移位、2,001 个复合/舍入用例。实际 wasm32 rustc 每仓库另验证 11,188 正例和 77 反例。合法但暂未支持的 i128/u128 不混入 compiler 反例。
- 16 个 Linux 架构、32 个 ELF 全部通过，stdout 与 x86_64 相同：aarch64、armel、armhf、i686、loongarch64、mips32、mips32el、mips64、mips64el、ppc32、ppc64、ppc64le、riscv64、s390x、sparc64、x86_64。此为标量组件验证，未证明完整目标 VM/JIT/DAP 等价。
- 原始 C++/Rust DWARF4/5 fixture 的 Watch、Hover、Variables、条件安装/更新和调用后停点：1572 次通过会话，Rust 新增复合条件 192 次。普通版连续 602.330 秒、ROS 连续 603.306 秒，分别 400/404 次持续测试会话，覆盖 instruction/unwind。
- 核验原始 guest/broker/adapter 的实际 wait=0、协议 EOF、归档成员字节与本轮拥有进程的 PIDFD/birth 关闭；通过阶段 OOM 事件均为零。

保护器另修复已完成结果压缩替换与目录计账的竞态，两仓库各通过完整 511 项单测，含新增 12 项回归。只有原始 guest/broker wait=0、broker 字节匹配、XZ 与原 JSON 字节一致且 fsync 完成的自身会话替换可计账；本次扫描保守计入原 JSON 字节数。未知文件消失仍失败，没有放宽限额或进程归属检查。

首次完整构建的 ELF 宏错误保留。原子源码写入在本机 ENOSPC 时没有截断文件，冻结清单从原始传输恢复；R64/R65 重复清单经 Linux 字节校验并 fsync 后清理。第一轮后续 DAP/soak 因共享 swap 被改为 max 而中止，恢复 0 后重跑；ROS 第二轮因旧保护器计账竞态中止，修复后重新完成整段持续测试。中断记录不计入通过数。

C1/Q1 标量组件的 14/32 份实际依赖文件，对应全部仓库编译依赖与最终 B4 完整产品切片字节相同。完整产品测试只证明冻结 B4 源码；后续其他 agent 对 CFI 等文件的修改已保留，本轮常量处理仍在，Rust 表达式与相关测试文件仍匹配已测字节。最终源码证明列出后续变化，不宣称当前整个工作区已验证。

主归档：/home/macromodel/Documents/uwvm3-implementation/retained-rust-context-r66-final/evidence.tar.xz；137377668 字节；SHA256：19e0c02f6fba7872c03815fc95ddc3c1fbbef37f3ed02b9c7e93cc533915a937。包含完整产品、六份新对象、源码、协议、失败片段和保护器修复；所有原始/归档成员字节已验证并 fsync。大文件保留在 Linux。

88 项能力统计及 native 等价标志保持原状态。本轮没有完成 i128/u128、Rust 全部语法/trait/重载/泛型和原生类型展示、其他语言完整 native 调试、完整跨架构 VM/JIT/DAP、uwvm-int full 实时语言调试；Rust 超范围/NaN 浮点转整数的 native 饱和语义也仍待补齐。
