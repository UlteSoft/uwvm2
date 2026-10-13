# Zig 根表达式类型预检与值类型一致性（2026-10-06）

uwvm2 与 uwvm2-ros 同步修复有限调试表达式求值器。
实际控制器提供声明类型回调时，普通及嵌套 `@as` 现在先检查声明类型，再读取值；
读取后的位宽、有无符号和整数/浮点类别必须与该声明一致。

## 问题与结果

假设声明的 `value` 是 i32：

| 情况 | 修复前 | 修复后 |
| --- | --- | --- |
| `@as(i16, value)` | 先读取 value，再发现类型不支持 narrowing | unsupported，零次值读取 |
| `@as(i64, value)`，类型回调找不到声明 | 忽略类型回调，借用读出的值类型并接受 | unavailable，零次值读取 |
| `@as(i64, value)`，声明 i32，读出的值却是 i64 | 两者均能转为 i64，掩盖源类型不一致 | unavailable，已读取一次的值不发布 |

第三类问题也可能被嵌套转换或共享的 C 风格条件表达式掩盖。
新检查在转换前核对源类型，避免只比较转换后的目标类型。
条件表达式示例属于调试器的有限 `?:` 语法，不是原生 Zig `if` 语法。

改动集中在 `source_scalar_expression.h`（新增约 20 行）。
`type_resolver_supplied` 区分显式缺省的 value-only DATA API；
有类型回调时，先调用既有、受预算限制的 `infer_type` 和 `check_zig_coercion`。
新路径检查声明是否可用、源类型转换是否支持；值返回后再核对源类型。
直接持有的字面量无需符号元数据；逻辑短路不产生新的类型或值查询。
默认 value-only API 和 const `no_type_resolver` 的既有行为保持不变。

实际控制器的两条求值路径都传入了同一选定帧的类型回调：
`controller.h` 的停止事务路径和 copied-local 路径。
它们的类型回调不走位置、carrier 或 guest memory 读取路径。
本轮通过源码阅读确认接入点；没有用组件测试替代实际 VM 停止事务资格证明。

原头文件 SHA-256:
`4c231e94412fdafa8de8614f0f9b169ebb1976bdb7c657cbd05e46c6c4d8e7da`；
修复后:
`7e520c7ced52b91e3e0dacd35f496226bd4a7a2a126d77fce23fa8af4a409fa3`。
两个仓库均分别用旧头文件复现以上三类失败。
代码没有新增 IO；测试中的字符串构造、参数读取和输出使用 fast_io。
没有修改 ROS 的模式限制或 ASM 的 guest 上下文边界。

## 验证

所有编译、原生执行、Python DAP 回归和 QEMU 执行均在 SSH linux 原有 64 GiB cgroup 中完成。
本机只读写源码、比对与保存证据。

- 两个仓库各通过 18 个原生阶段：8 次编译、7 个当前 fixture 执行、3 个旧实现预期失败复现。
- 新 `debug_source_zig_preflight.cc` 每次执行 9,652 条断言：
  11 种源类型、12 种转换目标、Wasm32/64 类型上下文，
  位宽/符号/整数浮点类别变更、非法 carrier 位宽、值不可用、声明缺失，
  嵌套转换、条件表达式、短路、默认及 const 无类型 API、两参数及三参数类型回调。
- 使用实际生产 DWARF pointee 类型遍历检查 `@as(i16, *nullp)` 等：
  拒绝不安全转换时没有值或指针读取，测试没有 guest memory reader。
- 既有 Zig、条件表达式、整数/浮点、字符字面量和 DAP bridge 均通过；
  字符 C++/DAP 差分各 11,802 条，合计 23,604 条。
- 实际公共 `run_debug_linux_qemu_components.py` 入口执行新 preflight 和上轮 typeproof 两个组件：
  32 个架构/仓库组合、64 个组件结果全部通过，
  新组件累计 308,864 条断言，上轮组件累计 94,720 条，合计 403,584 条。
  每个 QEMU 输出与对应固定 x86_64 原生组件逐字节一致。
  这些数字只描述有限 DATA 组件，不表示完整调试体验或性能一致。

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

实际 ELF machine、32/64 位和大小端均验证。
完整冻结生产源码、选定测试 fixture、实际依赖、编译/链接/QEMU 工具与已安装链接提供者、
原生及 QEMU ELF、输出日志、脚本、输入、监管回执均保存 SHA-256。
链接提供者按组合在运行前后核验。
本轮完整冻结生产源码 source_id:

- uwvm2: `sha256:6f2ca69a24fc4eae6084425f36a6aa2fbc67b5915bdff6b10574a9fd71b37925`
- uwvm2-ros: `sha256:38d8895bd96e21345c2cc5057f5867c51bbf7eeca9f46131ec450b6250f2ac3a`

34 个成功监管阶段累计 997.825 秒（含编译与监管，不作性能基准）；
最高自有进程树 RSS 687681536 字节。
memory.events 前后相同；自有 PIDFD 均证明退休、根进程已回收，
结束时共享 cgroup 名单没有本轮已跟踪的自有 PID，仍保留 init 和其他任务。

原有 64 GiB、swap=0、CPU、身份/血缘/PIDFD/回收检查没有改变。
仅有限、自有 DATA 测试使用 6 GiB 内存余量准入、自有 RSS 1 GiB、
输出文件 8 MiB、日志 1 MiB、输出总量 64 MiB、实际文件系统保留 1 GiB/4096 inode，
以及固定 TMPDIR 的监管入口；它不授权 VM/JIT/SDK/生产者工作。
完整产品的既有资源门槛未降低。

## 边界

仍保留有限数字表达式的原有限制：typed 8-bit bool/narrow carrier 歧义、
复合 CTFE、负号常量 CTFE、C cast 作为 Zig 类型证据、完整 Zig 语言求值等没有扩大。
本轮未新运行 Zig 编译器、完整当前 VM、八种语言生产者/优化 DWARF 或 native 级调试体验；
当前完整产品所需的资源余量仍不足。
ASM、WASIp1 和热更新/检查点接口没有在本轮修改。

原始源码与执行证据位于
`/run/user/1000/uwvm2-zig-preflight-20261006-a1`。
复跑公共组件的 case 参数：
`--cases debug_source_zig_preflight,debug_source_zig_typeproof`，
编译与执行仍须使用相应 cgroup 监管。

最终完整归档和持久化副本的准确状态在资格 JSON 的 `evidence_backup` 字段；
归档后补充报告/记录保存在已验证归档旁，原始压缩归档保持其哈希。

完整原始证据归档已逐文件验证：6,831 个文件，13,430,180 字节，SHA-256 `3add36182a54341969f4d4724f1255d3cc5744b1966d338964227ccf9a37fc54`。
完整持久化副本已保存于 SSH linux `/home/macromodel/Documents/uwvm3-implementation/uwvm2-zig-preflight-evidence-20261006-a1/zig-preflight-evidence-a1.tar.xz`，复制后 SHA-256 一致。本机完整归档复制曾遇到 ENOSPC；最终源码、报告和资格记录另存两个本机仓库。
