from pathlib import Path
import sys,subprocess,json,time
L=Path(__file__).parent;E=Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17');D=E/'rounds/wasip1-context-workers-20261007-r30'
ssh=['ssh','-i',str(Path.home()/'.ssh/id_ed25519'),'macromodel@100.123.133.75'];rows=[]
def run(phase,*args):
 argv=[*ssh,'python3','-u',str(E/('guard-joint-'+phase+'-r30.py')),'joint-'+phase+'-r30',*args]
 print('R30 RESUME',phase,*args,flush=True);start=time.monotonic();q=subprocess.run(argv)
 rows.append(dict(argv=argv,exit=q.returncode,seconds=time.monotonic()-start));(L/'resume-native-r30.json').write_text(json.dumps(rows,indent=2)+'\n');assert q.returncode==0,(phase,args)
def archive(platform,repo):
 run('archive',platform,repo)
 subprocess.run([sys.executable,'-u',str(L/'rehome_product.py'),platform,repo],check=True)
check="from pathlib import Path;import json;E=Path("+repr(str(E))+");assert json.loads((E/'guard-joint-rehome-r29-r30.json').read_text())['passed'];assert json.loads((E/'guard-joint-cross-r30-windows-uwvm2.json').read_text())['passed']"
subprocess.run([*ssh,'python3 -c '+__import__('shlex').quote(check)],check=True)
run('vm','windows','uwvm2');archive('windows','uwvm2')
run('cross','windows','uwvm2-ros');run('vm','windows','uwvm2-ros');archive('windows','uwvm2-ros')
for repo in ('uwvm2','uwvm2-ros'):
 run('cross','freebsd',repo);run('vm','freebsd',repo);archive('freebsd',repo)
subprocess.run([sys.executable,'-u',str(L/'pipeline_macos.py')],check=True)
print('R30 full native matrix completed',flush=True)
subprocess.run([sys.executable,'-u',str(L/'finish.py')],check=True)
