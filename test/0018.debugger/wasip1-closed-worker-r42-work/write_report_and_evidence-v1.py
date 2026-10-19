from pathlib import Path
import hashlib,json,tarfile,os
D=Path(__file__).parent;E=D.parent.parent
sha=lambda p:hashlib.file_digest(Path(p).open("rb"),"sha256").hexdigest()
q=json.loads((D/"qualified-v2.json").read_text());g=json.loads((D/"guard-matrix-v2.json").read_text());r=json.loads((D/"results-v2.json").read_text())
assert q["passed"] and q["fresh_native_cases"]==24 and q["closed_cohort_native_cases"]==16 and q["frontend_checks"]==24
assert g["passed"] and g["parent_events_unchanged"] and g["ram_disposable_products_retired"] and all(p["pidfd_retired"] for p in g["processes"])
assert Path("/proc/self/cgroup").read_text()==g["cgroup"]
assert q["source_manifest_sha256"]==sha(D/"source-manifest-v2.json") and q["source_equivalence_sha256"]==sha(D/"source-equivalence-v2.json")
assert q["results_sha256"]==sha(D/"results-v2.json") and q["controller_sha256"]==sha(D/"run_matrix-v2.py") and g["guard_sha256"]==sha(D/"guard_matrix-v2.py")
m=json.loads((D/"source-manifest-v2.json").read_text());eq=json.loads((D/"source-equivalence-v2.json").read_text())
assert len(m)==62 and len(eq["delta"])==12 and len(eq["unchanged_inputs"])==50 and all(sha(D/"source-v2"/k)==h for k,h in m.items())
closed=[]
for row in r["rows"]:
 log=D/(row["repo"]+"-"+row["stage"]+"-v2.log")
 assert row["passed"] and row["log_sha256"]==sha(log)
 if row["actual_native_runtime_execution"] and row["stage"].startswith(("core-","wasip1-")):
  s=log.read_text();n=2 if row["stage"].startswith("core-") else 1
  witness=f"CLOSED_COHORT workers={n} held_before_old_retirement=1 rechecked_after_old_join=1 cancel_actual_os_tls_join_before_open=1 guest_execution=0 PASS"
  assert witness in s
  closed.append(dict(repo=row["repo"],stage=row["stage"],workers=n,log_sha256=sha(log)))
for p,h in r["dependencies"].items():assert sha(p)==h
v=json.loads((D/"guard-matrix-v1.json").read_text())
assert not v["passed"] and v["parent_events_unchanged"] and v["ram_disposable_products_retired"] and all(p["pidfd_retired"] for p in v["processes"])
assert len(closed)==16 and sum(x["workers"] for x in closed)==24
report=dict(scope="Both repositories: real private closed candidate OS cohort precreated before old retirement",
 qualification=q,guard=g,closed_cohort_native_witnesses=closed,source_delta=eq["delta"],original_source_before=eq["before"],
 inherited_unchanged_inputs=eq["unchanged_inputs"],native_case_groups=dict(core=8,wasip1=8,registry=4,active_generation2=4),
 semantics=dict(whole_closed_cohort_precreated=True,root_debug_wasip1_scopes_held=True,actual_old_os_tls_join_recheck=True,
 cancellation_os_tls_join_before_old_admission_reopen=True,worker_destructor_precedes_borrowed_world_resource_destructors=True,
 existing_rehearsal_join_semantics_preserved=True,closed_worker_limit=256,new_io="FastIO native_thread and FastIO test output"),
 native_scope="Linux only; four targets passed frontend checks; inherited R34 source/dependencies plus exact owned delta",
 excluded_attempts=[dict(version=1,reason="explicit interruption before native execution to correct per-thread retained root counts",native_success_claimed=False,guard_sha256=sha(D/"guard-matrix-v1.json"))],
 limitations=dict(joint_world_publication=False,actual_runtime_pause_enrollment=False,execution_generation_lease=False,
 native_live_adoption=False,restored_worker_activation=False,new_world_replay=False,complete_instance_restore=False,
 other_three_os_native_delta_validation=False,real_os_create_failure_injection_tested=False),
 pending=["joint source/initializer/GC/WASIp1/code publication","adopt this same OS cohort into actual closed runtime generation and pause controller",
 "all-root and ledger startup proof before normal admission","new-world replay and restored proc_exit/terminal cleanup","three other OS native delta verification"],
 checkpoint_contract=dict(wasm_requires_same_stop_wasip1_checkpoint=True,cross_os_mount_reconfiguration_required=True,file_contents_in_checkpoint=False))
(D/"wasip1_closed_worker_r42_test_report.json").write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n")
md="""# WASIp1 checkpoint：发布前预创建 OS 线程（R42）

两仓库已同步实现真正保留的私有 OS 线程组：候选实例完成代码所有权准备后、旧实例开始退休前，分配全部固定 packet、ledger、FastIO native_thread，安装实际线程上的 root/debug/WASIp1 TLS，并整体停在私有关闭 gate 中。此前的短暂 rehearsal 线程仍按原语义全部 join，新的保留线程另行创建。

旧实例的执行排空与实际 OS/TLS join 完成后，管理器检查自己保留的同一线程组、真实 participant、native wrapper 和安装记录。放弃候选实例时，先取消 gate、实际 join 每个候选 OS 线程，核验根和 TLS 已清除，再重新打开原实例的普通入口。world 成员反向析构保证保留线程先退出，再释放其借用的 WASIp1 cache、帧、代码、GC 和 source。

新增 maximum_private_closed_workers（默认 256）和独立诊断字段；超出数量预算在旧实例退休前拒绝。诊断字段不授予运行权限。保留共享根在多个线程重复安装时，以原 rehearsal 的实际安装总量校验，避免只计算一次 retained roots。ROS 原模式及条件分支保留，C++ 新线程和输出使用 FastIO。

## 验证

- 最终 v2：24/24 Linux 原生用例，包括 Core 8、WASIp1 8、线程登记压力 4、活动 generation 2 嵌套帧 4。
- 其中 16 个用例真实保留、取消并 join 24 个新的候选 OS 线程，覆盖普通和间接调用、instruction 和 unwind；验证预算拒绝保留旧世界、whole-body lease、旧 TLS 未退出时保持关闭、旧 OS join 后重新核验、错误 token 拒绝、取消后真正 join 再打开旧入口。
- 24 项四目标 frontend：两个仓库的 runtime、Core 测试、WASIp1 测试分别编译 Linux、Windows、FreeBSD、macOS target。后三个 OS 本轮尚未原生运行。
- 原线程登记压力仍创建 2340 个真实 native worker、完成 1316 次 Wasm 返回 42；原 generation 2 同世界继续执行仍返回 1190。它们不证明 heap 回滚。
- 14 个 WAT 输入的 assemble/validator 共 28 项数据检查，不代表全部 Wasm 3.0 特性覆盖。
- v1 在原生测试开始前主动中断，修正 retained roots 计数；其进程全部物理退出、RAM 已清理、日志保留，未计入 v2 通过结果。

## 尚未完成

这个线程组仍属于私有候选世界。尚未联合发布新 source/initializer/GC/WASIp1/code，尚未给它真实执行 generation lease、迁入运行时 pause controller、在所有 root/ledger 就绪后打开入口，也尚未接通新世界 saved-frame 回放和恢复后的 proc_exit/终结清理。真实 OS create 失败的注入测试未覆盖。不能称为完整实例恢复或跨 OS 原生恢复通过。

Wasm checkpoint 必须同时 checkpoint WASIp1；跨 OS 需要重新配置 mount，状态包不保存文件内容。

## 资源与证据

全部本轮编译、validator 和原生测试位于原 64 GiB Linux cgroup；RAM 产品上限 768 MiB、aggregate RSS 上限 6 GiB、持久输出上限 16 MiB，原持久四 OS 套件磁盘保护不变。最终 guard 验证所有受管进程已物理退出、RAM 产品清理和父 memory.events 不变。归档包含精确 delta、原源码、完整命令/日志/散列并逐成员读回；同名 JSON 提供细节。
"""
(D/"wasip1_closed_worker_r42_test_report.md").write_text(md)
C=E/"cold-relocated-r42";C.mkdir(exist_ok=True);archive=C/"closed-worker-r42-evidence.tar.gz";assert not archive.exists()
members={p.name:dict(size=p.stat().st_size,sha256=sha(p)) for p in D.iterdir() if p.is_file() and not p.is_symlink() and p.name!="evidence-qualified.json"}
with tarfile.open(archive,"w:gz") as a:
 for name in sorted(members):a.add(D/name,arcname=name,recursive=False)
seen=set();logical=0
with tarfile.open(archive,"r:gz") as a:
 for member in a:
  assert member.isfile() and member.name in members and member.name not in seen
  data=a.extractfile(member).read();assert len(data)==members[member.name]["size"] and hashlib.sha256(data).hexdigest()==members[member.name]["sha256"]
  seen.add(member.name);logical+=len(data)
assert seen==set(members);os.chmod(archive,0o444)
(D/"evidence-qualified.json").write_text(json.dumps(dict(passed=True,archive=str(archive),sha256=sha(archive),size=archive.stat().st_size,members=members,
 all_members_fully_read_back=True,total_uncompressed_bytes=logical,source_manifest_sha256=sha(D/"source-manifest-v2.json"),
 native_qualification_sha256=sha(D/"qualified-v2.json"),native_guard_sha256=sha(D/"guard-matrix-v2.json"),
 report_json_sha256=sha(D/"wasip1_closed_worker_r42_test_report.json"),report_md_sha256=sha(D/"wasip1_closed_worker_r42_test_report.md")),indent=2)+"\n")
print(json.dumps(dict(passed=True,native_cases=24,closed_cohort_cases=16,frontends=24,archive_sha256=sha(archive),archive_bytes=archive.stat().st_size,members=len(members))),flush=True)
