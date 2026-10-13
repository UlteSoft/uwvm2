from pathlib import Path
import sys,json,os,tarfile
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
S=R/'source/uwvm2-candidate-c23-a3'
with N.lease():
 N.check(build=True)
 binding=json.loads((S/'source-binding-c23-a3.json').read_text())
 for f,sha in binding['files'].items():assert N.sha(S/f)==sha,f
 pp=R/'source/build-plan-typed-array-access-inline-static32-c23-a3.json';plan=json.loads(pp.read_text());BD=R/'build/deferred-typed-array-access-inline-static32';BD.mkdir(exist_ok=False)
 flags=plan['common']+next(iter(plan['variants'].values()))+['-I'+str(S/'src')]+['-I'+str(S/'third-parties'/x/'include') for x in ('fast_io','bizwen','boost_unordered')];env=dict(os.environ,TMPDIR=str(R/'tmp'))
 archive=R/'source/uwvm2-candidate-c23-a3-bound-source.tar.gz'
 with tarfile.open(archive,'w:gz') as t:t.add(S,arcname=S.name)
 N.persist(archive);assert N.sha(archive)==N.sha(N.BASE/'recovery/source'/archive.name)
 for i,tu in enumerate(plan['tus']):
  N.run('typed-array-access-compile-c23-a3-'+str(i),[plan['compiler'],*flags,'-c',S/tu,'-o',BD/(str(i)+'.o')],cpu=4,timeout=600,build=True,env=env);print('C23_COMPILED',i,flush=True)
 elf=BD/'uwvm';N.run('typed-array-access-link-c23-a3',[plan['compiler'],*[BD/(str(i)+'.o') for i in range(3)],'@'+plan['link_rsp'],'-stdlib=libc++','-pthread','-ldl','-lz','-l:libzstd.so.1','-lm','-lcrypto','-lssl','-Wl,--gc-sections','-Wl,--strip-debug','-o',elf],cpu=4,timeout=120,build=True,env=env);N.persist(elf)
 N.save(BD/'build-binding-c23-a3.json',{'ELF_sha':N.sha(elf),'plan_sha':N.sha(pp),'source_binding_sha':N.sha(S/'source-binding-c23-a3.json'),'source_archive_sha':N.sha(archive),'flags':flags,'objects':{str(i):N.sha(BD/(str(i)+'.o')) for i in range(3)},'status':'all three TUs rebuilt; qualification pending'})
 print('C23_ELF',N.sha(elf),flush=True)
