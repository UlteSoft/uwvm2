from pathlib import Path
import os, json, hashlib, sys
import qualified_archive as qa

D = Path(__file__).parent
E = D.parent.parent
assert sys.argv[1:] == ['joint-report-r34', 'final', 'all']
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
M = D / 'repaired-inputs-v6.json'
manifest = json.loads(M.read_text())
source_sha = sha(M)
assert source_sha == 'daa1fe8f66772e8c6e310b87c36477bac07dbab34aa7c5c9bcc455be72ddab3c'
assert len(manifest) == 8789
assert all(sha(D / 'repaired-inputs-v6' / n) == h for n, h in manifest.items())
groups = []
guard_receipts = {}

def guard(phase, *args):
    p = E / ('guard-' + '-'.join((phase, *args)) + '.json')
    g = json.loads(p.read_text())
    assert g['passed'] and g['actual_root_exit'] == 0
    assert all(r['pidfd_retired'] for r in g['retirement'])
    assert g['boot_id'] == 'c8d3550f-3a19-40d7-8507-a76c2045ace5'
    assert g['anchor_pid'] == 11166 and g['anchor_birth'] == 22661
    assert g['limits']['memory.max'] == '68719476736'
    assert g['limits']['memory.swap.max'] == '0'
    assert g['limits']['cpuset'] == '0,2,4,6,16-31'
    guard_receipts[p.name] = dict(path=str(p), sha256=sha(p), passed=True,
        owned_rss_upper_bytes=g['owned_peak_aggregate_rss_upper_bytes'],
        storage_peak_allocated_bytes=g['storage_peak_allocated_bytes'],
        parent_events_unchanged=g['parent_events_unchanged'],
        owned_process_count=len(g['retirement']),
        foreign_observed_not_owned=len(g['foreign_pids_observed_not_owned']))

for repo in ('uwvm2', 'uwvm2-ros'):
    for platform in ('linux-integrated', 'windows', 'freebsd', 'macos'):
        O = D / 'products' / platform / repo
        retirement = D / (repo + '-' + platform + '-product-retirement.json')
        q = json.loads(retirement.read_text())
        assert q['passed'] and q['all_payloads_read_back']
        assert q['source_manifest_sha256'] == source_sha
        assert sha(q['archive']) == q['archive_sha256']
        observed = {}
        with qa.open_reader(q['archive']) as a:
            for member in a:
                assert member.isfile() and member.name in q['payloads'] and member.name not in observed
                with a.extractfile(member) as f:
                    observed[member.name] = hashlib.file_digest(f, 'sha256').hexdigest()
        assert observed == q['payloads']
        guard('joint-archive-r34', platform, repo)
        if platform == 'linux-integrated':
            guard('joint-linux-r34', repo)
            native_path = O / 'native-execution.json'
            native = json.loads(native_path.read_text())
            assert native['passed'] and len(native['rows']) == 6
            rows = native['rows']
            for r in rows:
                assert r['passed'] and sha(O / (r['fixture'] + '-' + r['policy'] + '.log')) == r['log_sha256']
            assertion_count = native['counted_assertions']
        else:
            guard('joint-cross-r34', platform, repo)
            compiled = json.loads((O / 'qualified.json').read_text())
            assert compiled['passed'] and compiled['source_manifest_sha256'] == source_sha
            if platform in ('windows', 'freebsd'):
                guard('joint-object-cache-retire-r34', platform, repo)
                guard('joint-executable-cache-retire-r34', platform, repo)
                guard('joint-vm-r34', platform, repo)
                candidates = []
                for p in D.glob('vm-' + platform + '-joint-preparation-*/receipt.json'):
                    v = json.loads(p.read_text())
                    if v.get('passed') and v.get('target_source_manifest_sha256') == source_sha and all(t['repo'] == repo for t in v['tests']):
                        candidates.append((p, v))
                assert len(candidates) == 1
                native_path, native = candidates[0]
                assert native['base_before'] == native['base_after']
                assert native['kvm']['enabled'] and native['host_kvm_permissions_unchanged']
                assert sha(native_path.parent / 'serial.log') == native['serial_sha256']
                assert len(native['tests']) == 6 and all(native['test_statuses'].values())
                rows = [dict(fixture=t['fixture'], policy=t['policy'], passed=True,
                    checks=native['test_pass_checks'][t['label']] if t['counted_assertions'] else None,
                    physical_worker_witness=None if t['counted_assertions'] else native['test_pass_checks'][t['label']])
                    for t in native['tests']]
                assertion_count = sum(r['checks'] or 0 for r in rows)
            else:
                native_path = O / 'receipt.json'
                native = json.loads(native_path.read_text())
                assert native['passed'] and len(native['rows']) == 6
                assert native['source_manifest_sha256'] == source_sha
                rows = native['rows']
                assert all(r['passed'] and r['exit'] == 0 and r['aggregate_peak_upper_bytes'] < 2 << 30 for r in rows)
                assertion_count = sum(r['checks'] or 0 for r in rows)
        assert len(rows) == 6 and all(r['passed'] for r in rows)
        groups.append(dict(repo=repo, os='linux' if platform == 'linux-integrated' else platform,
            native_runs=6, counted_wasip1_assertions=assertion_count, rows=rows,
            native_receipt=str(native_path), native_receipt_sha256=sha(native_path),
            archive=str(q['archive']), archive_bytes=q['archive_bytes'], archive_sha256=q['archive_sha256'],
            recovery_receipt=str(retirement), recovery_receipt_sha256=sha(retirement),
            all_payloads_read_back_again=True, recovery_payload_count=len(observed)))

seen = set()
allocated = logical = files = 0
for parent, dirs, names in os.walk(E, followlinks=False):
    dirs[:] = [n for n in dirs if not (Path(parent) / n).is_symlink()]
    for n in names:
        s = (Path(parent) / n).lstat()
        logical += s.st_size
        files += 1
        key = (s.st_dev, s.st_ino)
        if key not in seen:
            seen.add(key)
            allocated += s.st_blocks * 512
v = os.statvfs(E)
policy = json.loads((E / 'storage-policy.json').read_text())
assert allocated < policy['directory_stop_bytes'] and logical < policy['directory_logical_stop_bytes']
assert v.f_bavail * v.f_frsize >= policy['volume_free_reserve_bytes']
result = dict(passed=True, round='R34', source_manifest_sha256=source_sha, source_files=len(manifest),
    native_runs=sum(g['native_runs'] for g in groups),
    counted_wasip1_assertions=sum(g['counted_wasip1_assertions'] for g in groups),
    whole_world_publication_implemented=False, restored_guest_replay_implemented=False,
    groups=groups, guards=guard_receipts, original_storage_policy=policy,
    final_storage=dict(allocated_bytes=allocated, logical_bytes=logical, files=files,
        volume_available_bytes=v.f_bavail * v.f_frsize, volume_available_inodes=v.f_favail),
    failed_and_historical_runs_excluded=True,
    archive_custody='Original bounded SSH Linux task volume; no new local cold allocation')
assert result['native_runs'] == 48 and result['counted_wasip1_assertions'] > 0
(D / 'final-native-matrix-qualified.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(dict(passed=True, native_runs=48, counted_wasip1_assertions=result['counted_wasip1_assertions'], groups=len(groups))), flush=True)
