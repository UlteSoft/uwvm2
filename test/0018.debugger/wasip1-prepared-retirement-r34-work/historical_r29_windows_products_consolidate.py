from contextlib import ExitStack
from itertools import zip_longest
from pathlib import Path, PurePosixPath
import copy
import hashlib
import json
import os
import sys
import tarfile
import qualified_archive as qa

D = Path(__file__).parent
E = D.parent.parent
H = E / 'rounds/wasip1-dispatch-20261007-r29'
assert sys.argv[1:] == ['joint-historical-r29-windows-products-consolidate-r34', 'windows', 'all']
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
inputs = []
expected = {}
modes = {}
for repo in ('uwvm2', 'uwvm2-ros'):
    P = H / (repo + '-windows-product-retirement.json')
    q = json.loads(P.read_text())
    A = H / (repo + '-windows-qualified-products.tar.gz')
    assert q['passed'] and q['all_payloads_read_back'] and q['archive'] == str(A)
    assert A.is_file() and not A.is_symlink() and A.stat().st_uid == os.getuid() == 1000
    assert sha(A) == q['archive_sha256']
    inputs.append(dict(label=repo, receipt=str(P), receipt_sha256=sha(P), original_archive=str(A),
        original_archive_sha256=sha(A), original_archive_bytes=A.stat().st_size, payloads=q['payloads']))
    for n, h in q['payloads'].items():
        path = PurePosixPath(n)
        assert not path.is_absolute() and '..' not in path.parts
        expected[repo + '/' + n] = h
A = D / 'historical-r29-windows-qualified-products-consolidated.tar.zst'
assert not A.exists()
with qa.open_writer(A) as target, ExitStack() as stack:
    readers = [stack.enter_context(tarfile.open(i['original_archive'], mode='r|gz')) for i in inputs]
    for members in zip_longest(*(iter(t) for t in readers)):
        for member, reader, info in zip(members, readers, inputs):
            if member is None:
                continue
            assert member.isfile() and member.name in info['payloads']
            m = copy.copy(member)
            m.name = info['label'] + '/' + member.name
            assert m.name not in modes
            modes[m.name] = dict(mode=member.mode, size=member.size)
            m.pax_headers = dict(member.pax_headers)
            m.pax_headers.pop('path', None)
            with reader.extractfile(member) as f:
                target.addfile(m, f)
seen = {}
with qa.open_reader(A) as archive:
    for m in archive:
        assert m.isfile() and m.name in expected and m.name not in seen
        assert modes[m.name] == dict(mode=m.mode, size=m.size)
        with archive.extractfile(m) as f:
            seen[m.name] = hashlib.file_digest(f, 'sha256').hexdigest()
assert seen == expected
assert all(sha(i['receipt']) == i['receipt_sha256'] for i in inputs)
q = dict(passed=True, all_payloads_read_back=True, all_member_sizes_modes_read_back=True,
    archive=str(A), archive_sha256=sha(A), archive_bytes=A.stat().st_size, inputs=inputs,
    payloads=expected, member_metadata=modes, native_execution_claimed=False,
    historical_round='R29', original_test_limits_unchanged=True)
(D / 'historical-r29-windows-products-consolidation-qualified.json').write_text(json.dumps(q, indent=2) + '\n')
for i in inputs:
    p = Path(i['original_archive'])
    assert sha(p) == i['original_archive_sha256']
    p.unlink()
print('Historical R29 native bytes, sizes and modes fully read back before container retirement',
    sum(i['original_archive_bytes'] for i in inputs), A.stat().st_size, flush=True)
