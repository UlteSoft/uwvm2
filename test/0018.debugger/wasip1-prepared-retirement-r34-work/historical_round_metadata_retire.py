from pathlib import Path
import sys, json, hashlib, re
import qualified_archive as qa

D = Path(__file__).parent
E = D.parent.parent
assert sys.argv[1:] == ['joint-round-metadata-retire-r34', 'final', 'all']
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
roots = [r / 'products' for r in sorted((E / 'rounds').iterdir())
    if re.fullmatch(r'wasip1-[a-z0-9-]+-r(?:28|29|30|31|32|33)', r.name)]
members = {}
kept = {}
for root in roots:
    assert root.is_dir() and not root.is_symlink()
    for p in root.rglob('*'):
        assert not p.is_symlink()
        if not p.is_file():
            continue
        assert p.stat().st_size < 8 << 20, ('historical metadata only', p)
        n = str(p.relative_to(E / 'rounds'))
        r = dict(sha256=sha(p), size=p.stat().st_size, mode=p.stat().st_mode & 0o777)
        members[n] = r
        if root.parent.name.endswith('-r33') and p.name == 'runtime-qualified.json' and 'linux-integrated' in p.parts:
            kept[n] = r  # Actual V8 compiler flag provenance still reads these two.
assert len(kept) == 2 and sum(r['size'] for r in members.values()) < 256 << 20
A = D / 'historical-r28-r33-native-product-metadata.tar.zst'
assert not A.exists()
with qa.open_writer(A) as t:
    for n in sorted(members):
        p = E / 'rounds' / n
        assert sha(p) == members[n]['sha256']
        t.add(p, arcname=n, recursive=False)
seen = {}
with qa.open_reader(A) as t:
    for m in t:
        assert m.isfile() and m.name in members and m.name not in seen
        with t.extractfile(m) as f:
            seen[m.name] = dict(sha256=hashlib.file_digest(f, 'sha256').hexdigest(), size=m.size, mode=m.mode & 0o777)
assert seen == members
proof = dict(passed=True, archive=str(A), archive_sha256=sha(A), archive_bytes=A.stat().st_size,
    payloads=members, kept_current_provenance=kept, all_payloads_read_back=True,
    native_execution_claimed=False, original_test_limits_unchanged=True,
    all_original_metadata_recoverable=True, readonly_cold_regions_untouched=True)
(D / 'historical-r28-r33-product-metadata-retirement-qualified.json').write_text(json.dumps(proof, indent=2) + '\n')
for n, r in members.items():
    p = E / 'rounds' / n
    assert sha(p) == r['sha256']
    if n not in kept:
        p.unlink()
for root in roots:
    for p in sorted((p for p in root.rglob('*') if p.is_dir()), key=lambda p: len(p.parts), reverse=True):
        if not any(p.iterdir()):
            p.rmdir()
    if not any(root.iterdir()):
        root.rmdir()
assert all(sha(E / 'rounds' / n) == r['sha256'] for n, r in kept.items())
print('Historical metadata fully recoverable before retiring raw copies; actual compiler provenance retained',
    sum(r['size'] for n, r in members.items() if n not in kept), A.stat().st_size, flush=True)
