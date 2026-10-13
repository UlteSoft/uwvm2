from pathlib import Path
import os, sys, json, hashlib, re, stat

D = Path(__file__).parent
E = D.parent.parent
assert sys.argv[1:] == ['joint-readonly-source-deduplicate-r34', 'final', 'all']
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
roots = [(D / 'repaired-inputs-v7', D / 'repaired-inputs-v7.json')]
for r in sorted((E / 'rounds').iterdir()):
    if not re.fullmatch(r'wasip1-[a-z0-9-]+-r(?:28|29|30|31|32|33)', r.name):
        continue
    for manifest in sorted(r.glob('*inputs*.json')):
        root = manifest.with_suffix('')
        if not root.is_dir() or root.is_symlink():
            continue
        m = json.loads(manifest.read_text())
        if isinstance(m, dict) and m and all(isinstance(n, str) and isinstance(h, str)
                and re.fullmatch('[0-9a-f]{64}', h) for n, h in m.items()):
            roots.append((root, manifest))
bindings = []
excluded_partial_namespaces = []
for root, manifest in roots:
    m = json.loads(manifest.read_text())
    assert len(m) <= 20000
    missing = [n for n in m if not (root / n).is_file() or (root / n).is_symlink()]
    if missing:
        assert root != D / 'repaired-inputs-v7'
        excluded_partial_namespaces.append(dict(root=str(root), manifest=str(manifest),
            missing_members=len(missing), namespace_untouched=True))
        continue
    for n, h in m.items():
        p = root / n
        assert p.is_file() and not p.is_symlink() and p.resolve().is_relative_to(root.resolve())
        assert p.stat().st_uid == 1000 and p.stat().st_dev == E.stat().st_dev and sha(p) == h
    bindings.append(dict(root=str(root), manifest=str(manifest), manifest_sha256=sha(manifest), members=m))
canonical = {}
changes = []
for binding in bindings:
    root = Path(binding['root'])
    for n, h in binding['members'].items():
        p = root / n
        info = p.stat()
        mode = stat.S_IMODE(info.st_mode)
        if mode & 0o222:
            continue  # Only existing immutable leaves are eligible.
        key = (h, mode)
        if key not in canonical:
            canonical[key] = p
            continue
        original = canonical[key]
        if os.path.samefile(p, original):
            continue
        assert sha(p) == sha(original) == h and stat.S_IMODE(original.stat().st_mode) == mode
        temp = p.with_name(p.name + '.r34-owned-dedup')
        assert not temp.exists()
        parent_mode = stat.S_IMODE(p.parent.stat().st_mode)
        try:
            if not parent_mode & 0o200:
                p.parent.chmod(parent_mode | 0o200)
            os.link(original, temp)
            os.replace(temp, p)
        finally:
            p.parent.chmod(parent_mode)
        assert sha(p) == h and os.path.samefile(p, original) and stat.S_IMODE(p.stat().st_mode) == mode
        changes.append(dict(path=str(p), canonical_path=str(original), sha256=h,
            mode=mode, original_inode=info.st_ino, original_link_count=info.st_nlink,
            original_allocated_bytes=info.st_blocks * 512, original_mtime_ns=info.st_mtime_ns))
for binding in bindings:
    assert sha(binding['manifest']) == binding['manifest_sha256']
    assert all(sha(Path(binding['root']) / n) == h for n, h in binding['members'].items())
proof = dict(passed=True, all_manifests_and_source_bytes_unchanged=True,
    all_original_paths_retained=True, readonly_leaf_modes_unchanged=True,
    directory_modes_restored=True, local_and_remote_cold_regions_untouched=True,
    original_limits_unchanged=True, native_execution_claimed=False, changes=changes,
    bindings=[{k: v for k, v in b.items() if k != 'members'} for b in bindings],
    excluded_partial_namespaces=excluded_partial_namespaces)
(D / 'readonly-historical-source-deduplication-qualified.json').write_text(json.dumps(proof, indent=2) + '\n')
print('Existing readonly, hash-identical historical source leaves share storage; all paths and manifests retained',
    len(changes), sum(r['original_allocated_bytes'] for r in changes if r['original_link_count'] == 1), flush=True)
