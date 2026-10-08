from pathlib import Path
import sys, json, hashlib
import qualified_archive as qa

D = Path(__file__).parent
E = D.parent.parent
assert sys.argv[1:] == ['joint-vm-v7-failure-retire-r34', 'windows', 'uwvm2']
R = D / 'vm-windows-joint-preparation-e3974982'
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
guard_path = E / 'guard-joint-vm-r34-windows-uwvm2.json'
guard = json.loads(guard_path.read_text())
assert not guard['passed'] and guard['actual_root_exit'] == -9
assert 'PermissionError' in guard['error']
assert all(r['pidfd_retired'] for r in guard['retirement'])
assert any(any(str(R / 'disk.qcow2') in a for a in i['argv']) for i in guard['history'])
native = json.loads((R / 'receipt.json').read_text())
assert not native['passed'] and native['nonce'].startswith('e3974982')
base = Path(native['base_path'])
s = base.stat()
assert dict(dev=s.st_dev, ino=s.st_ino, bytes=s.st_size, mtime=s.st_mtime_ns,
    ctime=s.st_ctime_ns, mode=s.st_mode) == native['base_before']
cache_path = D / 'products-v7/windows/uwvm2/link-executable-cache-retirement.json'
cache = json.loads(cache_path.read_text())
assert cache['passed'] and cache['all_payloads_read_back'] and sha(cache['archive']) == cache['archive_sha256']
seen = {}
with qa.open_reader(cache['archive']) as a:
    for m in a:
        assert m.isfile() and m.name in cache['payloads'] and m.name not in seen
        with a.extractfile(m) as f:
            seen[m.name] = hashlib.file_digest(f, 'sha256').hexdigest()
assert seen == cache['payloads']
rows = []
for name in ('disk.qcow2', 'windows.vars'):
    p = R / name
    assert p.is_file() and not p.is_symlink() and p.stat().st_uid == 1000
    assert p.stat().st_size < 1 << 30
    h = sha(p)
    if name == 'inputs.iso':
        assert h == native['iso_sha256']
    rows.append(dict(path=str(p), bytes=p.stat().st_size, sha256=h,
        purpose='failed owned VM disposable shipping/OS overlay; no guest checkpoint data'))
retained = {str(p.relative_to(R)): sha(p) for p in R.rglob('*')
    if p.is_file() and not p.is_symlink() and p.name not in ('inputs.iso', 'disk.qcow2', 'windows.vars')}
q = dict(passed=True, native_execution_passed=False, guard_sha256=sha(guard_path),
    native_receipt_sha256=sha(R / 'receipt.json'), base_unchanged=True,
    all_owned_processes_pidfd_retired=True, compiled_payloads_read_back=True,
    executable_cache_receipt_sha256=sha(cache_path), retired=rows,
    retained_diagnostics=retained, original_test_limits_unchanged=True)
(D / 'failed-windows-v7-vm-controller-retirement.json').write_text(json.dumps(q, indent=2) + '\n')
for r in rows:
    p = Path(r['path'])
    assert sha(p) == r['sha256']
    p.unlink()
assert all(sha(R / n) == h for n, h in retained.items())
print('Retired only failed owned Windows VM ISO/overlay/vars after death proof; native failure preserved', flush=True)
