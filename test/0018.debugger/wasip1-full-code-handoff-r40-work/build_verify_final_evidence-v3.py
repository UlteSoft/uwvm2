"""Seal owned R40 evidence; only the original-cgroup guard executes this."""
from pathlib import Path
import hashlib,json,os,stat,tarfile

D=Path(__file__).parent;E=D.parent.parent;C=E/"cold-relocated-r40"
def sha(p):
    with Path(p).open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()
load=lambda n:json.loads((D/n).read_text())
# Report verification and construction use this SAME bounded cgroup process.
import runpy
runpy.run_path(str(D/"write_report-v3.py"),run_name="__main__")
q=load("ram-native-v2-qualified.json");g=load("guard-ram-native-v2.json")
v=load("generation-native-v3-qualified.json");vg=load("guard-generation-native-v3.json")
a=load("analysis-compile-qualified-v1.json");ag=load("guard-analysis-compile-v1.json")
report=load("wasip1_full_code_handoff_r40_test_report.json")
x=load("active-generation-native-v1-qualified.json");xg=load("guard-active-generation-native-v1.json")
final_manifest=load("syntax-overlay-active-generation-v1.json");aeq=load("active-generation-v1-source-equivalence.json")
base=load("syntax-overlay-v1.json");manifest=load("syntax-overlay-generation-v2.json")
eq=load("generation-v2-source-equivalence.json")
assert q["passed"] and q["native_runs"]==q["fresh_native_runs"]==24 and q["reused_unchanged_native_runs"]==0
assert v["passed"] and v["fresh_native_cases"]==v["genuine_generation_2_cases"]==4 and v["frontend_checks"]==2
for proof,controller,guard in ((q,"ram_native_regression-v2.py",g),(v,"generation_native_regression-v3.py",vg)):
    assert sha(D/controller)==proof["controller_sha256"]
    assert guard["passed"] and guard["ram_disposable_products_retired"] and guard["parent_events_unchanged"]
    assert all(p["pidfd_retired"] for p in guard["processes"])
assert sha(D/"ram-native-v2-results.json")==q["results_sha256"]
assert sha(D/"generation-native-v3-results.json")==v["results_sha256"]
assert sha(D/"guard_ram_native-v2.py")==g["guard_sha256"]
assert sha(D/"guard_generation_native-v3.py")==vg["guard_sha256"]
assert a["passed"] and len(a["rows"])==16 and ag["passed"] and ag["parent_events_unchanged"]
assert all(p["pidfd_retired"] for p in ag["processes"])
assert sha(D/"analysis_compile_matrix-v1.py")==a["controller_sha256"]
assert sha(D/"guard_analysis_compile-v1.py")==ag["guard_sha256"]
assert Path("/proc/self/cgroup").read_text()==g["cgroup"]==vg["cgroup"]==ag["cgroup"]
assert len(base)==42 and len(manifest)==44 and eq["passed"] and eq["unchanged_inputs"]==base
assert sha(D/"syntax-overlay-v1.json")==q["source_manifest_sha256"]==a["source_manifest_sha256"]==eq["old_manifest_sha256"]
assert sha(D/"syntax-overlay-generation-v2.json")==v["source_manifest_sha256"]==eq["new_manifest_sha256"]
assert all(manifest[k]==h for k,h in base.items()) and eq["added_inputs"]=={k:h for k,h in manifest.items() if k not in base}
assert report["fresh_linux_native_cases"]==32 and report["fresh_frontend_checks"]==20 and report["genuine_generation_2_native_cases"]==8
assert x["passed"] and x["fresh_native_cases"]==x["genuine_active_generation_2_cases"]==4 and x["frontend_checks"]==2
assert xg["passed"] and xg["parent_events_unchanged"] and xg["ram_disposable_products_retired"] and all(p["pidfd_retired"] for p in xg["processes"])
assert sha(D/"active-generation-native-v1-results.json")==x["results_sha256"] and sha(D/"active_generation_native-v1.py")==x["controller_sha256"]
assert sha(D/"guard_active_generation_native-v1.py")==xg["guard_sha256"] and Path("/proc/self/cgroup").read_text()==xg["cgroup"]
assert len(final_manifest)==50 and aeq["passed"] and aeq["unchanged_inputs"]==base and aeq["previous_inputs_unchanged"]==manifest
assert all(final_manifest[k]==h for k,h in manifest.items())
assert sha(D/"syntax-overlay-active-generation-v1.json")==aeq["new_manifest_sha256"]==x["source_manifest_sha256"]==report["source_manifest_sha256"]
assert not report["world_publication"] and not report["restored_worker_startup"] and not report["guest_replay"]

def read_members(path,expected):
    seen=set()
    with tarfile.open(path,"r:gz") as archive:
        for member in archive:
            assert member.isfile() and member.name in expected and member.name not in seen
            row=expected[member.name];assert member.size==row["size"]
            assert hashlib.file_digest(archive.extractfile(member),"sha256").hexdigest()==row["sha256"]
            seen.add(member.name)
    assert seen==set(expected)
    return len(seen)

for name,entries,root in (("syntax-overlay-v1",base,D/"syntax-overlay-v1"),
                         ("syntax-overlay-generation-v2",manifest,D/"syntax-overlay-generation-v3"),
                         ("syntax-overlay-active-generation-v1",final_manifest,D/"syntax-overlay-active-generation-v1")):
    members={}
    for k,h in entries.items():
        path=root/k;assert sha(path)==h
        members[k]=dict(size=path.stat().st_size,sha256=h)
    assert read_members(D/(name+".tar.gz"),members)==len(entries)
before=load("before-source-r40-qualified.json")
assert before["passed"] and sha(D/"before-source-r40.tar.gz")==before["archive_sha256"]
assert read_members(D/"before-source-r40.tar.gz",before["members"])==before["member_count"]==27
files={"round/"+p.name:p for p in D.iterdir() if p.is_file() and not p.is_symlink() and
       not p.name.startswith(("r40-final-","guard-final-evidence"))}
rows={};identities={}
for name,path in sorted(files.items()):
    s=path.stat();assert stat.S_ISREG(s.st_mode) and s.st_uid==os.getuid()
    identities[name]=(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns)
    rows[name]=dict(size=s.st_size,sha256=sha(path))
C.mkdir(mode=0o700)
out=C/"full-code-handoff-r40-evidence.tar.gz"
with out.open("xb") as f:
    with tarfile.open(fileobj=f,mode="w:gz",compresslevel=6) as archive:
        for name,path in sorted(files.items()):archive.add(path,arcname=name,recursive=False)
    f.flush();os.fsync(f.fileno())
assert out.stat().st_size<8<<20
for name,path in files.items():
    s=path.stat();assert (s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns)==identities[name]
    assert sha(path)==rows[name]["sha256"]
count=read_members(out,rows);os.chmod(out,0o444)
qualification=dict(passed=True,archive=str(out),archive_sha256=sha(out),archive_bytes=out.stat().st_size,
    member_count=count,members=rows,full_members_read_back=True,source_manifest_sha256=x["source_manifest_sha256"],
    base_source_manifest_sha256=q["source_manifest_sha256"],source_files=50,
    linux_native_cases=32,fresh_native_cases=32,reused_unchanged_native_cases=0,genuine_generation_2_cases=8,active_generation_2_nested_cases=4,
    before_source_members=27,frontend_checks=20,world_publication=False,restored_worker_startup=False,guest_replay=False,
    cgroup=g["cgroup"])
with (D/"r40-final-evidence-qualified.json").open("x") as f:
    json.dump(qualification,f,indent=2);f.write("\n");f.flush();os.fsync(f.fileno())
print("R40 owned evidence fully read back",count,out.stat().st_size,flush=True)
