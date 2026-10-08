from pathlib import Path
import sys,subprocess,json,hashlib,time
D=Path(__file__).parent;E=D.parent.parent
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
assert sha(D/'inputs.json')=='194ff28bd7ea121fa338698e9cff6ba427c52fb7522e2fca9ae066ff1433b9be'
steps=[]
for repo in ('uwvm2','uwvm2-ros'):
 steps.extend([('linux-integrated',repo),('archive','linux-integrated',repo)])
for osname in ('windows','freebsd'):
 for repo in ('uwvm2','uwvm2-ros'):
  steps.extend([('cross',osname,repo),('vm',osname,repo),('archive',osname,repo)])
rows=[]
for phase,*args in steps:
 cmd=[sys.executable,str(E/('guard-joint-'+phase+'-r29.py')),'joint-'+phase+'-r29',*args]
 print('R29 PIPELINE',phase,*args,flush=True);start=time.monotonic()
 p=subprocess.run(cmd)
 row=dict(argv=cmd,exit=p.returncode,seconds=time.monotonic()-start);rows.append(row);(D/'pipeline-native-r29.json').write_text(json.dumps(rows,indent=2)+'\n')
 assert p.returncode==0,(phase,args)
print('R29 Linux Windows FreeBSD native matrix completed',flush=True)
