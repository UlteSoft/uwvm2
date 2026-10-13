"""Seal owned R39 evidence; execute only through the original-cgroup guard."""
from pathlib import Path
import hashlib, json, os, stat, tarfile

D = Path(__file__).parent
E = D.parent.parent
C = E / "cold-relocated-r39"
sha = lambda p: hashlib.file_digest(Path(p).open("rb"), "sha256").hexdigest()
load = lambda name: json.loads((D / name).read_text())
q = load("ram-native-v2-qualified.json")
g = load("guard-ram-native-v2.json")
a = load("analysis-compile-qualified-v2.json")
ag = load("guard-analysis-compile-v2.json")
assert q["passed"] and q["native_runs"] == q["fresh_native_runs"] == 24
assert q["reused_unchanged_native_runs"] == 0 and not q["full_world_publication"]
assert sha(D / "ram-native-v2-results.json") == q["results_sha256"]
assert sha(D / "ram_native_regression-v2.py") == q["controller_sha256"]
assert sha(D / "guard_ram_native-v2.py") == g["guard_sha256"]
assert g["passed"] and g["ram_disposable_products_retired"] and g["parent_events_unchanged"]
assert all(p["pidfd_retired"] for p in g["processes"])
assert a["passed"] and len(a["rows"]) == 16
assert ag["passed"] and ag["parent_events_unchanged"] and all(p["pidfd_retired"] for p in ag["processes"])
assert sha(D / "analysis_compile_matrix-v2.py") == a["controller_sha256"]
assert sha(D / "guard_analysis_compile-v2.py") == ag["guard_sha256"]
assert Path("/proc/self/cgroup").read_text() == g["cgroup"] == ag["cgroup"]
manifest = load("syntax-overlay-v2.json")
assert len(manifest) == 38 and sha(D / "syntax-overlay-v2.json") == q["source_manifest_sha256"] == a["source_manifest_sha256"]

def read_members(path, expected):
    seen = set()
    with tarfile.open(path, "r:gz") as archive:
        for member in archive:
            assert member.isfile() and member.name in expected and member.name not in seen
            row = expected[member.name]
            assert member.size == row["size"]
            assert hashlib.file_digest(archive.extractfile(member), "sha256").hexdigest() == row["sha256"]
            seen.add(member.name)
    assert seen == set(expected)
    return len(seen)

source_members = {k: {"sha256": v, "size": (D / "syntax-overlay-v2" / k).stat().st_size} for k, v in manifest.items()}
read_members(D / "syntax-overlay-v2.tar.gz", source_members)
before = load("before-source-r39-qualified.json")
assert before["passed"] and sha(D / "before-source-r39.tar.gz") == before["archive_sha256"]
assert read_members(D / "before-source-r39.tar.gz", before["members"]) == before["member_count"] == 19

files = {"round/" + p.name: p for p in D.iterdir() if p.is_file() and not p.is_symlink() and
         not p.name.startswith(("r39-final-", "guard-final-evidence"))}
for name in ("cold-custody-qualified.json", "custody-intent.json"):
    files["cold-custody/" + name] = C / name
rows = {}
identities = {}
for name, path in sorted(files.items()):
    s = path.stat()
    assert stat.S_ISREG(s.st_mode) and s.st_uid == os.getuid()
    identities[name] = (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns)
    rows[name] = {"size": s.st_size, "sha256": sha(path)}
out = C / "publication-records-r39-evidence.tar.gz"
assert not out.exists()
with out.open("xb") as f:
    with tarfile.open(fileobj=f, mode="w:gz", compresslevel=6) as archive:
        for name, path in sorted(files.items()):
            archive.add(path, arcname=name, recursive=False)
    f.flush()
    os.fsync(f.fileno())
assert out.stat().st_size < 8 << 20
for name, path in files.items():
    s = path.stat()
    assert (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns) == identities[name]
    assert sha(path) == rows[name]["sha256"]
count = read_members(out, rows)
os.chmod(out, 0o444)
qualification = dict(passed=True, archive=str(out), archive_sha256=sha(out), archive_bytes=out.stat().st_size,
    member_count=count, members=rows, full_members_read_back=True, source_manifest_sha256=q["source_manifest_sha256"],
    linux_native_cases=24, fresh_native_cases=24, reused_unchanged_native_cases=0,
    before_source_members=19, frontend_checks=16, world_publication=False,
    restored_worker_startup=False, guest_replay=False, cgroup=g["cgroup"])
with (D / "r39-final-evidence-qualified.json").open("x") as f:
    json.dump(qualification, f, indent=2)
    f.write("\n")
    f.flush()
    os.fsync(f.fileno())
print("R39 owned evidence fully read back", count, out.stat().st_size, flush=True)
