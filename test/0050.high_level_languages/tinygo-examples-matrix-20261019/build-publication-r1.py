import pathlib,json,hashlib,os,shutil,tarfile,gzip,resource
S=pathlib.Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-tinygo-complete-examples-20261019-a7')
os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,512<<20));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def identity(p):
 h=hashlib.sha256();n=0
 with p.open('rb')as f:
  while True:
   b=f.read(65536)
   if not b:break
   n+=len(b);h.update(b)
 return {'bytes':n,'sha256':h.hexdigest()}
reader=json.loads((S/'readback-checkpoint-r1.json').read_bytes())
assert reader['status']=='INDEPENDENT_INERT_TINYGO_PARTIAL_EXAMPLE_READBACK_PASSED'and reader['products']==121 and reader['ordinary_commands']==34
assert json.loads((S/'producer-result-r4.json').read_bytes())['status']=='NOT_RUN_CAPACITY'
assert all(not(pathlib.Path('/proc')/str(pid)).exists()for pid in [3966053,3966055,3989316,3989771,4001510,4008422])
docs=json.loads((S/'sdk-documentation-manifest.json').read_bytes())
for row in docs['members']:assert identity(S/'current-docs-r2'/row['path'])=={k:row[k]for k in ['bytes','sha256']}
root=S/'publication';root.mkdir()
chosen=[]
for p in S.iterdir():
 if p.is_file()and p.suffix in ['.json','.py','.log','.md']:chosen.append(p)
for name in ['source-r1','current-docs-r1','current-docs-r2','validation-tools','build-runs-r1','build-runs-r2','build-runs-r4','products-r1','products-r2','ordinary-partial-runs-r1','ordinary-partial-runs-r2','ordinary-partial-runs-r3','ordinary-partial-runs-r4']:
 d=S/name
 if d.exists():chosen.extend(p for p in d.rglob('*')if p.is_file())
chosen=sorted(set(chosen))
mapping=[]
for p in chosen:
 assert not p.is_symlink()and p.suffix!='.cc'
 relative=p.relative_to(S);q=root/relative;q.parent.mkdir(parents=True,exist_ok=True);original=identity(p);shutil.copyfile(p,q);assert identity(q)==original
 mapping.append({'original_path':str(p),'packet_path':str(relative),**original})
(root/'input-path-map.json').write_text(json.dumps({'original_stage':str(S),'members':mapping,'external_UWVM_and_compiler_binaries_included':False},indent=2)+'\n')
members=[{'path':str(p.relative_to(root)),**identity(p)}for p in sorted(root.rglob('*'))if p.is_file()]
manifest={'status':'INERT_PARTIAL_TINYGO_EXAMPLE_CHECKPOINT','all15_complete':False,'members':members}
(root/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
archive=S/'publication-data.tar.gz'
with tarfile.open(archive,'w:gz',compresslevel=6)as tar:
 for p in sorted(root.rglob('*')):
  if p.is_file():tar.add(p,arcname=str(p.relative_to(root)),recursive=False)
uncompressed=0;h=hashlib.sha256()
with gzip.open(archive,'rb')as f:
 while True:
  b=f.read(65536)
  if not b:break
  uncompressed+=len(b);h.update(b)
with tarfile.open(archive,'r:gz')as tar:
 allmembers=tar.getmembers();assert len(allmembers)==len(members)+1 and all(m.isfile()and not pathlib.PurePosixPath(m.name).is_absolute()and'..'not in pathlib.PurePosixPath(m.name).parts for m in allmembers)
receipt={'status':'LINUX_PACKET_MANIFEST_ARCHIVE_AND_FULL_GZIP_EOF_VERIFIED','payload_files_excluding_manifest':len(members),'manifest':identity(root/'MANIFEST.json'),'archive':identity(archive),'gzip_uncompressed_bytes':uncompressed,'gzip_uncompressed_sha256':h.hexdigest(),'new_performance_credit':False,'SDK_producer_source_changed':False}
(S/'publication-transport.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt))
