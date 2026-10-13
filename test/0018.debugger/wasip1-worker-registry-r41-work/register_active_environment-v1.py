from pathlib import Path
import fcntl,hashlib,json,os
D=Path(__file__).parent;E=D.parent.parent;A=E.parent/"wasip1-active-environment.json"
sha=lambda p:hashlib.file_digest(Path(p).open("rb"),"sha256").hexdigest()
lock=os.open(E/"suite.lock",os.O_RDWR);fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
e=json.loads((D/"evidence-qualified.json").read_text());g=json.loads((D/"guard-evidence-v1.json").read_text())
n=json.loads((D/"qualified-v5.json").read_text());ng=json.loads((D/"guard-matrix-v5.json").read_text())
assert e["passed"] and g["passed"] and g["parent_events_unchanged"] and all(p["pidfd_retired"] for p in g["processes"])
assert n["passed"] and ng["passed"] and ng["ram_disposable_products_retired"] and ng["parent_events_unchanged"] and all(p["pidfd_retired"] for p in ng["processes"])
assert e["sha256"]==sha(e["archive"]) and n["source_manifest_sha256"]==sha(D/"source-manifest-v5.json")
assert sha(E/"guard.py")=="096f87e9f257926b873718da10cfa08f3c822ddd145cf99b4071db3f3009d7f0"
assert sha(E/"storage-policy.json")=="fc24a09a9b5f83e2722b72249e06de39e6cd495a7988cea9d88f1ca67f2f95fc"
raw=A.read_bytes();old=json.loads(raw);ident=A.stat()
assert old["selected_boot"]==Path("/proc/sys/kernel/random/boot_id").read_text().strip()=="b5e7a71f-8b40-4f39-8a41-fdce3f8fd2cb"
key="r41_checkpoint_worker_registry_reuse";assert key not in old
field=dict(source_manifest=str(D/"source-manifest-v5.json"),source_manifest_sha256=sha(D/"source-manifest-v5.json"),source_files=56,
 unchanged_inherited_source_files=50,owned_source_delta_files=6,live_native_capacity=256,canonical_retry_history_separated=True,
 slot_reused_only_after_actual_os_tls_join=True,unrelated_completed_owner_not_pinned_under_registry_mutex=True,
 initial_and_private_restored_factories_share_registration_helper=True,linux_native_cases=24,fresh_native_cases=24,
 pre_fix_expected_failed_native_cases=1,compiler_frontend_checks=16,stress_native_workers=2340,stress_actual_guest_returns_42=1316,
 world_publication=False,restored_worker_startup=False,new_world_replay=False,complete_instance_restore=False,
 original_same_world_generation2_continuation=1190,windows_macos_freebsd_native_delta_validation=False,
 native_qualification=str(D/"qualified-v5.json"),native_qualification_sha256=sha(D/"qualified-v5.json"),
 native_guard=str(D/"guard-matrix-v5.json"),native_guard_sha256=sha(D/"guard-matrix-v5.json"),
 evidence_archive=e["archive"],evidence_archive_sha256=e["sha256"],evidence_archive_size=e["size"],
 evidence_qualification=str(D/"evidence-qualified.json"),evidence_qualification_sha256=sha(D/"evidence-qualified.json"),
 evidence_guard=str(D/"guard-evidence-v1.json"),evidence_guard_sha256=sha(D/"guard-evidence-v1.json"),
 pending=["joint-world publication","preallocated restored native startup cohort","new-world replay","other three OS native delta validation"],
 original_storage_policy_unchanged=True,wasm_checkpoint_requires_same_stop_wasip1_checkpoint=True)
new=dict(old);new[key]=field;temporary=A.with_name(A.name+".r41-new")
assert not temporary.exists()
with temporary.open("xb") as output:output.write((json.dumps(new,indent=2)+"\n").encode());output.flush();os.fsync(output.fileno())
os.chmod(temporary,ident.st_mode&0o777)
now=A.stat();assert (now.st_dev,now.st_ino,now.st_size,now.st_mtime_ns)==(ident.st_dev,ident.st_ino,ident.st_size,ident.st_mtime_ns) and A.read_bytes()==raw
os.replace(temporary,A);result=json.loads(A.read_text());assert all(result[k]==v for k,v in old.items()) and len(result)==len(old)+1
receipt=dict(passed=True,preserved_prior_fields=len(old),before_sha256=hashlib.sha256(raw).hexdigest(),after_sha256=sha(A),
 appended_field=key,registration_only_not_native_execution=True)
(D/"active-environment-update.json").write_text(json.dumps(receipt,indent=2)+"\n");print(json.dumps(receipt))
