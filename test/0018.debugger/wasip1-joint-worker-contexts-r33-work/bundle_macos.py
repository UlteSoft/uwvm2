from pathlib import Path
from contextlib import ExitStack
import sys,json,hashlib,copy
D=Path(__file__).parent;E=D.parent.parent
assert sys.argv[1:]==['joint-bundle-macos-r33']
import qualified_archive as qa
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
original={};payloads={};manifests={}
for repo in ('uwvm2','uwvm2-ros'):
 p=D/(repo+'-macos-product-retirement.json');a=json.loads(p.read_text());O=D/'products/macos'/repo
 q=json.loads((O/'qualified.json').read_text());native=json.loads((O/'receipt.json').read_text());manifest=D/('ros-repaired-inputs.json' if repo=='uwvm2-ros' else 'inputs.json')
 assert a['passed'] and a['all_payloads_read_back'] and sha(a['archive'])==a['archive_sha256']
 assert native['passed'] and len(native['rows'])==8 and all(r['passed'] and r['pid_reaped'] and r['aggregate_peak_upper_bytes']<2<<30 for r in native['rows'])
 assert native['source_manifest_sha256']==q['source_manifest_sha256']==sha(manifest)
 original[repo]=a;manifests[repo]=sha(manifest);payloads.update({repo+'/'+n:h for n,h in a['payloads'].items()})
class Hashed:
 def __init__(self,f):self.f=f;self.hash=hashlib.sha256()
 def read(self,n=-1):b=self.f.read(n);self.hash.update(b);return b
archive=D/'macos-both-repositories-qualified-products.tar.zst';assert not archive.exists();seen={}
with qa.open_writer(archive) as output:
 with ExitStack() as stack:
  parents={repo:stack.enter_context(qa.open_reader(a['archive'])) for repo,a in original.items()}
  sources={repo:iter(parent) for repo,parent in parents.items()}
  # Adjacent corresponding objects/executables share the bounded Zstd window;
  # every source remains an exact original payload, including native signatures.
  while sources:
   for repo in tuple(sources):
    try:m=next(sources[repo])
    except StopIteration:del sources[repo];continue
    assert m.isfile() and m.name in original[repo]['payloads'];name=repo+'/'+m.name;assert name not in seen
    # Extract from the iterator's actual parent tar reader, retained by ExitStack.
    # Keep that parent explicitly below rather than recovering a file pointer.
    parent=parents[repo]
    with parent.extractfile(m) as raw:
     hashed=Hashed(raw);info=copy.copy(m);info.pax_headers=dict(m.pax_headers);info.pax_headers.pop('path',None);info.name=name
     output.addfile(info,hashed);assert hashed.hash.hexdigest()==payloads[name];seen[name]=hashed.hash.hexdigest()
assert seen==payloads
observed={}
with qa.open_reader(archive) as t:
 for m in t:
  assert m.isfile() and m.name in payloads and m.name not in observed
  with t.extractfile(m) as f:observed[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert observed==payloads
proof=dict(passed=True,all_payloads_read_back=True,archive=str(archive),archive_sha256=sha(archive),archive_bytes=archive.stat().st_size,payloads=payloads,source_manifests=manifests,originals={r:dict(archive=a['archive'],archive_sha256=a['archive_sha256'],archive_bytes=a['archive_bytes'],payloads=a['payloads']) for r,a in original.items()},native_tests_unchanged=True,original_archives_preserved_until_local_readback=True,logical_compiled_products=2,physical_bundle_files=1,cgroup=Path('/proc/self/cgroup').read_text())
(D/'macos-both-repositories-bundle-qualified.json').write_text(json.dumps(proof,indent=2)+'\n')
print('Both exact native macOS payloads bundled and fully read back',archive.stat().st_size,flush=True)
