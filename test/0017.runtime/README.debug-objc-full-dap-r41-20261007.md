# Objective-C / Objective-C++ 数组修复的完整 VM 与真实 DAP 验证（R41）

R40 修复的 Objective-C / Objective-C++ omitted DW_AT_lower_bound 已通过两个新编译完整 VM 的实际 guest/DAP 验证。数组以 0 为默认下界，二维 grid 可展开、分页和直接求值；越界访问被拒绝。

本次完整产品的范围是 **R35 已冻结全部源码 + source_dwarf_index.h 这一处修复**，DAP 使用另行固定的 R41辅助源码。所有三个 product TU（runtime、host、main）重新编译并重新链接。当前整个工作区、最新 ROS LLVM provider、所有语言 native 等价以及完整跨架构 VM/JIT/DAP 仍没有取得资格。

| Repository | 新 source ID | 新 VM SHA256 | 实际依赖文件数 |
|---|---|---|---|
| uwvm2 | sha256:7dabb9544e11390246d5657307514c562cfe396c5c113e0dd19b8e8dce2c8c68 | cec6f847f61d31d8c722050b665047015265c98273183d472da152d2113ea453 | 3446 |
| uwvm2-ros | sha256:615582fe79697513568f5a9404c526f1cb177ed3efb4d38757cb779bb55792b5 | c945d07cfb353c2b143f621a1b60c8dba6e2bd2370de70d0d4f6e4685092c03b | 3411 |

ROS 产品保持 uwvm-int full / LLVM-JIT full；LLVM 来自原 R35 单独固定的 LLVM23 developer provider。其资格不能转移给新 ROS provider。

## 实际 producer 与调试行为

使用 R40 原始 Clang21 Wasm fixture、源码路径和 producer receipt，Wasm/DWARF bytes 未重写。每仓库 16 profiles：C、C++、Objective-C、Objective-C++ × Wasm32/64 × DWARF4/5。Wasm64 同时启用 memory64、table64。两种 LLVM call-stack policy 的短程矩阵共 64 sessions 通过。

每个实际 session 检查原始源码断点和当前帧、Source variables 以及 watch/hover/variables 的同一只读对象树、枚举数值/名称和负值/未命名值、二维数组元素与分页、只读 self-pointer leaf。每个 session 新增 18 个直接数组元素求值和 12 个上下界非法访问。失败求值会使适配器引用失效，因此在实际单步前重新获得并验证活引用，随后才断言旧 frame/scope/object 引用被真实单步退役。

每个 guest 原始 4096 次对象自检自然退出 0；原始 guest OS wait、broker/adapter returncode 和真实 DAP EOF 均核验。复制 metadata 不生成 evaluateName、memoryReference、setter 或 pointer-following 权限。

## 长测与回归

- uwvm2: 101 sessions, 604.458 秒；16 profiles 均运行 instruction 和 unwind。
- uwvm2-ros: 104 sessions, 605.667 秒；16 profiles 均运行 instruction 和 unwind。

本轮短程和长测合计 269 个 enum/array sessions，3219 次 enum 求值，4842 次直接数组元素求值、3228 次边界拒绝、1076 次树呈现和 16128 个复制树行。这些是重复验证次数，不是独立功能数量。

每仓库 127 个 DAP unit tests 通过。原始 TinyGo O0 数组/字符串的两个实际 guest/DAP 回归 session 通过，包括 180 次既有 len/cap 比较。其布局和 scheduler 资格范围仍限原始 fixture。

12 个完整构建或测试 guard jobs 通过，所有原始 source/build inputs 和 108 个 runtime library pins 前后不变。R40 原始 R35 VM 的 Objective-C unavailable-bound 失败记录保留，未改成 PASS。

## cgroup、磁盘与证据

所有编译、功能测试和 VM 运行均通过原 birth/PIDFD supervisor 进入 Linux 64 GiB / swap0 cgroup。构建仍限 16 GiB owned RSS、2 GiB outputs、512 MiB/file；DAP 仍限 1 GiB owned RSS、64 MiB outputs。原 arena free-space floor 9288400896 bytes、host 25 GiB reserve 和 inode floor 4096 未降低。所有 OOM counters 不变，原 owned descendants 已退役/reap。

旧 R32/R33 本任务副本逐文件核验归档后退役，原始 fixtures、R35 frozen source/products 和 peer 文件保留；清理证明保留所有 path/size/SHA。新产品中间目标文件各自 stream-hash/fsync 归档后才退役，VM/源码/构建凭据留存。成功会话的 stdio 原始 bytes 以每会话 protocol.tar.xz 及逐成员 SHA 留存，实际 EOF/guest wait 保留于 receipt。

机器记录：[debug_objc_full_dap_qualification_20261007.json](debug_objc_full_dap_qualification_20261007.json)。Linux 证据目录 /home/macromodel/Documents/uwvm3-implementation/retained-objc-full-r41-final；主归档及 sidecar proof 单独记录最终 SHA。准备阶段的未启动 cut 和失败记录亦留存，不计为产品测试 PASS。

## 仍待完成

Rust payload/niche variant 的真实 DAP projection、其他语言动态/优化布局和 pretty-printer、标准 Go/goroutines、Zig/AssemblyScript native 体验、实际 IDE/general fast attach、完整 QEMU 各架构 VM/JIT/DAP、最新 ROS provider 与当前全部并发源码仍需继续实现或验证。aggregate 能力保持 partial。ASM 仅允许 Wasm 生成上下文，禁止进入 VM/host helper；本轮没有扩大该边界。


R42 有限后续：上述 Rust payload/niche variant 的实际 Wasm32 O0 DWARF4/5 DAP projection 已在同一原 R41 产品上完成；完整 Rust/native/Wasm64 范围仍待验证。参见 [README.debug-rust-variants-dap-r42-20261007.md](README.debug-rust-variants-dap-r42-20261007.md)。
