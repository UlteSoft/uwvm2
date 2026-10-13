from pathlib import Path
import json, hashlib, tarfile, os, resource

L = Path(__file__).parent
O = L / 'macos/uwvm2'
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
proof_path = L / 'early-local-product-retirement.json'
assert not proof_path.exists()
q = json.loads((O / 'qualified.json').read_text())
receipt = json.loads((O / 'receipt.json').read_text())
cold = json.loads((L / 'uwvm2-macos-local-cold-qualified.json').read_text())
remote = json.loads((L / 'uwvm2-macos-product-retirement.json').read_text())
archive = Path(cold['archive'])
assert receipt['passed'] and len(receipt['rows']) == 6
assert q['passed'] and cold['passed'] and remote['passed']
assert sha(L / 'inputs.json') == receipt['source_manifest_sha256'] == cold['source_manifest_sha256']
assert sha(O / 'qualified.json') == receipt['qualified_sha256']
assert archive.parent == Path('/Users/liyinan/Documents/MacroModel/wasip1-r30-owned-cold-evidence')
assert archive.is_file() and not archive.is_symlink() and archive.stat().st_mode & 0o222 == 0
assert sha(archive) == cold['archive_sha256'] == remote['archive_sha256']
assert cold['payloads'] == remote['payloads']
observed = {}
with tarfile.open(archive, 'r|gz') as t:
    for m in t:
        assert m.isfile() and m.name in cold['payloads'] and m.name not in observed
        with t.extractfile(m) as f:
            observed[m.name] = hashlib.file_digest(f, 'sha256').hexdigest()
assert observed == cold['payloads']
assert observed['receipt.json'] == sha(O / 'receipt.json')
retired = []
for name, key in [('fixture.exe', 'binary_sha256'), ('environment-group.exe', 'group_binary_sha256')]:
    p = O / name
    assert p.is_file() and not p.is_symlink()
    assert sha(p) == q[key] == receipt[key] == observed[name]
    retired.append(dict(path=str(p), bytes=p.stat().st_size, sha256=sha(p), archive_member=name))
free = lambda: os.statvfs(O).f_bavail * os.statvfs(O).f_frsize
before = free()
upper = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss + (128 << 20)
assert upper < 2 << 30
proof = dict(passed=False, source_manifest_sha256=sha(L / 'inputs.json'),
             archive=str(archive), archive_sha256=sha(archive),
             all_payloads_read_back=True, native_macos_tests_already_passed=6,
             native_admission_bytes=4 << 30, free_before_bytes=before,
             retirement_reason='qualified duplicate products; preserve native admission threshold',
             retired=retired, memory_upper_bytes=upper, limit_bytes=2 << 30)
proof_path.write_text(json.dumps(proof, indent=2) + '\n')
for row in retired:
    p = Path(row['path'])
    assert p.stat().st_size == row['bytes'] and sha(p) == row['sha256']
    p.unlink()
proof.update(passed=True, free_after_bytes=free(), retired_bytes=sum(r['bytes'] for r in retired))
proof_path.write_text(json.dumps(proof, indent=2) + '\n')
print('qualified local duplicate products retired', proof['retired_bytes'], 'free', proof['free_after_bytes'], flush=True)
assert proof['free_after_bytes'] > proof['native_admission_bytes']
