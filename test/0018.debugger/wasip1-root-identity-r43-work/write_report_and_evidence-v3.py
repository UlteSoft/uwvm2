from pathlib import Path
import hashlib,json,tarfile,os
D=Path(__file__).parent;E=D.parent.parent
sha=lambda p:hashlib.file_digest(Path(p).open("rb"),"sha256").hexdigest()
q=json.loads((D/"qualified-v4.json").read_text());g=json.loads((D/"guard-matrix-v4.json").read_text());r=json.loads((D/"results-v4.json").read_text())
assert q["passed"] and q["fresh_native_cases"]==36 and q["closed_cohort_native_cases"]==16 and q["frontend_checks"]==24
assert g["passed"] and g["parent_events_unchanged"] and g["ram_disposable_products_retired"] and all(p["pidfd_retired"] for p in g["processes"])
assert Path("/proc/self/cgroup").read_text()==g["cgroup"]
assert q["source_manifest_sha256"]==sha(D/"source-manifest-v4.json") and q["source_equivalence_sha256"]==sha(D/"source-equivalence-v4.json")
assert q["results_sha256"]==sha(D/"results-v4.json") and q["controller_sha256"]==sha(D/"run_matrix-v4.py") and g["guard_sha256"]==sha(D/"guard_matrix-v4.py")
m=json.loads((D/"source-manifest-v4.json").read_text());eq=json.loads((D/"source-equivalence-v4.json").read_text())
assert len(m)==80 and len(eq["delta"])==30 and len(eq["unchanged_inputs"])==50 and eq["owned_delta_files"]==22
assert all(sha(D/"source-v3"/k)==h for k,h in m.items())
witnesses=[];groups={}
for row in r["rows"]:
 log=Path(row.get("log_file",str(D/(row["repo"]+"-"+row["stage"]+"-v4.log"))))
 assert row["passed"] and row["log_sha256"]==sha(log)
 if not row["actual_native_runtime_execution"]:continue
 stage=row["stage"];s=log.read_text()
 if stage.startswith("active-generation2-"):
  assert "ROOT_IDENTITY private_generation2=1 live_seal=0 PASS" in s;group="active_generation2"
 elif stage.startswith("private-generation2-"):
  assert "GENERATION2 private_effective_body_owners=1" in s and "whole_restore=0 PASS" in s;group="private_generation2"
 elif stage.startswith("native-code-"):
  assert "debug_native_code_runtime: PASS actual entries, recursive identities, musttail continuation, generation two, alias, stale ticket, reset" in s;group="no_profile_native_code"
 elif stage.startswith("observation-"):
  assert "checkpoint_actual_observation: PASS actual typed debug values; executable_restore=false" in s;group="observation_only"
 elif stage.startswith("registry-"):
  assert "actual_workers=585 actual_guest_returns_42=329 weak_holes_reused=320 PASS" in s;group="registry"
 elif stage.startswith(("core-","wasip1-")):
  n=2 if stage.startswith("core-") else 1
  assert f"CLOSED_COHORT workers={n} held_before_old_retirement=1 rechecked_after_old_join=1 cancel_actual_os_tls_join_before_open=1 guest_execution=0 PASS" in s
  group="core" if n==2 else "wasip1"
 else:raise AssertionError(stage)
 groups[group]=groups.get(group,0)+1
 witnesses.append(dict(repo=row["repo"],stage=stage,group=group,log_sha256=sha(log)))
assert groups==dict(core=8,wasip1=8,registry=4,active_generation2=4,private_generation2=4,no_profile_native_code=4,observation_only=4)
for p,h in r["dependencies"].items():assert sha(p)==h
v3=json.loads((D/"guard-matrix-v3.json").read_text());f3=json.loads((D/"results-v3.json").read_text())
assert not v3["passed"] and v3["parent_events_unchanged"] and v3["ram_disposable_products_retired"] and all(p["pidfd_retired"] for p in v3["processes"])
assert q["reused_exact_source_frontend_checks"]==24 and q["fresh_frontend_checks"]==0
assert sum(row["actual_native_runtime_execution"] for row in f3["rows"])==0
assert sha(D/"source-manifest-v3.json")==sha(D/"source-manifest-v4.json")
v2=json.loads((D/"guard-matrix-v2.json").read_text())
assert not v2["passed"] and v2["parent_events_unchanged"] and v2["ram_disposable_products_retired"] and all(p["pidfd_retired"] for p in v2["processes"])
v=json.loads((D/"guard-matrix-v1.json").read_text())
assert not v["passed"] and v["parent_events_unchanged"] and v["ram_disposable_products_retired"] and all(p["pidfd_retired"] for p in v["processes"])
report=dict(scope="Both repositories: authentic compiled root-generation identity, private preparation remains unsealed",
 qualification=q,guard=g,native_witnesses=witnesses,native_case_groups=groups,
 source_delta=eq["delta"],owned_source_delta_files=22,added_unchanged_regression_inputs=eq["added_unchanged_regression_inputs"],
 original_source_before=eq["before"],inherited_unchanged_inputs=eq["unchanged_inputs"],
 qualified_runtime_projection=eq["local_runtime_projection"],
 semantics=dict(immutable_owned_root_rows=True,strong_source_profile_plan_identity=True,
 actual_engine_context_module_and_typed_raw_resume_entries_matched=True,
 actual_root_seal_requires_current_registry_source_initializer_validation_native_owner_adoption=True,
 actual_materializer_removes_previous_identity_before_recompilation=True,
 generation_number_does_not_choose_checkpoint_root_or_hot_transaction=True,
 genuine_hot_generation_transaction_fallback_preserved=True,
 no_checkpoint_profile_native_selection_uses_actual_retained_owner_slot_then_sealed_body_checks=True,
 private_prepared_root_is_unsealed=True,private_identity_allocation_charged_before_engine_handoff=True,
 public_native_step_read_permission_expanded=False,new_io="FastIO output, no new filesystem/provider API"),
 native_scope="Linux original 64 GiB cgroup; exact unchanged C++ source frontend checks reused from v3 after complete dependency/log/source verification; native objects/links freshly rebuilt in v4; not entire current peer-dirty local TU",
 excluded_attempts=[dict(version=1,reason="explicitly interrupted before native execution to complete actual re-materializer identity cleanup and non-checkpoint-profile ASM route",
 native_success_claimed=False,guard_sha256=sha(D/"guard-matrix-v1.json")),
 dict(version=2,reason="ROS real full-only materializer has three parameters; corrected private friend signature before native execution and added exact module initializer phase check",native_success_claimed=False,guard_sha256=sha(D/"guard-matrix-v2.json")),
 dict(version=3,reason="Python fixture list extension accidentally altered registry case tuple; failed after 24 successful exact-source frontend checks and fresh first runtime compile, before native execution. Corrected only harness; exact frontend rows requalified in v4",native_success_claimed=False,frontend_successes_reused=24,guard_sha256=sha(D/"guard-matrix-v3.json"))],
 limitations=dict(joint_world_publication=False,fresh_restored_generation2_root_published_or_executed=False,
 actual_runtime_pause_enrollment=False,execution_generation_lease=False,
 staged_native_live_adoption=False,restored_worker_activation=False,new_world_replay=False,complete_instance_restore=False,
 other_three_os_native_delta_validation=False,explicit_allocation_failure_injection_tested=False),
 pending=["joint source/initializer/GC/WASIp1/code publication and authentic adoption of staged native owner",
 "seal fresh restored generation2 root only after real adoption, then test actual fresh-root execution",
 "adopt same precreated OS cohort into real closed runtime generation and pause controller",
 "all-root and ledger startup proof before admission; new-world saved-frame replay",
 "restored proc_exit and terminal cleanup; other three OS native verification"],
 checkpoint_contract=dict(wasm_requires_same_stop_wasip1_checkpoint=True,cross_os_mount_reconfiguration_required=True,file_contents_in_checkpoint=False))
(D/"wasip1_root_identity_r43_test_report.json").write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n")
md="""# WASIp1 checkpoint：真实根代代码身份（R43）

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
"""
(D/"wasip1_root_identity_r43_test_report.md").write_text(md)
C=E/"cold-relocated-r43";C.mkdir(exist_ok=True);archive=C/"root-identity-r43-evidence.tar.gz";assert not archive.exists()
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
 all_members_fully_read_back=True,total_uncompressed_bytes=logical,source_manifest_sha256=sha(D/"source-manifest-v4.json"),
 native_qualification_sha256=sha(D/"qualified-v4.json"),native_guard_sha256=sha(D/"guard-matrix-v4.json"),
 report_json_sha256=sha(D/"wasip1_root_identity_r43_test_report.json"),report_md_sha256=sha(D/"wasip1_root_identity_r43_test_report.md")),indent=2)+"\n")
print(json.dumps(dict(passed=True,native_cases=36,closed_cohort_cases=16,frontends=24,archive_sha256=sha(archive),archive_bytes=archive.stat().st_size,members=len(members))),flush=True)
