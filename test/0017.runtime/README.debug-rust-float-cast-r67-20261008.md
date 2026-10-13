# R67：Rust 浮点 cast、余数和极小字面量修复

日期：2026-10-08。uwvm2 与 uwvm2-ros 同步补齐有限 Rust 标量表达式的浮点转整数 `as`：先向零截断，NaN 转为 0，越界及无穷大饱和到目标上下界，覆盖 i/u8、16、32、64 和 guest isize/usize。使用精确的二次幂上界，在宿主整数转换前完成判断，避免 i64/u64 上界舍入导致越界转换。规则依据 [Rust Numeric Cast](https://doc.rust-lang.org/reference/expressions/operator-expr.html#numeric-cast)。

同时支持 Rust 同类型 f32/f64 的 `%`，有限值使用对应精度的 fmod，符号跟随被除数；有限数对无穷大取余保留原数及负零，零除数、无穷大被除数和 NaN 产生 NaN。f32/f64 极小字面量允许正确舍入为零，负号保留负零；溢出为无穷大、未完全消费的输入和未赋值的扫描失败仍拒绝。DAP 语法检查同步接受这些合法 Rust 字面量。

~~~text
300.0f32 as u8                  -> 255
-1.0f32 as u8                   -> 0
(0.0f32 / 0.0f32) as i32        -> 0
(1.0f64 / 0.0f64) as i64        -> 9223372036854775807
7.5f32 % 2.0f32                 -> 1.5
-4.0f32 % 2.0f32                -> -0
1e-50f32 as u8                  -> 0
-1e-400f64                      -> -0
false && ((1.0f32 % 0.0f32) == 0.0f32) -> false
false && ((1.0f32 % 1.0f64) == 0.0f32) -> 拒绝：死分支仍检查类型
~~~

C/C++ 既有非有限值和越界转换处理保持原行为，其他语言的浮点 `%` 不因本轮 Rust 改动而获得支持。Rust bool/char 的非法 cast、isize/usize 名义类型、死分支类型预检和 guest 停帧访问边界保持有效。C++ IO 继续使用 fast_io；新增极小浮点解析使用 parse_by_scan，以 NaN 哨兵区分真正舍入为零与未赋值的失败。没有新增 guest 函数调用、状态写入或宿主 VM 调试权限。

所有编译器、组件、单测、VM 和 QEMU 执行均位于 SSH Linux 既有 64GiB cgroup，swap 为 0，普通用户 UID 1000，宿主空闲下限为 25GiB，保留原临时文件系统限额。两个完整 LLVM-JIT full 产品各重新编译三个 TU 并链接。普通版使用已固定的 LLVM23 provider；ROS 使用当前 canonical 23.1.1-uwvm-ros.13 X86 provider，重新构建 64 份 archive，12,874 项 canonical 源码校验通过。未回退并发更新后的 ROS provider 版本；ROS mode 范围保持 uwvm-int full 和 LLVM-JIT full。

| 验证 | 结果与范围 |
| --- | --- |
| C++ 组件 | 每仓库 7 个通过，Rust 数值检查每仓库 3,891 项 |
| DAP 单测 | 每仓库 512 项通过，包含字面量下溢 admission 回归 |
| 生产解析器对照 | 每仓库 13,597 项：3,000 字面量、6,096 移位、2,001 复合/舍入、2,000 浮点 cast、500 浮点余数 |
| 真实 wasm32 rustc | 每仓库 13,720 正例、83 反例；正例分批编译，未放宽 8MiB 单文件限额 |
| 跨架构标量组件 | 16 个 Linux 架构、32 个实际 ELF，每个 3,891 检查，stdout 与 x86_64 相同 |
| 原始完整产品 DAP | 1312 次通过会话：592 数值、720 条件，新增 Rust cast/余数条件 160 次 |
| 普通版持续回归 | 607.532 秒，288 会话，154656 表达式验证 |
| ROS 持续回归 | 607.119 秒，288 会话，154656 表达式验证 |
| 旧产品对照 | 16 次基线会话复现原始缺失，单独保留，不计入新产品通过数 |

跨架构列表：aarch64、armel、armhf、i686、loongarch64、mips32、mips32el、mips64、mips64el、ppc32、ppc64、ppc64le、riscv64、s390x、sparc64、x86_64。该结果只证明这组标量组件，完整目标 VM/JIT/DAP 和各目标 native 调试体验仍未验证。

真实 DAP 使用原始 Rust/C++ DWARF4/5 fixture，覆盖 Watch、Hover、Variables、instruction/unwind、条件安装/更新及调用后停点。valid/invalid 表达式为 143/85，本轮新增合法 36、非法 6。每次会话核验原始 guest/broker/adapter 的实际 wait=0、协议 EOF 和原始协议归档字节；false 条件通过实际调用后的停点确认执行继续。通过阶段的 cgroup memory.events 全为零。最终审计关闭 70 个本轮命令根，核对 12275 组 PID/birth，未发现仍存活的同 birth 自身进程；共享 cgroup 可包含 keeper 和其他 agent，未宣称全局 FD 关闭。

早期失败片段保留并排除：ROS 旧 .12 SDK 与新 .13 合同不匹配、SDK13 未就绪的组件链接、rustc 单批输出超过原 8MiB 限额、fast_io 下溢行为暴露的解析拒绝，以及 DAP 下溢期望整数 0 而协议实际为字符串 "0" 的 fixture 错误。修复后重新构建并完成全部最终测试。SDK 初始准备/环境校验失败也单独保留，不计入通过。

早期 DAP 切片含生成的 .pyc，缓存写入使整体源码清单发生变化。相关运行和两段约 600 秒 soak 均只作历史记录，不计入最终通过。最终 DAP、基线和持续测试重新冻结不含 .pyc 的源码，设置 PYTHONDONTWRITEBYTECODE=1、独立空缓存路径，重新执行整套测试及每仓库至少 600 秒持续测试；源码清单逐字节保持不变。完整 C++ 构建的实际仓库依赖也与最终组件、DAP 和矩阵切片逐字节对应。

本轮 7 个修改文件在两个本机仓库中完全相同，并与最终测试字节相同。完整产品结论针对冻结源码切片；交付证明列出并发产生的 src 变化：普通版 16 个、ROS 16 个。已保留这些变更，未宣称当前整个工作区完成验证。

普通版完整产品 SHA256：b29350b6c50eb6422e5193988d7a03a28d1421b71649b325700d34491973ae86。
ROS 完整产品 SHA256：6195d92ac64b5129910719dbd69acb24c20b5fd4ed5d43f2a46a85685fb05574。
主归档：/home/macromodel/Documents/uwvm3-implementation/retained-rust-float-cast-r67-final/evidence.tar.xz；365219236 字节；SHA256：b409e02ae1575231150c16d00c86b518c9303ce686f49370a4d211795d2766bd。包括两份完整产品、六份新对象、canonical ROS13 源码/头文件及 64 份新 X86 archive、源码清单、实际协议、失败片段和保护器记录；所有成员按原始字节验证并 fsync。大文件保留在 Linux。

为控制目录占用，普通版三份对象和 provider13 的 1,768 份对象经压缩归档逐成员校验、fsync 后移除重复 raw 文件；普通版对象从精确对象归档流式纳入主归档。R65/R66 的 12 份旧对象仅在核对既有历史主归档成员后移除重复 raw 文件，历史完整产品及基线输入保留。未清理其他 agent 的目录或降低测试限额。详细 SHA/字节、删除前证明及外部归档 pins 见 qualification 与保留记录。

88 项能力状态保持 14 implemented、39 partial、33 missing、1 separate_level、1 prohibited_by_scope；full native 等价标志保持 false。R66 列出的 Rust 饱和浮点 cast 缺口已补齐，但 i128/u128 carrier、完整 Rust 语法、trait/泛型/重载和原生类型展示、默认 DWARF5 debug_names producer、其他语言/runtime 完整 native 调试、完整跨架构 VM/JIT/DAP、uwvm-int full 实时语言调试仍未完成。本轮没有实际运行 GDB/LLDB 或 IDE UI。
