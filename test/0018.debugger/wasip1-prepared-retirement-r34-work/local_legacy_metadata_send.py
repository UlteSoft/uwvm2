from pathlib import Path
import hashlib
import json
import os
import resource
import shlex
import subprocess
import tarfile

L = Path(__file__).parent
B = L.parent
D = '/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/rounds/wasip1-prepared-retirement-20261008-r34'
names = ('wasip1-world-install-r28-work', 'wasip1-dispatch-r29-work', 'wasip1-context-workers-r30-work',
    'wasip1-joint-worker-contexts-r31-work', 'wasip1-joint-worker-contexts-r32-work', 'wasip1-joint-worker-contexts-r33-work')
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
rows = []
for name in names:
    root = B / name
    assert root.is_dir() and root.stat().st_uid == os.getuid()
    for p in root.rglob('*'):
        if not p.is_file() or p.is_symlink() or p.suffix == '.py' or p.stat().st_size < 32768:
            continue
        s = p.stat()
        assert s.st_uid == os.getuid()
        rows.append(dict(local_path=str(p.resolve()), member=str(p.relative_to(B)), bytes=s.st_size,
            mode=s.st_mode & 0o777, sha256=sha(p), local_device=s.st_dev, local_inode=s.st_ino))
assert rows and sum(r['bytes'] for r in rows) < 128 << 20
request = L / 'local-legacy-metadata-request.json'
data=json.dumps(dict(rows=rows, remote_archive=D + '/local-r28-r33-large-metadata.tar.gz'), indent=2) + '\n'
if request.exists():assert request.read_text()==data
else:request.write_text(data)
key = str(Path.home() / '.ssh/id_ed25519')
host = 'macromodel@100.123.133.75'
subprocess.run(['scp', '-i', key, str(request), host + ':' + D + '/'], check=True)
receiver = """from pathlib import Path
import sys,resource,json
p=Path('""" + D + """/local-r28-r33-large-metadata.tar.gz')
resource.setrlimit(resource.RLIMIT_FSIZE,(64<<20,64<<20))
assert not p.exists()
total=0
with p.open('xb') as f:
    while data:=sys.stdin.buffer.read(65536):
        total+=len(data);assert total<64<<20;f.write(data)
print(json.dumps(dict(received_bytes=total)))
"""
proc = subprocess.Popen(['ssh', '-i', key, host, 'python3 -c ' + shlex.quote(receiver)], stdin=subprocess.PIPE)
with tarfile.open(fileobj=proc.stdin, mode='w|gz', dereference=True) as archive:
    for row in rows:
        p = Path(row['local_path'])
        assert sha(p) == row['sha256']
        archive.add(p, arcname=row['member'], recursive=False)
proc.stdin.close()
assert proc.wait(timeout=60) == 0
assert resource.getrusage(resource.RUSAGE_SELF).ru_maxrss + (1 << 30) + (128 << 20) < 2 << 30
print('Legacy metadata streamed without creating a local archive; raw originals retained until independent readback',
    len(rows), sum(r['bytes'] for r in rows), flush=True)
