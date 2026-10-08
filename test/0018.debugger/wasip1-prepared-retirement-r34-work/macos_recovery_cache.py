from pathlib import Path
import hashlib
import json
import sys
import qualified_archive as qa

D = Path(__file__).parent
E = D.parent.parent
R = E / 'rounds/wasip1-cross-jit-20261007-r26'
assert sys.argv[1:] == ['joint-macos-recovery-cache-r34', 'macos', 'uwvm2']
O = D / 'products-v8/macos/uwvm2'
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
failed_guard = E / 'guard-joint-cross-r34-macos-uwvm2.json'
g = json.loads(failed_guard.read_text())
assert not g['passed'] and g['actual_root_exit'] == -9 and all(r['pidfd_retired'] for r in g['retirement'])
assert g['limits']['memory.max'] == '68719476736' and g['limits']['memory.swap.max'] == '0'
assert 'filesystem reserve' in g['error']
saved_guard = O / 'interrupted-cross-guard.json'
assert not saved_guard.exists()
saved_guard.write_bytes(failed_guard.read_bytes())
M = D / 'repaired-inputs-v8.json'
assert sha(M) == '171e9f277d349278c53669b49d60d5727f73575fa9847d14dd40a5741924d73f'
results = json.loads((O / 'results.json').read_text())
assert all(r['passed'] and r['exit'] == 0 for r in results)
members = {}
rows = []
for label in ('runtime', 'host-api'):
    q = json.loads((O / (label + '-qualified.json')).read_text())
    row = next(r for r in results if r['name'] == label + '-compile')
    p = O / (label + '.o')
    assert q['argv'] == row['argv'] and sha(O / (row['name'] + '.log')) == row['log_sha256']
    assert sha(q['argv'][0]) == q['compiler_sha256']
    assert all(sha(k) == h for k, h in q['dependencies'].items())
    assert sha(p) == q['object_sha256']
    members[p.name] = q['object_sha256']
    rows.append(dict(path=str(p), archive_member=p.name, bytes=p.stat().st_size,
        mode=p.stat().st_mode & 0o777, sha256=q['object_sha256'], qualification_sha256=sha(O / (label + '-qualified.json'))))
A = D / 'uwvm2-macos-v8-interrupted-compile-object-cache.tar.zst'
with qa.open_writer(A) as a:
    for row in rows:
        a.add(row['path'], arcname=row['archive_member'], recursive=False)
seen = {}
with qa.open_reader(A) as a:
    for m in a:
        assert m.isfile() and m.name in members and m.name not in seen
        with a.extractfile(m) as f:
            seen[m.name] = hashlib.file_digest(f, 'sha256').hexdigest()
assert seen == members
sdk_receipt = R / 'llvm-macos/library-retirement.json'
sdk = json.loads(sdk_receipt.read_text())
assert sdk['passed'] and sdk['all_payloads_read_back'] and sha(sdk['archive']) == sdk['archive_sha256']
import tarfile
seen_sdk = {}
sdk_rows = []
with tarfile.open(sdk['archive'], 'r|gz') as archive:
    for m in archive:
        assert m.isfile() and m.name in sdk['payload_sha256'] and m.name not in seen_sdk
        with archive.extractfile(m) as f:
            seen_sdk[m.name] = hashlib.file_digest(f, 'sha256').hexdigest()
        p = R / 'llvm-macos' / m.name
        assert p.is_file() and not p.is_symlink() and sha(p) == seen_sdk[m.name]
        sdk_rows.append(dict(path=str(p), bytes=p.stat().st_size, sha256=seen_sdk[m.name]))
assert seen_sdk == sdk['payload_sha256']
q = dict(passed=True, all_payloads_read_back=True, source_manifest_sha256=sha(M),
    archive=str(A), archive_sha256=sha(A), payloads=members, retired_raw=rows,
    sdk_original_receipt=str(sdk_receipt), sdk_original_receipt_sha256=sha(sdk_receipt),
    sdk_archive_sha256=sdk['archive_sha256'], sdk_restored_then_retired=sdk_rows,
    failed_cross_guard=str(saved_guard), failed_cross_guard_sha256=sha(saved_guard),
    native_execution_claimed=False, original_resource_limits_unchanged=True)
(O / 'interrupted-object-cache-retirement.json').write_text(json.dumps(q, indent=2) + '\n')
for row in rows + sdk_rows:
    p = Path(row['path'])
    assert sha(p) == row['sha256']
    p.unlink()
print('Interrupted Mac compile objects and exact restored SDK fully read back before raw retirement',
    sum(r['bytes'] for r in rows + sdk_rows), flush=True)
