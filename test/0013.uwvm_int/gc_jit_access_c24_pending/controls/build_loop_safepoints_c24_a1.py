from pathlib import Path
import sys,json,os,tarfile
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
S=R/'source/uwvm2-candidate-c24-a1'
with N.lease():
 N.check(build=True)
 binding=json.loads((S/'source-binding-c24-a1.json').read_text())
 for f,sha in binding['files'].items():assert N.sha(S/f)==sha,f
 pp=R/'source/build-plan-typed-array-access-inline-safepoint-c24-a1.json';plan=json.loads(pp.read_text());BD=R/'build/deferred-typed-array-access-inline-safepoint';disk=N.BASE/'artifacts/c24-candidate-a1/build';disk.mkdir(exist_ok=False);BD.symlink_to(disk,target_is_directory=True)
 flags=plan['common']+next(iter(plan['variants'].values()))+['-I'+str(S/'src')]+['-I'+str(S/'third-parties'/x/'include') for x in ('fast_io','bizwen','boost_unordered')];env=dict(os.environ,TMPDIR=str(R/'tmp'))
 archive=R/'source/uwvm2-candidate-c24-a1-bound-source.tar.gz'
 with tarfile.open(archive,'w:gz') as t:t.add(S,arcname=S.name,recursive=True)
 # S is a directory symlink; bind the actual tree, not a link-only tar.
 if archive.stat().st_size<1000000:
  archive.unlink()
  with tarfile.open(archive,'w:gz') as t:t.add(S.resolve(),arcname=S.name,recursive=True)
 N.persist(archive);assert N.sha(archive)==N.sha(N.BASE/'recovery/source'/archive.name)
 for i,tu in enumerate(plan['tus']):
  N.run('typed-array-access-compile-c24-a1-'+str(i),[plan['compiler'],*flags,'-c',S/tu,'-o',BD/(str(i)+'.o')],cpu=8,timeout=600,build=True,env=env);print('C24_COMPILED',i,flush=True)
 elf=BD/'uwvm';N.run('typed-array-access-link-c24-a1',[plan['compiler'],*[BD/(str(i)+'.o') for i in range(3)],'@'+plan['link_rsp'],'-stdlib=libc++','-pthread','-ldl','-lz','-l:libzstd.so.1','-lm','-lcrypto','-lssl','-Wl,--gc-sections','-Wl,--strip-debug','-o',elf],cpu=8,timeout=120,build=True,env=env);N.persist(elf)
 N.save(BD/'build-binding-c24-a1.json',dict(ELF_sha=N.sha(elf),plan_sha=N.sha(pp),source_binding_sha=N.sha(S/'source-binding-c24-a1.json'),source_archive_sha=N.sha(archive),flags=flags,objects={str(i):N.sha(BD/(str(i)+'.o')) for i in range(3)},status='all3TUs same33flags as C23, qualification pending'))
 print('C24_ELF',N.sha(elf),flush=True)
