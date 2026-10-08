# Rust payload / niche variant 的真实 DAP 验证（R42）

两个仓库已修复 DAP 遇到 Rust variant 标记时整棵对象退回文本的问题。VM 已选定的活动分支和载荷现在可以在 Source variables、Watch、Hover、Variables 中展开、分页；每次真实单步后旧 frame、scope、object、variant group 和 active branch 引用全部失效。

本轮使用原 R41 两个完整 VM，未重新构建当前整个工作区。产品是 R35 frozen full source 加 Objective-C omitted array-bound 修复；ROS 使用原固定 LLVM23 developer provider。新源码只涉及 Python DAP presentation 和测试，没有新增 C++ I/O；VM 原 fast_io formatter 与 guest 上下文权限保持原样。

## 真实编译产物和显示

恢复的官方 rustc 1.99.0（b940084d7，LLVM23.1.1）和 wasm32-unknown-unknown 标准库都核对官方分发 SHA256。恢复只复制固定 bin/lib 组件，不执行安装脚本；编译、validator、VM 和 DAP 功能测试均在原 64 GiB / swap0 cgroup 中执行。最终 fixture 为 no_std、O0、完整 debuginfo，分别请求 DWARF4/5，Wasm/DWARF 字节未重写。目标只显式导出 _start 和 source_variant_probe；编译器本身的导出规则保留。

[Rust codegen 文档](https://doc.rust-lang.org/rustc/codegen-options/index.html#dwarf-version)定义完整 debuginfo 和 DWARF 版本选项。以下显示是实际源停点读取，不是依据 Rust 类型名重建的值：

| 字段 | VM 实际活动分支 | 判别值 / 实际载荷 |
|---|---|---|
| negative | Negative | -3 / signed=-3 |
| positive | Positive | 7 / unsigned=7 |
| empty | Empty | 11 / 空载荷 |
| some: Option<NonZeroU32> | Some (default) | 13 / 嵌套 NonZero 字段=13 |
| none: Option<NonZeroU32> | None | 0 / 空载荷 |

在该源码断点选择真实 source_variant_probe frame，Watch 输入 object。展开 negative → <variant-part> → Negative → Negative → signed，值为 -3；展开 some → <variant-part> → Some → Some → __0 → __0 → __0，值为 13。这些层级保留原编译器 DWARF 结构，尚没有 Rust pretty-printer 折叠。next 显示 guest 指针，只作为只读叶子，不追踪到 host 内存。

对象树有 41 行，包含 seed=5、五个变体字段、二维数组 1..6 和 self-pointer。VM 选择 default 分支后 DAP 只展示其复制结果，不读取判别地址、不猜测 inactive payload。结构性 variant wrapper 在原协议中的 type 为 unavailable，这不表示其实际载荷不可读；选定状态与 payload 由独立复制字段显示。

## 验证结果

最终固定 cut dap-a5：每仓库 132 个 DAP unit tests 通过；DWARF4/5 × instruction/unwind 短程矩阵通过，修复前适配器的 4 个对照会话确认同一真实对象为整段 opaque text。

- uwvm2 持续 603.412 秒，39 个会话。
- uwvm2-ros 持续 601.473 秒，39 个会话。

短程及长测共 86 个修复后 Rust 会话，344 次对象树呈现、14104 行复制节点、1720 次活动分支检查、1548 次直接二维数组求值、1032 次边界拒绝、516 次实际单步后的旧引用拒绝。每个 guest 原始循环自检 4096 次，自然退出 0；原 guest OS wait、broker/adapter returncode、真实 EOF 和原始协议归档逐成员 hash 均核验。

C/C++/Objective-C/Objective-C++ × Wasm32/64 × DWARF4/5 的 32 个既有 enum/array 会话回归通过；原 TinyGo O0 array/text 两个会话和 180 次 builtin 比较通过。这些是重复测试计数，不是独立功能数量。

输入和 108 个 DAP runtime library pins 前后不变。DAP 保持 1 GiB owned RSS / 64 MiB outputs / 8 MiB file；arena floor 9288400896 bytes、host reserve 25 GiB、inode floor 4096 均未降低。最终长测峰值分别为约 225.5 / 224.8 MiB；所有 OOM counters 不变，原监督器确认 owned descendants 退役与 reap。

原 900 秒网络下载超时、带整库 export-all 的初版 fixture 超过 1 GiB、两个测试行数误写为40的失败记录均保留，没有改记 PASS。29 个完整 guard jobs 通过，4 个原始失败 jobs 保持失败。

## 范围和仍未完成的工作

解析仅接受完整 canonical variant-part / active-variant（可带 default 和有限 unavailable reason），检查实际 parent extent 与唯一活动分支。未知扩展、type-only variant、截断/omitted packets 保持完整 opaque；不发布部分树，不生成 evaluateName、memoryReference、setter 或标签派生 selector。

本轮仅资格化上述官方 Rust Wasm32 O0 fixture 和原 R41 Linux x86_64 full 产品。Rust Wasm64、优化变量位置、动态/嵌套复杂变体、trait/generic/collections、pretty-printer、完整 Rust 表达式和 native GDB 等价仍需独立实现或验证。[官方 Wasm64 target 文档](https://doc.rust-lang.org/stable/rustc/platform-support/wasm64-unknown-unknown.html)说明该目标没有预编译 artifacts，需自行构建 target 或 std；本轮没有将 Wasm32 PASS 转给 Wasm64。

当前整个并发工作区、最新 ROS LLVM provider、实际 IDE GUI 和完整 QEMU 各架构 VM/JIT/DAP 均未取得本轮资格。现有跨架构 DATA component 结果保持原范围。[GDB Rust 文档](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Rust.html)仅作为语言功能比较依据，本轮未运行 native GDB。ASM 调试仍只允许 Wasm 生成上下文，禁止进入 VM/host helpers。

机器证据：[debug_rust_variants_dap_qualification_20261007.json](debug_rust_variants_dap_qualification_20261007.json)。原始证据与 official compiler archives 留在 Linux /home/macromodel/Documents/uwvm3-implementation/retained-rust-variants-r42-final；archive-proof-r42.json 给出逐文件和逐成员 SHA。R41 VM/全源码归档仍在原 R41 保留目录，机器记录包含其产品 SHA 与原 build receipt SHA。


R43 后续：原树保持不变，折叠值和直接变体求值现显示已复制活动 case/有限完整载荷，见 [README.debug-rust-variant-summaries-dap-r43-20261007.md](README.debug-rust-variant-summaries-dap-r43-20261007.md)。R42 原证据和资格范围保留。
