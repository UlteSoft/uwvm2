from pathlib import Path
from contextlib import ExitStack
from itertools import zip_longest
import sys, json, hashlib, copy
import qualified_archive as qa

D = Path(__file__).parent
assert sys.argv[1] == 'joint-native-v8-products-consolidate-r34' and sys.argv[3] == 'all'
platform = sys.argv[2]
assert platform in ('linux-integrated', 'windows', 'freebsd', 'macos')
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
manifest_hash = sha(D / 'repaired-inputs-v8.json')
assert manifest_hash == '171e9f277d349278c53669b49d60d5727f73575fa9847d14dd40a5741924d73f'
inputs = []
expected = {}
for repo in ('uwvm2', 'uwvm2-ros'):
    P = D / (repo + '-' + platform + '-v8-product-retirement.json')
    q = json.loads(P.read_text())
    A = Path(q['archive'])
    assert q['passed'] and q['all_payloads_read_back'] and q['source_manifest_sha256'] == manifest_hash
    assert sha(A) == q['archive_sha256']
    O = D / 'products-v8' / platform / repo
    if platform in ('linux-integrated', 'macos'):
        native = json.loads((O / ('native-execution.json' if platform == 'linux-integrated' else 'receipt.json')).read_text())
        assert native['passed'] and len(native['rows']) == 6 and all(r['passed'] for r in native['rows'])
    else:
        matches = []
        for p in D.glob('vm-' + platform + '-joint-preparation-*/receipt.json'):
            native = json.loads(p.read_text())
            if native.get('passed') and native.get('target_source_manifest_sha256') == manifest_hash and all(t['repo'] == repo for t in native['tests']):
                matches.append(native)
        assert len(matches) == 1 and len(matches[0]['tests']) == 6 and all(matches[0]['test_statuses'].values())
    i = dict(label=repo, receipt=str(P), receipt_sha256=sha(P), original_archive=str(A),
        original_archive_sha256=sha(A), original_archive_bytes=A.stat().st_size, payloads=q['payloads'])
    inputs.append(i)
    for n, h in q['payloads'].items():
        expected[repo + '/' + n] = h
A = D / ('v8-' + platform + '-native-products-consolidated.tar.zst')
assert not A.exists()
with qa.open_writer(A) as target, ExitStack() as stack:
    readers = [stack.enter_context(qa.open_reader(i['original_archive'])) for i in inputs]
    for members in zip_longest(*(iter(t) for t in readers)):
        for member, reader, info in zip(members, readers, inputs):
            if member is None:
                continue
            assert member.isfile() and member.name in info['payloads']
            m = copy.copy(member)
            m.name = info['label'] + '/' + member.name
            m.pax_headers = dict(member.pax_headers)
            m.pax_headers.pop('path', None)
            with reader.extractfile(member) as f:
                target.addfile(m, f)
seen = {}
with qa.open_reader(A) as t:
    for m in t:
        assert m.isfile() and m.name in expected and m.name not in seen
        with t.extractfile(m) as f:
            seen[m.name] = hashlib.file_digest(f, 'sha256').hexdigest()
assert seen == expected
q = dict(passed=True, all_payloads_read_back=True, archive=str(A), archive_sha256=sha(A),
    archive_bytes=A.stat().st_size, inputs=inputs, payloads=expected,
    source_manifest_sha256=manifest_hash, native_execution_claimed=False,
    original_test_limits_unchanged=True)
(D / ('v8-' + platform + '-products-consolidation-qualified.json')).write_text(json.dumps(q, indent=2) + '\n')
for i in inputs:
    p = Path(i['original_archive'])
    assert sha(p) == i['original_archive_sha256']
    p.unlink()
print('Final V8 native product bytes completely read back before retiring separate containers', platform,
    sum(i['original_archive_bytes'] for i in inputs), A.stat().st_size, flush=True)
