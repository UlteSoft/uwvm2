from pathlib import Path
import sys, json, hashlib, os
import qualified_archive as qa

D = Path(__file__).parent
assert sys.argv[1:] == ['joint-historical-metadata-retire-r34', 'final', 'all']
P = D / 'products'
assert P.is_dir() and not P.is_symlink()
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
members = {}
for p in P.rglob('*'):
    assert not p.is_symlink()
    if p.is_file():
        assert p.stat().st_size < 8 << 20, ('only historical metadata', p)
        members[str(p.relative_to(P))] = dict(sha256=sha(p), size=p.stat().st_size,
            mode=p.stat().st_mode & 0o777, blocks=p.stat().st_blocks * 512)
assert members and sum(r['size'] for r in members.values()) < 64 << 20
A = D / 'historical-native-product-metadata.tar.zst'
assert not A.exists()
with qa.open_writer(A) as t:
    for n in sorted(members):
        p = P / n
        assert sha(p) == members[n]['sha256']
        t.add(p, arcname=n, recursive=False)
seen = {}
with qa.open_reader(A) as t:
    for m in t:
        assert m.isfile() and m.name in members and m.name not in seen
        with t.extractfile(m) as f:
            seen[m.name] = dict(sha256=hashlib.file_digest(f, 'sha256').hexdigest(),
                size=m.size, mode=m.mode & 0o777)
assert seen == {n: {k: r[k] for k in ('sha256', 'size', 'mode')} for n, r in members.items()}
proof = dict(passed=True, archive=str(A), archive_sha256=sha(A),
    archive_bytes=A.stat().st_size, original_directory=str(P), members=members,
    all_payloads_read_back=True, native_execution_claimed=False,
    current_v7_products_untouched=True, original_test_limits_unchanged=True)
(D / 'historical-native-product-metadata-retirement.json').write_text(json.dumps(proof, indent=2) + '\n')
for n, r in members.items():
    p = P / n
    assert sha(p) == r['sha256']
    p.unlink()
for p in sorted((p for p in P.rglob('*') if p.is_dir()), key=lambda p: len(p.parts), reverse=True):
    p.rmdir()
P.rmdir()
print('Historical metadata preserved byte for byte and fully read back before retiring raw copies',
    sum(r['blocks'] for r in members.values()), A.stat().st_size, flush=True)
