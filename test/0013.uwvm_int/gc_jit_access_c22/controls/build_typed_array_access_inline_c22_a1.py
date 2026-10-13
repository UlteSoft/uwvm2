from pathlib import Path
import sys,json,os,tarfile
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
S=R/'source/uwvm2-candidate-c22'
with N.lease():
 N.check(build=True)
 binding=json.loads((S/'source-binding-c22.json').read_text())
 for f,sha in binding['files'].items():assert N.sha(S/f)==sha,f
 pp=R/'source/build-plan-typed-array-access-inline-c22.json';plan=json.loads(pp.read_text());BD=R/'build/deferred-typed-array-access-inline';BD.mkdir(exist_ok=False)
 flags=plan['common']+next(iter(plan['variants'].values()))+['-I'+str(S/'src')]+['-I'+str(S/'third-parties'/x/'include') for x in ('fast_io','bizwen','boost_unordered')];env=dict(os.environ,TMPDIR=str(R/'tmp'))
 archive=R/'source/uwvm2-candidate-c22-bound-source.tar.gz'
 with tarfile.open(archive,'w:gz') as t:t.add(S,arcname=S.name)
 N.persist(archive);assert N.sha(archive)==N.sha(N.BASE/'recovery/source'/archive.name)
 for i,tu in enumerate(plan['tus']):
  N.run('typed-array-access-compile-c22-a1-'+str(i),[plan['compiler'],*flags,'-c',S/tu,'-o',BD/(str(i)+'.o')],cpu=4,timeout=600,build=True,env=env);print('C22_COMPILED',i,flush=True)
 elf=BD/'uwvm';N.run('typed-array-access-link-c22-a1',[plan['compiler'],*[BD/(str(i)+'.o') for i in range(3)],'@'+plan['link_rsp'],'-stdlib=libc++','-pthread','-ldl','-lz','-l:libzstd.so.1','-lm','-lcrypto','-lssl','-Wl,--gc-sections','-Wl,--strip-debug','-o',elf],cpu=4,timeout=120,build=True,env=env);N.persist(elf)
 N.save(BD/'build-binding-c22.json',{'ELF_sha':N.sha(elf),'plan_sha':N.sha(pp),'source_binding_sha':N.sha(S/'source-binding-c22.json'),'source_archive_sha':N.sha(archive),'flags':flags,'objects':{str(i):N.sha(BD/(str(i)+'.o')) for i in range(3)},'status':'all three TUs rebuilt; qualification pending'})
 print('C22_ELF',N.sha(elf),flush=True)
