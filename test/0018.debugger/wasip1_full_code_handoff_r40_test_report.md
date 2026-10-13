# R40：真实 full-code owner 的私有交接

uwvm2 和 uwvm2-ros 已同步实现候选内部的真实所有权交接。所有资源、dispatch、间接调用、root 和私有 worker 检查成功后，将真实 finalizeObject 成功的 LLVM engine/context、compiler local metadata、typed target 数组、有效 body owner 和 frozen native endpoint observer 移入 R39 已分配的最终 full-runtime 记录。

typed target 数组直接移动同一分配，保持 LLVM IR 已嵌入的地址；safe-point view 继续指向同一 bitmap。原 staged 对象不再拥有 engine/context/local metadata/typed targets，只保留用于比较的实际交接地址和真实入口 DATA。交接后检查 FINAL owner 的实际资源、source/profile control block、dense/function/generation/plan、CFI、完整槽位和地址保持，再允许准备成功。execution drain 与真实 OS/TLS join 后再次检查这些 FINAL owner。

pending range listener 在 engine 活着时先 detach，避免 FINAL publication 比 staged 对象先销毁后，listener 析构再访问已释放 engine。诊断 range/provenance 保留为未提交 DATA，未注册全局范围。native endpoint observer 移入 FINAL owner 后仍是 staged-only/frozen、epoch 0、LIVE seal false，helpers/raw adapter 没有 Wasm ASM 调试权限。候选的私有解析 body 仍是 uncommitted source DATA，未伪装成 hot-replacement commit。

新增准备诊断 runtime_full_code_owners_transferred_privately、runtime_full_code_targets_allocation_preserved 及 module/function/effective-body 计数；新失败枚举 full_code_owner_handoff_declined 追加在末尾，保留旧枚举值。退休结果 full_code_owners_rechecked_after_physical_join 只在真实物理 join 后成功复核时设置。

| 本轮验证 | 结果 |
|---|---:|
| 两仓库 Linux 测试/runtime 与 Windows/FreeBSD/macOS runtime 前端 | 16 项全新检查通过 |
| 新增 dormant/active generation 2 测试的 Linux 前端 | 4 项全新检查通过 |
| 前端阶段 Wasm 组装／validator | 8 项通过 |
| 原生阶段 Wasm 组装／validator | 40 项通过 |
| Linux LLVM full 基础原生回归 | 24/24 全新用例通过 |
| dormant generation 2 的原生交接用例 | 4/4 全新用例通过 |
| active generation 2 嵌套帧、继续执行与再次 checkpoint | 4/4 全新用例通过 |
| 本轮其他 OS 原生 | 未执行 |

Linux 在第一轮运行至 23/24 时重启；旧日志完整保留，但没有完整 suite 资格和回收记录，因此这 23 个用例不计入本报告的合格总数。挂回经 UUID 校验的同一 8 GiB 卷，复用已恢复的原 64 GiB cgroup 并核对新 boot/anchor 后，重新构建和执行整个基础 suite。已完成的重启前 16 个前端检查有独立、完整的资格与物理回收记录，冻结输入逐字节未变，保留该有效前端证据；新增测试前端和全部原生资格来自重启后。

基础 24 个原生用例覆盖 core、WASIp1、完整单实例、preload，以及普通／间接调用和 instruction/unwind 策略。两仓库 runtime 和 host API 均重新构建、真实链接 LLVM。检查候选准备后的真实所有权及地址保持、旧线程退休前拒绝、body/TLS 等待、实际物理 join 后复核、candidate 保留与丢弃、原 source/epoch 与原世界继续可用。

新增 4 个用例在真实暂停的两 worker cohort 上，通过现有热替换 API 将未在执行的 callback 函数实际提交到 generation 2（i32.const 17）。准备候选时验证恰好 1 个真实 effective-body owner 已移入 FINAL 记录；退休与物理 join 后再次复核；丢弃候选后运行原世界，原有主函数仍返回 42，真实已提交 callback 返回 17。两个仓库分别执行 instruction/unwind。测试未伪造 generation、commit、publication 或 LIVE seal，也未执行恢复后的候选世界。

另外 4 个 active generation 2 用例从正式 WAT 组装和 validator 成功的 replacement module 中，按真实 Code section 提取完整第一函数 body，再通过真实热替换提交 child 到 generation 2。当前停住的父子两个 native/Wasm 帧包含真实 i31 局部值 21；原停止点与保留世界继续执行后的新停止点均准备并丢弃候选，每次确认恰好 1 个有效 body 和真实 LLVM owner 已交接，共验证 8 次 active body 交接。旧暂停、共享控制块伪造、过小输出、root/worker 配额均被拒绝。真实 parent/child 继续执行、再次 checkpoint、两次清理/drain 和第二次继续执行最终得到 1190，原 guest 前缀各执行一次、下一 child 副作用执行一次。此路径使用保留的原世界；捕获后将原内存改为 4321，继续执行仍看到 4321，明确不代表完整实例或文件/GC 回滚，也没有执行新候选世界。

编译和运行都在原 SSH Linux 64 GiB cgroup，产物仅写入本轮独立 tmpfs。768 MiB RAM 文件、6 GiB 自有 RSS 和共享 cgroup 60 GiB 停止线维持。合格测试的所有自有进程已规范回收、RAM 产物已删除，父 memory.events 未变。中断前的未完成 attempt 由重启结束，没有正常回收资格，未混入这些清理结论。原持久四 OS suite 的磁盘/inode 门槛未降低，macOS 本机 2 GiB 限制保留。基础 42 个冻结输入逐字节不变，只新增四个 generation 2 测试源文件，并冻结四个正式 WAT 输入，最终范围为 50 个文件叠加未变 R34 依赖。前端检查不能代表其他 OS 的原生运行；本轮未覆盖其他 agent 修改的整个 checkout。

**实际联合 world 发布、恢复 worker 启动和 guest replay 仍未接通。** 后续需要在真实共同提交中完成 source/initializer serial/GC/WASIp1/代码身份/native LIVE adoption，再让完整新 worker cohort 在 closed startup generation 下安装真实 roots/ledgers/participants，确认 ALL seed 后开放 admission 并回放。Wasm 与 WASIp1 必须在同一实际停止点一起 checkpoint；外部文件内容和 I/O 副作用不会回滚。
