from pathlib import Path
import os,json,hashlib,tarfile,gzip,resource
os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,512<<20))
O=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-moonbit-image-20261019-a9')
assert json.loads((O/'readback-r4.json').read_bytes())['status']=='INDEPENDENT_MOONBIT_IMAGE_FINITE_MATRIX_READBACK_PASSED'
P=O/'publication-r2';P.mkdir()
def H(p):
 h=hashlib.sha256()
 with p.open('rb')as f:
  while b:=f.read(65536):h.update(b)
 return h.hexdigest()
selected={};cpp=[]
for p in sorted(O.rglob('*')):
 if not p.is_file():continue
 rel=p.relative_to(O)
 if rel.parts[0]=='moon-home-r2' or rel.parts[0].startswith('publication-'):continue
 if p.name.startswith('publisher-'):continue
 if '.git'in rel.parts or '__pycache__'in rel.parts:continue
 if any(part=='moon-home'for part in rel.parts):continue
 if p.suffix in ('.core','.mi','.mbti'):continue
 if p.name.startswith('.'):continue
 if p.name in ('native-image-contract',)or p.suffix in ('.json','.log','.py','.mjs','.wasm','.map','.stdout','.stderr','.md','.mbt','.pkg','.mod','.work','.h','.cc'):
  dst=str(rel)
  if p.suffix=='.cc':
   dst+='.source';cpp.append({'original_path':str(p),'archive_path':dst,'bytes':p.stat().st_size,'sha256':H(p)})
  assert dst not in selected;selected[dst]=p
original=P/'SOURCE_PATHS.json';original.write_text(json.dumps({'cpp_renaming_reason':'Root xmake scans test/**.cc by basename; inert snapshots use .cc.source','cpp':cpp},indent=2)+'\n');selected['SOURCE_PATHS.json']=original
manifest={'format':'uwvm-language-finite-evidence-v1','lane':'MoonBit Image','file_count':len(selected),'members':[{'path':k,'bytes':p.stat().st_size,'sha256':H(p)}for k,p in sorted(selected.items())],'external_large_inputs':'Pinned native compiler/UWVM/reference executables, R99 restore archive and private intermediate compiler core inputs are external. No binary fixture is executed during publication.'}
m=P/'MANIFEST.json';m.write_text(json.dumps(manifest,indent=2)+'\n')
a=P/'moonbit-image-sdk-20261019.tar.gz'
with a.open('xb')as raw:
 with gzip.GzipFile(filename='',fileobj=raw,mode='wb',compresslevel=6,mtime=0)as zipped:
  with tarfile.open(fileobj=zipped,mode='w|')as tar:
   for name,p in [('MANIFEST.json',m)]+sorted(selected.items()):
    info=tar.gettarinfo(str(p),name);info.uid=info.gid=0;info.uname=info.gname='';info.mtime=0;info.mode=0o644
    with p.open('rb')as f:tar.addfile(info,f)
h=hashlib.sha256();n=0
with gzip.open(a,'rb')as f:
 while b:=f.read(65536):h.update(b);n+=len(b)
receipt={'status':'INERT_MOONBIT_IMAGE_PACKET_READY','archive':str(a),'archive_bytes':a.stat().st_size,'archive_sha256':H(a),'gzip_EOF_bytes':n,'gzip_EOF_sha256':h.hexdigest(),'manifest_bytes':m.stat().st_size,'manifest_sha256':H(m),'payload_files':len(selected),'reader':{'path':str(O/'readback-r4.py'),'sha256':H(O/'readback-r4.py')},'readback':{'path':str(O/'readback-r4.json'),'sha256':H(O/'readback-r4.json')}}
(P/'transport.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt))
