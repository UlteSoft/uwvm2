from pathlib import Path
import os, json, hashlib, sys
import qualified_archive as qa

D = Path(__file__).parent
E = D.parent.parent
assert sys.argv[1:] == ['joint-report-r34', 'final', 'all']
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
M = D / 'repaired-inputs-v8.json'
manifest = json.loads(M.read_text())
source_sha = sha(M)
assert source_sha == '171e9f277d349278c53669b49d60d5727f73575fa9847d14dd40a5741924d73f'
assert len(manifest) == 8791
assert all(sha(D / 'repaired-inputs-v8' / n) == h for n, h in manifest.items())
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
        O = D / 'products-v8' / platform / repo
        retirement = D / (repo + '-' + platform + '-v8-product-retirement.json')
        q = json.loads(retirement.read_text())
        assert q['passed'] and q['all_payloads_read_back']
        assert q['source_manifest_sha256'] == source_sha
        recovery_archive=q['archive'];recovery_hash=q['archive_sha256'];recovery_bytes=q['archive_bytes'];prefix=None
        if not Path(recovery_archive).exists():
            current_consolidation=D/('v8-'+platform+'-products-consolidation-qualified.json')
            assert current_consolidation.is_file()
            c=json.loads(current_consolidation.read_text())
            assert c['passed'] and c['all_payloads_read_back']
            assert c['source_manifest_sha256']==source_sha
            choices=[i for i in c['inputs'] if i['receipt_sha256']==sha(retirement) and i['payloads']==q['payloads']]
            assert len(choices)==1
            assert choices[0]['label']==repo
            guard('joint-native-v8-products-consolidate-r34', platform, 'all')
            prefix=choices[0]['label']+'/'
            recovery_archive=c['archive'];recovery_hash=c['archive_sha256'];recovery_bytes=c['archive_bytes']
        assert sha(recovery_archive) == recovery_hash
        observed = {}
        complete={}
        with qa.open_reader(recovery_archive) as a:
            for member in a:
                assert member.isfile() and member.name not in complete
                with a.extractfile(member) as f:
                    h=hashlib.file_digest(f, 'sha256').hexdigest()
                complete[member.name]=h
                if prefix is None:observed[member.name]=h
                elif member.name.startswith(prefix):observed[member.name[len(prefix):]]=h
        if prefix is not None:assert complete==c['payloads']
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
            archive=str(recovery_archive), archive_bytes=recovery_bytes, archive_sha256=recovery_hash,
            archive_member_prefix=prefix, original_container_sha256=q['archive_sha256'],
            recovery_receipt=str(retirement), recovery_receipt_sha256=sha(retirement),
            all_payloads_read_back_again=True, recovery_payload_count=len(observed)))

historical_recovery = []
for historical_platform, historical_phase in (('macos', 'joint-historical-r29-products-consolidate-r34'),
    ('windows', 'joint-historical-r29-windows-products-consolidate-r34'),
    ('freebsd', 'joint-historical-r29-freebsd-products-consolidate-r34')):
    historical = D / ('historical-r29-' + historical_platform + '-products-consolidation-qualified.json')
    if not historical.exists():
        continue
    h = json.loads(historical.read_text())
    assert h['passed'] and h['all_payloads_read_back'] and h['all_member_sizes_modes_read_back']
    assert sha(h['archive']) == h['archive_sha256']
    assert all(sha(i['receipt']) == i['receipt_sha256'] for i in h['inputs'])
    actual = {}
    with qa.open_reader(h['archive']) as a:
        for member in a:
            assert member.isfile() and member.name not in actual
            assert h['member_metadata'][member.name] == dict(size=member.size, mode=member.mode)
            with a.extractfile(member) as f:
                actual[member.name] = hashlib.file_digest(f, 'sha256').hexdigest()
    assert actual == h['payloads']
    guard(historical_phase, historical_platform, 'all')
    historical_recovery.append(dict(receipt=str(historical), receipt_sha256=sha(historical),
        archive=h['archive'], archive_sha256=h['archive_sha256'], payload_count=len(actual),
        all_payloads_sizes_modes_read_back_again=True, historical_runs_not_counted=True))

transport_custody = None
transport = D / 'local-historical-inputs-custody-qualified.json'
if transport.exists():
    h = json.loads(transport.read_text())
    assert h['passed'] and h['exact_original_compressed_bytes_preserved']
    import tarfile
    for row in h['rows']:
        p = Path(row['remote_path'])
        assert p.stat().st_size == row['bytes'] and p.stat().st_mode & 0o777 == row['mode'] == 0o444
        assert sha(p) == row['sha256']
        actual = {}
        with tarfile.open(p, 'r|gz') as archive:
            for member in archive:
                assert member.isfile() or member.isdir()
                if not member.isfile():
                    continue
                assert member.name not in actual
                with archive.extractfile(member) as f:
                    actual[member.name] = hashlib.file_digest(f, 'sha256').hexdigest()
        assert actual == row['payloads']
    guard('joint-local-inputs-custody-r34', 'linux', 'all')
    transport_custody = dict(receipt=str(transport), receipt_sha256=sha(transport),
        archive_bytes=sum(r['bytes'] for r in h['rows']), all_payloads_read_back_again=True,
        original_compressed_bytes_and_readonly_modes_preserved=True)

recovery_maintenance = {}
legacy_custody = D / 'local-legacy-metadata-custody-qualified.json'
if legacy_custody.exists():
    h = json.loads(legacy_custody.read_text())
    assert h['passed'] and h['all_payloads_sizes_modes_read_back']
    assert sha(h['archive']) == h['archive_sha256']
    expected = {r['member']: r for r in h['rows']}
    actual = {}
    import tarfile
    with tarfile.open(h['archive'], 'r|gz') as archive:
        for member in archive:
            assert member.isfile() and member.name in expected and member.name not in actual
            row = expected[member.name]
            assert member.size == row['bytes'] and member.mode == row['mode']
            with archive.extractfile(member) as f:
                actual[member.name] = hashlib.file_digest(f, 'sha256').hexdigest()
            assert actual[member.name] == row['sha256']
    assert set(actual) == set(expected)
    guard('joint-local-metadata-custody-r34', 'linux', 'all')
    recovery_maintenance['historical_local_metadata'] = dict(receipt=str(legacy_custody),
        receipt_sha256=sha(legacy_custody), archive_sha256=h['archive_sha256'],
        all_payloads_sizes_modes_read_back_again=True, native_execution_claimed=False)
trim = D / 'trim-own-ext4-qualified.json'
if trim.exists():
    t = json.loads(trim.read_text())
    assert t['passed'] and t['all_existing_file_bytes_sizes_modes_owners_unchanged']
    assert t['helper_actual_pidfd_retired'] and t['native_test_disk_and_memory_limits_unchanged']
    assert t['volume_device'] == '7:31' and t['limit_bytes'] == 8 << 30
    assert t['boot_id'] == 'c8d3550f-3a19-40d7-8507-a76c2045ace5'
    assert t['controller_sha256'] == sha(D / 'trim_owned_volume.py')
    recovery_maintenance['own_volume_trim'] = dict(receipt=str(trim), receipt_sha256=sha(trim),
        existing_files=t['existing_files'], inventory_sha256=t['inventory_sha256'],
        image_allocated_bytes_released=t['before_image_allocated_bytes']-t['after_image_allocated_bytes'],
        all_existing_file_bytes_sizes_modes_owners_unchanged=True, original_test_limits_unchanged=True)
recovered = D / 'products-v8/macos/uwvm2/interrupted-object-cache-retirement.json'
if recovered.exists():
    t = json.loads(recovered.read_text())
    assert t['passed'] and t['all_payloads_read_back'] and t['source_manifest_sha256'] == source_sha
    assert sha(t['failed_cross_guard']) == t['failed_cross_guard_sha256']
    assert sha(t['archive']) == t['archive_sha256']
    actual = {}
    with qa.open_reader(t['archive']) as archive:
        for member in archive:
            assert member.isfile() and member.name not in actual
            with archive.extractfile(member) as f:
                actual[member.name] = hashlib.file_digest(f, 'sha256').hexdigest()
    assert actual == t['payloads']
    guard('joint-macos-recovery-cache-r34', 'macos', 'uwvm2')
    recovery_maintenance['interrupted_macos_compile'] = dict(receipt=str(recovered), receipt_sha256=sha(recovered),
        all_compilation_objects_read_back_again=True, failed_attempt_not_counted_as_native=True)

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
    groups=groups, guards=guard_receipts, historical_recovery=historical_recovery,
    original_local_transport_custody=transport_custody, original_storage_policy=policy,
    recovery_maintenance=recovery_maintenance,
    final_storage=dict(allocated_bytes=allocated, logical_bytes=logical, files=files,
        volume_available_bytes=v.f_bavail * v.f_frsize, volume_available_inodes=v.f_favail),
    failed_and_historical_runs_excluded=True,
    archive_custody='Original bounded SSH Linux task volume; no new local cold allocation')
assert result['native_runs'] == 48 and result['counted_wasip1_assertions'] > 0
(D / 'final-native-matrix-qualified.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(dict(passed=True, native_runs=48, counted_wasip1_assertions=result['counted_wasip1_assertions'], groups=len(groups))), flush=True)
