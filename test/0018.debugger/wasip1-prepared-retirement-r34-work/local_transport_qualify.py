from pathlib import Path, PurePosixPath
import hashlib
import json
import os
import sys
import tarfile

D = Path(__file__).parent
assert sys.argv[1:] == ['joint-local-inputs-custody-r34', 'linux', 'all']
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
request = D / 'local-historical-inputs-custody-request.json'
rows = json.loads(request.read_text())['rows']
assert len(rows) == 2 and {r['label'] for r in rows} == {'r31', 'r32'}
qualified = []
for r in rows:
    p = D / (r['label'] + '-local-original-inputs.tar.gz')
    assert str(p) == r['remote_path'] and p.is_file() and not p.is_symlink()
    assert p.stat().st_uid == os.getuid() == 1000 and p.stat().st_size == r['bytes']
    assert sha(p) == r['sha256'] and r['mode'] == 0o444
    payloads = {}
    with tarfile.open(p, 'r|gz') as a:
        for m in a:
            n = PurePosixPath(m.name)
            assert not n.is_absolute() and '..' not in n.parts
            assert m.isfile() or m.isdir()
            if not m.isfile():
                continue
            assert m.name not in payloads
            with a.extractfile(m) as f:
                payloads[m.name] = hashlib.file_digest(f, 'sha256').hexdigest()
    assert payloads and sha(p) == r['sha256']
    os.chmod(p, r['mode'])
    assert p.stat().st_mode & 0o777 == r['mode']
    qualified.append(dict(**r, payloads=payloads, all_source_members_read_back=True))
q = dict(passed=True, request_sha256=sha(request), rows=qualified,
    exact_original_compressed_bytes_preserved=True, no_local_cold_region_modified=True,
    native_execution_claimed=False)
(D / 'local-historical-inputs-custody-qualified.json').write_text(json.dumps(q, indent=2) + '\n')
print('Two original local source archives fully hashed, tar-read and retained readonly on original task volume', flush=True)
