# R59：直接浮点常量的语言调试修复（2026-10-08）

本轮同步修改 uwvm2 和 uwvm2-ros，补齐 bounded `DW_AT_const_value` binary32/binary64。优化后的 C/C++ `const float`、`const double` 与负零原先不可读，现在在真实停点可通过 Source variables、Watch、Hover 和 CLI 读取，保留 const 类型与原始浮点位模式。

## 修复边界

- 支持与类型宽度完全一致的 `DW_FORM_block/block1/block2/block4`、`data4/data8`，以及 LLVM 实际输出的 unsigned LEB 原始浮点位模式（`DW_FORM_udata`）。LLVM 的 `DwarfUnit::addConstantFPValue` 先 bitcast，再以 unsigned 常量记录：[官方生成代码](https://llvm.org/docs/doxygen/DwarfUnit_8cpp_source.html)。[DWARF 5](https://dwarfstd.org/doc/DWARF5.pdf) 定义常量按上下文及目标表示解释。
- 使用已有 fast_io `parse_by_scan` 小端读取和 fast_io 输出；复制到有界 owned 字节，不读取 guest 内存、不转换整数数值、不猜测指针。
- 类型须为已知 4/8 字节 `DW_ATE_float`，只接受 Wasm32/64 地址宽度；短/长块、溢出 binary32、exprloc、signed LEB、其他浮点格式仍明确不可用。
- 保留原有 concrete DIE、scope/type/stop/participant/Code PC 来源和 generation 绑定；混合 location 属性、失效 frame/scope、赋值请求仍受原约束。

## 测试方法

全部实际编译、validator、DWARF、VM、DAP、C++ 组件和 QEMU 执行均在 SSH linux 原 64 GiB、swap 0 cgroup 内，保留 birth/PIDFD 原始监督、OS wait、子进程退役、资源收据。

两仓库各 3 个完整 TU 重新编译链接；ROS 使用真实 X86 LLVM `23.1.1-uwvm-ros.12` provider，核对真实依赖与静态库顺序。源码采用完整冻结捕获及精确 paired 自有补丁，不覆盖其他 agent 修改。

Clang 21 编译 C/C++、O1、DWARF4/5 原始 Wasm，经过 validator 与官方 DWARF verify。每仓库 8 个 CLI 组合（两种调用栈策略）均核对六个常量的 concrete DIE 来源。旧产品对同一批未改写的 Wasm 的三个浮点常量均不可用，修复产品均可读。

新增和保留的组件验证了真实解码接口、NaN payload、负零、无 locals/guest memory 的常量读取，以及不匹配/不支持元数据拒绝。QEMU 只验证有界 constant-plan DATA 在 16 个目标架构上的位模式、限定类型、ELF 元数据、依赖和输出一致性；不代表各目标完整 LLVM parser/VM/JIT/DAP 认证。

完整结果、计数、循环时长、归档 SHA 和资源观察由同步 JSON 收据记录，测试尚未结束时不记 PASS。

## 尚未完成

全部八种语言的 native dbg 等价能力、新浮点常量路径的 Rust/TinyGo/标准 Go/Zig/Objective-C/AssemblyScript 真实 producer 验证、signed LEB/其他浮点格式/string/aggregate 直接常量、uwvm-int full、本轮 module BMI 和完整跨架构 VM/JIT/DAP 均未计为完成。ASM 仍严格限定 Wasm 生成的上下文，禁止调试 VM 本身。

首轮真实 producer 测试发现只支持 block/fixed 不够，LLVM 实际输出 udata；该失败收据保留。测试封装遗漏 JSON 及 renderer 匹配问题也保留，建立新的冻结 cut 后重跑，失败轮次未混入最终通过计数。

历史 88 项全能力统计不因本轮有限路径通过而自动改为 implemented。

## 最终结果

|仓库|单测|C++组件|修复前/后 CLI|DAP 会话（含循环）|循环秒数|循环会话|
|---|---:|---:|---:|---:|---:|---:|
|uwvm2|493|11|8 / 8|544|601.805|536|
|uwvm2-ros|493|11|8 / 8|544|600.576|536|

总计 39168 次真实 DAP evaluate，13056 条停点来源收据；CLI 另有 96 条 concrete DIE 来源。32 个实际目标 ELF DATA 全部通过。原始 guest/broker wait、DAP EOF、旧引用退役和只读守卫通过；cgroup OOM 未增长。

归档 269676708 bytes，SHA256 0619e347c8e9f991c3f507270b1062b9138032ff702509b9018138475394b3fa；75800 个常规成员逐一流式验证，fsync 后清理自有可恢复对象和重复上传 tar 833771911 bytes。另删除已失败且可重建的自有未完成归档 166687588 bytes。VM、Wasm、ELF、源码、静态库和协议收据保留。SSH 最终可用 27.20 GiB。

完整同步收据：[debug_float_constants_qualification_r59_20261008.json](debug_float_constants_qualification_r59_20261008.json)。SSH canonical：/home/macromodel/Documents/uwvm3-implementation/retained-float-constants-r59-final/。完整构建之后的并发 source 新增/修改详见 JSON，不混入本轮资格。归档阶段的 shared roster 与 hard-link 处理修正记录保留；实际测试收据未改写。
