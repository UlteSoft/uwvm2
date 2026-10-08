"""Append R40 evidence only, preserving every prior and concurrent field."""
from pathlib import Path
import fcntl,hashlib,json,os,stat

D=Path(__file__).parent;E=D.parent.parent;A=E.parent/"wasip1-active-environment.json"
def sha(p):
    with Path(p).open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()
load=lambda p:json.loads(Path(p).read_text())
key="r40_checkpoint_full_code_handoff"
r=load(D/"wasip1_full_code_handoff_r40_test_report.json")
q=load(D/"ram-native-v1-qualified.json");g=load(D/"guard-ram-native-v1.json")
v=load(D/"generation-native-v2-qualified.json");vg=load(D/"guard-generation-native-v2.json")
eq=load(D/"r40-final-evidence-qualified.json");eg=load(D/"guard-final-evidence.json")
assert q["passed"] and q["fresh_native_runs"]==q["native_runs"]==24 and q["reused_unchanged_native_runs"]==0
assert v["passed"] and v["fresh_native_cases"]==v["genuine_generation_2_cases"]==4
assert r["fresh_linux_native_cases"]==eq["fresh_native_cases"]==28 and r["fresh_frontend_checks"]==eq["frontend_checks"]==18
for guard in (g,vg,eg):
    assert guard["passed"] and guard["parent_events_unchanged"] and all(x["pidfd_retired"] for x in guard["processes"])
assert g["ram_disposable_products_retired"] and vg["ram_disposable_products_retired"]
assert eq["passed"] and eq["full_members_read_back"] and eq["member_count"]==len(eq["members"])
assert sha(eq["archive"])==eq["archive_sha256"]
assert sha(D/"syntax-overlay-generation-v2.json")==eq["source_manifest_sha256"]==v["source_manifest_sha256"]==r["source_manifest_sha256"]
assert not r["world_publication"] and not r["restored_worker_startup"] and not r["guest_replay"] and not r["live_native_seal_issued"]
fields=dict(source_manifest=str(D/"syntax-overlay-generation-v2.json"),source_manifest_sha256=r["source_manifest_sha256"],
    source_files=44,unchanged_base_source_files=42,source_equivalence=str(D/"generation-v2-source-equivalence.json"),
    source_equivalence_sha256=sha(D/"generation-v2-source-equivalence.json"),
    actual_engine_context_owners_transferred_privately=True,typed_target_allocation_preserved=True,
    safe_point_bitmap_allocations_preserved=True,pending_range_listener_detached_before_owner_move=True,
    final_full_code_owners_rechecked_after_authentic_drain_and_physical_join=True,
    generation_greater_than_one_native_handoff_tested=True,genuine_generation_2_cases=4,
    original_committed_callback_after_candidate_discard=17,
    world_publication=False,restored_worker_startup=False,guest_replay=False,live_native_seal_issued=False,
    source_seal_issued=False,initializer_serial_issued=False,asm_vm_or_host_context_granted=False,
    wasm_and_wasip1_same_actual_stop_required=True,linux_native_cases=28,fresh_native_cases=28,reused_unchanged_native_cases=0,
    native_qualification=str(D/"ram-native-v1-qualified.json"),native_qualification_sha256=sha(D/"ram-native-v1-qualified.json"),
    native_guard=str(D/"guard-ram-native-v1.json"),native_guard_sha256=sha(D/"guard-ram-native-v1.json"),
    generation_qualification=str(D/"generation-native-v2-qualified.json"),generation_qualification_sha256=sha(D/"generation-native-v2-qualified.json"),
    generation_guard=str(D/"guard-generation-native-v2.json"),generation_guard_sha256=sha(D/"guard-generation-native-v2.json"),
    owned_native_processes_physically_reaped=r["owned_native_processes_reaped"],owned_ram_products_retired=True,
    frontend_checks=18,fresh_frontend_checks=18,frontend_wasm_checks=8,native_fixture_wasm_checks=32,
    frontend_qualification=str(D/"analysis-compile-qualified-v1.json"),frontend_qualification_sha256=sha(D/"analysis-compile-qualified-v1.json"),
    other_os_native_completed_this_round=False,other_os_base_native_cases_pending=72,
    other_os_generation_2_native_cases_pending=12,
    source_scope=r["source_scope"],original_native_storage_policy_unchanged=True,persistent_four_os_suite_admitted=False,
    report=str(D/"wasip1_full_code_handoff_r40_test_report.json"),report_sha256=sha(D/"wasip1_full_code_handoff_r40_test_report.json"),
    evidence_archive=eq["archive"],evidence_archive_sha256=eq["archive_sha256"],evidence_members_read_back=eq["member_count"],
    evidence_qualification=str(D/"r40-final-evidence-qualified.json"),evidence_qualification_sha256=sha(D/"r40-final-evidence-qualified.json"),
    evidence_guard=str(D/"guard-final-evidence.json"),evidence_guard_sha256=sha(D/"guard-final-evidence.json"),
    before_source_archive=str(D/"before-source-r40.tar.gz"),before_source_archive_sha256=sha(D/"before-source-r40.tar.gz"))

with (E / "suite.lock").open("r+") as lease:
    fcntl.flock(lease, fcntl.LOCK_EX | fcntl.LOCK_NB)
    before_bytes = A.read_bytes()
    before = json.loads(before_bytes)
    s = A.stat()
    assert stat.S_ISREG(s.st_mode) and s.st_uid == os.getuid()
    assert before["active_directory"] == str(E)
    if key in before:
        assert before[key] == fields
        print("R40 already registered with the same evidence")
    else:
        after = dict(before)
        after[key] = fields
        temporary = A.with_name("." + A.name + ".r40-" + str(os.getpid()) + ".tmp")
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
        with (D / "r40-active-environment-update.json").open("x") as f:
            json.dump(receipt, f, indent=2)
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
        print(json.dumps(receipt))
