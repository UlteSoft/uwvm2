# Go / TinyGo len、cap 语言调试补齐 — R33，2026-10-07

两个仓库已同步实现有限的 Go string/slice `len` / `cap`，并修复减法边界与普通同名变量的空格探测。C++ 的输出、字节编码及 owned 字符串构造沿用 fast_io。查询复用原有真实 stop/frame 和一致的 guest-copy 事务；不调用 Go/host 函数，长度查询不会解引用描述符的数据指针。到达描述符前的 pointer/member 路径仍需现有 guest 读取资格。

| 表达式 | 原始 TinyGo 编译器结果 | 实际 debugger 结果 |
|---|---:|---:|
| len(box.Numbers) | 3 | 3 |
| cap(box.Numbers) | 5 | 5 |
| len(box.Text)，内容为 aλ🙂 | 7 | 7 |
| len(box.Nil)、cap(box.Nil)、len(box.Empty) | 0 | 0 |
| len(box.Numbers) + cap(box.Numbers) | 8 | 8 |
| len(box.Numbers)-1 | 2 | 2 |
| (cap(box.Numbers))-1 | 4 | 4 |

[Go 规范](https://go.dev/ref/spec#Length_and_capacity)规定 string 长度按字节计，slice 的长度和容量返回 int。这里只支持已有、经过校验的 Go/TinyGo string/slice 描述符；array/pointer-array、map/channel、命名类型的所有变体、builtin shadowing 和完整 Go 表达式语义仍未完成。Rust `.len()` 不因本次实现自动取得资格。`cap(string)`、嵌套 builtin、赋值、调用、结果的 member/index/dereference，以及缺失/非法的所需字段均拒绝。若 slice 两个字段都已知，还检查 0 <= len <= cap 和 signed int 范围；缺失独立字段不会伪造所需字段。

启动普通仓库的 full LLVM 产品：

```sh
uwvm -Rdbg -Rcc jit -Rcm full -Rct 0 -Rllvm-call-stack instruction -Rllvm-cache-path disable --run tinygo-O0.wasm
```

ROS 的 full LLVM 产品省去 `-Rcc jit -Rcm full`。在真实断点取得 thread 和 stop-id 后，例如：

```text
print 1 2 len(box.Numbers)
print-frame 1 2 0 cap(box.Numbers)
ptype 1 2 len(box.Text)
```

示例的 1、2 必须替换为实际当前停点的 participant / stop-id。`ptype` 输出 `type=int kind=scalar byte-size=4`；类型推导不读取描述符的长度/容量字段。DAP watch/hover/variables 的有限语法已同步接入，48 项新语法和 fake-broker 的前后 stop 校验通过；真实 frame CLI 已验证，完整新 Go DAP broker/IDE 会话尚未验证。

| 最终检查 | uwvm2 | uwvm2-ros |
|---|---:|---:|
| 新鲜完整产品 TU + link | 3 + 1 | 3 + 1 |
| 原始 TinyGo -O0 模块的 validator / 完整 DWARF verify | PASS | PASS |
| 新版实际 -Rdbg 会话，两栈策略 | 2 | 2 |
| 同一最终 fixture 的旧版拒绝对照，会话 / 表达式 | 2 / 18 | 2 / 18 |
| 持续真实会话 | 98 | 99 |
| 持续编译器结果比较 | 882 | 891 |
| 持续时间，秒 | 605.330 | 603.996 |
| C17/C23/C++/Objective-C/Objective-C++、guest32/64、DWARF4/5、两栈策略回归，会话 / 比较 | 40 / 720 | 40 / 720 |
| 原始 UBSan component 回归 | 8 | 8 |
| 独立 integer/IEEE-f32 property 检查 | 170000 | 170000 |
| 最后 exact-root 空格修复的 native Go descriptor 断言 | 145 | 145 |
| 原有 DAP 正 / 负语法检查与实际 C++ value rows | 595 / 330 / 595 | 595 / 330 / 595 |
| 最后源码的真实 QEMU component：x86_64/aarch64/ppc64/riscv64 | 4 × 145 | 4 × 145 |

持续测试重复九个有限表达式，包含显式 frame、同停点重复值、类型、拒绝不支持表达式、伪造和真实过期 stop-id、原始 guest 的结果自检、exit 0 和 managed shutdown；重复次数不是功能数量。QEMU 使用真正对应架构 ELF，ppc64 为大端，guest descriptor 宽度 4/8 均由 component 覆盖。这是 component 资格，不是跨架构完整 VM/JIT 或 native 调试体验等价；其余架构完整产品仍未完成。

所有编译、目标程序和功能 Python 均在 SSH Linux 原 64 GiB / swap=0 cgroup、原 CPU 集合内，经 birth/PIDFD supervisor 执行。独立 full-build 角色保留 16 GiB owned RSS / 2 GiB outputs；有限 CLI/component 角色保留更小的 1 GiB RSS / 64 MiB outputs、8 MiB 单文件上限和原 6 GiB admission。原有 guards 保留。最后 CLI 角色的新增副本仅将输出统计限定在该 CLI 私有目录，并额外保留 9288400896 字节 arena floor；没有扩大 RSS、输出或单文件上限。累计 46 个 guard job 通过，29 个失败记录保留（其中本次 6 个输出范围误计在启动前拒绝，2 个新 C fixture 的 linker driver 配置失败）；两个不合规 label 在启动前拒绝。全部已启动的 owned roots 已回收，descendant PIDFD 已退休，无新增 OOM。

第一份 fixture 没有使用 box，TinyGo 没有给它 location；随后位于参数有效范围之外的断点也正确拒绝读值。最终 fixture 实际使用 box，并在编译器声明参数仍有效的位置停下；Wasms、DWARF 和源码路径没有重写。保留的失败还包括 SDK driver/link/include 配置、TinyGo 默认 reactor + scheduler none、ptype formatter 断言和 DAP 测试初始 stop DATA。更正后的独立输出目录通过，原失败没有改写成 PASS。TinyGo -O1 产物的 runtime/float.go 内联范围仍未通过 LLVM 完整 verifier，未计入通过；规范 -O0 构建使用 [TinyGo 的 extldflags 接口](https://raw.githubusercontent.com/tinygo-org/tinygo/v0.42.0/main.go)传入 linker -O0 并保留严格验证。

前表的原始完整 VM 和 197 个持续会话绑定指纹 sha256:20ce590b4ceeb8376f5d133d5e2119003661a2762f8cd879753b7dbc8ef6a311 和 sha256:3ed7d8a44822bdf3094108e31bf799f1936f8e94287ed6ae47d3716b1ecdda24。末尾普通变量名 `len ` / `cap ` 的 lookahead 修复现已重新编译所有 3 个 TU 并链接两个完整 VM，最终指纹为 sha256:9728560107d6c69a8b541d0db37e87ffbfae57b7cd12b01a1bad21a411c93c32 和 sha256:c3fb7155b05a73e2bffff43774f853f4d510d6b1d0d58c5f150b1e82488a68e7。最终产品每仓库另通过 TinyGo 两个真实会话 / 18 次比较、C 同名变量及空格/减法两个真实会话 / 16 次比较，以及原 C-family 40 会话 / 720 次比较；合计 88 个实际会话、1508 次结果比较，均验证 guest exit 0、managed shutdown 和过期 stop 拒绝。原 10 分钟 soak 的统计仍属于前一指纹，不冒充最终指纹的持续测试。LLVM23 ROS.11 developer provider 为单独固定的历史 SDK；最新 ROS 完整 LLVM provider parity 仍未验证。ASM 仍仅限 Wasm 产生的上下文，禁止调试 VM/私有 helper；本轮没有修改 ASM、checkpoint 或 WASIp1 的权限。

机器记录：[debug_go_builtins_qualification_20261007.json](debug_go_builtins_qualification_20261007.json)。生产和公共 runners 见 `source_dwarf_expression.h`、`source_scalar_expression.h`、`source_language_expression.h`、`tools/debug/dap_adapter.py`、`run_debug_source_tinygo_builtins_cli.py`、`run_debug_tinygo_builtins_soak.py` 和 `run_debug_go_builtins_qemu.py`。源码、成功/失败日志、原始产品和对象副本集中在 Linux 证据归档；不把 SDK 和大二进制复制到本机。归档哈希另附，最终 workspace 与 frozen cuts 的差异记录单独保存，其他 agent 的修改保留。

Linux 主归档：/home/macromodel/Documents/uwvm3-implementation/retained-go-builtins-r33-final/go-builtins-r33-evidence.tar.xz，107423556 bytes，SHA-256 69c58ccdcb25e51b71a5d0eaab61132f592f7b84f4ad2277e4826ff0b1fc5bd4。全部 24600 个成员已逐一验证并 fsync。大产物留在 Linux，本机只保留小型说明和 sidecars。对象原始副本在主归档验证后退休；前一指纹的两个 VM 二进制在主归档逐成员验证后也已从 arena 退休，原构建 receipts、源码、日志和 guards 保留，二进制可由主归档恢复。

最后完整产品与回归的独立追加归档：/home/macromodel/Documents/uwvm3-implementation/retained-go-builtins-r33-final-linked/go-builtins-r33-final-linked-evidence.tar.xz，82444956 bytes，SHA-256 816d150bb046e1e8d6398cd46d7414e0096ffdc8ed6a8f6e6e1f6355304b1b41。全部 8152 个成员逐一验证并 fsync；包含最后 frozen 源码、两个完整 VM、6 个对象、公共驱动、成功/失败日志和 guards。校验后仅清理 6 个重复对象副本，释放 353864712 字节；最后两个 VM 保留可执行。前一主归档保持不变。
