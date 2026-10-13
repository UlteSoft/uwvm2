from pathlib import Path
import json, hashlib, tarfile

L = Path(__file__).parent
B = L.parents[3]
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
parent = L / 'repaired-inputs-v7.json'
M = json.loads(parent.read_text())
changed = json.loads((L / 'serial-tls-repair-source.json').read_text())['fixtures']
assert len(changed) == 4
for r in changed:
    assert M[r['path']] == r['before_sha256'] and sha(B / r['path']) == r['after_sha256']
    M[r['path']] = r['after_sha256']
manifest = L / 'repaired-inputs-v8.json'
assert not manifest.exists()
manifest.write_text(json.dumps(M, indent=2) + '\n')
A = L / 'repair-v8-delta.tar.gz'
assert not A.exists()
with tarfile.open(A, 'w:gz', compresslevel=6) as t:
    for r in changed:
        t.add(B / r['path'], arcname=r['path'], recursive=False)
request = dict(parent_manifest_sha256=sha(parent), overlay_manifest_sha256=sha(manifest),
    delta_sha256=sha(A), new_paths=[], changed_paths=[r['path'] for r in changed],
    source_files=len(M), production_library_unchanged=True)
(L / 'repair-v8-request.json').write_text(json.dumps(request, indent=2) + '\n')
s = (L / 'repair_bootstrap_v7.py').read_text().replace('joint-repair-v7-inputs-r34', 'joint-repair-v8-inputs-r34').replace('repair-v7-', 'repair-v8-')
s = s.replace('repaired-inputs-v7', 'repaired-inputs-v8').replace('repaired-inputs-v6', 'repaired-inputs-v7')
s = s.replace('Read-only V7 8791-file input: same V6 library plus two dedicated bounded-loop Core fixtures',
    'Read-only V8 8791-file input: same V7 production library, serial TLS cleanup with partial-exit refusal')
(L / 'repair_bootstrap_v8.py').write_text(s)
g = (L / 'guard-joint-repair-v7-inputs-r34.py').read_text().replace('joint-repair-v7-inputs-r34', 'joint-repair-v8-inputs-r34').replace('repair_bootstrap_v7.py', 'repair_bootstrap_v8.py')
(L / 'guard-joint-repair-v8-inputs-r34.py').write_text(g)
print(json.dumps(request))
