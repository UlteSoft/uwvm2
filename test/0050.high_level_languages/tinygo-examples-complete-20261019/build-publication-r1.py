import pathlib,json,hashlib,os,shutil,tarfile,gzip,resource
S=pathlib.Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-tinygo-existing-products-20261019-a8')
A7=S.parent/'root-language-tinygo-complete-examples-20261019-a7'
os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,512<<20));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def identity(p):
 h=hashlib.sha256();n=0
 with p.open('rb')as f:
  for b in iter(lambda:f.read(65536),b''):n+=len(b);h.update(b)
 return {'bytes':n,'sha256':h.hexdigest()}
reader=json.loads((S/'readback-r2.json').read_bytes());assert reader['status']=='INDEPENDENT_INERT_TINYGO_COMPLETE_EXAMPLE_MATRIX_READBACK_PASSED'and reader['products']==135 and reader['ordinary_commands']==390 and reader['debug_full_value_workflows']==15 and reader['kernel_functional']['cases']==28
assert all(not(pathlib.Path('/proc')/str(pid)).exists()for pid in [4038353,4041877,4044849,4046808,4048427,4058346])
ancestor_root=A7/'publication';ancestor_manifest=identity(ancestor_root/'MANIFEST.json');assert ancestor_manifest['sha256']=='5b2cb5dda162ad76cdfeb46b57bcc625d55a69d19fb7d6c76ce9ba6b8375e9b3'
ancestor=json.loads((ancestor_root/'MANIFEST.json').read_bytes())
for r in ancestor['members']:assert identity(ancestor_root/r['path'])=={k:r[k]for k in ['bytes','sha256']}
docs=json.loads((S/'sdk-documentation-manifest.json').read_bytes())
for r in docs['members']:assert identity(S/'current-docs-r1'/r['path'])=={k:r[k]for k in ['bytes','sha256']}
association=json.loads((S/'active-source-association.json').read_bytes());assert association['producer_source_code_changed']is False and len(association['members'])==65
root=S/'publication';root.mkdir()
ancestors={'required_sibling_packet':'../tinygo-examples-matrix-20261019','manifest':ancestor_manifest,'original_Linux_stage':str(A7),'payload_members':ancestor['members'],'source_snapshot':'source-r1','required_for_source_earlier_products_and_closed_logs':True,'external_compiler_UWVM_reference_binaries_remain_separately_pinned':True}
(root/'ANCESTORS.json').write_text(json.dumps(ancestors,indent=2)+'\n')
chosen=[p for p in S.iterdir()if p.is_file()and p.suffix in ['.json','.py','.log','.md']]
for name in ['current-docs-r1','producer-runs-r1','products-r1','ordinary-runs-r2','ordinary-runs-r3','debug-r1','kernel-runs-r1','kernel-products-r1']:
 p=S/name
 if p.exists():chosen.extend(q for q in p.rglob('*')if q.is_file())
mapping=[]
for p in sorted(set(chosen)):
 assert not p.is_symlink()and p.suffix!='.cc';relative=p.relative_to(S);q=root/relative;q.parent.mkdir(parents=True,exist_ok=True);before=identity(p);shutil.copyfile(p,q);assert identity(q)==before;mapping.append({'original_path':str(p),'packet_path':str(relative),**before})
(root/'input-path-map.json').write_text(json.dumps({'original_stage':str(S),'members':mapping,'ancestor_mapping':'ANCESTORS.json','SDK_producer_source_changed':False},indent=2)+'\n')
members=[{'path':str(p.relative_to(root)),**identity(p)}for p in sorted(root.rglob('*'))if p.is_file()]
(root/'MANIFEST.json').write_text(json.dumps({'status':'FINITE_TINYGO_EXAMPLE_DBG_CODEC_MATRIX_COMPLETE_PERFORMANCE_UNQUALIFIED','all15_complete':False,'members':members},indent=2)+'\n')
archive=S/'publication-data.tar.gz'
with tarfile.open(archive,'w:gz',compresslevel=6)as tar:
 for p in sorted(root.rglob('*')):
  if p.is_file():tar.add(p,arcname=str(p.relative_to(root)),recursive=False)
size=0;h=hashlib.sha256()
with gzip.open(archive,'rb')as f:
 for b in iter(lambda:f.read(65536),b''):size+=len(b);h.update(b)
with tarfile.open(archive,'r:gz')as tar:
 allmembers=tar.getmembers();assert len(allmembers)==len(members)+1 and all(x.isfile()and not pathlib.PurePosixPath(x.name).is_absolute()and'..'not in pathlib.PurePosixPath(x.name).parts for x in allmembers)
r={'status':'LINUX_PACKET_ANCESTOR_MANIFEST_ARCHIVE_AND_FULL_GZIP_EOF_VERIFIED','payload_files_excluding_manifest':len(members),'manifest':identity(root/'MANIFEST.json'),'ancestor_manifest':ancestor_manifest,'archive':identity(archive),'gzip_uncompressed_bytes':size,'gzip_uncompressed_sha256':h.hexdigest(),'all15_complete':False,'new_performance_credit':False}
(S/'publication-transport.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
