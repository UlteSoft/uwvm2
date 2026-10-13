# Rust 嵌套活动变体和数组载荷摘要（R44）

uwvm2 和 uwvm2-ros 已补齐有限嵌套活动变体摘要，并修正数组路径的显示。R43 能展开内层复制树，但遇到嵌套 enum/Option 时只显示外层 case；数组载荷的显示路径是 items.[0]。R44 在原真实 source_nested_variant_probe 源停点显示以下摘要，完整子树、类型和只读属性保留。

| Watch / Hover / Variables 表达式 | 实际复制摘要 |
|---|---|
| object.negative | Carry { payload: Negative { signed: -3 } } |
| object.positive | Carry { payload: Positive { unsigned: 7 } } |
| object.empty | Empty |
| object.some | Wrapped { payload: Some { __0.__0.__0: 13 } } |
| object.none | Wrapped { payload: None } |
| object.array | Packet { items[0]: 17, items[1]: 19 } |
| object.deep | Top { payload: Carry { payload: Negative { signed: -3 } } } |

在新 fixture 的源断点选中 source_nested_variant_probe frame 后，Watch 输入 object.deep 可直接看到三层活动分支及 -3 载荷。展开 object.negative → <variant-part> → Carry → Carry → payload → <variant-part> → Negative → Negative → signed 仍显示 -3。数组载荷展开 items 后仍是两个原 indexed children，可以分页。__0.__0.__0 仍是实际编译器字段的显示文本，没有猜测转成 Rust tuple/Option pretty-printer 语法。

整个对象保留 76 行，包括 seed=5、七个变体 owner、13 个实际活动分支及 guest self-pointer。修复前四个真实会话验证同一树已可展开、内层载荷已可读；外层摘要却是 Carry、Wrapped、Top，数组摘要路径带多余点号。本轮没有为了展示补造或改写 Wasm/DWARF。

## 实现和边界

每一层摘要都要求该复制 owner 的唯一 variant-part 和唯一可用 active branch。标签、default 标记、判别值和 payload 均来自 VM 已完成的选择；适配器只格式化已验证的完整 DATA。独立识别同名 case wrapper 后计算表示，仍保留原 wrapper 和所有 child handles。普通容器字段按原路径显示，数组下标直接附到前一字段名，因此 items[0] 与 __0.__0.__0 都只是显示文本。

外层和所有内层共享 64 个复制节点、8 层 payload path、32 个复制值/嵌套摘要项、256 UTF-8 bytes 的预算；包含内层 selection 的结构开销。无法完成整个载荷摘要时只保留已证明的外层 case 名，不显示部分 payload。内层仍可独立显示其已知 case，完整树继续可展开。未知/歧义/不可读 selection、非基本字段名、未知载荷和过深/过多节点都不能绕过限制。

不根据类型名或 Rust ABI 选择分支，不读取 inactive payload、不发额外查询、不追踪指针；不从复制标签生成 selector、evaluateName、memoryReference 或写接口。用户显式 object.FIELD 查询仍走原 frame/stop/guest transaction。没有新增 C++ I/O，VM 的原 fast_io formatter 与 ASM guest 上下文边界保持原样。

## 验证

官方 rustc 1.99.0 与原 Wasm32 标准库/工具闭包 195 文件 pins 前后不变。新 no_std O0 fixture 请求完整 debuginfo 和 DWARF4/5，链接只显式导出 _start、source_nested_variant_probe，再由原 wasm-tools validator 验证。编译、validator、VM、DAP 和全部本轮功能测试均在 SSH Linux 原 64 GiB / swap0 cgroup 及 birth/PIDFD 监督器中执行。

每仓库 413 项冻结 DAP tests 通过，包含新增嵌套 case、数组 indexed paging、不可读内层、共享节点/值/深度预算及未知标签拒绝检查。unit_suite_files 在机器记录中列出实际文件；三个依赖外部 controller formatter packets 的独立 runner 没有在本轮调用，不新增其 WASIp1 runtime 资格。

- uwvm2：实际DAP循环 601.035 秒，30 个会话。
- uwvm2-ros：实际DAP循环 616.112 秒，31 个会话。

DWARF4/5 × instruction/unwind 短测及长测共 69 个修复后嵌套 Rust 会话，276 次对象树呈现、20976 行复制节点、3588 次活动分支检查、1449 次直接变体求值、3381 次 owner 摘要检查、483 次真实单步旧引用拒绝。每个会话检查四种入口同一 76 行树、21 次直接 object.FIELD 求值；实际单步后旧 frame、scope、root object、外层 group、内层 group/branch 等七个引用拒绝。每个 guest 自检 4096 次、结果55，自然退出0；核验原 guest OS wait、broker/adapter returncode、实际 EOF 与原协议归档逐成员 SHA/fsync。

原 R43 Rust 变体/Option 4 个会话、C/C++/Objective-C/Objective-C++ × Wasm32/64 × DWARF4/5 的 32 个 enum/array 会话、TinyGo 两个 array/text 会话和 180 次 builtin 比较回归通过。计数表示重复检查，不是独立功能数或原生等价百分比。

20 个原 guard jobs 中18个通过；首次 producer 导出参数误用旧函数名造成的两个原失败保持失败记录，修正参数后在新 cut 编译验证。原 receipts 未覆盖。两个 producer 各6个 closed pins，修复前/后 DAP cut 分别198/202个 closed code/runtime pins，以及108个 DAP运行库 pins 前后不变。owned RSS 仍限1GiB，outputs64MiB/file8MiB/log1MiB；arena floor9288400896 bytes、host reserve25GiB、inode floor4096未降低。所有 OOM counters不变，监督器确认本轮原 birth/PIDFD绑定进程退役与reap，保留keeper和其他agent进程。

## 仍未完成

本轮复用原 R41 两个 scoped full VM，没有重新构建当前并发整个工作区。ROS只使用原固定 LLVM23 provider的full产品；最新ROS provider尚未资格化。有限嵌套复制摘要不等于完整Rust pretty-printer、collections、trait/generic、优化位置、Wasm64、完整表达式、native GDB或实际IDE GUI等价。[GDB Rust文档](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Rust.html)是语言功能比较依据，本轮没有运行native GDB；文中历史rustc限制不作为当前编译器结论。直接DW_TAG_variant子树等不同layout以及更多生产者仍需独立资格。完整QEMU各架构VM/JIT/DAP仍未完成，ASM仍禁止进入VM/host helpers。

能力统计仅给aggregate与rust_variant追加本轮有限证据，保留原partial状态及其他86类，source_status_counts不变。

机器证据：[debug_rust_nested_variants_dap_qualification_20261007.json](debug_rust_nested_variants_dap_qualification_20261007.json)。Linux原始证据：/home/macromodel/Documents/uwvm3-implementation/retained-rust-nested-variants-r44-final。新主归档 3567780 bytes / 961 members，SHA256 281d1744496dfa0e299a933886cf55f2e85f0faba613fa09dbb37724818d2200；archive-proof-r44.json 给出逐文件和逐成员hash、原输入前后不变及fsync。原R41 VM与R42工具链归档按机器记录SHA引用，没有重复保存大文件。
