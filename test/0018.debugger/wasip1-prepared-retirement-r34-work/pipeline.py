from pathlib import Path
import subprocess,json,time,sys,hashlib
L=Path(__file__).parent;E='/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17';D=E+'/rounds/wasip1-prepared-retirement-20261008-r34';ssh=['ssh','-i','/Users/liyinan/.ssh/id_ed25519','macromodel@100.123.133.75'];scp=['scp','-i','/Users/liyinan/.ssh/id_ed25519'];rows=json.loads((L/'pipeline.json').read_text()) if (L/'pipeline.json').exists() else []
def run(label,argv):
 old=next((r for r in rows if r['label']==label and r['exit']==0),None)
 if old:assert old['argv']==list(map(str,argv));print('completed stage retained',label,flush=True);return
 print('R34 PIPELINE',label,flush=True);start=time.monotonic();p=subprocess.run(list(map(str,argv)));row=dict(label=label,argv=list(map(str,argv)),exit=p.returncode,seconds=time.monotonic()-start);rows.append(row);(L/'pipeline.json').write_text(json.dumps(rows,indent=2)+'\n');assert p.returncode==0,label

def remote(phase,*args,guard=None):
 run('-'.join((phase,*args)),[*ssh,'python3','-u',E+'/'+(guard or 'guard-joint-'+phase+'-r34.py'),'joint-'+phase+'-r34',*args])

remote('archive','linux-integrated','uwvm2')
remote('linux','uwvm2-ros');remote('archive','linux-integrated','uwvm2-ros')
for platform in ('windows','freebsd'):
 for repo in ('uwvm2','uwvm2-ros'):
  remote('cross',platform,repo);remote('object-cache-retire',platform,repo);remote('executable-cache-retire',platform,repo);remote('vm',platform,repo);remote('archive',platform,repo)
for repo in ('uwvm2','uwvm2-ros'):
 remote('cross','macos',repo,guard='guard-joint-cross-macos-ordinary-12g-r34.py' if repo=='uwvm2' else None)
 run('macos-cross-receipt-'+repo,[*scp,'macromodel@100.123.133.75:'+D+'/products/macos/'+repo+'/qualified.json',L/(repo+'-macos-cross-qualified.json')])
 run('macos-native-'+repo,[sys.executable,'-u',L/'pipeline_macos.py',repo])
print('R34 both repositories and four OS actual native matrix completed',flush=True)
