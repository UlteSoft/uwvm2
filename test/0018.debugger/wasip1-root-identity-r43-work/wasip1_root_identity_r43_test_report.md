# WASIp1 checkpoint：真实根代代码身份（R43）

两仓库同步加入 TU 私有 root-generation identity。身份保留真实 compiler plan、source/profile 的 control block、engine/context/module，以及每个函数的 generation、Wasm 表达式范围、typed/raw/resume 入口。没有公开 factory、反序列化恢复入口或调用者可发行的句柄。

原先 checkpoint plan 与 ASM native-owner 查找将 generation != 1 一律视为 hot replacement。现在 checkpoint 根代码按实际编译和采用的身份选择；匹配失败才检查真实 committed replacement 的 engine、source、epoch、plan 和入口。恢复准备出的 generation 2 根代码可以保留自己的私有身份，无须伪造 committed replacement，但本轮仍未发布或运行它。

真实冷编译在所有符号解析及 native engine 采用后准备身份，typed slot release 后、READY 前 seal。seal 核对真实当前 module registry、selected canonical source、实际 initializer serial、actual validated source/epoch；启用 native owner table 时还要求同一 engine 的真正非 staged、已 detach、已 seal listener。重新 materialize 时先丢弃旧身份，防止失败/重编译残留。普通 ASM 无 checkpoint profile 时按实际 retained-owner slot 选择 cold/hot native owner，再执行原有 source、engine、generation、role、PC 范围校验，不扩大到 VM helper。

候选世界在 native 代码所有权转移前计入全部 identity/row 预算，转移后保留 immutable root DATA；后续 private preflight 逐条检查同一 plan、code、engine 和入口，并强制 epoch/initializer seal 均为零、source 未初始化、record 未 READY。它不是当前世界，不会凭诊断字段或 generation 整数获得运行权限。

## 实测

- 最终 v4：36/36 Linux 原生用例，两仓库各 18。Core 8、WASIp1 8、registry 4、active generation 2 嵌套帧 4、private generation 2 退休候选 4、无 checkpoint profile 的 ASM 代码捕获 4、只观察 checkpoint 4。
- checkpoint 根代正常进入实际 typed-value capture；genuine committed generation 2 的原世界 continuation/hot-owner fallback 继续通过。private generation 2 的代码身份保持未 seal，active 嵌套 case 输出 ROOT_IDENTITY private_generation2=1 live_seal=0 PASS。
- ASM 代码捕获覆盖真实原生入口、递归、musttail、generation 2、alias、失效 ticket 和 reset；只观察 checkpoint 保持 executable_restore=false。这里不是 CLI/DAP ASM 单步的全量验证。
- 原 closed cohort 的创建、保持、实际旧 OS/TLS join 后重检、取消新 OS/TLS join 后再打开旧入口仍通过；registry 压力仍为 2340 个实际 native worker 和 1316 次 Wasm 返回 42。
- 24 项 Linux、Windows、FreeBSD、macOS target frontend 检查通过（v3 完成，v4 C++ 源码无改动，逐项核对相同 80 项源码、命令、日志与每个 target 依赖后复用）。后三个 OS 本轮没有原生运行。
- 18 个 WAT 输入共 36 项 assemble/validator 检查，不代表全部 Wasm 3.0 特性覆盖。
- v1 在原生执行前主动中断，补上重编译身份清理和无 checkpoint profile 的 ASM 路径；v2 在 ROS runtime frontend 因入口签名不一致失败。两次均未执行原生测试，受管进程全部退出、RAM 清理、parent memory.events 未变；它们的编译结果不计入最终 v3。

- v3 测试脚本扩展 fixture 列表时误改了 registry 参数元组，在首个原生用例前退出；33 个进程全部物理退出，RAM 已清理，memory.events 未变。v4 只修正脚本，重新编译、链接所有原生二进制；复用已通过的精确相同源码 frontend 结果。

## 源码与资源边界

本轮冻结 80 项输入：50 项原资格输入不变、22 项本轮源码/测试修改、8 项新增未修改的回归输入。真实 native/frontends 使用此前已资格的 runtime TU 加本轮精确修改；本机 TU 中其他 agent 的无关修改原样保留，没有混入本轮资格或声称整个 dirty tree 已测。ROS root identity friend 使用实际三参数 full-only 入口，主仓库使用原四参数入口。C++ 输出使用 FastIO。

编译、validator、原生测试全部在原 64 GiB/no-swap Linux cgroup。RAM 产品上限 768 MiB、aggregate RSS 上限 6 GiB、本轮持久输出上限 16 MiB；最终 guard 证明所有受管进程物理退出、RAM 清理、parent memory.events 不变。完整日志、命令、精确 delta/原始源码、散列和资格报告归档并逐成员读回。

## 尚未完成

尚未联合发布 source/initializer/GC/WASIp1/code；candidate native owner 仍 staged，fresh restored generation 2 根身份尚未真正 seal、发布或执行。同一预创建 OS cohort 也未迁入真实 closed runtime generation / pause controller，未完成 all-root/ledger 启动证明、新世界 saved-frame 回放、恢复后的 proc_exit/终结清理、其他三个 OS 的 native 验证。不能称为完整实例恢复已实现。显式分配失败注入未覆盖。

Wasm checkpoint 必须同时 checkpoint WASIp1；跨 OS 恢复需重新配置 mount，状态包不保存文件内容。
