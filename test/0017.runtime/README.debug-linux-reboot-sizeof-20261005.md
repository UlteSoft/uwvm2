SSH Linux 重启后，已启动原来的 `uwvm3-implementation` 容器，恢复其原 cgroup：memory.max 为 64GiB（68719476736 字节），swap.max 为 0，CPU 集合为 `0,2,4,6,16-31`。所有编译、编译器版本检查、VM、QEMU 和测试都通过原 PIDFD 保护器在此 cgroup 内执行，本轮没有新增 OOM/OOM kill。Go 1.27.1、TinyGo 0.42.0、Zig 0.17.0、AssemblyScript 0.28.20 的原编译路线已恢复，真实 ROS.11 LLVM SDK 已重新构建。

uwvm2 与 uwvm2-ros 同步修复了条件表达式中的 `sizeof`：大小查询使用独立的类型大小元数据，可以接受完整聚合体、指针和空指针目标。只读数值求值仍检查当前真实帧、stop 与 activation 身份。位域、缺失或不完整类型明确拒绝，直接 `sizeof` 和条件表达式内的 `sizeof` 使用一致规则。沿用现有 fast_io 路径，没有新增 I/O 后端，也没有扩大 ASM 对 VM 或宿主的调试权限。

在本轮真实 C/C++ fixture 的当前停点可测试以下命令，THREAD、STOP、FRAME 取自该停点：

```text
print-frame THREAD STOP FRAME 1 ? 7 : sizeof(packet)
print-frame THREAD STOP FRAME 0 ? 7 : sizeof(packet)
print-frame THREAD STOP FRAME 0 ? 7 : sizeof(*null_packet)
print-frame THREAD STOP FRAME 0 ? 7 : sizeof(null_packet)
print-frame THREAD STOP FRAME sizeof(bits.flags)
```

前两例分别得到 7 和 28；第三例得到 28，求大小不解引用空指针；第四例按 guest32/64 分别得到 4/8。最后一例因操作数是位域而拒绝，即使它出现在未选中的条件分支也拒绝。

| 本轮验证 | 结果及范围 |
| --- | --- |
| 两个完整产品 | 当前冻结源码、全新 main/runtime/host 对象，真实 ROS.11 SDK；full interpreter 与 LLVM full |
| QEMU | 16 个 Linux profiles × 两仓库 × 三组件，共 96 组通过；实际目标 ELF 输出与 native x86_64 基线比较 |
| DAP | 每仓库 234 项协议/语料回归，共 468 项通过 |
| 真实停点 | 24 个数值用例，其中 20 个 C/C++/Objective-C 条件表达式用例；8 个源码断点用例；guest32/64、instruction/unwind |
| 源码 run-to | 256 个 DWARF4/5、until/advance、递归/返回、拒绝/断点/替换用例通过 |
| 连续运行 | 1829.55 秒，60 轮、1920 次真实 guest 启动/退出及 worker join，379920 次控制器操作、54720 个实际源码位置，全部通过 |
| 恢复的语言路线 | TinyGo 数值 4 项、Zig 数值 4 项、AssemblyScript source-map 断点和 into/over/out 4 项通过 |

跨架构结果覆盖组件语义，尚未取得这些架构完整 VM/JIT 调试体验与 x86_64 相同的资格。AssemblyScript source-map 不提供类型变量信息，类型值查询仍不可用。标准 gc Go 的当前真实产物没有 DWARF；优化和非优化版本的普通仓库 LLVM 调试启动均在 180 秒超时，ROS 对应队列尚未执行。两仓库这两种 Go 产物的 full interpreter 正常运行并退出，共 4 项通过，这不代表 `-Rdbg` 通过；`-Rdbg` 当前只进入 LLVM full 调试器。

完整原生语言能力仍是 14 implemented、38 partial、34 missing、1 separate_level、1 prohibited_by_scope，共 88 类。有限数值 `?:` 仍标为 partial，窄整数/bool 结果、enum/pointer/class/glvalue、重载和各语言完整原生语义仍缺失。

本轮普通/ROS 的冻结源码分别为 `769f70f26c95663c86db398a9a4dfe9f92c3a8e0e3c40898bf0531c9429844bd` 与 `ff68e598da424ca95c7af82d05f80bc19ea624eee6b59b7807053c020b43e188`。冻结后其他 agent 修改了 console/session、native 和文档文件，均保留；本轮 PASS 对应上述精确输入，不覆盖后续并行改动。ROS runtime help 确认只暴露 full interpreter 与 full LLVM，`-Raot` 全称是 `--runtime-aot`。

完整证据见 [本轮验证记录](debug_linux_reboot_sizeof_qualification_20261005.json)，原失败尝试见 [条件表达式历史记录](debug_conditional_expression_qualification_20261005.json)，剩余能力见 [完整清单](language_debug_capabilities_20261004.json)。原始日志、记录、两个产品和 QEMU ELF 已备份至本机 `/tmp/uwvm2-language-sizeof-20261005/completion-evidence-a1.tar.gz`，108941789 字节，8115 个成员均逐字节核对，SHA-256 为 `004d5a64e8803db7a359ef422427b1ffe1566f69dd04770074e3066007397916`。两份当前源码归档与 ROS.11 源码归档另行保留；原远端文件未删除。
