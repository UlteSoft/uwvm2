from pathlib import Path
import hashlib,json,os

D=Path(__file__).parent
def sha(p):
    with Path(p).open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()
load=lambda n:json.loads((D/n).read_text())
q=load("ram-native-v2-qualified.json");g=load("guard-ram-native-v2.json")
a=load("analysis-compile-qualified-v1.json");ag=load("guard-analysis-compile-v1.json")
n=load("ram-native-v2-results.json")
v=load("generation-native-v3-qualified.json");vg=load("guard-generation-native-v3.json")
vn=load("generation-native-v3-results.json");eq=load("generation-v2-source-equivalence.json")
base=load("syntax-overlay-v1.json"); final=load("syntax-overlay-generation-v2.json")
original_final=final
x=load("active-generation-native-v1-qualified.json");xg=load("guard-active-generation-native-v1.json")
xn=load("active-generation-native-v1-results.json");aeq=load("active-generation-v1-source-equivalence.json")
final=load("syntax-overlay-active-generation-v1.json")
assert q["passed"] and q["native_runs"]==q["fresh_native_runs"]==24 and q["reused_unchanged_native_runs"]==0
assert v["passed"] and v["fresh_native_cases"]==v["genuine_generation_2_cases"]==4 and v["frontend_checks"]==2
for proof in (g,vg,ag,xg):
    assert proof["passed"] and proof["parent_events_unchanged"] and all(p["pidfd_retired"] for p in proof["processes"])
assert g["ram_disposable_products_retired"] and vg["ram_disposable_products_retired"] and xg["ram_disposable_products_retired"]
assert a["passed"] and len(a["rows"])==16
assert sha(D/"ram-native-v2-results.json")==q["results_sha256"]
assert sha(D/"generation-native-v3-results.json")==v["results_sha256"]
assert sha(D/"generation_native_regression-v3.py")==v["controller_sha256"]
assert sha(D/"guard_generation_native-v3.py")==vg["guard_sha256"]
assert sha(D/"syntax-overlay-v1.json")==q["source_manifest_sha256"]==a["source_manifest_sha256"]==eq["old_manifest_sha256"]
assert sha(D/"syntax-overlay-generation-v2.json")==v["source_manifest_sha256"]==eq["new_manifest_sha256"]
assert v["base_frontend_qualification_sha256"]==sha(D/"analysis-compile-qualified-v1.json")
assert v["source_equivalence_sha256"]==sha(D/"generation-v2-source-equivalence.json")
assert eq["passed"] and eq["unchanged_inputs"]==base and len(base)==42 and len(original_final)==44
assert eq["added_inputs"]=={k:h for k,h in original_final.items() if k not in base} and all(original_final[k]==h for k,h in base.items())
native=[r for r in n["rows"] if r["actual_native_runtime_execution"]]
extra=[r for r in vn["rows"] if r["actual_native_runtime_execution"]]
assert len(native)==24 and all(r["passed"] for r in native)
assert len(extra)==4 and all(r["passed"] and r["actual_hot_replacement_generation_2"] and r["actual_private_effective_body_owner"] and r["original_committed_callback_after_discard"] for r in extra)
assert sum(r["compiler_frontend_only"] and r["passed"] for r in vn["rows"])==2
for collection,suffix in ((n["rows"],"ram-native-v2"),(vn["rows"],"generation-v2")):
    for row in collection:
        log=D/(row["repo"]+"-"+row["stage"]+"-"+suffix+".log")
        assert sha(log)==row["log_sha256"]
assert x["passed"] and x["fresh_native_cases"]==x["genuine_active_generation_2_cases"]==4 and x["frontend_checks"]==2
assert sha(D/"active-generation-native-v1-results.json")==x["results_sha256"]
assert sha(D/"active_generation_native-v1.py")==x["controller_sha256"] and sha(D/"guard_active_generation_native-v1.py")==xg["guard_sha256"]
assert x["base_native_qualification_sha256"]==sha(D/"ram-native-v2-qualified.json") and x["dormant_generation_qualification_sha256"]==sha(D/"generation-native-v3-qualified.json")
assert x["source_equivalence_sha256"]==sha(D/"active-generation-v1-source-equivalence.json")
assert aeq["passed"] and aeq["unchanged_inputs"]==base and aeq["previous_inputs_unchanged"]==original_final and len(final)==50
assert aeq["new_manifest_sha256"]==sha(D/"syntax-overlay-active-generation-v1.json")==x["source_manifest_sha256"]
assert all(final[k]==h for k,h in original_final.items())
active=[r for r in xn["rows"] if r["actual_native_runtime_execution"]]
assert len(active)==4 and all(r["passed"] and r["actual_active_generation2_body_handoffs"] and r["original_retained_world_continuation_1190"] for r in active)
assert sum(r["compiler_frontend_only"] and r["passed"] for r in xn["rows"])==2
for row in xn["rows"]:assert sha(D/(row["repo"]+"-"+row["stage"]+"-active-generation-v1.log"))==row["log_sha256"]
assert g["cgroup"]==vg["cgroup"]==ag["cgroup"]==xg["cgroup"]
fixtures=lambda rows:sum("-assemble" in r["stage"] or "-validate" in r["stage"] for r in rows)
assert len(a["wasm_data_fixture_checks"])==8 and fixtures(n["rows"])==28 and fixtures(vn["rows"])==4 and fixtures(xn["rows"])==8
recovery=load("reboot-recovery-r40-qualified.json")
assert recovery["passed"] and recovery["storage_policy_unchanged"] and recovery["reused_anchor"] and recovery["reused_volume"]
assert g["boot_id"]==vg["boot_id"]==xg["boot_id"]==recovery["boot_id"]
report=dict(round="r40",implemented="genuine private final full-code owner handoff",
    source_manifest_sha256=sha(D/"syntax-overlay-active-generation-v1.json"),source_files=50,
    reboot_recovery_sha256=sha(D/"reboot-recovery-r40-qualified.json"),recovered_original_environment_after_reboot=True,
    interrupted_preboot_cases_preserved_but_excluded=23,base_native_attempt_version=2,generation_native_attempt_version=3,
    base_source_manifest_sha256=sha(D/"syntax-overlay-v1.json"),unchanged_base_source_files=42,
    fresh_linux_native_cases=32,reused_native_cases=0,fresh_frontend_checks=20,
    frontend_target_os=["linux","windows","freebsd","macos"],additional_linux_test_frontend_checks=4,
    wasm_frontend_data_checks=8,native_fixture_data_checks=40,genuine_generation_2_native_cases=8,
    dormant_generation_2_native_cases=4,active_generation_2_nested_native_cases=4,active_generation_2_handoff_episodes=8,
    native_qualification_sha256=sha(D/"ram-native-v2-qualified.json"),native_guard_sha256=sha(D/"guard-ram-native-v2.json"),
    generation_qualification_sha256=sha(D/"generation-native-v3-qualified.json"),generation_guard_sha256=sha(D/"guard-generation-native-v3.json"),
    frontend_qualification_sha256=sha(D/"analysis-compile-qualified-v1.json"),frontend_guard_sha256=sha(D/"guard-analysis-compile-v1.json"),
    source_equivalence_sha256=sha(D/"active-generation-v1-source-equivalence.json"),
    active_generation_qualification_sha256=sha(D/"active-generation-native-v1-qualified.json"),active_generation_guard_sha256=sha(D/"guard-active-generation-native-v1.json"),
    original_retained_world_nested_continuation_tested=True,original_retained_world_nested_continuation_result=1190,
    actual_engine_context_owners_transferred_privately=True,original_typed_target_allocation_preserved=True,
    original_safe_point_bitmap_allocations_preserved=True,pending_range_listener_detached_before_owner_move=True,
    native_endpoint_observer_staged_only=True,live_native_seal_issued=False,genuine_final_owners_rechecked_after_physical_join=True,
    transferred_effective_body_is_uncommitted_parser_data=True,generation_greater_than_one_native_handoff_case_tested=True,
    original_actual_committed_callback_after_candidate_discard=17,
    owned_peak_rss_bytes=max(g["owned_peak_aggregate_rss_upper_bytes"],vg["owned_peak_aggregate_rss_upper_bytes"],xg["owned_peak_aggregate_rss_upper_bytes"]),
    cgroup_peak_bytes=max(g["cgroup_peak_bytes"],vg["cgroup_peak_bytes"],xg["cgroup_peak_bytes"]),
    owned_ram_peak_bytes=max(g["ram_peak_allocated_bytes"],vg["ram_peak_allocated_bytes"],xg["ram_peak_allocated_bytes"]),
    owned_native_processes_reaped=len(g["processes"])+len(vg["processes"])+len(xg["processes"]),owned_frontend_processes_reaped=len(ag["processes"]),
    all_owned_processes_reaped=True,process_cleanup_scope="completed qualifying attempts only; interrupted preboot attempt ended by reboot, no normal reap receipt",
    owned_ram_products_deleted=True,parent_memory_events_unchanged=True,
    original_persistent_four_os_suite_admitted=False,original_storage_policy_unchanged=True,original_cgroup=g["cgroup"],
    native_other_os_completed_this_round=False,source_scope="50 frozen inputs over authenticated unchanged R34 dependencies; 42 base files unchanged, plus 4 generation tests and 4 official Wasm fixtures",
    authority_diagnostics_scope="new private candidate world only; original published world and genuine hot replacement retain their own actual authority",
    source_initializer_serial_issued=False,source_seal_issued=False,world_publication=False,
    restored_worker_startup=False,guest_replay=False,wasm_and_wasip1_same_stop_required=True,asm_vm_or_host_context_granted=False)
text="""# R40：真实 full-code owner 的私有交接

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
"""
for name,value in (("wasip1_full_code_handoff_r40_test_report.json",json.dumps(report,indent=2)+"\n"),
                   ("wasip1_full_code_handoff_r40_test_report.md",text)):
    with (D/name).open("x") as f:f.write(value);f.flush();os.fsync(f.fileno())
print(json.dumps(report))
