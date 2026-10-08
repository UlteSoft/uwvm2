from pathlib import Path
import sys,subprocess,json,hashlib,time
D=Path(__file__).parent;E=D.parent.parent
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
assert sha(D/'inputs.json')=='aaa1bd283b4ec5ca0cefd28229083a609c9284fcf33fe84650ff2441f4a86f85'
steps=[('archive','linux-integrated','uwvm2'),('linux-integrated','uwvm2-ros'),('archive','linux-integrated','uwvm2-ros')]
for osname in ('windows','freebsd'):
 for repo in ('uwvm2','uwvm2-ros'):
  steps.extend([('cross',osname,repo),('vm',osname,repo),('archive',osname,repo)])
rows=[]
for phase,*args in steps:
 cmd=[sys.executable,str(E/('guard-joint-'+phase+'-r28.py')),'joint-'+phase+'-r28',*args]
 print('R28 PIPELINE',phase,*args,flush=True);start=time.monotonic()
 p=subprocess.run(cmd)
 row=dict(argv=cmd,exit=p.returncode,seconds=time.monotonic()-start);rows.append(row);(D/'pipeline-native-r28.json').write_text(json.dumps(rows,indent=2)+'\n')
 assert p.returncode==0,(phase,args)
 if phase=='archive' and args[0]=='linux-integrated' and args[1]=='uwvm2':
  group=json.loads((D/'products/linux-integrated/uwvm2/environment-group-qualified.json').read_text());checks=group['native_cases']['instruction']['checks']
  p=D/'vm.py';s=p.read_text();assert "checks=1,minimum=True" in s;s=s.replace('checks=1,minimum=True','checks='+str(checks));p.write_text(s)
print('R28 Linux Windows FreeBSD native matrix completed',flush=True)
