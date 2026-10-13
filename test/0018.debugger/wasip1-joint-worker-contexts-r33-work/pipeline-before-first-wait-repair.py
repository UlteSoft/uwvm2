from pathlib import Path
import subprocess,json,time,sys,shlex
L=Path(__file__).parent;E=Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17')
ssh=['ssh','-i',str(Path.home()/'.ssh/id_ed25519'),'macromodel@100.123.133.75']
rows=[]
def run(label,argv):
 print('R33 PIPELINE',label,flush=True);at=time.monotonic();q=subprocess.run(list(map(str,argv)))
 rows.append(dict(label=label,argv=list(map(str,argv)),exit=q.returncode,seconds=time.monotonic()-at));(L/'pipeline-remainder.json').write_text(json.dumps(rows,indent=2)+'\n');assert q.returncode==0,label
def guard(phase,*args):run(phase+'-'+'-'.join(args),[*ssh,'python3','-u',E/('guard-joint-'+phase+'-r33.py'),'joint-'+phase+'-r33',*args])
def archive(platform,repo):
 guard('archive',platform,repo);run('local-cold-'+platform+'-'+repo,[sys.executable,'-u',L/'rehome_product.py',platform,repo])
# The first Linux job was issued interactively. Wait for its actual retired guard
# proof instead of adopting its processes or reusing a failed binary.
proof=E/'guard-joint-linux-integrated-r33-uwvm2.json'
code='from pathlib import Path;import json,sys;p=Path('+repr(str(proof))+');sys.exit(10 if not p.exists() else 0 if json.loads(p.read_text())["passed"] else 20)'
at=time.monotonic()
while True:
 q=subprocess.run([*ssh,'python3 -c '+shlex.quote(code)],stdout=subprocess.DEVNULL)
 if q.returncode==0:break
 assert q.returncode==10 and time.monotonic()-at<1200,'first actual Linux qualification failed';time.sleep(10)
archive('linux-integrated','uwvm2');guard('linux-integrated','uwvm2-ros');archive('linux-integrated','uwvm2-ros')
for platform in ('windows','freebsd'):
 for repo in ('uwvm2','uwvm2-ros'):
  guard('cross',platform,repo);guard('vm',platform,repo);archive(platform,repo)
run('native-macos-matrix',[sys.executable,'-u',L/'pipeline_macos.py'])
guard('report')
print('R33 all matrix qualified',flush=True)
