from pathlib import Path
import json,hashlib,shutil,os
O=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-zig-sdk-resources-20261019-a5')
def ident(p):
 h=hashlib.sha256();n=0
 with p.open('rb')as f:
  for b in iter(lambda:f.read(1<<20),b''):n+=len(b);h.update(b)
 return {'bytes':n,'sha256':h.hexdigest()}
verified=json.loads((O/'independent-readback.json').read_bytes())
assert verified['status']=='INDEPENDENT_INERT_ZIG_RESOURCE_NATIVE_MATRIX_READBACK_PASSED'
P=O/'publication';P.mkdir();mapping=[];members=[]
def copy(p,rel):
 assert not p.is_symlink()
 dest=rel+'.source'if rel.endswith('.cc')else rel
 q=P/dest;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
 mark=ident(p);assert ident(q)==mark
 if dest!=rel:mapping.append({'original_path':str(p),'original_relative_path':rel,'stored_path':dest,**mark})
 members.append({'path':dest,**mark})
for p in sorted(O.iterdir()):
 if p.is_file()and p.suffix in['.json','.py','.log']:copy(p,p.name)
allowed=['source-r1','source-r2','source-r3','source-inventory-current','source-current-guides','source-current-guides-r2','codec-probe-source','products','products-r2','products-r3','products-r4','codec-probe-products','build-runs-r1','build-runs-r2','build-runs-r3','build-runs-r4','native-build-runs-r1','native-products-r1','ordinary-runs','codec-ordinary-runs','codec-probe-build-runs','source-gate-runs','matrix-smoke-r1','matrix-smoke-r2','matrix-smoke-r3','matrix-final-r1','debug-smoke-r1','debug-pure-smoke-r2','debug-pure-smoke-r3','debug-codec-smoke-r5','debug-codec-final-r1','reused-inputs']
for name in allowed:
 d=O/name
 if not d.exists():continue
 for p in sorted(d.rglob('*')):
  if p.is_file():
   if name.startswith('products')and p.suffix not in['.wasm','.json']:continue
   if name=='native-products-r1'and p.suffix not in['.d','.map']:continue
   copy(p,str(p.relative_to(O)))
report=(O/'report-next.md').read_bytes();(P/'README.md').write_bytes(report)
(P/'source-payload-map.json').write_text(json.dumps({'status':'ARCHIVAL_CPP_STORED_AS_DATA_ONLY','reason':'Root xmake scans test/**.cc; archival C++ payloads must not be discovered as targets.','members':mapping},indent=2)+'\n')
assert not list(P.rglob('*.cc'))
rows=[{'path':str(p.relative_to(P)),**ident(p)}for p in sorted(P.rglob('*'))if p.is_file()]
manifest={'status':'INERT_PUBLICATION_PACKET','root':str(O),'scope':'Source snapshots, Wasms and result/log data; not complete15-language/SDK/performance qualification. No archived script is auto-executed.','members':rows}
(P/'packet-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
out={'status':'ZIG_SDK_PUBLICATION_PACKET_HASHED','packet':str(P),'files':len(rows)+1,'manifest':ident(P/'packet-manifest.json'),'README':ident(P/'README.md'),'inert_reader':ident(O/'independent-readback.json'),'bytes':sum(r['bytes']for r in rows)}
(O/'publication-handoff.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out))
