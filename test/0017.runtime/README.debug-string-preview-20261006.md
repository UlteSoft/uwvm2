本轮在 uwvm2 与 uwvm2-ros 同步修复 language-level 字符串显示，并修复真实 i686 编译暴露的三处比较类型问题。

Rust `&str`、已识别的 Go/TinyGo string 类型先验证完整声明范围是否落在 Wasm32/64 地址空间内，再调用原有受限读取接口。有效长字符串最多读取一次、复制 4096 字节，显示前缀并标记 truncated。空字符串不需要读取；未资格化指针、未知/无效长度、短拷贝、超长拷贝、读取失败和零读取预算均不会发布文字。现有格式化器转义 ESC、换行与 NUL。生产字符串构造及新增 C++ 回归 I/O 使用 fast_io；没有新增文件、VM 读取或 ASM 权限。

两处 DWARF piece 位数溢出检查改用 `std::cmp_greater`，保留原来的 uint64 位数上限；LEB 消费长度与 `std::ptrdiff_t{5}` 比较，保留原来五字节限制。修复前的实际 i686 `-Wall -Wextra -Werror` 编译失败日志保留。第三方头作为 system include，生产调试器头仍接受完整 warnings-as-errors 检查。

新增 `debug_source_string_preview.cc` 有 106 个 query 检查，覆盖 Rust/Go/TinyGo 元数据形状 × Wasm32/64、完整范围溢出、边界最后一字节、8192/4096/4097 字节字符串、embedded NUL、控制字符、读取失败、预算、未知位和 C 同名结构拒绝。回调故意接受任何地址，以验证范围拒绝确实发生在语言层、回调调用之前。旧版字符串生产 header 用同一 fixture 编译，分别在范围和长字符串两项断言失败；这是旧源码复现，不能算旧版 PASS。

| 最终验证 | 结果与范围 |
| --- | --- |
| Native 字符串回归 | 两仓库各 106 个 query 检查通过，额外检查截断及终端控制字符格式化 |
| 旧源码复现 | 最终两仓库各两项实际断言失败：越界范围调用 reader、长字符串完全没有显示；四项 expected failure 均通过复现条件 |
| 关联 native 回归 | 两仓库的序列、语言表达式、DWARF pieces 和 immutable-local 全部通过 |
| 跨架构字符串 | 16 profiles × 两仓库 = 32 项通过，共 3392 个 query 检查；真实目标 ELF 与 native/QEMU，全部 stdout 相同 |
| i686 关联回归 | 两仓库各 pieces 与 immutable-local，共四项通过；后者每次实际执行 13 个 LEB/bitmap 检查 |
| 资源与时间 | 最终 23 个受控成功 episode 的实际编译/执行合计 564.48 秒，峰值 owned RSS 612499456 字节；各 episode memory.events 前后相同。此时间不是 VM 连续运行或性能数据 |
| 完整新产品 | 未资格化；只读资源检查时临时空间 1505787904 字节，原门槛 9288400896 字节 |

profiles：x86_64、aarch64、i686、riscv64、ppc64、ppc64le、ppc32、mips64、mips64el、mips32、mips32el、sparc64、loongarch64、s390x、armhf、armel。

首次 native 通过 episode 计数 max=162/oom=0/oom_kill=0，后续入场前共享组计数已变为 max=166695/oom=3/oom_kill=1。最终各资格 episode 前后均维持后一个值；不能把共享组整段历史称为零 OOM，也不归因未持有 PIDFD 的其他进程。

所有实际编译与 native/QEMU 执行均位于 SSH linux 已恢复的原 64 GiB cgroup：memory.max=68719476736、swap.max=0、cpuset=0,2,4,6,16-31、测试 affinity=16-31。为只编译少量 header 的 DATA 回归建立了单独有界保护器：6 GiB 入场余量、距共享上限 1 GiB 时停止、aggregate owned RSS 1 GiB、单文件 8 MiB、日志 1 MiB、全套自有输出 64 MiB、临时磁盘保留 1 GiB；最终逐项任务在入场前额外等待 64 MiB 的完整自有输出余量。它固定 runner 哈希与准确参数，保留 boot/init/birth/UID/PIDFD/exec/ancestry/回收检查，只允许本套组件验证。完整产品、VM/JIT、SDK 与 producer 安装不能使用这个保护器。

原完整产品保护器的 9 GiB 入场余量、16 GiB owned RSS、512 MiB 单文件、32 MiB 日志和 9288400896 字节磁盘保留均未修改。原 cgroup 限制、挂载和其他 agent 进程未修改。归档空间紧张时，仅在重验本地 9118 个成员已完整备份的 189722071 字节旧归档 SHA-256 后删除本任务该远端重复归档；源码、日志、测试与他人文件保留。整机新构建的只读资源检查未满足原门槛；没有启动本轮新完整产品编译，也没有本轮新完整 CLI/VM 通过结果。

失败与未启动记录均保留：最初 4 GiB 组件磁盘策略拒绝入场；准备脚本误替换数值导致 64 GiB 身份检查拒绝入场，实际 cgroup 未变；fixture 的 fast_io 字符串参数编译修正；i686 三处 production warning；pieces 回归旧 aggregate initializer 与新增 declaration_identity 字段不兼容的真实编译失败；ROS 组件测试两次共享内存压力中止；逐架构任务标签格式检查在入场前拒绝一次。后来将 ROS 各架构分成独立任务，每项重新检查入场余量，压力和身份门槛没有放宽。pieces fixture 改成字段初始化并用 fast_io 构造类型名，保持已有断言，最终测试版本单独固定。资源停止、准备失败与实际用例断言失败分别记录。早期组件保护器 receipt 中 RSS/file-limit 标签仍写历史值，实际 enforce 为 1 GiB/8 MiB；后续保护器已修正标签，旧日志不覆盖。

这些是实际目标 ELF 中的语言适配器与 owned DATA 回归：DWARF-shaped Rust/Go/TinyGo 类型和受限冷读取回调，没有实际新 producer DWARF、live frame、VM guest-memory lease 或全语言 native 体验资格。4096 是字节上限，不能等同 [GDB 的字符显示与用户配置功能](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Print-Settings.html)。标准 gc Go Wasip1 冷启动超时尚未解决；AssemblyScript typed values、完整 executable checkpoint restore、完整多语言与跨架构 VM/JIT parity 仍未完成。ASM 仍只能调试 Wasm 产生的上下文，不能调试 VM 或私有 helper。此前 1839.18 秒旧版本整机回归不能用于本轮新源码。

冻结生产源码 ID：uwvm2 为 bcaa5b9be81614fcaf525c1764fa6fd8eb86bc9eadf16122de861f68a4245341；uwvm2-ros 为 3a319bd18360b41b04ff2a3c308d59a40c9ffe6b7a7ed2c26bf43edd85b93baf。它们是原 R6 immutable cut 加本轮两处 warning 修复，后来修改的 pieces fixture 单独固定为 72e7fb14e4cadb8fbffdd253863a675e0c650cb18d6f95ed2141ab983cac90e6。字符串 fixture 为 c829c44d950bbd57d4581cca0b989b2d1afc1a1f1065f1abb77a3c06decef78b。永久 QEMU runner 的新 case 注册已同步，但该入口本身未执行；本轮由准确 SHA/argv 固定的 private runner 驱动。

最终生产 header SHA-256：source_language_expression.h = 885d40be3e29943588ecf36300119313ebe30b09a3b4a06369c065b078b2db9b；source_dwarf_pieces.h = 9c891fad7af40ddbd886f530af03417bd63b087c85c072180d723e64963e075b；source_dwarf_expression.h = 9a6366ee597f80ff64e1dfa38d9f27625cd34d040684bf1b1298b47eb8e22d9f。只对冻结切片及其实际输入给出 PASS，其他 agent 后续整机修改没有被这一结果覆盖。

逐项路径、哈希和结果见 [qualification JSON](debug_string_preview_qualification_20261006.json)。完整原始目录保留在 SSH linux 的 /tmp/uwvm2-string-preview-20261006-a1，含 source manifests、candidate-*、out-*、control-* 和所有失败日志。本机归档传输因 ENOSPC 失败，未完成传输的文件仅在失败大小/hash 已保存后退休；远端新归档也因原 1 GiB reserve 加 128 MiB 归档预算不足而未启动，不能声称本轮已有完整压缩备份。原始 frozen 源码、实际二进制、结果和日志未删除。
