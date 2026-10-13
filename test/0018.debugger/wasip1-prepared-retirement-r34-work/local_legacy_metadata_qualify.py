from pathlib import Path, PurePosixPath
import hashlib
import json
import sys
import tarfile

D = Path(__file__).parent
assert sys.argv[1:] == ['joint-local-metadata-custody-r34', 'linux', 'all']
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
P = D / 'local-legacy-metadata-request.json'
q = json.loads(P.read_text())
A = D / 'local-r28-r33-large-metadata.tar.gz'
assert q['remote_archive'] == str(A) and A.is_file() and not A.is_symlink() and A.stat().st_size < 64 << 20
expected = {r['member']: r for r in q['rows']}
assert len(expected) == len(q['rows'])
seen = {}
with tarfile.open(A, 'r|gz') as archive:
    for m in archive:
        name = PurePosixPath(m.name)
        assert not name.is_absolute() and '..' not in name.parts
        assert m.isfile() and m.name in expected and m.name not in seen
        r = expected[m.name]
        assert m.size == r['bytes'] and m.mode == r['mode']
        with archive.extractfile(m) as f:
            seen[m.name] = hashlib.file_digest(f, 'sha256').hexdigest()
        assert seen[m.name] == r['sha256']
assert set(seen) == set(expected)
out = dict(passed=True, archive=str(A), archive_bytes=A.stat().st_size, archive_sha256=sha(A),
    request_sha256=sha(P), rows=q['rows'], all_payloads_sizes_modes_read_back=True,
    native_execution_claimed=False, current_sources_and_cold_regions_untouched=True)
(D / 'local-legacy-metadata-custody-qualified.json').write_text(json.dumps(out, indent=2) + '\n')
print('All historical local metadata bytes, sizes and modes fully read back before local retirement', len(seen), flush=True)
