from pathlib import Path
import fcntl,hashlib,json,os
D=Path(__file__).parent;E=D.parent.parent;A=E.parent/"wasip1-active-environment.json"
sha=lambda p:hashlib.file_digest(Path(p).open("rb"),"sha256").hexdigest()
lock=os.open(E/"suite.lock",os.O_RDWR);fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
e=json.loads((D/"evidence-qualified.json").read_text());g=json.loads((D/"guard-evidence-v3.json").read_text())
n=json.loads((D/"qualified-v4.json").read_text());ng=json.loads((D/"guard-matrix-v4.json").read_text())
assert e["passed"] and g["passed"] and g["parent_events_unchanged"] and all(p["pidfd_retired"] for p in g["processes"])
assert n["passed"] and ng["passed"] and ng["ram_disposable_products_retired"] and ng["parent_events_unchanged"] and all(p["pidfd_retired"] for p in ng["processes"])
assert e["sha256"]==sha(e["archive"]) and n["source_manifest_sha256"]==sha(D/"source-manifest-v4.json")
assert sha(E/"guard.py")=="096f87e9f257926b873718da10cfa08f3c822ddd145cf99b4071db3f3009d7f0"
assert sha(E/"storage-policy.json")=="fc24a09a9b5f83e2722b72249e06de39e6cd495a7988cea9d88f1ca67f2f95fc"
raw=A.read_bytes();old=json.loads(raw);ident=A.stat()
assert old["boot"]==Path("/proc/sys/kernel/random/boot_id").read_text().strip()=="b5e7a71f-8b40-4f39-8a41-fdce3f8fd2cb"
key="r43_checkpoint_root_generation_identity";assert key not in old
field=dict(source_manifest=str(D/"source-manifest-v4.json"),source_manifest_sha256=sha(D/"source-manifest-v4.json"),source_files=80,
 unchanged_inherited_source_files=50,owned_source_delta_files=22,added_unchanged_regression_inputs=8,real_private_closed_os_cohort_precreated=True,
 authentic_root_generation_identity=True,current_cold_root_actual_compiler_adoption_seal=True,private_candidate_root_identity_unsealed=True,
 committed_hot_generation2_fallback_validated=True,root_generation_number_is_not_hot_transaction_permission=True,
 no_profile_native_owner_selection_uses_actual_retained_slot=True,vm_helper_debug_permission_expanded=False,
 fresh_restored_generation2_root_published_or_executed=False,
 native_cases=36,closed_cohort_native_cases=16,private_generation2_native_cases=4,observation_only_native_cases=4,no_checkpoint_profile_native_code_cases=4,closed_cohort_os_workers=24,compiler_frontend_checks=24,
 old_os_tls_join_recheck=True,cancel_actual_os_tls_join_before_old_admission_open=True,
 actual_runtime_generation_lease=False,world_publication=False,restored_worker_activation=False,new_world_replay=False,complete_instance_restore=False,
 windows_macos_freebsd_native_delta_validation=False,
 native_qualification=str(D/"qualified-v4.json"),native_qualification_sha256=sha(D/"qualified-v4.json"),
 native_guard=str(D/"guard-matrix-v4.json"),native_guard_sha256=sha(D/"guard-matrix-v4.json"),
 evidence_archive=e["archive"],evidence_archive_sha256=e["sha256"],evidence_archive_size=e["size"],
 evidence_qualification=str(D/"evidence-qualified.json"),evidence_qualification_sha256=sha(D/"evidence-qualified.json"),
 evidence_guard=str(D/"guard-evidence-v3.json"),evidence_guard_sha256=sha(D/"guard-evidence-v3.json"),
 pending=["joint-world source/initializer/GC/WASIp1/code publication and staged native-owner actual adoption","fresh restored generation2 root actual seal and execution","same precreated OS cohort actual runtime generation/pause enrollment","closed startup and new-world replay and terminal cleanup","other three OS native delta validation"],
 original_storage_policy_unchanged=True,wasm_checkpoint_requires_same_stop_wasip1_checkpoint=True)
new=dict(old);new[key]=field;temporary=A.with_name(A.name+".r43-new")
assert not temporary.exists()
with temporary.open("xb") as output:output.write((json.dumps(new,indent=2)+"\n").encode());output.flush();os.fsync(output.fileno())
os.chmod(temporary,ident.st_mode&0o777)
now=A.stat();assert (now.st_dev,now.st_ino,now.st_size,now.st_mtime_ns)==(ident.st_dev,ident.st_ino,ident.st_size,ident.st_mtime_ns) and A.read_bytes()==raw
os.replace(temporary,A);result=json.loads(A.read_text());assert all(result[k]==v for k,v in old.items()) and len(result)==len(old)+1
receipt=dict(passed=True,preserved_prior_fields=len(old),before_sha256=hashlib.sha256(raw).hexdigest(),after_sha256=sha(A),
 appended_field=key,registration_only_not_native_execution=True)
(D/"active-environment-update.json").write_text(json.dumps(receipt,indent=2)+"\n");print(json.dumps(receipt))
