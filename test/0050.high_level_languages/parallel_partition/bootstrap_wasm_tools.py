#!/usr/bin/env python3
"""Install one pinned official wasm-tools binary in the admitted Linux arena."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import urllib.request

VERSION = '1.262.0'
SHA256 = '655b8df67d54c3ef9b52fc237c4e94bbfbb5f5f43825245cb0940a7002a9ed65'
CGROUP = '/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
assert sys.platform == 'linux' and Path('/proc/self/cgroup').read_text() == '0::' + CGROUP + '\n'
tools = Path(sys.argv[1])
assert tools.is_absolute()
dest = tools / ('wasm-tools-' + VERSION)
dest.mkdir(parents=True, exist_ok=True)
url = 'https://api.github.com/repos/bytecodealliance/wasm-tools/releases/tags/v' + VERSION
with urllib.request.urlopen(url, timeout=30) as response:
    release = json.load(response)
assert release['tag_name'] == 'v' + VERSION
asset = next(a for a in release['assets'] if a['name'] == 'wasm-tools-' + VERSION + '-x86_64-linux.tar.gz')
assert asset['digest'] == 'sha256:' + SHA256
archive = dest / asset['name']
request = urllib.request.Request(asset['url'] + '?download=1', headers={'Accept': 'application/octet-stream'})
try:
    with urllib.request.urlopen(request, timeout=60) as response, archive.open('wb') as output:
        while block := response.read(1 << 20):
            output.write(block)
    assert archive.stat().st_size == asset['size']
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == SHA256
    with tarfile.open(archive) as source:
        member = next(m for m in source.getmembers() if m.isfile() and m.name.endswith('/wasm-tools'))
        binary = dest / 'wasm-tools'
        binary.write_bytes(source.extractfile(member).read())
        binary.chmod(0o755)
    version = subprocess.check_output([str(binary), '--version'], text=True).strip()
    assert version == 'wasm-tools ' + VERSION or version.startswith('wasm-tools ' + VERSION + ' (')
    receipt = {'release_tag': release['tag_name'], 'release_url': release['html_url'],
               'release_id': release['id'], 'asset': asset, 'archive_sha256': SHA256,
               'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(), 'version': version,
               'binary_path': str(binary), 'archive_removed': True}
    (dest / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
finally:
    archive.unlink(missing_ok=True)
print(json.dumps(receipt), flush=True)
