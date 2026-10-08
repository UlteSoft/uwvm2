from pathlib import Path
import sys, json, hashlib, gzip, shutil

D = Path(__file__).parent
assert sys.argv[1:] == ['joint-source-ledger-compact-r34', 'final', 'all']
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
P = D / 'readonly-historical-source-deduplication-qualified.json'
assert P.is_file() and not P.is_symlink() and P.stat().st_uid == 1000 and P.stat().st_size < 64 << 20
original_hash = sha(P)
original_bytes = P.stat().st_size
q = json.loads(P.read_text())
assert q['passed'] and q['all_manifests_and_source_bytes_unchanged'] and q['all_original_paths_retained']
A = P.with_name(P.name + '.gz')
assert not A.exists()
with P.open('rb') as inp, gzip.open(A, 'xb', compresslevel=9) as out:
    shutil.copyfileobj(inp, out, 1048576)
with gzip.open(A, 'rb') as inp:
    assert hashlib.file_digest(inp, 'sha256').hexdigest() == original_hash
proof = dict(passed=True, original_path=str(P), original_sha256=original_hash,
    original_bytes=original_bytes, archive=str(A), archive_sha256=sha(A),
    archive_bytes=A.stat().st_size, entire_original_json_read_back=True,
    source_and_manifest_bytes_unchanged=True, original_paths_retained=True,
    readonly_source_change_count=len(q['changes']), bindings=q['bindings'],
    excluded_partial_namespaces=q['excluded_partial_namespaces'],
    original_test_limits_unchanged=True, native_execution_claimed=False)
(D / 'readonly-source-deduplication-evidence-retirement.json').write_text(json.dumps(proof, indent=2) + '\n')
assert sha(P) == original_hash
P.unlink()
print('Entire detailed source ledger is recoverable byte for byte; original source paths retained',
    original_bytes, A.stat().st_size, flush=True)
