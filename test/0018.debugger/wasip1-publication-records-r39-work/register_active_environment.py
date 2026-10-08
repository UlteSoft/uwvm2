"""Append this round only, preserving every concurrent/prior environment field."""
from pathlib import Path
import fcntl, hashlib, json, os, stat

D = Path(__file__).parent
E = D.parent.parent
A = E.parent / "wasip1-active-environment.json"
sha = lambda p: hashlib.file_digest(Path(p).open("rb"), "sha256").hexdigest()
load = lambda p: json.loads(Path(p).read_text())
key = "r39_checkpoint_full_publication_records"
q = load(D / "ram-native-v2-qualified.json")
g = load(D / "guard-ram-native-v2.json")
eq = load(D / "r39-final-evidence-qualified.json")
eg = load(D / "guard-final-evidence.json")
cq = load(E / "cold-relocated-r39/cold-custody-qualified.json")
assert q["passed"] and q["fresh_native_runs"] == q["native_runs"] == 24 and q["reused_unchanged_native_runs"] == 0
assert not q["full_world_publication"] and sha(D / "ram-native-v2-results.json") == q["results_sha256"]
assert g["passed"] and g["parent_events_unchanged"] and g["ram_disposable_products_retired"]
assert all(x["pidfd_retired"] for x in g["processes"])
assert eq["passed"] and eq["full_members_read_back"] and eq["member_count"] == 156
assert sha(eq["archive"]) == eq["archive_sha256"]
assert eg["passed"] and eg["parent_events_unchanged"] and all(x["pidfd_retired"] for x in eg["processes"])
assert cq["passed"] and cq["archive_files"] == 6
fields = dict(
    source_manifest=str(D / "syntax-overlay-v2.json"), source_manifest_sha256=q["source_manifest_sha256"],
    final_full_runtime_records_privately_preallocated=True, combined_function_quota_before_retirement=True,
    full_publication_records_rechecked_after_authentic_drain_and_physical_join=True,
    world_publication=False, restored_worker_startup=False, guest_replay=False,
    live_native_seal_issued=False, source_seal_issued=False, initializer_serial_issued=False,
    asm_vm_or_host_context_granted=False, wasm_and_wasip1_same_actual_stop_required=True,
    linux_native_cases=24, fresh_native_cases=24, reused_unchanged_native_cases=0,
    native_qualification=str(D / "ram-native-v2-qualified.json"), native_qualification_sha256=sha(D / "ram-native-v2-qualified.json"),
    native_guard=str(D / "guard-ram-native-v2.json"), native_guard_sha256=sha(D / "guard-ram-native-v2.json"),
    owned_native_processes_physically_reaped=len(g["processes"]), owned_ram_products_retired=True,
    frontend_checks=16, fresh_frontend_checks=16, frontend_wasm_checks=8, native_fixture_wasm_checks=28,
    frontend_qualification=str(D / "analysis-compile-qualified-v2.json"),
    frontend_qualification_sha256=sha(D / "analysis-compile-qualified-v2.json"),
    other_os_native_completed_this_round=False, other_os_native_cases_pending=72,
    source_scope="38 frozen delta inputs over authenticated unchanged R34 dependencies; concurrent checkout not claimed",
    original_native_storage_policy_unchanged=True, persistent_four_os_suite_admitted=False,
    evidence_archive=eq["archive"], evidence_archive_sha256=eq["archive_sha256"],
    evidence_members_read_back=eq["member_count"], evidence_qualification=str(D / "r39-final-evidence-qualified.json"),
    evidence_qualification_sha256=sha(D / "r39-final-evidence-qualified.json"),
    evidence_guard=str(D / "guard-final-evidence.json"), evidence_guard_sha256=sha(D / "guard-final-evidence.json"),
    before_source_archive=str(D / "before-source-r39.tar.gz"), before_source_archive_sha256=sha(D / "before-source-r39.tar.gz"),
    old_r33_archives_relocated_to=str(E / "cold-relocated-r39"), old_r33_archive_files=6,
    old_r33_custody_qualification=str(E / "cold-relocated-r39/cold-custody-qualified.json"),
    old_r33_custody_qualification_sha256=sha(E / "cold-relocated-r39/cold-custody-qualified.json"),
    cold_local_release_receipt=str(D / "cold-relocation-receipt.json"),
    cold_local_release_receipt_sha256=sha(D / "cold-relocation-receipt.json"),
    owned_local_allocated_bytes_released=264507392)
with (E / "suite.lock").open("r+") as lease:
    fcntl.flock(lease, fcntl.LOCK_EX | fcntl.LOCK_NB)
    before_bytes = A.read_bytes()
    before = json.loads(before_bytes)
    s = A.stat()
    assert stat.S_ISREG(s.st_mode) and s.st_uid == os.getuid()
    assert before["active_directory"] == str(E)
    if key in before:
        assert before[key] == fields
        print("R39 already registered with the same evidence")
    else:
        after = dict(before)
        after[key] = fields
        temporary = A.with_name("." + A.name + ".r39-" + str(os.getpid()) + ".tmp")
        with temporary.open("x") as f:
            os.fchmod(f.fileno(), stat.S_IMODE(s.st_mode))
            json.dump(after, f, indent=2)
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
        now = A.stat()
        assert (now.st_dev, now.st_ino, now.st_mtime_ns, now.st_size) == (s.st_dev, s.st_ino, s.st_mtime_ns, s.st_size)
        assert A.read_bytes() == before_bytes
        os.replace(temporary, A)
        fd = os.open(A.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
        actual = load(A)
        assert actual[key] == fields and all(actual[k] == v for k, v in before.items())
        receipt = dict(passed=True, appended_key=key, original_fields_preserved=len(before),
            before_sha256=hashlib.sha256(before_bytes).hexdigest(), after_sha256=sha(A),
            evidence_archive_sha256=eq["archive_sha256"], original_storage_policy_unchanged=True)
        with (D / "r39-active-environment-update.json").open("x") as f:
            json.dump(receipt, f, indent=2)
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
        print(json.dumps(receipt))
