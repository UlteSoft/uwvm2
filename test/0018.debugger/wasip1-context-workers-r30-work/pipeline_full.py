from pathlib import Path
import sys,subprocess,json,time,hashlib
L=Path(__file__).parent;E=Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17');D=E/'rounds/wasip1-context-workers-20261007-r30'
ssh=['ssh','-i',str(Path.home()/'.ssh/id_ed25519'),'macromodel@100.123.133.75'];rows=[]
def run(phase,*args):
 argv=[*ssh,'python3','-u',str(E/('guard-joint-'+phase+'-r30.py')),'joint-'+phase+'-r30',*args]
 print('R30 PIPELINE',phase,*args,flush=True);start=time.monotonic();q=subprocess.run(argv)
 rows.append(dict(argv=argv,exit=q.returncode,seconds=time.monotonic()-start));(L/'pipeline-full-r30.json').write_text(json.dumps(rows,indent=2)+'\n');assert q.returncode==0,(phase,args)
def archive(platform,repo):
 run('archive',platform,repo)
 subprocess.run([sys.executable,'-u',str(L/'rehome_product.py'),platform,repo],check=True)
# The first Linux build/test was dispatched explicitly and must be qualified.
check="import json,time;from pathlib import Path;p=Path("+repr(str(E/'guard-joint-linux-integrated-r30-uwvm2.json'))+")\nstart=time.monotonic()\nwhile not p.exists():\n assert time.monotonic()-start<1800\n time.sleep(2)\nq=json.loads(p.read_text());assert q['passed'] and q['actual_root_exit']==0"
subprocess.run([*ssh,'python3 -c '+__import__('shlex').quote(check)],check=True)
archive('linux-integrated','uwvm2')
run('linux-integrated','uwvm2-ros');archive('linux-integrated','uwvm2-ros')
for platform in ('windows','freebsd'):
 for repo in ('uwvm2','uwvm2-ros'):
  run('cross',platform,repo);run('vm',platform,repo);archive(platform,repo)
subprocess.run([sys.executable,'-u',str(L/'pipeline_macos.py')],check=True)
print('R30 four native operating system matrices qualified',flush=True)
